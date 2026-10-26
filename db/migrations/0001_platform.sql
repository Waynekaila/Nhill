-- Nhill migration 0001 — platform tables (establishments, identity, access, subscriptions)
-- Conventions: money = bigint minor units + char(3) currency; rates = int basis points;
-- quantities = bigint thousandths; timestamps = timestamptz (UTC).

CREATE FUNCTION nh_current_establishment() RETURNS uuid
LANGUAGE sql STABLE AS $$
    SELECT NULLIF(current_setting('app.establishment_id', true), '')::uuid
$$;

CREATE FUNCTION nh_touch_updated_at() RETURNS trigger
LANGUAGE plpgsql AS $$
BEGIN
    NEW.updated_at := now();
    RETURN NEW;
END $$;

CREATE TABLE currencies (
    code         char(3) PRIMARY KEY CHECK (code ~ '^[A-Z]{3}$'),
    minor_units  smallint NOT NULL CHECK (minor_units BETWEEN 0 AND 4),
    name_fr      text NOT NULL,
    name_en      text NOT NULL,
    active       boolean NOT NULL DEFAULT true
);

CREATE TABLE platform_settings (
    key         text PRIMARY KEY,
    value       jsonb NOT NULL,
    description text NOT NULL DEFAULT '',
    updated_at  timestamptz NOT NULL DEFAULT now(),
    updated_by  uuid
);

CREATE TABLE compliance_profiles (
    code            text NOT NULL CHECK (code ~ '^[A-Z]{2,8}$'),
    version         integer NOT NULL CHECK (version > 0),
    country_code    char(2),
    definition      jsonb NOT NULL,
    content_sha256  text NOT NULL,
    validation_note text NOT NULL DEFAULT '',
    loaded_at       timestamptz NOT NULL DEFAULT now(),
    PRIMARY KEY (code, version)
);

-- ---------------------------------------------------------------- establishments
CREATE TABLE establishments (
    id                      uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    kind                    text NOT NULL CHECK (kind IN ('individual', 'company')),
    trade_name              text NOT NULL CHECK (length(btrim(trade_name)) BETWEEN 1 AND 200),
    legal_name              text CHECK (legal_name IS NULL OR length(legal_name) <= 200),
    country_code            char(2) NOT NULL CHECK (country_code ~ '^[A-Z]{2}$'),
    city                    text,
    address                 text,
    sector                  text,
    preferred_language      text NOT NULL DEFAULT 'fr' CHECK (preferred_language IN ('fr', 'en')),
    main_currency           char(3) NOT NULL REFERENCES currencies(code),
    email                   text,
    phone                   text,
    website                 text,
    vat_registered          boolean NOT NULL DEFAULT false,
    compliance_profile_code text NOT NULL,
    logo_object_id          uuid,
    status                  text NOT NULL DEFAULT 'active' CHECK (status IN ('active', 'suspended', 'closed')),
    status_reason           text,
    created_at              timestamptz NOT NULL DEFAULT now(),
    updated_at              timestamptz NOT NULL DEFAULT now()
);
CREATE TRIGGER establishments_touch BEFORE UPDATE ON establishments
    FOR EACH ROW EXECUTE FUNCTION nh_touch_updated_at();
CREATE INDEX establishments_name_idx ON establishments (lower(trade_name));

-- Normalised legal identifiers: the database itself refuses two establishments
-- with the same identifier in the same country (duplicate prevention).
CREATE TABLE establishment_identifiers (
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    kind             text NOT NULL CHECK (kind ~ '^[a-z_]{2,24}$'),
    country_code     char(2) NOT NULL,
    value            text NOT NULL,              -- as typed by the user
    normalized_value text NOT NULL,              -- uppercase, no separators
    PRIMARY KEY (establishment_id, kind),
    UNIQUE (country_code, kind, normalized_value)
);

