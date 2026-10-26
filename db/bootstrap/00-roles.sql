-- Nhill — one-time cluster bootstrap, run as a PostgreSQL superuser:
--
--   psql -v owner_password="'...'" -v app_password="'...'" -f 00-roles.sql
--
-- Two roles:
--   nhill_owner : owns the schema, runs migrations. Never used by the API.
--   nhill_app   : used by the running server. Owns nothing, is subject to
--                     row-level security, has no DDL rights.
\set ON_ERROR_STOP on

SELECT 'CREATE ROLE nhill_owner LOGIN' WHERE NOT EXISTS (SELECT FROM pg_roles WHERE rolname = 'nhill_owner')\gexec
SELECT 'CREATE ROLE nhill_app LOGIN NOSUPERUSER NOBYPASSRLS NOCREATEDB NOCREATEROLE' WHERE NOT EXISTS (SELECT FROM pg_roles WHERE rolname = 'nhill_app')\gexec

ALTER ROLE nhill_owner WITH PASSWORD :owner_password;
ALTER ROLE nhill_app WITH PASSWORD :app_password NOBYPASSRLS;

SELECT 'CREATE DATABASE nhill OWNER nhill_owner ENCODING ''UTF8'' TEMPLATE template0'
WHERE NOT EXISTS (SELECT FROM pg_database WHERE datname = 'nhill')\gexec

-- All Nhill objects live in the schema "nhill", created by the first migration
-- (run as nhill_owner). The application role never creates anything.
ALTER ROLE nhill_owner SET search_path = nhill;
ALTER ROLE nhill_app SET search_path = nhill;

\connect nhill
GRANT CREATE ON DATABASE nhill TO nhill_owner;
REVOKE CREATE ON SCHEMA public FROM PUBLIC;
REVOKE CREATE ON DATABASE nhill FROM PUBLIC;
