-- Nhill migration 0009 — Nhill POS (point of sale) attached to an establishment.
--
--   pos_registers        cash registers (points of sale), each tied to a warehouse;
--                        their number is limited by the plan (plans.max_registers);
--   pos_register_users   cashiers assigned to a register;
--   pos_payment_methods  payment methods offered at the counter (cash, card, mobile money…);
--   pos_sessions         cash sessions: opening float, closing count, variance, Z number;
--   pos_cash_movements   cash in / cash out during a session;
--   pos_approvals        one-time supervisor approvals (discount, return, void, cash out);
--   sales_promo_codes    promo codes of the establishment for its own sales.
-- Receipts are sales documents of type "receipt" (same numbering, snapshots, stock
-- movements and payments as Nhill Business). Returns and voids are credit notes.

-- ---------------------------------------------------------------- roles
ALTER TABLE memberships DROP CONSTRAINT memberships_role_check;
ALTER TABLE memberships ADD CONSTRAINT memberships_role_check
    CHECK (role IN ('owner', 'admin', 'manager', 'sales', 'accountant', 'viewer', 'cash_manager', 'cashier'));
ALTER TABLE invitations DROP CONSTRAINT invitations_role_check;
ALTER TABLE invitations ADD CONSTRAINT invitations_role_check
    CHECK (role IN ('admin', 'manager', 'sales', 'accountant', 'viewer', 'cash_manager', 'cashier'));

-- ---------------------------------------------------------------- registers and methods
CREATE TABLE pos_registers (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    code             text NOT NULL CHECK (code ~ '^[A-Z0-9-]{1,12}$'),
    name             text NOT NULL CHECK (length(btrim(name)) BETWEEN 1 AND 80),
    location         text NOT NULL DEFAULT '',
    warehouse_id     uuid NOT NULL,
    active           boolean NOT NULL DEFAULT true,
    created_at       timestamptz NOT NULL DEFAULT now(),
    updated_at       timestamptz NOT NULL DEFAULT now(),
    UNIQUE (establishment_id, code),
    UNIQUE (establishment_id, id),
    FOREIGN KEY (establishment_id, warehouse_id) REFERENCES warehouses (establishment_id, id)
);
CREATE TRIGGER pos_registers_touch BEFORE UPDATE ON pos_registers FOR EACH ROW EXECUTE FUNCTION nh_touch_updated_at();

CREATE TABLE pos_register_users (
    establishment_id uuid NOT NULL,
    register_id      uuid NOT NULL,
    user_id          uuid NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    created_at       timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (register_id, user_id),
    FOREIGN KEY (establishment_id, register_id) REFERENCES pos_registers (establishment_id, id) ON DELETE CASCADE
);
CREATE INDEX pos_register_users_user ON pos_register_users (establishment_id, user_id);

CREATE TABLE pos_payment_methods (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    code             text NOT NULL CHECK (code ~ '^[a-z0-9_-]{1,24}$'),
    label            text NOT NULL CHECK (length(btrim(label)) BETWEEN 1 AND 60),
    kind             text NOT NULL CHECK (kind IN ('cash', 'card', 'mobile_money', 'bank_transfer', 'cheque', 'other')),
    account_id       uuid,
    active           boolean NOT NULL DEFAULT true,
    sort_order       integer NOT NULL DEFAULT 0,
    created_at       timestamptz NOT NULL DEFAULT now(),
    UNIQUE (establishment_id, code),
    UNIQUE (establishment_id, id),
    FOREIGN KEY (establishment_id, account_id) REFERENCES cash_accounts (establishment_id, id)
);

CREATE TABLE pos_settings (
    establishment_id    uuid PRIMARY KEY REFERENCES establishments(id) ON DELETE CASCADE,
    walk_in_party_id    uuid,
    max_discount_bp     integer NOT NULL DEFAULT 1000 CHECK (max_discount_bp BETWEEN 0 AND 10000),
    allow_price_override boolean NOT NULL DEFAULT false,
    receipt_footer      text NOT NULL DEFAULT '',
    receipt_width_mm    integer NOT NULL DEFAULT 80 CHECK (receipt_width_mm IN (58, 80)),
    updated_at          timestamptz NOT NULL DEFAULT now(),
    FOREIGN KEY (establishment_id, walk_in_party_id) REFERENCES parties (establishment_id, id)
);