-- HMAC fingerprints of identifiers that already consumed a free trial.
-- Kept pseudonymous so the rule survives deletion of the establishment.
CREATE TABLE trial_registry (
    fingerprint      bytea PRIMARY KEY,
    establishment_id uuid,
    created_at       timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE attachment_requests (
    id                  uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id    uuid NOT NULL REFERENCES establishments(id),
    requester_name      text NOT NULL,
    requester_email     text NOT NULL,
    requester_phone     text,
    message             text,
    status              text NOT NULL DEFAULT 'open' CHECK (status IN ('open', 'approved', 'rejected')),
    resolution_note     text,
    resolved_by         uuid,
    resolved_at         timestamptz,
    created_at          timestamptz NOT NULL DEFAULT now()
);

-- ---------------------------------------------------------------- establishment codes
CREATE TABLE establishment_codes (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    code_hmac        bytea NOT NULL UNIQUE,      -- HMAC-SHA256(server key, canonical code)
    code_cipher      bytea NOT NULL,             -- secretbox(code) for display to owners
    code_nonce       bytea NOT NULL,
    status           text NOT NULL CHECK (status IN ('active', 'superseded', 'revoked')),
    created_at       timestamptz NOT NULL DEFAULT now(),
    expires_at       timestamptz NOT NULL,
    ended_at         timestamptz,
    ended_reason     text,
    created_by_type  text NOT NULL CHECK (created_by_type IN ('system', 'user', 'admin')),
    created_by       uuid,
    CHECK (expires_at > created_at)
);
CREATE UNIQUE INDEX establishment_codes_one_active
    ON establishment_codes (establishment_id) WHERE status = 'active';
CREATE INDEX establishment_codes_expiry ON establishment_codes (expires_at) WHERE status = 'active';

-- ---------------------------------------------------------------- users & access
CREATE TABLE users (
    id                  uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    email               text NOT NULL UNIQUE CHECK (email = lower(email) AND position('@' in email) > 1),
    password_hash       text NOT NULL,
    full_name           text NOT NULL CHECK (length(btrim(full_name)) BETWEEN 1 AND 120),
    phone               text,
    preferred_language  text NOT NULL DEFAULT 'fr' CHECK (preferred_language IN ('fr', 'en')),
    status              text NOT NULL DEFAULT 'active' CHECK (status IN ('active', 'disabled')),
    password_changed_at timestamptz NOT NULL DEFAULT now(),
    created_at          timestamptz NOT NULL DEFAULT now(),
    updated_at          timestamptz NOT NULL DEFAULT now()
);
CREATE TRIGGER users_touch BEFORE UPDATE ON users FOR EACH ROW EXECUTE FUNCTION nh_touch_updated_at();

CREATE TABLE memberships (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    user_id          uuid NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    role             text NOT NULL CHECK (role IN ('owner', 'admin', 'manager', 'sales', 'accountant', 'viewer')),
    status           text NOT NULL DEFAULT 'active' CHECK (status IN ('active', 'disabled')),
    created_at       timestamptz NOT NULL DEFAULT now(),
    UNIQUE (establishment_id, user_id)
);
CREATE INDEX memberships_user ON memberships (user_id);

CREATE TABLE invitations (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    email            text NOT NULL CHECK (email = lower(email)),
    role             text NOT NULL CHECK (role IN ('admin', 'manager', 'sales', 'accountant', 'viewer')),
    activation_hash  bytea NOT NULL,             -- SHA-256 of the activation code
    expires_at       timestamptz NOT NULL,
    accepted_at      timestamptz,
    revoked_at       timestamptz,
    created_by       uuid NOT NULL REFERENCES users(id),
    created_at       timestamptz NOT NULL DEFAULT now()
);
CREATE UNIQUE INDEX invitations_pending
    ON invitations (establishment_id, email) WHERE accepted_at IS NULL AND revoked_at IS NULL;

CREATE TABLE sessions (
    id                    uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id               uuid NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    establishment_id      uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    access_hash           bytea NOT NULL UNIQUE,
    access_expires_at     timestamptz NOT NULL,
    refresh_hash          bytea NOT NULL UNIQUE,
    previous_refresh_hash bytea,
    refresh_expires_at    timestamptz NOT NULL,
    created_at            timestamptz NOT NULL DEFAULT now(),
    last_seen_at          timestamptz NOT NULL DEFAULT now(),
    revoked_at            timestamptz,
    revoke_reason         text,
    user_agent            text,
    ip                    text
);
CREATE INDEX sessions_user ON sessions (user_id) WHERE revoked_at IS NULL;
CREATE INDEX sessions_prev_refresh ON sessions (previous_refresh_hash) WHERE previous_refresh_hash IS NOT NULL;

-- Short-lived tickets produced by a successful establishment-code check.
CREATE TABLE join_tickets (
    ticket_hash      bytea PRIMARY KEY,
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    expires_at       timestamptz NOT NULL,
    created_at       timestamptz NOT NULL DEFAULT now()
);

-- Sliding-window counters for throttling (shared by all server instances).
CREATE TABLE auth_throttle (
    bucket       text NOT NULL,
    window_start timestamptz NOT NULL,
    hits         integer NOT NULL DEFAULT 0,
    PRIMARY KEY (bucket, window_start)
);

-- ---------------------------------------------------------------- plans & subscriptions
CREATE TABLE plans (
    id              uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    code            text NOT NULL UNIQUE CHECK (code ~ '^[a-z0-9_-]{2,40}$'),
    name_fr         text NOT NULL,
    name_en         text NOT NULL,
    description_fr  text NOT NULL DEFAULT '',
    description_en  text NOT NULL DEFAULT '',
    billing_period  text NOT NULL CHECK (billing_period IN ('monthly', 'annual')),
    price_minor     bigint NOT NULL CHECK (price_minor >= 0),
    currency        char(3) NOT NULL REFERENCES currencies(code),
    tax_rate_bp     integer NOT NULL DEFAULT 0 CHECK (tax_rate_bp BETWEEN 0 AND 10000),
    max_users       integer CHECK (max_users IS NULL OR max_users > 0),
    audience        text NOT NULL DEFAULT 'any' CHECK (audience IN ('individual', 'company', 'any')),
    features        jsonb NOT NULL DEFAULT '{}'::jsonb,
    active          boolean NOT NULL DEFAULT true,
    sort_order      integer NOT NULL DEFAULT 0,
    created_at      timestamptz NOT NULL DEFAULT now(),
    updated_at      timestamptz NOT NULL DEFAULT now()
);
CREATE TRIGGER plans_touch BEFORE UPDATE ON plans FOR EACH ROW EXECUTE FUNCTION nh_touch_updated_at();

CREATE TABLE subscriptions (
    id                    uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id      uuid NOT NULL UNIQUE REFERENCES establishments(id) ON DELETE CASCADE,
    plan_id               uuid REFERENCES plans(id),
    pending_plan_id       uuid REFERENCES plans(id),
    status                text NOT NULL CHECK (status IN ('trialing', 'active', 'past_due', 'suspended', 'canceled')),
    is_trial              boolean NOT NULL DEFAULT false,
    period_start          timestamptz NOT NULL,
    period_end            timestamptz NOT NULL,
    grace_until           timestamptz,
    reminder_sent_for     timestamptz,           -- period_end the J-7 reminder was sent for
    auto_renew_consent_at timestamptz,           -- explicit consent, never implicit
    canceled_at           timestamptz,
    created_at            timestamptz NOT NULL DEFAULT now(),
    updated_at            timestamptz NOT NULL DEFAULT now(),
    -- period_end = period_start for an establishment created without any right
    -- (free trial already consumed): it must subscribe before writing.
    CHECK (period_end >= period_start),
    CHECK (grace_until IS NULL OR grace_until >= period_end)
);
CREATE TRIGGER subscriptions_touch BEFORE UPDATE ON subscriptions FOR EACH ROW EXECUTE FUNCTION nh_touch_updated_at();
CREATE INDEX subscriptions_due ON subscriptions (period_end) WHERE status IN ('trialing', 'active', 'past_due');

CREATE TABLE subscription_events (
    id               bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    subscription_id  uuid NOT NULL REFERENCES subscriptions(id) ON DELETE CASCADE,
    establishment_id uuid NOT NULL,
    kind             text NOT NULL,
    from_status      text,
    to_status        text,
    period_start     timestamptz,
    period_end       timestamptz,
    details          jsonb NOT NULL DEFAULT '{}'::jsonb,
    actor_type       text NOT NULL CHECK (actor_type IN ('system', 'user', 'admin', 'provider')),
    actor_id         uuid,
    created_at       timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX subscription_events_sub ON subscription_events (subscription_id, created_at);

-- A request to pay for a plan. Paid only when a verified provider webhook says so.
CREATE TABLE subscription_orders (
    id                 uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id   uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    plan_id            uuid NOT NULL REFERENCES plans(id),
    amount_minor       bigint NOT NULL CHECK (amount_minor >= 0),
    currency           char(3) NOT NULL REFERENCES currencies(code),
    status             text NOT NULL DEFAULT 'pending' CHECK (status IN ('pending', 'paid', 'failed', 'canceled', 'refunded')),
    provider           text,
    provider_reference text,
    created_by         uuid,
    created_at         timestamptz NOT NULL DEFAULT now(),
    paid_at            timestamptz,
    UNIQUE (provider, provider_reference)
);

CREATE TABLE payment_webhook_events (
    id              bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    provider        text NOT NULL,
    event_id        text NOT NULL,
    received_at     timestamptz NOT NULL DEFAULT now(),
    payload         jsonb NOT NULL,
    processed_at    timestamptz,
    outcome         text,
    UNIQUE (provider, event_id)
);

-- ---------------------------------------------------------------- administration
CREATE TABLE admin_users (
    id                 uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    email              text NOT NULL UNIQUE CHECK (email = lower(email)),
    full_name          text NOT NULL,
    password_hash      text,
    totp_cipher        bytea,
    totp_nonce         bytea,
    totp_enabled       boolean NOT NULL DEFAULT false,
    role               text NOT NULL CHECK (role IN ('superadmin', 'billing', 'support', 'auditor')),
    status             text NOT NULL DEFAULT 'pending' CHECK (status IN ('pending', 'active', 'disabled')),
    setup_token_hash   bytea,
    setup_expires_at   timestamptz,
    last_totp_step     bigint,                   -- refuses TOTP replay within a step
    last_login_at      timestamptz,
    created_at         timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE admin_sessions (
    id          uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    admin_id    uuid NOT NULL REFERENCES admin_users(id) ON DELETE CASCADE,
    token_hash  bytea NOT NULL UNIQUE,
    expires_at  timestamptz NOT NULL,
    revoked_at  timestamptz,
    ip          text,
    created_at  timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE announcements (
    id                 uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    kind               text NOT NULL CHECK (kind IN ('info', 'maintenance', 'release')),
    title_fr           text NOT NULL,
    title_en           text NOT NULL,
    body_fr            text NOT NULL DEFAULT '',
    body_en            text NOT NULL DEFAULT '',
    starts_at          timestamptz NOT NULL DEFAULT now(),
    ends_at            timestamptz,
    min_client_version text,
    created_by         uuid REFERENCES admin_users(id),
    created_at         timestamptz NOT NULL DEFAULT now()
);

CREATE TABLE notifications (
    id               uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    establishment_id uuid NOT NULL REFERENCES establishments(id) ON DELETE CASCADE,
    user_id          uuid REFERENCES users(id) ON DELETE CASCADE,  -- NULL = owners/admins
    kind             text NOT NULL,
    params           jsonb NOT NULL DEFAULT '{}'::jsonb,
    created_at       timestamptz NOT NULL DEFAULT now(),
    read_at          timestamptz
);
CREATE INDEX notifications_est ON notifications (establishment_id, created_at DESC);

CREATE TABLE email_outbox (
    id           uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    to_address   text NOT NULL,
    template     text NOT NULL,
    language     text NOT NULL DEFAULT 'fr',
    params       jsonb NOT NULL DEFAULT '{}'::jsonb,
    status       text NOT NULL DEFAULT 'pending' CHECK (status IN ('pending', 'sent', 'failed')),
    attempts     integer NOT NULL DEFAULT 0,
    last_error   text,
    created_at   timestamptz NOT NULL DEFAULT now(),
    sent_at      timestamptz
);

-- Append-only audit trail (UPDATE/DELETE are not granted to the application role).
CREATE TABLE audit_log (
    id               bigint GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    establishment_id uuid,
    actor_type       text NOT NULL CHECK (actor_type IN ('user', 'admin', 'system', 'anonymous', 'provider')),
    actor_id         uuid,
    action           text NOT NULL,
    entity           text,
    entity_id        text,
    details          jsonb NOT NULL DEFAULT '{}'::jsonb,
    ip               text,
    created_at       timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX audit_log_est ON audit_log (establishment_id, created_at DESC);
CREATE INDEX audit_log_action ON audit_log (action, created_at DESC);
