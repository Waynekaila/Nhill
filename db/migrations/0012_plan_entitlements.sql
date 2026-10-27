-- Nhill migration 0012 — configurable plans (3.0.0):
--   1. a catalogue of the capabilities the server enforces (features and quantitative
--      limits), with a value per plan that billing administrators change in Nhill Console
--      without any code change;
--   2. plan conditions: kind, visibility, self-service, renewal, grace, reminders, trial
--      duration and the plan used when a subscription expires;
--   3. automatic assignment rules applied when an establishment is created;
--   4. durations of 1 to 60 months for prices;
--   5. reminders per period and the computed access of an establishment.
--
-- Three notions are kept apart on purpose (docs/*/plans-et-fonctionnalites.md):
--   subscription status : lifecycle of the contract (subscriptions.status);
--   payment             : subscription_orders.status — a paid order extends the period, a
--                         pending one never grants anything;
--   access              : nh_subscription_access() — full, read_only or none, with a reason.
-- No existing column is dropped: the legacy plan limits are kept up to date from the
-- new table so that 2.x clients reading them see the same values.

-- ---------------------------------------------------------------- 1. capabilities
CREATE TABLE plan_capabilities (
    key            text PRIMARY KEY CHECK (key ~ '^[a-z][a-z0-9_.]{1,60}$'),
    kind           text NOT NULL CHECK (kind IN ('feature', 'limit')),
    label_fr       text NOT NULL,
    label_en       text NOT NULL,
    description_fr text NOT NULL DEFAULT '',
    description_en text NOT NULL DEFAULT '',
    unit           text,                                  -- limits: what is counted
    -- Value used when a plan has no entry for the capability.
    default_enabled boolean,                              -- features
    default_limit   integer CHECK (default_limit IS NULL OR default_limit >= 0),   -- limits, NULL = unlimited
    -- Value during a trial started without a plan (2.x behaviour).
    trial_enabled   boolean,
    trial_limit     integer CHECK (trial_limit IS NULL OR trial_limit >= 0),
    sort_order      integer NOT NULL DEFAULT 0,
    CHECK ((kind = 'feature' AND default_enabled IS NOT NULL AND trial_enabled IS NOT NULL
            AND default_limit IS NULL AND trial_limit IS NULL)
        OR (kind = 'limit' AND default_enabled IS NULL AND trial_enabled IS NULL AND unit IS NOT NULL))
);

INSERT INTO plan_capabilities (key, kind, label_fr, label_en, description_fr, description_en, unit,
                               default_enabled, default_limit, trial_enabled, trial_limit, sort_order) VALUES
    ('sales.invoices', 'feature', 'Factures et avoirs', 'Invoices and credit notes',
     'Émettre des factures et des avoirs ; les avoirs ne sont jamais bloqués par le quota mensuel de documents.',
     'Issue invoices and credit notes; credit notes are never blocked by the monthly document quota.', NULL, true, NULL, true, NULL, 10),
    ('sales.quotes', 'feature', 'Devis et bons de commande', 'Quotes and sales orders',
     'Créer des devis et des bons de commande.', 'Create quotes and sales orders.', NULL, true, NULL, true, NULL, 20),
    ('pos', 'feature', 'Caisse Nhill POS', 'Nhill POS till',
     'Caisses, sessions, ventes au comptoir, tickets, retours et rapports X/Z avec Nhill POS.',
     'Tills, sessions, counter sales, receipts, returns and X/Z reports with Nhill POS.', NULL, true, NULL, true, NULL, 30),
    ('purchases', 'feature', 'Achats fournisseurs', 'Supplier purchases',
     'Enregistrer et réceptionner des achats.', 'Record and receive purchases.', NULL, true, NULL, true, NULL, 40),
    ('expenses', 'feature', 'Dépenses et justificatifs', 'Expenses and receipts',
     'Enregistrer des dépenses avec leurs justificatifs.', 'Record expenses with their receipts.', NULL, true, NULL, true, NULL, 50),
    ('stock.multi_warehouse', 'feature', 'Plusieurs dépôts et transferts', 'Several warehouses and transfers',
     'Créer d''autres dépôts que le dépôt principal et transférer du stock entre eux.',
     'Create warehouses besides the main one and transfer stock between them.', NULL, false, NULL, true, NULL, 60),
    ('multi_currency', 'feature', 'Documents en plusieurs devises', 'Documents in several currencies',
     'Émettre des documents dans une autre devise que la devise principale.',
     'Issue documents in a currency other than the main currency.', NULL, true, NULL, true, NULL, 70),
    ('catalog.import', 'feature', 'Import du catalogue (CSV)', 'Catalogue import (CSV)',
     'Importer des produits depuis un fichier CSV.', 'Import products from a CSV file.', NULL, true, NULL, true, NULL, 80),
    ('reports.advanced', 'feature', 'Rapports d''analyse', 'Analysis reports',
     'Ventes par produit avec marge estimée et activité par employé. Les journaux et exports de données restent toujours disponibles.',
     'Sales by product with estimated margin and activity per employee. Journals and data exports always stay available.',
     NULL, true, NULL, true, NULL, 90),
    ('users.max', 'limit', 'Utilisateurs', 'Users',
     'Membres actifs et invitations en attente.', 'Active members and pending invitations.', 'users', NULL, NULL, NULL, NULL, 110),
    ('pos.registers.max', 'limit', 'Caisses POS', 'POS tills',
     'Caisses actives.', 'Active tills.', 'registers', NULL, NULL, NULL, 1, 120),
    ('stock.warehouses.max', 'limit', 'Dépôts', 'Warehouses',
     'Dépôts actifs, dépôt principal compris.', 'Active warehouses, main one included.', 'warehouses', NULL, NULL, NULL, NULL, 130),
    ('catalog.products.max', 'limit', 'Produits et services', 'Products and services',
     'Articles actifs du catalogue.', 'Active catalogue items.', 'products', NULL, NULL, NULL, NULL, 140),
    ('documents.month.max', 'limit', 'Documents émis par mois', 'Documents issued per month',
     'Devis, commandes, factures et tickets émis dans le mois civil ; les avoirs, retours et annulations ne sont jamais bloqués.',
     'Quotes, orders, invoices and receipts issued in the calendar month; credit notes, returns and voids are never blocked.',
     'documents', NULL, NULL, NULL, NULL, 150),
    ('storage.mb.max', 'limit', 'Stockage des fichiers (Mo)', 'File storage (MB)',
     'Logo et justificatifs de dépenses.', 'Logo and expense receipts.', 'mb', NULL, NULL, NULL, NULL, 160);

-- ---------------------------------------------------------------- 2. values per plan
CREATE TABLE plan_entitlements (
    plan_id     uuid NOT NULL REFERENCES plans(id) ON DELETE CASCADE,
    key         text NOT NULL REFERENCES plan_capabilities(key),
    enabled     boolean,                                   -- features
    limit_value integer CHECK (limit_value IS NULL OR limit_value >= 0),   -- limits, NULL = unlimited
    updated_at  timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (plan_id, key)
);

-- Existing plan settings become entries (no value is lost).
INSERT INTO plan_entitlements (plan_id, key, limit_value) SELECT id, 'users.max', max_users FROM plans WHERE max_users IS NOT NULL;
INSERT INTO plan_entitlements (plan_id, key, limit_value) SELECT id, 'pos.registers.max', max_registers FROM plans WHERE max_registers IS NOT NULL;
INSERT INTO plan_entitlements (plan_id, key, limit_value) SELECT id, 'stock.warehouses.max', max_warehouses FROM plans WHERE max_warehouses IS NOT NULL;
INSERT INTO plan_entitlements (plan_id, key, enabled)
SELECT id, 'stock.multi_warehouse', (features ->> 'multi_warehouse')::boolean FROM plans
 WHERE features ? 'multi_warehouse' AND jsonb_typeof(features -> 'multi_warehouse') = 'boolean';

-- Entries must match the kind of their capability.
CREATE FUNCTION nh_check_plan_entitlement() RETURNS trigger
LANGUAGE plpgsql SET search_path = nhill, pg_catalog AS $$
DECLARE k text;
BEGIN
    SELECT kind INTO k FROM plan_capabilities WHERE key = NEW.key;
    IF k = 'feature' AND (NEW.enabled IS NULL OR NEW.limit_value IS NOT NULL) THEN
        RAISE EXCEPTION 'feature % needs enabled and no limit', NEW.key USING ERRCODE = '23514';
    END IF;
    IF k = 'limit' AND NEW.enabled IS NOT NULL THEN
        RAISE EXCEPTION 'limit % takes limit_value only', NEW.key USING ERRCODE = '23514';
    END IF;
    NEW.updated_at := now();
    RETURN NEW;
END $$;
REVOKE ALL ON FUNCTION nh_check_plan_entitlement() FROM PUBLIC;
CREATE TRIGGER plan_entitlements_check BEFORE INSERT OR UPDATE ON plan_entitlements
    FOR EACH ROW EXECUTE FUNCTION nh_check_plan_entitlement();

-- Legacy columns of plans (read by 2.x clients) follow the entries.
CREATE FUNCTION nh_sync_plan_legacy_limits() RETURNS trigger
LANGUAGE plpgsql SET search_path = nhill, pg_catalog AS $$
DECLARE p uuid := COALESCE(NEW.plan_id, OLD.plan_id);
BEGIN
    UPDATE plans SET
        -- GREATEST ignores NULL: "unlimited" (NULL) is kept explicitly.
        max_users      = (SELECT CASE WHEN limit_value IS NULL THEN NULL ELSE GREATEST(limit_value, 1) END
                            FROM plan_entitlements WHERE plan_id = p AND key = 'users.max'),
        max_registers  = (SELECT limit_value FROM plan_entitlements WHERE plan_id = p AND key = 'pos.registers.max'),
        max_warehouses = (SELECT CASE WHEN limit_value IS NULL THEN NULL ELSE GREATEST(limit_value, 1) END
                            FROM plan_entitlements WHERE plan_id = p AND key = 'stock.warehouses.max'),
        features       = CASE WHEN EXISTS (SELECT 1 FROM plan_entitlements WHERE plan_id = p AND key = 'stock.multi_warehouse')
                              THEN features || jsonb_build_object('multi_warehouse',
                                   (SELECT enabled FROM plan_entitlements WHERE plan_id = p AND key = 'stock.multi_warehouse'))
                              ELSE features - 'multi_warehouse' END
     WHERE id = p;
    RETURN NULL;
END $$;
REVOKE ALL ON FUNCTION nh_sync_plan_legacy_limits() FROM PUBLIC;
CREATE TRIGGER plan_entitlements_legacy AFTER INSERT OR UPDATE OR DELETE ON plan_entitlements
    FOR EACH ROW EXECUTE FUNCTION nh_sync_plan_legacy_limits();

-- And the reverse: a statement that writes the legacy columns directly (2.x tooling,
-- maintenance SQL) updates the entries, so both views never disagree. Skipped when the
-- write comes from the trigger above (depth > 1).
CREATE FUNCTION nh_sync_plan_entitlements_from_legacy() RETURNS trigger
LANGUAGE plpgsql SET search_path = nhill, pg_catalog AS $$
BEGIN
    IF pg_trigger_depth() > 1 THEN RETURN NULL; END IF;
    IF TG_OP = 'INSERT' OR NEW.max_users IS DISTINCT FROM OLD.max_users THEN
        IF NEW.max_users IS NULL THEN
            DELETE FROM plan_entitlements WHERE plan_id = NEW.id AND key = 'users.max';
        ELSE
            INSERT INTO plan_entitlements (plan_id, key, limit_value) VALUES (NEW.id, 'users.max', NEW.max_users)
            ON CONFLICT (plan_id, key) DO UPDATE SET limit_value = EXCLUDED.limit_value;
        END IF;
    END IF;
    IF TG_OP = 'INSERT' OR NEW.max_registers IS DISTINCT FROM OLD.max_registers THEN
        IF NEW.max_registers IS NULL THEN
            DELETE FROM plan_entitlements WHERE plan_id = NEW.id AND key = 'pos.registers.max';
        ELSE
            INSERT INTO plan_entitlements (plan_id, key, limit_value) VALUES (NEW.id, 'pos.registers.max', NEW.max_registers)
            ON CONFLICT (plan_id, key) DO UPDATE SET limit_value = EXCLUDED.limit_value;
        END IF;
    END IF;
    IF TG_OP = 'INSERT' OR NEW.max_warehouses IS DISTINCT FROM OLD.max_warehouses THEN
        IF NEW.max_warehouses IS NULL THEN
            DELETE FROM plan_entitlements WHERE plan_id = NEW.id AND key = 'stock.warehouses.max';
        ELSE
            INSERT INTO plan_entitlements (plan_id, key, limit_value) VALUES (NEW.id, 'stock.warehouses.max', NEW.max_warehouses)
            ON CONFLICT (plan_id, key) DO UPDATE SET limit_value = EXCLUDED.limit_value;
        END IF;
    END IF;
    IF (TG_OP = 'INSERT' OR NEW.features -> 'multi_warehouse' IS DISTINCT FROM OLD.features -> 'multi_warehouse') THEN
        IF jsonb_typeof(NEW.features -> 'multi_warehouse') = 'boolean' THEN
            INSERT INTO plan_entitlements (plan_id, key, enabled)
            VALUES (NEW.id, 'stock.multi_warehouse', (NEW.features ->> 'multi_warehouse')::boolean)
            ON CONFLICT (plan_id, key) DO UPDATE SET enabled = EXCLUDED.enabled;
        ELSE
            DELETE FROM plan_entitlements WHERE plan_id = NEW.id AND key = 'stock.multi_warehouse';
        END IF;
    END IF;
    RETURN NULL;
END $$;
REVOKE ALL ON FUNCTION nh_sync_plan_entitlements_from_legacy() FROM PUBLIC;
CREATE TRIGGER plans_legacy_to_entitlements AFTER INSERT OR UPDATE OF max_users, max_registers, max_warehouses, features
    ON plans FOR EACH ROW EXECUTE FUNCTION nh_sync_plan_entitlements_from_legacy();

-- ---------------------------------------------------------------- 3. plan conditions
ALTER TABLE plans
    ADD COLUMN kind                    text NOT NULL DEFAULT 'paid' CHECK (kind IN ('paid', 'trial', 'free', 'custom')),
    ADD COLUMN visibility              text NOT NULL DEFAULT 'public' CHECK (visibility IN ('public', 'private')),
    ADD COLUMN self_service            boolean NOT NULL DEFAULT true,     -- orderable from Nhill Business
    ADD COLUMN auto_renew_allowed      boolean NOT NULL DEFAULT true,
    ADD COLUMN grace_days              integer CHECK (grace_days IS NULL OR grace_days BETWEEN 0 AND 90),
    ADD COLUMN reminder_days           integer[] CHECK (reminder_days IS NULL OR (cardinality(reminder_days) <= 6
                                           AND 0 <= ALL (reminder_days) AND 90 >= ALL (reminder_days))),
    ADD COLUMN trial_days              integer CHECK (trial_days IS NULL OR trial_days BETWEEN 1 AND 365),
    ADD COLUMN expiry_fallback_plan_id uuid REFERENCES plans(id),
    ADD COLUMN archived_at             timestamptz,
    ADD CONSTRAINT plans_fallback_not_self CHECK (expiry_fallback_plan_id IS NULL OR expiry_fallback_plan_id <> id);

-- ---------------------------------------------------------------- 4. durations
ALTER TABLE plan_prices DROP CONSTRAINT plan_prices_duration_months_check;
ALTER TABLE plan_prices ADD CONSTRAINT plan_prices_duration_months_check CHECK (duration_months BETWEEN 1 AND 60);

-- ---------------------------------------------------------------- 5. assignment rules
-- Applied in priority order when an establishment is created. "trial" rules need a
-- trial that was never granted to the same identity; "grant" rules always apply
-- (free or complimentary plans). No matching rule = 2.x trial (platform settings).
CREATE TABLE plan_assignment_rules (
    id             uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    priority       integer NOT NULL DEFAULT 100 CHECK (priority BETWEEN 0 AND 10000),
    active         boolean NOT NULL DEFAULT true,
    audience       text NOT NULL DEFAULT 'any' CHECK (audience IN ('individual', 'company', 'any')),
    country_code   char(2) CHECK (country_code IS NULL OR country_code ~ '^[A-Z]{2}$'),
    plan_id        uuid NOT NULL REFERENCES plans(id),
    mode           text NOT NULL CHECK (mode IN ('trial', 'grant')),
    duration_days  integer CHECK (duration_days IS NULL OR duration_days BETWEEN 1 AND 3660),   -- NULL: plan trial_days, or open-ended grant
    description    text NOT NULL DEFAULT '' CHECK (length(description) <= 300),
    created_by     uuid REFERENCES admin_users(id),
    created_at     timestamptz NOT NULL DEFAULT now(),
    updated_at     timestamptz NOT NULL DEFAULT now()
);
CREATE TRIGGER plan_assignment_rules_touch BEFORE UPDATE ON plan_assignment_rules
    FOR EACH ROW EXECUTE FUNCTION nh_touch_updated_at();
CREATE INDEX plan_assignment_rules_order ON plan_assignment_rules (priority, created_at) WHERE active;

-- ---------------------------------------------------------------- 6. subscriptions
ALTER TABLE subscriptions
    ADD COLUMN open_ended          boolean NOT NULL DEFAULT false,   -- free / complimentary plan without end
    ADD COLUMN suspension_reason   text CHECK (suspension_reason IS NULL OR suspension_reason IN
                                       ('expired', 'admin', 'no_trial')),
    ADD COLUMN assignment_rule_id  uuid REFERENCES plan_assignment_rules(id) ON DELETE SET NULL;
-- Reasons of the suspensions already recorded.
UPDATE subscriptions s SET suspension_reason = CASE
        WHEN s.period_end = s.period_start AND NOT s.is_trial THEN 'no_trial'
        WHEN EXISTS (SELECT 1 FROM subscription_events ev WHERE ev.subscription_id = s.id AND ev.kind = 'admin_suspend'
                      AND NOT EXISTS (SELECT 1 FROM subscription_events l WHERE l.subscription_id = s.id AND l.id > ev.id))
             THEN 'admin'
        ELSE 'expired' END
 WHERE s.status = 'suspended';

-- One row per reminder sent (several reminders per period, e.g. 14, 7, 3 and 1 day).
CREATE TABLE subscription_reminders (
    subscription_id uuid NOT NULL REFERENCES subscriptions(id) ON DELETE CASCADE,
    period_end      timestamptz NOT NULL,
    days_before     integer NOT NULL CHECK (days_before BETWEEN 0 AND 90),
    sent_at         timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (subscription_id, period_end, days_before)
);
-- Reminders already sent by 2.x are not repeated.
INSERT INTO subscription_reminders (subscription_id, period_end, days_before)
SELECT id, reminder_sent_for, nh_setting_int('reminder_days_before', 7) FROM subscriptions WHERE reminder_sent_for IS NOT NULL
ON CONFLICT DO NOTHING;

INSERT INTO platform_settings (key, value, description) VALUES
    ('reminder_schedule_days', '[14, 7, 3, 1]', 'Rappels avant l''échéance (jours), si le plan n''en fixe pas')
ON CONFLICT (key) DO NOTHING;

-- Quotas: issued documents per month (all types) and attachments size.
CREATE INDEX sales_documents_issued ON sales_documents (establishment_id, issued_at) WHERE issued_at IS NOT NULL;

-- ---------------------------------------------------------------- 7. computed access
-- full      : every operation allowed by the role and the plan;
-- read_only : consultation and exports only (expired, suspended, canceled, no trial,
--             establishment suspended by the platform);
-- none      : establishment closed.
-- Dates are compared with now() here, so access never depends on when the scheduled job
-- last ran (a period that just ended moves to the grace period at once).
CREATE FUNCTION nh_subscription_access(p_est uuid)
RETURNS TABLE (access text, reason text, access_until timestamptz, effective_plan_id uuid, is_trial boolean,
               status text, grace_end timestamptz)
LANGUAGE sql STABLE SET search_path = nhill, pg_catalog AS $$
    WITH x AS (
        SELECT e.status AS est_status, s.id AS sub_id, s.status, s.is_trial, s.period_end, s.open_ended,
               s.suspension_reason,
               CASE WHEN s.pending_price_id IS NOT NULL AND s.pending_from <= now()
                    THEN (SELECT plan_id FROM plan_prices WHERE id = s.pending_price_id) ELSE s.plan_id END AS plan_id,
               COALESCE(s.grace_until, s.period_end + make_interval(days =>
                        COALESCE(p.grace_days, nh_setting_int('grace_days', 7)))) AS grace_end
          FROM establishments e
          LEFT JOIN subscriptions s ON s.establishment_id = e.id
          LEFT JOIN plans p ON p.id = s.plan_id
         WHERE e.id = p_est
    )
    SELECT CASE WHEN est_status = 'closed' THEN 'none'
                WHEN est_status = 'suspended' OR sub_id IS NULL OR status IN ('suspended', 'canceled') THEN 'read_only'
                WHEN open_ended OR now() < grace_end THEN 'full'
                ELSE 'read_only' END,
           CASE WHEN est_status = 'closed' THEN 'establishment_closed'
                WHEN est_status = 'suspended' THEN 'establishment_suspended'
                WHEN sub_id IS NULL THEN 'no_subscription'
                WHEN status = 'canceled' THEN 'canceled'
                WHEN status = 'suspended' THEN COALESCE(suspension_reason, 'suspended')
                WHEN open_ended OR now() < period_end THEN CASE WHEN is_trial THEN 'trial' ELSE 'active' END
                WHEN now() < grace_end THEN 'grace'
                ELSE 'expired' END,
           CASE WHEN est_status IN ('closed', 'suspended') OR sub_id IS NULL OR status IN ('suspended', 'canceled')
                     OR open_ended THEN NULL
                WHEN now() < period_end THEN period_end
                WHEN now() < grace_end THEN grace_end END,
           plan_id, COALESCE(is_trial, false), status, grace_end
      FROM x
$$;
GRANT EXECUTE ON FUNCTION nh_subscription_access(uuid) TO nhill_app;

-- Usage counted against the limits. Counts only, never content (same rule as the
-- platform statistics of migration 0006): callable for one establishment at a time.
CREATE FUNCTION nh_establishment_usage(p_est uuid)
RETURNS TABLE (users integer, registers integer, warehouses integer, products integer, documents_month integer,
               storage_bytes bigint)
LANGUAGE plpgsql SECURITY DEFINER SET search_path = nhill, pg_catalog AS $$
DECLARE
    previous text := current_setting('app.establishment_id', true);
    month_start timestamptz := date_trunc('month', now() AT TIME ZONE nh_tz(p_est)) AT TIME ZONE nh_tz(p_est);
BEGIN
    PERFORM set_config('app.establishment_id', p_est::text, true);
    RETURN QUERY SELECT
        ((SELECT count(*) FROM memberships WHERE establishment_id = p_est AND status = 'active') +
         (SELECT count(*) FROM invitations WHERE establishment_id = p_est AND accepted_at IS NULL
             AND revoked_at IS NULL AND expires_at > now()))::integer,
        (SELECT count(*) FROM pos_registers WHERE active)::integer,
        (SELECT count(*) FROM warehouses WHERE active)::integer,
        (SELECT count(*) FROM products WHERE active)::integer,
        (SELECT count(*) FROM sales_documents WHERE issued_at >= month_start AND doc_type <> 'credit_note')::integer,
        (SELECT COALESCE(sum(byte_size), 0) FROM objects)::bigint;
    PERFORM set_config('app.establishment_id', COALESCE(previous, ''), true);
END $$;
REVOKE ALL ON FUNCTION nh_establishment_usage(uuid) FROM PUBLIC;
GRANT EXECUTE ON FUNCTION nh_establishment_usage(uuid) TO nhill_app;

-- ---------------------------------------------------------------- privileges
GRANT SELECT, UPDATE (label_fr, label_en, description_fr, description_en, sort_order) ON plan_capabilities TO nhill_app;
GRANT SELECT, INSERT, UPDATE, DELETE ON plan_entitlements, plan_assignment_rules TO nhill_app;
GRANT SELECT, INSERT ON subscription_reminders TO nhill_app;
