-- Nhill migration 0008 — console operations and billing:
--   plan prices per duration and currency, plan changes (upgrade / downgrade),
--   subscription promo codes, payment providers (secrets encrypted by the server),
--   support tickets, technical error events and console notifications.
--
-- Vocabulary kept distinct on purpose:
--   establishment id      : permanent identity (uuid), never shown as a secret;
--   establishment code    : renewable join code (establishment_codes), rotates;
--   subscription          : usage rights (subscriptions), driven by paid orders.

-- ---------------------------------------------------------------- plans
ALTER TABLE plans
    ADD COLUMN tier            integer NOT NULL DEFAULT 0,          -- order used for upgrade / downgrade
    ADD COLUMN max_registers   integer CHECK (max_registers IS NULL OR max_registers >= 0),  -- POS registers
    ADD COLUMN max_warehouses  integer CHECK (max_warehouses IS NULL OR max_warehouses > 0);
UPDATE plans SET tier = sort_order;

CREATE TABLE plan_prices (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    plan_id          uuid NOT NULL REFERENCES plans(id) ON DELETE CASCADE,
    duration_months  integer NOT NULL CHECK (duration_months IN (1, 3, 6, 12, 24, 36)),
    currency         char(3) NOT NULL REFERENCES currencies(code),
    price_minor      bigint NOT NULL CHECK (price_minor >= 0),
    active           boolean NOT NULL DEFAULT true,
    created_at       timestamptz NOT NULL DEFAULT now(),
    updated_at       timestamptz NOT NULL DEFAULT now(),
    UNIQUE (plan_id, duration_months, currency)
);
CREATE TRIGGER plan_prices_touch BEFORE UPDATE ON plan_prices FOR EACH ROW EXECUTE FUNCTION nh_touch_updated_at();
INSERT INTO plan_prices (plan_id, duration_months, currency, price_minor)
SELECT id, CASE billing_period WHEN 'annual' THEN 12 ELSE 1 END, currency, price_minor FROM plans;
-- The legacy columns of plans (billing_period, price_minor, currency) remain the plan's
-- main price and are mirrored into plan_prices.
CREATE FUNCTION nh_plan_main_price() RETURNS trigger
LANGUAGE plpgsql SET search_path = nhill, pg_catalog AS $$
BEGIN
    INSERT INTO plan_prices (plan_id, duration_months, currency, price_minor)
    VALUES (NEW.id, CASE NEW.billing_period WHEN 'annual' THEN 12 ELSE 1 END, NEW.currency, NEW.price_minor)
    ON CONFLICT (plan_id, duration_months, currency) DO UPDATE SET price_minor = EXCLUDED.price_minor, active = true;
    RETURN NULL;
END $$;
REVOKE ALL ON FUNCTION nh_plan_main_price() FROM PUBLIC;
CREATE TRIGGER plans_main_price AFTER INSERT OR UPDATE OF billing_period, price_minor, currency ON plans
    FOR EACH ROW EXECUTE FUNCTION nh_plan_main_price();

-- ---------------------------------------------------------------- subscriptions
ALTER TABLE subscriptions
    ADD COLUMN price_id          uuid REFERENCES plan_prices(id),
    ADD COLUMN pending_price_id  uuid REFERENCES plan_prices(id),     -- paid downgrade, effective at pending_from
    ADD COLUMN pending_from      timestamptz,
    ADD COLUMN failed_payments   integer NOT NULL DEFAULT 0,
    ADD COLUMN last_failure_at   timestamptz;
UPDATE subscriptions s SET price_id = pp.id
FROM plan_prices pp JOIN plans p ON p.id = pp.plan_id
WHERE s.plan_id = pp.plan_id AND pp.currency = p.currency
  AND pp.duration_months = CASE p.billing_period WHEN 'annual' THEN 12 ELSE 1 END;

