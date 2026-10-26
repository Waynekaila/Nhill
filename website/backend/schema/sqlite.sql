-- Nhill website — form submissions (SQLite). Distinct from the Nhill application database.
CREATE TABLE IF NOT EXISTS submissions (
    id                 TEXT PRIMARY KEY,
    kind               TEXT NOT NULL CHECK (kind IN ('demo', 'pilot', 'partner', 'contact')),
    lang               TEXT NOT NULL DEFAULT 'fr' CHECK (lang IN ('fr', 'en')),
    name               TEXT NOT NULL CHECK (length(name) BETWEEN 1 AND 120),
    organisation       TEXT CHECK (organisation IS NULL OR length(organisation) <= 160),
    email              TEXT NOT NULL CHECK (length(email) <= 200),
    phone              TEXT CHECK (phone IS NULL OR length(phone) <= 30),
    city               TEXT CHECK (city IS NULL OR length(city) <= 80),
    sector             TEXT,
    team_size          TEXT,
    partner_type       TEXT,
    meeting_mode       TEXT,
    message            TEXT CHECK (message IS NULL OR length(message) <= 2000),
    consent_privacy_at TEXT NOT NULL,
    consent_marketing  INTEGER NOT NULL DEFAULT 0 CHECK (consent_marketing IN (0, 1)),
    ip_hash            TEXT,
    user_agent         TEXT,
    status             TEXT NOT NULL DEFAULT 'new' CHECK (status IN ('new', 'contacted', 'qualified', 'closed')),
    created_at         TEXT NOT NULL,
    delete_after       TEXT NOT NULL
);
CREATE INDEX IF NOT EXISTS submissions_email ON submissions (email);
CREATE INDEX IF NOT EXISTS submissions_delete_after ON submissions (delete_after);
CREATE TABLE IF NOT EXISTS rate_limits (
    bucket       TEXT NOT NULL,
    window_start TEXT NOT NULL,
    hits         INTEGER NOT NULL,
    PRIMARY KEY (bucket, window_start)
);
