-- Nhill migration 0011 — forgotten passwords (2.1.0).
--
--   users.must_change_password       set when an establishment administrator gives the
--                                    member a temporary password: the next sign-in only
--                                    yields a one-time ticket to choose a new password;
--   users.temporary_password_expires_at  end of validity of that temporary password;
--   password_resets                  one-time secrets of the recovery procedures:
--     purpose 'email_code'           8-digit code e-mailed to an establishment
--                                    administrator (or to an account shared by several
--                                    establishments), stored as HMAC-SHA-256 under a
--                                    server key, limited lifetime and number of attempts;
--     purpose 'forced_change'        random ticket returned by the sign-in that requires a
--                                    new password (SHA-256 of a 256-bit token).
-- No secret is stored in clear text; a secret is unusable once used_at or revoked_at is set.

ALTER TABLE users ADD COLUMN must_change_password boolean NOT NULL DEFAULT false;
ALTER TABLE users ADD COLUMN temporary_password_expires_at timestamptz;

CREATE TABLE password_resets (
    id          uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id     uuid NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    purpose     text NOT NULL CHECK (purpose IN ('email_code', 'forced_change')),
    secret_hash bytea NOT NULL,
    expires_at  timestamptz NOT NULL,
    attempts    integer NOT NULL DEFAULT 0 CHECK (attempts >= 0),
    used_at     timestamptz,
    revoked_at  timestamptz,
    ip          text,
    created_at  timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX password_resets_user_open ON password_resets (user_id, purpose)
    WHERE used_at IS NULL AND revoked_at IS NULL;
CREATE UNIQUE INDEX password_resets_ticket ON password_resets (secret_hash) WHERE purpose = 'forced_change';

GRANT SELECT, INSERT, UPDATE, DELETE ON password_resets TO nhill_app;
