#!/usr/bin/env bash
# =============================================================================
# Nhill — way back after an update (nhill-update): previous package and the database
# copy taken just before the update.
#
#   sudo nhill-rollback [--yes]
#
# The database used since the update is renamed nhill_failed_<date> (never deleted
# here): what was recorded since the update stays available for analysis or re-entry.
# Without a copy of the previous package, only the database is put back and the newer
# program migrates it again at start: install the previous package by hand to go back.
# =============================================================================
set -euo pipefail
YES=0
[[ "${1:-}" == --yes ]] && YES=1
die() { printf '\nERROR: %s\n' "$*" >&2; exit 1; }
[[ $EUID -eq 0 ]] || die "run as root (sudo)"
STATE=/var/lib/nhill/update-state
[[ -f $STATE ]] || die "no update to undo ($STATE missing)"
# shellcheck disable=SC1090
. "$STATE"
[[ -n "${PRE_DB:-}" ]] || die "$STATE incomplete"
sudo -u postgres psql -qtAc "SELECT 1 FROM pg_database WHERE datname = '$PRE_DB'" | grep -q 1 || die "database copy $PRE_DB not found"
echo "Way back: Nhill Server ${NEW_VERSION:-?} -> ${OLD_VERSION:-?}, database copy $PRE_DB."
if [[ $YES != 1 ]]; then read -r -p "Continue? Data recorded since the update will be set aside. [y/N] " a; [[ "$a" == [yYoO]* ]] || die "cancelled"; fi

systemctl stop nhill-server
if [[ -n "${OLD_DEB:-}" && -f "$OLD_DEB" ]]; then
  apt-get install -y -q --allow-downgrades "$OLD_DEB" >/dev/null
  echo "  ok  package $(dpkg-query -W -f='${Version}' nhill-server) installed"
else
  echo "  !!  previous package not available: program kept, database only" >&2
fi
failed="nhill_failed_$(date -u +%Y%m%d%H%M%S)"
sudo -u postgres psql -q -v ON_ERROR_STOP=1 <<SQL
SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE datname IN ('nhill', '$PRE_DB') AND pid <> pg_backend_pid();
ALTER DATABASE nhill RENAME TO $failed;
ALTER DATABASE $PRE_DB RENAME TO nhill;
-- Database-level privileges are not copied with the database: given again.
GRANT CONNECT, CREATE ON DATABASE nhill TO nhill_owner;
GRANT CONNECT ON DATABASE nhill TO nhill_app;
DO \$\$ BEGIN IF EXISTS (SELECT FROM pg_roles WHERE rolname = 'nhill_backup') THEN
  GRANT CONNECT ON DATABASE nhill TO nhill_backup; END IF; END \$\$;
SQL
echo "  ok  database restored from $PRE_DB; the database of the update is kept as $failed"
systemctl start nhill-server
for _ in $(seq 1 90); do
  if curl -fsS --max-time 3 http://127.0.0.1:8080/health 2>/dev/null | grep -q '"database":true'; then
    rm -f "$STATE"
    echo "  ok  Nhill Server answers: $(curl -fsS http://127.0.0.1:8080/health)"
    echo "When $failed is no longer needed: sudo -u postgres dropdb $failed"
    exit 0
  fi
  sleep 1
done
journalctl -u nhill-server -n 40 --no-pager >&2 || true
die "Nhill Server does not answer after the way back: restore the latest backup (docs/fr/deploiement-vps-hostinger.md § Restauration) or the Hostinger snapshot"
