#!/usr/bin/env bash
# Restores an encrypted Nhill backup into a NEW database, then checks it.
# Never restores over the production database: switch only after verification.
#   nhill-restore.sh BACKUP.dump.age AGE_IDENTITY_FILE TARGET_DB
# Requires: age, pg_restore, psql, a PostgreSQL superuser connection (PGHOST/PGUSER...).
set -euo pipefail
backup="$1"; identity="$2"; target="$3"
[ -f "$backup" ] && [ -f "$identity" ] || { echo "usage: $0 BACKUP.dump.age IDENTITY TARGET_DB" >&2; exit 2; }
if [ -f "$backup.sha256" ]; then sha256sum -c "$backup.sha256"; fi
psql -v ON_ERROR_STOP=1 -d postgres -c "CREATE DATABASE \"$target\" OWNER nhill_owner"
# As superuser: ownership (nhill_owner) and grants come from the dump; the
# superuser can load rows into tables with forced row-level security.
age -d -i "$identity" "$backup" | pg_restore --exit-on-error -d "$target"
# The roles nhill_owner and nhill_app must exist in the target cluster
# (db/bootstrap/00-roles.sql); grants and row-level security policies are in the dump.
echo "--- verification"
psql -d "$target" -At -c "SELECT 'migrations: ' || count(*) FROM nhill.schema_migrations"
psql -d "$target" -At -c "SELECT 'establishments: ' || count(*) FROM nhill.establishments"
psql -d "$target" -At -c "SELECT 'audit entries: ' || count(*) FROM nhill.audit_log"
psql -d "$target" -At -c "SELECT 'RLS forced on sales_documents: ' || relforcerowsecurity FROM pg_class WHERE oid = 'nhill.sales_documents'::regclass"
echo "Restored into $target. Point NH_DATABASE_URL to it only after checking the application."