-- ---------------------------------------------------------------- payment providers
-- Non-secret settings in clear; secrets sealed by the server (XSalsa20-Poly1305 with a
-- key derived from NH_SECRET_KEY, which never reaches the database). The API never
-- returns them: it only lists which secret fields are set.
CREATE TABLE payment_providers (
    id             uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    code           text NOT NULL UNIQUE CHECK (code ~ '^[a-z0-9][a-z0-9_-]{1,39}$'),  -- webhook path segment
    kind           text NOT NULL CHECK (kind IN ('generic_hmac', 'stripe', 'paypal', 'payoneer')),
    display_name   text NOT NULL CHECK (length(display_name) BETWEEN 1 AND 80),
    environment    text NOT NULL DEFAULT 'test' CHECK (environment IN ('test', 'live')),
    enabled        boolean NOT NULL DEFAULT false,
    currencies     char(3)[] NOT NULL DEFAULT '{}',
    settings       jsonb NOT NULL DEFAULT '{}'::jsonb,
    secret_cipher  bytea,
    secret_nonce   bytea,
    secret_fields  text[] NOT NULL DEFAULT '{}',
    sort_order     integer NOT NULL DEFAULT 0,
    created_at     timestamptz NOT NULL DEFAULT now(),
    updated_at     timestamptz NOT NULL DEFAULT now(),
    updated_by     uuid REFERENCES admin_users(id),
    CHECK ((secret_cipher IS NULL) = (secret_nonce IS NULL))
);
CREATE TRIGGER payment_providers_touch BEFORE UPDATE ON payment_providers FOR EACH ROW EXECUTE FUNCTION nh_touch_updated_at();

-- ---------------------------------------------------------------- promo codes (subscriptions)
CREATE TABLE promo_codes (
    id                     uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    code                   text NOT NULL UNIQUE CHECK (code ~ '^[A-Z0-9][A-Z0-9_-]{2,31}$'),
    description            text NOT NULL DEFAULT '',
    discount_kind          text NOT NULL CHECK (discount_kind IN ('percent', 'fixed')),
    percent_bp             integer CHECK (percent_bp BETWEEN 1 AND 10000),
    amount_minor           bigint CHECK (amount_minor > 0),
    currency               char(3) REFERENCES currencies(code),
    valid_from             timestamptz NOT NULL DEFAULT now(),
    valid_until            timestamptz,
    max_redemptions        integer CHECK (max_redemptions IS NULL OR max_redemptions > 0),
    max_per_establishment  integer NOT NULL DEFAULT 1 CHECK (max_per_establishment > 0),
    plan_ids               uuid[],                       -- NULL = every plan
    min_duration_months    integer CHECK (min_duration_months IS NULL OR min_duration_months > 0),
    new_customers_only     boolean NOT NULL DEFAULT false,  -- never paid an order before
    audience               text NOT NULL DEFAULT 'any' CHECK (audience IN ('individual', 'company', 'any')),
    active                 boolean NOT NULL DEFAULT true,
    created_by             uuid REFERENCES admin_users(id),
    created_at             timestamptz NOT NULL DEFAULT now(),
    updated_at             timestamptz NOT NULL DEFAULT now(),
    CHECK ((discount_kind = 'percent' AND percent_bp IS NOT NULL AND amount_minor IS NULL AND currency IS NULL)
        OR (discount_kind = 'fixed' AND amount_minor IS NOT NULL AND currency IS NOT NULL AND percent_bp IS NULL)),
    CHECK (valid_until IS NULL OR valid_until > valid_from)
);
CREATE TRIGGER promo_codes_touch BEFORE UPDATE ON promo_codes FOR EACH ROW EXECUTE FUNCTION nh_touch_updated_at();

-- ---------------------------------------------------------------- subscription orders
ALTER TABLE subscription_orders DROP CONSTRAINT subscription_orders_status_check;
ALTER TABLE subscription_orders
    ADD CONSTRAINT subscription_orders_status_check
        CHECK (status IN ('pending', 'paid', 'failed', 'canceled', 'refunded', 'expired')),
    ADD COLUMN kind              text NOT NULL DEFAULT 'renewal'
        CHECK (kind IN ('new', 'renewal', 'upgrade', 'downgrade', 'admin')),
    ADD COLUMN price_id          uuid REFERENCES plan_prices(id),
    ADD COLUMN duration_months   integer,
    ADD COLUMN subtotal_minor    bigint,
    ADD COLUMN discount_minor    bigint NOT NULL DEFAULT 0 CHECK (discount_minor >= 0),
    ADD COLUMN credit_minor      bigint NOT NULL DEFAULT 0 CHECK (credit_minor >= 0),   -- prorated unused time
    ADD COLUMN tax_minor         bigint NOT NULL DEFAULT 0 CHECK (tax_minor >= 0),
    ADD COLUMN promo_code_id     uuid REFERENCES promo_codes(id),
    ADD COLUMN provider_id       uuid REFERENCES payment_providers(id),
    ADD COLUMN provider_session  text,                 -- checkout session / order id at the provider
    ADD COLUMN checkout_url      text,
    ADD COLUMN expires_at        timestamptz,
    ADD COLUMN failure_reason    text;
