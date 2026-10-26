-- Nhill website — schema (run as nhill_site_owner, in database nhill_site).
--   psql "host=… dbname=nhill_site user=nhill_site_owner sslmode=require" -f 001_init.sql
BEGIN;
REVOKE ALL ON SCHEMA public FROM PUBLIC;
CREATE SCHEMA IF NOT EXISTS site AUTHORIZATION nhill_site_owner;
SET search_path = site;
CREATE TABLE IF NOT EXISTS submissions (
    id                 uuid PRIMARY KEY,
    kind               text NOT NULL CHECK (kind IN ('demo', 'pilot', 'partner', 'contact')),
    lang               text NOT NULL DEFAULT 'fr' CHECK (lang IN ('fr', 'en')),
    name               text NOT NULL CHECK (length(name) BETWEEN 1 AND 120),
    organisation       text CHECK (organisation IS NULL OR length(organisation) <= 160),
    email              text NOT NULL CHECK (length(email) <= 200),
    phone              text CHECK (phone IS NULL OR length(phone) <= 30),
    city               text CHECK (city IS NULL OR length(city) <= 80),
    sector             text,
    team_size          text,
    partner_type       text,
    meeting_mode       text,
    message            text CHECK (message IS NULL OR length(message) <= 2000),
    consent_privacy_at timestamptz NOT NULL,
    consent_marketing  smallint NOT NULL DEFAULT 0 CHECK (consent_marketing IN (0, 1)),
    ip_hash            text,
    user_agent         text,
    status             text NOT NULL DEFAULT 'new' CHECK (status IN ('new', 'contacted', 'qualified', 'closed')),
    created_at         timestamptz NOT NULL,
    delete_after       timestamptz NOT NULL
);
CREATE INDEX IF NOT EXISTS submissions_email ON submissions (email);
CREATE INDEX IF NOT EXISTS submissions_delete_after ON submissions (delete_after);
CREATE TABLE IF NOT EXISTS rate_limits (
    bucket       text NOT NULL,
    window_start timestamptz NOT NULL,
    hits         integer NOT NULL,
    PRIMARY KEY (bucket, window_start)
);
-- Least privilege
GRANT USAGE ON SCHEMA site TO nhill_site_app, nhill_site_maint;
GRANT INSERT ON submissions TO nhill_site_app;                               -- cannot read back
GRANT SELECT, INSERT, UPDATE ON rate_limits TO nhill_site_app;
GRANT SELECT, UPDATE, DELETE ON submissions TO nhill_site_maint;
GRANT SELECT, DELETE ON rate_limits TO nhill_site_maint;
COMMIT;
