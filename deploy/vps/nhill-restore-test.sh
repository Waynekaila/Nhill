#!/usr/bin/env bash
# =============================================================================
# Nhill — restore test of an encrypted backup, OUTSIDE the production server.
#
#   deploy/vps/nhill-restore-test.sh BACKUP.dump.age AGE-IDENTITY.txt [--pg-bin DIR] [--keep]
#
# Run it on the machine that holds the age PRIVATE key (administrator's computer, a
# separate test VPS), at least once a month and after every change of the backup setup.
# It starts a throw-away PostgreSQL cluster in a temporary folder (local socket only,
# nothing listens on the network), restores the backup into it, then checks:
#   - checksum of the file (BACKUP.dump.age.sha256 next to it, when present);
#   - the restore itself (pg_restore stops at the first error);
#   - migrations, establishments, users, sales documents and audit entries counted;
#   - row-level security still forced on every tenant table;
#   - the application role sees no business row without an establishment (isolation).
# The cluster and the decrypted data are deleted at the end (--keep to inspect them).
# Requires: age, PostgreSQL server binaries (initdb, pg_ctl, pg_restore, psql) of a
# version >= the production server (16).
# =============================================================================
set -euo pipefail
BACKUP="${1:-}"; IDENTITY="${2:-}"; shift 2 || true
PGBIN="" KEEP=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --pg-bin) PGBIN="$2"; shift ;;
    --keep) KEEP=1 ;;
    *) echo "unknown option: $1" >&2; exit 2 ;;
  esac
  shift
done
die() { printf 'ERROR: %s\n' "$*" >&2; exit 1; }
[[ -f "$BACKUP" && -f "$IDENTITY" ]] || die "usage: $0 BACKUP.dump.age AGE-IDENTITY.txt [--pg-bin DIR] [--keep]"
command -v age >/dev/null || die "age is required"
if [[ -z "$PGBIN" ]]; then
  PGBIN="$(ls -d /usr/lib/postgresql/*/bin 2>/dev/null | sort -V | tail -1 || true)"
  [[ -n "$PGBIN" ]] || PGBIN="$(dirname "$(command -v initdb 2>/dev/null || echo /nonexistent/initdb)")"
fi
for b in initdb pg_ctl pg_restore psql; do [[ -x "$PGBIN/$b" ]] || die "$b not found in $PGBIN (--pg-bin DIR)"; done
if [[ -f "$BACKUP.sha256" ]]; then
  # Hash only: the .sha256 file of older backups names the server's directory.
  [[ "$(cut -d' ' -f1 "$BACKUP.sha256")" == "$(sha256sum < "$BACKUP" | cut -d' ' -f1)" ]] || die "checksum mismatch: $BACKUP"
  echo "ok    checksum            $(basename "$BACKUP").sha256"
fi
head -c 21 "$BACKUP" | grep -q 'age-encryption.org/v1' || die "$BACKUP is not an age-encrypted file"

WORK="$(mktemp -d "${TMPDIR:-/tmp}/nhill-restore-test-XXXX")"
chmod 700 "$WORK"
cleanup() {
  "$PGBIN/pg_ctl" -D "$WORK/data" -m immediate stop >/dev/null 2>&1 || true
  if [[ $KEEP == 1 ]]; then echo "kept: $WORK"; else rm -rf "$WORK"; fi
}
trap cleanup EXIT
"$PGBIN/initdb" -D "$WORK/data" -U postgres --auth=trust --encoding=UTF8 >/dev/null
"$PGBIN/pg_ctl" -D "$WORK/data" -l "$WORK/postgres.log" -w \
  -o "-c listen_addresses='' -c unix_socket_directories='$WORK' -c port=54329" start >/dev/null
export PGHOST="$WORK" PGPORT=54329 PGUSER=postgres
psql() { "$PGBIN/psql" -X -v ON_ERROR_STOP=1 -qtA "$@"; }
psql -d postgres -c "CREATE ROLE nhill_owner NOLOGIN; CREATE ROLE nhill_app NOLOGIN NOBYPASSRLS; CREATE ROLE nhill_backup NOLOGIN BYPASSRLS"
psql -d postgres -c "CREATE DATABASE nhill_restore_test"
start=$(date +%s)
age -d -i "$IDENTITY" "$BACKUP" | "$PGBIN/pg_restore" --exit-on-error -d nhill_restore_test 2>"$WORK/restore.log" \
  || { cat "$WORK/restore.log" >&2; die "pg_restore failed"; }
echo "ok    restore             $(( $(date +%s) - start )) s"
q() { psql -d nhill_restore_test -c "$1"; }
mig="$(q "SELECT count(*) FROM nhill.schema_migrations")"
last="$(q "SELECT max(version) FROM nhill.schema_migrations")"
echo "ok    migrations          $mig (last: $last)"
for t in establishments users sales_documents audit_log; do
  printf 'ok    %-19s %s rows\n' "$t" "$(q "SELECT count(*) FROM nhill.$t")"
done
read -r pol forced < <(q "SELECT count(*), count(*) FILTER (WHERE c.relrowsecurity AND c.relforcerowsecurity)
  FROM pg_policy p JOIN pg_class c ON c.oid = p.polrelid JOIN pg_namespace n ON n.oid = c.relnamespace
  WHERE n.nspname = 'nhill' AND p.polname LIKE '%\_tenant'" | tr '|' ' ')
[[ "$pol" -ge 20 && "$pol" == "$forced" ]] || die "row-level security not forced everywhere ($forced/$pol)"
echo "ok    row-level security  $forced/$pol tenant tables forced"
visible="$(psql -d nhill_restore_test -c "SET ROLE nhill_app; SET search_path TO nhill; SELECT count(*) FROM parties")"
[[ "$visible" == 0 ]] || die "isolation: $visible business rows visible to the application role without an establishment"
echo "ok    isolation           application role sees 0 row without an establishment"
echo "RESTORE TEST PASSED: $(basename "$BACKUP") can be restored."