UPDATE subscription_orders SET subtotal_minor = amount_minor WHERE subtotal_minor IS NULL;
CREATE INDEX subscription_orders_est ON subscription_orders (establishment_id, created_at DESC);
CREATE INDEX subscription_orders_open ON subscription_orders (expires_at) WHERE status = 'pending';

CREATE TABLE promo_redemptions (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    promo_code_id    uuid NOT NULL REFERENCES promo_codes(id),
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    order_id         uuid NOT NULL UNIQUE REFERENCES subscription_orders(id) ON DELETE CASCADE,
    status           text NOT NULL DEFAULT 'reserved' CHECK (status IN ('reserved', 'confirmed', 'released')),
    discount_minor   bigint NOT NULL CHECK (discount_minor >= 0),
    currency         char(3) NOT NULL REFERENCES currencies(code),
    created_at       timestamptz NOT NULL DEFAULT now(),
    settled_at       timestamptz
);
CREATE INDEX promo_redemptions_code ON promo_redemptions (promo_code_id) WHERE status <> 'released';
CREATE INDEX promo_redemptions_est ON promo_redemptions (establishment_id, promo_code_id) WHERE status <> 'released';

-- ---------------------------------------------------------------- support tickets
CREATE TABLE support_tickets (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    number           bigint GENERATED ALWAYS AS IDENTITY UNIQUE,
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    created_by       uuid REFERENCES users(id),
    source           text NOT NULL CHECK (source IN ('business', 'pos', 'console')),
    category         text NOT NULL CHECK (category IN ('bug', 'question', 'billing', 'feature')),
    severity         text NOT NULL DEFAULT 'normal' CHECK (severity IN ('low', 'normal', 'high', 'critical')),
    subject          text NOT NULL CHECK (length(subject) BETWEEN 3 AND 160),
    status           text NOT NULL DEFAULT 'open'
        CHECK (status IN ('open', 'in_progress', 'waiting_customer', 'resolved', 'closed')),
    app_version      text,
    platform         text,
    assigned_to      uuid REFERENCES admin_users(id),
    created_at       timestamptz NOT NULL DEFAULT now(),
    updated_at       timestamptz NOT NULL DEFAULT now(),
    last_message_at  timestamptz NOT NULL DEFAULT now(),
    resolved_at      timestamptz
);
CREATE TRIGGER support_tickets_touch BEFORE UPDATE ON support_tickets FOR EACH ROW EXECUTE FUNCTION nh_touch_updated_at();
CREATE INDEX support_tickets_est ON support_tickets (establishment_id, last_message_at DESC);
CREATE INDEX support_tickets_queue ON support_tickets (status, last_message_at DESC);

