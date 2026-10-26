-- Nhill migration 0005 — helper functions used by the API layer.

-- ISO-8601 UTC rendering used in every API response.
CREATE FUNCTION nh_iso(ts timestamptz) RETURNS text
LANGUAGE sql IMMUTABLE AS $$
    SELECT to_char(ts AT TIME ZONE 'UTC', 'YYYY-MM-DD"T"HH24:MI:SS"Z"')
$$;
GRANT EXECUTE ON FUNCTION nh_iso(timestamptz) TO nhill_app;

-- 15-minute window start for throttling counters.
CREATE FUNCTION nh_window(minutes integer) RETURNS timestamptz
LANGUAGE sql STABLE AS $$
    SELECT to_timestamp(floor(extract(epoch FROM now()) / (minutes * 60)) * (minutes * 60))
$$;
GRANT EXECUTE ON FUNCTION nh_window(integer) TO nhill_app;

CREATE FUNCTION nh_setting_int(p_key text, p_default integer) RETURNS integer
LANGUAGE sql STABLE AS $$
    SELECT COALESCE((SELECT (value #>> '{}')::integer FROM platform_settings WHERE key = p_key), p_default)
$$;
GRANT EXECUTE ON FUNCTION nh_setting_int(text, integer) TO nhill_app;