-- ---------------------------------------------------------------- sessions
CREATE TABLE pos_sessions (
    id                  uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id    uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    register_id         uuid NOT NULL,
    number              integer NOT NULL CHECK (number > 0),          -- Z number, per register
    status              text NOT NULL DEFAULT 'open' CHECK (status IN ('open', 'closed')),
    currency            char(3) NOT NULL REFERENCES currencies(code),
    opened_by           uuid NOT NULL REFERENCES users(id),
    opened_at           timestamptz NOT NULL DEFAULT now(),
    opening_float_minor bigint NOT NULL DEFAULT 0 CHECK (opening_float_minor >= 0),
    closed_by           uuid REFERENCES users(id),
    closed_at           timestamptz,
    expected            jsonb,      -- {method_code: minor} computed at closing
    counted             jsonb,      -- {method_code: minor} declared at closing
    cash_expected_minor bigint,
    cash_counted_minor  bigint,
    cash_variance_minor bigint,
    closing_note        text NOT NULL DEFAULT '',
    UNIQUE (establishment_id, id),
    UNIQUE (register_id, number),
    FOREIGN KEY (establishment_id, register_id) REFERENCES pos_registers (establishment_id, id),
    CHECK ((status = 'closed') = (closed_at IS NOT NULL))
);
CREATE UNIQUE INDEX pos_sessions_one_open ON pos_sessions (register_id) WHERE status = 'open';
CREATE UNIQUE INDEX pos_sessions_one_open_per_user ON pos_sessions (establishment_id, opened_by) WHERE status = 'open';
CREATE INDEX pos_sessions_list ON pos_sessions (establishment_id, opened_at DESC);

-- Closed sessions are immutable (Z report).
CREATE FUNCTION nh_guard_pos_session() RETURNS trigger
LANGUAGE plpgsql SET search_path = nhill, pg_catalog AS $$
BEGIN
    IF OLD.status = 'closed' THEN
        RAISE EXCEPTION 'closed cash session % is immutable', OLD.number USING ERRCODE = 'NH002';
    END IF;
    RETURN NEW;
END $$;
REVOKE ALL ON FUNCTION nh_guard_pos_session() FROM PUBLIC;
CREATE TRIGGER pos_sessions_guard BEFORE UPDATE ON pos_sessions FOR EACH ROW EXECUTE FUNCTION nh_guard_pos_session();

CREATE TABLE pos_cash_movements (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id uuid NOT NULL,
    session_id       uuid NOT NULL,
    kind             text NOT NULL CHECK (kind IN ('cash_in', 'cash_out')),
    amount_minor     bigint NOT NULL CHECK (amount_minor > 0),
    reason           text NOT NULL CHECK (length(btrim(reason)) BETWEEN 3 AND 300),
    created_by       uuid NOT NULL REFERENCES users(id),
    approved_by      uuid REFERENCES users(id),
    created_at       timestamptz NOT NULL DEFAULT now(),
    FOREIGN KEY (establishment_id, session_id) REFERENCES pos_sessions (establishment_id, id)
);
CREATE INDEX pos_cash_movements_session ON pos_cash_movements (establishment_id, session_id);

CREATE TABLE pos_approvals (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    token_hash       bytea NOT NULL UNIQUE,
    action           text NOT NULL CHECK (action IN ('discount', 'price_override', 'return', 'void', 'cash_out', 'close_other')),
    approver_id      uuid NOT NULL REFERENCES users(id),
    requested_by     uuid NOT NULL REFERENCES users(id),
    session_id       uuid,
    expires_at       timestamptz NOT NULL,
    used_at          timestamptz,
    created_at       timestamptz NOT NULL DEFAULT now(),
    FOREIGN KEY (establishment_id, session_id) REFERENCES pos_sessions (establishment_id, id)
);

-- Receipts, returns and voids are sales documents linked to a cash session.
ALTER TABLE sales_documents ADD COLUMN pos_session_id uuid;
ALTER TABLE sales_documents ADD CONSTRAINT sales_documents_pos_session_fkey
    FOREIGN KEY (establishment_id, pos_session_id) REFERENCES pos_sessions (establishment_id, id);
CREATE INDEX sales_documents_pos_session ON sales_documents (establishment_id, pos_session_id) WHERE pos_session_id IS NOT NULL;
ALTER TABLE sales_documents DROP CONSTRAINT sales_documents_doc_type_check;
ALTER TABLE sales_documents ADD CONSTRAINT sales_documents_doc_type_check
    CHECK (doc_type IN ('quote', 'order', 'invoice', 'credit_note', 'receipt'));