CREATE TABLE support_ticket_messages (
    id          bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    ticket_id   uuid NOT NULL REFERENCES support_tickets(id) ON DELETE CASCADE,
    author_type text NOT NULL CHECK (author_type IN ('user', 'admin', 'system')),
    author_id   uuid,
    body        text NOT NULL CHECK (length(body) BETWEEN 1 AND 10000),
    internal    boolean NOT NULL DEFAULT false,      -- console-only note, never shown to the establishment
    created_at  timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX support_ticket_messages_ticket ON support_ticket_messages (ticket_id, id);

-- ---------------------------------------------------------------- technical errors
CREATE TABLE error_events (
    id               bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    source           text NOT NULL CHECK (source IN ('server', 'business', 'console', 'pos')),
    level            text NOT NULL DEFAULT 'error' CHECK (level IN ('warning', 'error', 'fatal')),
    code             text NOT NULL CHECK (length(code) BETWEEN 1 AND 100),
    message          text NOT NULL CHECK (length(message) <= 2000),
    fingerprint      text NOT NULL,
    context          jsonb NOT NULL DEFAULT '{}'::jsonb,
    establishment_id uuid REFERENCES establishments(id) ON DELETE SET NULL,
    user_id          uuid,
    app_version      text,
    platform         text,
    occurrences      integer NOT NULL DEFAULT 1,
    first_seen       timestamptz NOT NULL DEFAULT now(),
    last_seen        timestamptz NOT NULL DEFAULT now(),
    resolved_at      timestamptz,
    resolved_by      uuid REFERENCES admin_users(id)
);
CREATE UNIQUE INDEX error_events_open ON error_events (source, fingerprint) WHERE resolved_at IS NULL;
CREATE INDEX error_events_recent ON error_events (last_seen DESC);

-- ---------------------------------------------------------------- console notifications
CREATE TABLE admin_notifications (
    id               bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    kind             text NOT NULL,
    params           jsonb NOT NULL DEFAULT '{}'::jsonb,
    establishment_id uuid REFERENCES establishments(id) ON DELETE CASCADE,
    created_at       timestamptz NOT NULL DEFAULT now(),
    read_at          timestamptz,
    read_by          uuid REFERENCES admin_users(id)
);
CREATE INDEX admin_notifications_unread ON admin_notifications (created_at DESC) WHERE read_at IS NULL;

INSERT INTO platform_settings (key, value, description) VALUES
    ('order_expiry_hours',         '48', 'Validité d''une demande de paiement d''abonnement (heures)'),
    ('promo_check_max_per_15min',  '15', 'Vérifications de code promotionnel par établissement et par 15 minutes'),
    ('tickets_max_per_day',        '20', 'Tickets d''assistance par établissement et par jour'),
    ('client_errors_max_per_15min','60', 'Rapports d''erreur client par utilisateur et par 15 minutes')
ON CONFLICT (key) DO NOTHING;

-- ---------------------------------------------------------------- hardening (Supabase advisors)
-- Row-level security: evaluate the tenant once per statement (init plan) instead of per row.
DO $$
DECLARE r record;
BEGIN
    FOR r IN SELECT c.relname FROM pg_policy p JOIN pg_class c ON c.oid = p.polrelid
             JOIN pg_namespace n ON n.oid = c.relnamespace
             WHERE n.nspname = 'nhill' AND p.polname = c.relname || '_tenant' LOOP
        EXECUTE format('DROP POLICY %I ON %I', r.relname || '_tenant', r.relname);
        EXECUTE format('CREATE POLICY %I ON %I USING (establishment_id = (SELECT nh_current_establishment())) '
                       'WITH CHECK (establishment_id = (SELECT nh_current_establishment()))', r.relname || '_tenant', r.relname);
    END LOOP;
END $$;
-- Fixed search_path on every function (definer functions already had one).
ALTER FUNCTION nh_current_establishment() SET search_path = nhill, pg_catalog;
ALTER FUNCTION nh_touch_updated_at() SET search_path = nhill, pg_catalog;
ALTER FUNCTION nh_guard_sales_document() SET search_path = nhill, pg_catalog;
ALTER FUNCTION nh_guard_sales_line() SET search_path = nhill, pg_catalog;
ALTER FUNCTION nh_guard_purchase() SET search_path = nhill, pg_catalog;
ALTER FUNCTION nh_iso(timestamptz) SET search_path = nhill, pg_catalog;
ALTER FUNCTION nh_window(integer) SET search_path = nhill, pg_catalog;
ALTER FUNCTION nh_setting_int(text, integer) SET search_path = nhill, pg_catalog;
ALTER FUNCTION nh_tz(uuid) SET search_path = nhill, pg_catalog;
ALTER FUNCTION nh_local_today() SET search_path = nhill, pg_catalog;
ALTER FUNCTION nh_local_start(date) SET search_path = nhill, pg_catalog;
ALTER FUNCTION nh_local_date(timestamptz) SET search_path = nhill, pg_catalog;
-- Indexes for tenant-scoped foreign keys used by lookups and cascades.
CREATE INDEX sessions_est ON sessions (establishment_id);
CREATE INDEX sales_document_lines_doc ON sales_document_lines (establishment_id, document_id);
CREATE INDEX purchase_lines_purchase ON purchase_lines (establishment_id, purchase_id);
CREATE INDEX payments_document ON payments (establishment_id, document_id) WHERE document_id IS NOT NULL;
CREATE INDEX payments_purchase ON payments (establishment_id, purchase_id) WHERE purchase_id IS NOT NULL;
CREATE INDEX payments_expense ON payments (establishment_id, expense_id) WHERE expense_id IS NOT NULL;
CREATE INDEX attachment_requests_est ON attachment_requests (establishment_id);
CREATE INDEX join_tickets_est ON join_tickets (establishment_id);
CREATE INDEX admin_sessions_admin ON admin_sessions (admin_id);
CREATE INDEX notifications_user ON notifications (user_id) WHERE user_id IS NOT NULL;

-- ---------------------------------------------------------------- privileges
GRANT SELECT, INSERT, UPDATE, DELETE ON plan_prices TO nhill_app;
GRANT SELECT, INSERT, UPDATE ON payment_providers, promo_codes, promo_redemptions, support_tickets,
    error_events, admin_notifications TO nhill_app;
GRANT SELECT, INSERT ON support_ticket_messages TO nhill_app;
GRANT USAGE ON ALL SEQUENCES IN SCHEMA nhill TO nhill_app;
