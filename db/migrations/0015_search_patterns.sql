-- Nhill migration 0015 — searches take % and _ literally.
--
-- The list searches used ILIKE '%' || $1 || '%': a user typing "%" or "_" matched more
-- rows than intended (no injection: the value is a bound parameter, only a wider
-- pattern). nh_like_contains escapes the three special characters of LIKE (backslash is
-- PostgreSQL's default escape character).
CREATE FUNCTION nh_like_contains(p text) RETURNS text
LANGUAGE sql IMMUTABLE STRICT SET search_path = pg_catalog AS $$
    SELECT '%' || replace(replace(replace(p, '\', '\\'), '%', '\%'), '_', '\_') || '%'
$$;
GRANT EXECUTE ON FUNCTION nh_like_contains(text) TO nhill_app;
