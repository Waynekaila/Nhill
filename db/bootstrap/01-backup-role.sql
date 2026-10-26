-- Nhill — backup role, run once as a PostgreSQL superuser:
--
--   psql -v backup_password="'...'" -d nhill -f 01-backup-role.sql
--
-- Business tables use FORCE ROW LEVEL SECURITY with no exception, so pg_dump
-- cannot read them with the owner or the application role. nhill_backup is
-- the only role allowed to bypass row-level security: READ-ONLY
-- (pg_read_all_data), used only by the backup job, ideally through a local
-- socket with peer/scram authentication restricted in pg_hba.conf.
\set ON_ERROR_STOP on
SELECT 'CREATE ROLE nhill_backup LOGIN NOSUPERUSER NOCREATEDB NOCREATEROLE BYPASSRLS'
WHERE NOT EXISTS (SELECT FROM pg_roles WHERE rolname = 'nhill_backup')\gexec
ALTER ROLE nhill_backup WITH PASSWORD :backup_password BYPASSRLS;
GRANT pg_read_all_data TO nhill_backup;
-- Read-only by default: any write attempt fails.
ALTER ROLE nhill_backup SET default_transaction_read_only = on;
ALTER ROLE nhill_backup SET search_path = nhill;
