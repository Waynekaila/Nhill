-- Nhill website database — roles and database (run ONCE as a PostgreSQL superuser).
-- Distinct from the Nhill application database and roles (nhill_owner / nhill_app).
--   psql -v ON_ERROR_STOP=1 -v owner_pw=… -v app_pw=… -v maint_pw=… -f 000_roles.sql
-- Generate each password with: openssl rand -base64 30
CREATE ROLE nhill_site_owner LOGIN PASSWORD :'owner_pw';        -- owns the schema, runs migrations
CREATE ROLE nhill_site_app   LOGIN PASSWORD :'app_pw';        -- web backend: insert only
CREATE ROLE nhill_site_maint LOGIN PASSWORD :'maint_pw';        -- maintenance CLI: read, export, delete
CREATE DATABASE nhill_site OWNER nhill_site_owner ENCODING 'UTF8' TEMPLATE template0;
REVOKE ALL ON DATABASE nhill_site FROM PUBLIC;
GRANT CONNECT ON DATABASE nhill_site TO nhill_site_app, nhill_site_maint;
-- The roles find the tables of schema "site" without qualifying them.
ALTER ROLE nhill_site_app   IN DATABASE nhill_site SET search_path = site;
ALTER ROLE nhill_site_maint IN DATABASE nhill_site SET search_path = site;