-- The status rule (an unnamed table constraint) is replaced by a named one including receipts.
DO $$
DECLARE c text;
BEGIN
    SELECT conname INTO c FROM pg_constraint
     WHERE conrelid = 'sales_documents'::regclass AND contype = 'c'
       AND pg_get_constraintdef(oid) LIKE '%converted%' AND pg_get_constraintdef(oid) LIKE '%credit_note%';
    EXECUTE format('ALTER TABLE sales_documents DROP CONSTRAINT %I', c);
END $$;
ALTER TABLE sales_documents ADD CONSTRAINT sales_documents_status_check CHECK (
    (doc_type = 'quote'       AND status IN ('draft', 'sent', 'accepted', 'declined', 'converted')) OR
    (doc_type = 'order'       AND status IN ('draft', 'confirmed', 'converted', 'canceled')) OR
    (doc_type = 'invoice'     AND status IN ('draft', 'issued', 'partially_paid', 'paid', 'credited')) OR
    (doc_type = 'receipt'     AND status IN ('draft', 'issued', 'partially_paid', 'paid', 'credited')) OR
    (doc_type = 'credit_note' AND status IN ('draft', 'issued'))
);

-- ---------------------------------------------------------------- sales promo codes
CREATE TABLE sales_promo_codes (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    code             text NOT NULL CHECK (code ~ '^[A-Z0-9][A-Z0-9_-]{2,31}$'),
    description      text NOT NULL DEFAULT '',
    percent_bp       integer NOT NULL CHECK (percent_bp BETWEEN 1 AND 10000),
    min_amount_minor bigint NOT NULL DEFAULT 0 CHECK (min_amount_minor >= 0),
    currency         char(3) REFERENCES currencies(code),          -- for min_amount
    valid_from       timestamptz NOT NULL DEFAULT now(),
    valid_until      timestamptz,
    max_redemptions  integer CHECK (max_redemptions IS NULL OR max_redemptions > 0),
    active           boolean NOT NULL DEFAULT true,
    created_by       uuid REFERENCES users(id),
    created_at       timestamptz NOT NULL DEFAULT now(),
    UNIQUE (establishment_id, code),
    UNIQUE (establishment_id, id),
    CHECK (valid_until IS NULL OR valid_until > valid_from)
);

CREATE TABLE sales_promo_redemptions (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id uuid NOT NULL,
    promo_id         uuid NOT NULL,
    document_id      uuid NOT NULL UNIQUE,
    discount_minor   bigint NOT NULL CHECK (discount_minor >= 0),
    created_at       timestamptz NOT NULL DEFAULT now(),
    FOREIGN KEY (establishment_id, promo_id) REFERENCES sales_promo_codes (establishment_id, id),
    FOREIGN KEY (establishment_id, document_id) REFERENCES sales_documents (establishment_id, id)
);
CREATE INDEX sales_promo_redemptions_promo ON sales_promo_redemptions (establishment_id, promo_id);

INSERT INTO platform_settings (key, value, description) VALUES
    ('trial_max_registers', '1', 'Caisses POS autorisées pendant l''essai gratuit'),
    ('pos_approval_minutes', '3', 'Validité d''une autorisation de superviseur POS (minutes)')
ON CONFLICT (key) DO NOTHING;

-- ---------------------------------------------------------------- security
DO $$
DECLARE
    t text;
BEGIN
    FOREACH t IN ARRAY ARRAY['pos_registers', 'pos_register_users', 'pos_payment_methods', 'pos_settings', 'pos_sessions',
                             'pos_cash_movements', 'pos_approvals', 'sales_promo_codes', 'sales_promo_redemptions'] LOOP
        EXECUTE format('ALTER TABLE %I ENABLE ROW LEVEL SECURITY', t);
        EXECUTE format('ALTER TABLE %I FORCE ROW LEVEL SECURITY', t);
        EXECUTE format('CREATE POLICY %I ON %I USING (establishment_id = (SELECT nh_current_establishment())) '
                       'WITH CHECK (establishment_id = (SELECT nh_current_establishment()))', t || '_tenant', t);
    END LOOP;
END $$;
GRANT SELECT, INSERT, UPDATE ON pos_registers, pos_payment_methods, pos_settings, pos_sessions, pos_approvals,
    sales_promo_codes TO nhill_app;
GRANT SELECT, INSERT, DELETE ON pos_register_users TO nhill_app;
GRANT SELECT, INSERT ON pos_cash_movements, sales_promo_redemptions TO nhill_app;
