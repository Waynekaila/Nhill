#!/usr/bin/env bash
# =============================================================================
# Nhill — update of Nhill Server on the VPS, with a way back.
#
#   sudo nhill-update NEW.deb --sha256sums SHA256SUMS   (or --sha256 HEX)
#                     [--reinstall] [--no-rollback] [--yes]
#
#   1. checks the package (name, checksum, version newer than the installed one);
#   2. runs an encrypted backup now (and its off-site copy, when configured);
#   3. stops the API and copies the database locally (nhill_pre_<version>_<date>):
#      the way back, usable without the backup key;
#   4. installs the package; at start, the server applies the new migrations
#      (forward only: an older server refuses a database migrated by a newer one);
#   5. waits for /health; on failure, goes back automatically (nhill-rollback):
#      previous package + database copy; the failed database is kept for analysis.
# Downtime: from step 3 to the end of step 5 (usually under a minute for a small base).
# Before an important update, also take a Hostinger snapshot (VPS panel → Snapshots).
# =============================================================================
set -euo pipefail
DEB="" SUMS="" SHA="" REINSTALL=0 ROLLBACK=1 YES=0
while [[ $# -gt 0 ]]; do
  case "$1" in
    --sha256sums) SUMS="$2"; shift ;;
    --sha256) SHA="$2"; shift ;;
    --reinstall) REINSTALL=1 ;;
    --no-rollback) ROLLBACK=0 ;;
    --yes) YES=1 ;;
    -h|--help) sed -n '2,19p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    -*) echo "unknown option: $1" >&2; exit 2 ;;
    *) DEB="$1" ;;
  esac
  shift
done
say() { printf '\n==> %s\n' "$*"; }
die() { printf '\nERROR: %s\n' "$*" >&2; exit 1; }
[[ $EUID -eq 0 ]] || die "run as root (sudo)"
[[ -n "$DEB" && -f "$DEB" ]] || die "usage: nhill-update NEW.deb --sha256sums SHA256SUMS"
STATE=/var/lib/nhill/update-state
REL=/var/lib/nhill/releases
install -d -m 0750 "$REL"

say "Package checks"
[[ "$(dpkg-deb -f "$DEB" Package)" == nhill-server ]] || die "$DEB is not the nhill-server package"
if [[ -n "$SHA" ]]; then echo "$SHA  $DEB" | sha256sum -c --quiet || die "checksum mismatch"
elif [[ -n "$SUMS" ]]; then
  expected="$(awk -v f="$(basename "$DEB")" '$2==f || $2=="*"f {print $1}' "$SUMS")"
  [[ -n "$expected" ]] || die "$(basename "$DEB") not listed in $SUMS"
  echo "$expected  $DEB" | sha256sum -c --quiet || die "checksum mismatch"
else die "--sha256sums or --sha256 required"; fi
new="$(dpkg-deb -f "$DEB" Version)"
old="$(dpkg-query -W -f='${Version}' nhill-server 2>/dev/null || true)"
[[ -n "$old" ]] || die "nhill-server is not installed (first installation: provision.sh)"
if dpkg --compare-versions "$new" lt "$old"; then die "$new is older than the installed $old (going back: nhill-rollback)"; fi
if [[ "$new" == "$old" && $REINSTALL != 1 ]]; then die "$new is already installed (--reinstall to install it again)"; fi
old_deb="$(ls -1t "$REL"/nhill-server_"${old}"_*.deb 2>/dev/null | head -1 || true)"
[[ -n "$old_deb" ]] || echo "  !!  copy of the installed package $old not found in $REL: the automatic way back will restore the database only" >&2
echo "  installed: $old   new: $new"
if [[ $YES != 1 ]]; then read -r -p "Update Nhill Server $old -> $new now (short interruption)? [y/N] " a; [[ "$a" == [yYoO]* ]] || die "cancelled"; fi

say "Backup before the update"
systemctl start nhill-backup.service || die "backup failed: update cancelled (journalctl -u nhill-backup)"
echo "  ok  $(ls -1t /var/backups/nhill/archives/nhill-*.dump.age | head -1)"

say "Database copy (way back)"
db_mb="$(sudo -u postgres psql -qtAc "SELECT pg_database_size('nhill') / 1048576")"
free_mb="$(( $(df --output=avail -k /var/lib/postgresql | tail -1) / 1024 ))"
(( free_mb > db_mb * 2 + 1024 )) || die "not enough disk space for a copy of the database (${db_mb} MB, ${free_mb} MB free)"
stamp="$(date -u +%Y%m%d%H%M%S)"
pre="nhill_pre_$(echo "$old" | tr -c 'A-Za-z0-9\n' '_')_$stamp"
systemctl stop nhill-server
sudo -u postgres psql -q -v ON_ERROR_STOP=1 -c \
  "SELECT pg_terminate_backend(pid) FROM pg_stat_activity WHERE datname = 'nhill' AND pid <> pg_backend_pid()" >/dev/null
sudo -u postgres createdb -T nhill "$pre" || { systemctl start nhill-server; die "database copy failed: update cancelled, service restarted"; }
cat >"$STATE" <<EOF
OLD_VERSION=$old
OLD_DEB=$old_deb
NEW_VERSION=$new
PRE_DB=$pre
STAMP=$stamp
EOF
chmod 600 "$STATE"
echo "  ok  $pre (${db_mb} MB)"

say "Installation of $new"
cp -f "$DEB" "$REL/$(basename "$DEB")"
apt_opts=(-y -q --allow-downgrades)
[[ $REINSTALL == 1 ]] && apt_opts+=(--reinstall)
apt-get install "${apt_opts[@]}" "$REL/$(basename "$DEB")" >/dev/null
systemctl start nhill-server || true
okay=0
for _ in $(seq 1 90); do
  if curl -fsS --max-time 3 http://127.0.0.1:8080/health 2>/dev/null | grep -q '"database":true'; then okay=1; break; fi
  sleep 1
done
if [[ $okay != 1 ]]; then
  journalctl -u nhill-server -n 40 --no-pager >&2 || true
  if [[ $ROLLBACK == 1 ]]; then
    echo "  !!  Nhill Server $new does not answer: going back to $old" >&2
    /usr/sbin/nhill-rollback --yes || die "automatic way back failed: see above, then nhill-rollback"
    die "update to $new failed; $old restored (data written after $stamp: none, the service was stopped)"
  fi
  die "Nhill Server $new does not answer (--no-rollback: nothing undone)"
fi
echo "  ok  Nhill Server $new answers ($(curl -fsS http://127.0.0.1:8080/health))"

say "Housekeeping"
# Keep the two most recent database copies and the three most recent packages.
mapfile -t copies < <(sudo -u postgres psql -qtAc "SELECT datname FROM pg_database WHERE datname LIKE 'nhill\\_pre\\_%' ORDER BY datname DESC")
for c in "${copies[@]:2}"; do sudo -u postgres dropdb "$c" && echo "  removed old copy $c"; done
ls -1t "$REL"/nhill-server_*.deb | tail -n +4 | xargs -r rm -f
/usr/sbin/nhill-healthcheck || echo "  !!  health check reports a problem (above)" >&2
echo
echo "Nhill Server updated: $old -> $new. Way back (data written since the update is lost,"
echo "kept apart in the database nhill_failed_*): sudo nhill-rollback"
