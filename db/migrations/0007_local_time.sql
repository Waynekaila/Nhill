-- Nhill migration 0007 — local time of each establishment.
--
-- Dates of documents, cash sessions and daily reports follow the establishment's
-- time zone (Kinshasa is UTC+1, Lubumbashi UTC+2): an invoice issued at 00:30 in
-- Kinshasa is dated that day, not the previous UTC day. Timestamps stay
-- timestamptz (UTC internally).

ALTER TABLE establishments ADD COLUMN timezone text NOT NULL DEFAULT 'UTC';

-- Time zone of an establishment ('UTC' when unknown).
CREATE FUNCTION nh_tz(p_est uuid) RETURNS text
LANGUAGE sql STABLE AS $$
    SELECT COALESCE((SELECT timezone FROM establishments WHERE id = p_est), 'UTC')
$$;

-- Today's date for the current establishment (transaction setting app.establishment_id).
CREATE FUNCTION nh_local_today() RETURNS date
LANGUAGE sql STABLE AS $$
    SELECT (now() AT TIME ZONE nh_tz(nh_current_establishment()))::date
$$;

-- Start of a local calendar day of the current establishment, as an instant.
CREATE FUNCTION nh_local_start(p_day date) RETURNS timestamptz
LANGUAGE sql STABLE AS $$
    SELECT p_day::timestamp AT TIME ZONE nh_tz(nh_current_establishment())
$$;

-- Local date of an instant for the current establishment.
CREATE FUNCTION nh_local_date(p_ts timestamptz) RETURNS date
LANGUAGE sql STABLE AS $$
    SELECT (p_ts AT TIME ZONE nh_tz(nh_current_establishment()))::date
$$;

ALTER TABLE sales_documents ALTER COLUMN issue_date SET DEFAULT nh_local_today();
ALTER TABLE purchases ALTER COLUMN purchase_date SET DEFAULT nh_local_today();
ALTER TABLE expenses ALTER COLUMN expense_date SET DEFAULT nh_local_today();

GRANT EXECUTE ON FUNCTION nh_tz(uuid), nh_local_today(), nh_local_start(date), nh_local_date(timestamptz) TO nhill_app;
