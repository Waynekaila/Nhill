#!/usr/bin/env bash
# Nhill — acceptance test of a VPS installed by deploy/vps/provision.sh (run as root on
# that machine; used by the CI job "vps-test" on a fresh Ubuntu 24.04 virtual machine).
#
#   sudo deploy/vps/tests/acceptance.sh API_DOMAIN AGE-IDENTITY.txt PACKAGE.deb SHA256SUMS
#
# Exercises what the operator relies on: exposure (ports, firewall), permissions of the
# secrets, API through Caddy, health check, encrypted backup, restore outside the server,
# update with a way back, refusal of a newer database by an older server, token of the
# first administrator. Exit code 1 at the first failed check.
set -euo pipefail
DOMAIN="$1" IDENTITY="$2" DEB="$3" SUMS="$4"
HERE="$(cd "$(dirname "$0")/.." && pwd)"
CA=/var/lib/caddy/.local/share/caddy/pki/authorities/local/root.crt
n=0
pass() { n=$((n + 1)); printf 'PASS %02d  %s\n' "$n" "$*"; }
fail() { printf 'FAIL     %s\n' "$*" >&2; exit 1; }
health_ok() { curl -fsS --max-time 3 http://127.0.0.1:8080/health 2>/dev/null | grep -q '"database":true'; }
wait_health() { for _ in $(seq 1 "${1:-60}"); do health_ok && return 0; sleep 1; done; return 1; }
sql() { sudo -u postgres psql -qtAX -d nhill -c "$1"; }

# --- exposure
listen="$(ss -ltnH)"
grep -qE '(127\.0\.0\.1|\[::1\]):5432' <<<"$listen" || fail "PostgreSQL not listening on loopback"
! awk '{print $4}' <<<"$listen" | grep -E ':5432$' | grep -vqE '^(127\.0\.0\.1|\[::1\]):5432$' || fail "PostgreSQL reachable from the network"
! awk '{print $4}' <<<"$listen" | grep -E ':8080$' | grep -vqE '^(127\.0\.0\.1|\[::1\]):8080$' || fail "API port 8080 reachable from the network"
awk '{print $4}' <<<"$listen" | grep -qE '(\*|0\.0\.0\.0|\[::\]):443$' || fail "nothing listens on 443"
pass "ports: PostgreSQL 5432 and API 8080 on loopback only, Caddy on 443"
ufw status | grep -q '^Status: active' || fail "ufw inactive"
rules="$(ufw status | sed -n '/^--/,$p')"
! grep -E '5432|8080' <<<"$rules" >/dev/null || fail "firewall opens 5432 or 8080"
grep -q '443/tcp' <<<"$rules" && grep -q '80/tcp' <<<"$rules" || fail "firewall rules for 80/443 missing"
pass "firewall active: $(grep -c ALLOW <<<"$rules") allow rules, no database or API port"

# --- secrets
[[ "$(stat -c '%a %U' /etc/nhill/server.env)" == "600 nhill" ]] || fail "server.env permissions"
[[ "$(stat -c '%a %U' /etc/nhill/backup.env)" == "600 nhill-backup" ]] || fail "backup.env permissions"
[[ "$(stat -c '%a %U' /var/backups/nhill/.pgpass)" == "600 nhill-backup" ]] || fail ".pgpass permissions"
grep -q '^NH_SECRET_KEY=[0-9a-f]\{64\}$' /etc/nhill/server.env || fail "NH_SECRET_KEY missing"
! sudo -u postgres psql -qtAc "SELECT 1 FROM pg_roles WHERE rolname = 'nhill_setup'" | grep -q 1 || fail "temporary setup role still present"
! grep -rIq 'AGE-SECRET-KEY' /etc/nhill /var/backups/nhill 2>/dev/null || fail "an age private key is stored on the server"
pass "secrets: files mode 600, no temporary superuser left, no backup private key on the server"

# --- API
curl -fsS --max-time 10 --resolve "$DOMAIN:443:127.0.0.1" --cacert "$CA" "https://$DOMAIN/health" | grep -q '"status":"ok"' || fail "https health"
meta="$(curl -fsS --resolve "$DOMAIN:443:127.0.0.1" --cacert "$CA" "https://$DOMAIN/api/v1/meta")"
jq -e '.product == "Nhill" and .api == "v1"' <<<"$meta" >/dev/null || fail "meta: $meta"
hdr="$(curl -fsSI --resolve "$DOMAIN:443:127.0.0.1" --cacert "$CA" "https://$DOMAIN/health")"
grep -qi '^strict-transport-security' <<<"$hdr" || fail "HSTS header missing"
code="$(curl -s -o /dev/null -w '%{http_code}' --resolve "$DOMAIN:80:127.0.0.1" "http://$DOMAIN/health")"
[[ "$code" == 308 || "$code" == 301 ]] || fail "http not redirected to https ($code)"
pass "API through Caddy: TLS verified, HSTS, http redirected, version $(jq -r .version <<<"$meta")"
grep -q "^NH_PUBLIC_BASE_URL=https://$DOMAIN$" /etc/nhill/server.env || fail "NH_PUBLIC_BASE_URL"
pass "public address configured: https://$DOMAIN"

# --- health check
/usr/sbin/nhill-healthcheck >/tmp/health.txt || { cat /tmp/health.txt; fail "health check"; }
systemctl is-enabled --quiet nhill-healthcheck.timer && systemctl is-enabled --quiet nhill-backup.timer || fail "timers not enabled"
pass "health check: $(grep -c '^ok' /tmp/health.txt) checks passed; timers enabled (health, backup)"

# --- backup and restore outside the server
systemctl start nhill-backup.service
last="$(ls -1t /var/backups/nhill/archives/nhill-*.dump.age | head -1)"
head -c 21 "$last" | grep -q 'age-encryption.org/v1' || fail "backup not encrypted"
! grep -aq 'nhill_owner' "$last" || fail "backup readable in clear"
off="$(mktemp -d /tmp/offsite-XXXX)"; cp "$last" "$last.sha256" "$off/"; cp "$IDENTITY" "$off/key.txt"
chmod 755 "$off"; chmod 644 "$off"/*; chown -R nobody "$off"
# shellcheck disable=SC2024  # run as root: the output file is written by root
sudo -u nobody env TMPDIR="$off" bash "$HERE/nhill-restore-test.sh" "$off/$(basename "$last")" "$off/key.txt" >/tmp/restore.txt 2>&1 \
  || { cat /tmp/restore.txt; fail "restore test"; }
grep -q 'RESTORE TEST PASSED' /tmp/restore.txt || fail "restore test verdict"
rm -rf "$off"
pass "backup encrypted with age; restored outside the server: $(grep -c '^ok' /tmp/restore.txt) checks passed"

# --- update with a way back
admins_before="$(sql "SELECT count(*) FROM nhill.admin_users")"
audit_before="$(sql "SELECT count(*) FROM nhill.audit_log")"
/usr/sbin/nhill-update "$DEB" --sha256sums "$SUMS" --reinstall --yes >/tmp/update.txt 2>&1 || { cat /tmp/update.txt; fail "update"; }
pre="$(sudo -u postgres psql -qtAc "SELECT datname FROM pg_database WHERE datname LIKE 'nhill\_pre\_%' ORDER BY datname DESC LIMIT 1")"
[[ -n "$pre" ]] || fail "no database copy before the update"
health_ok || fail "API down after update"
pass "update: backup + database copy $pre, package reinstalled, API back"
sql "INSERT INTO nhill.audit_log (actor_type, action, entity, details) VALUES ('system', 'ci.after_update', 'ci', '{}'::jsonb)" >/dev/null
/usr/sbin/nhill-rollback --yes >/tmp/rollback.txt 2>&1 || { cat /tmp/rollback.txt; fail "rollback"; }
wait_health 30 || fail "API down after rollback"
[[ "$(sql "SELECT count(*) FROM nhill.admin_users")" == "$admins_before" ]] || fail "administrators changed by the rollback"
[[ "$(sql "SELECT count(*) FROM nhill.audit_log")" == "$audit_before" ]] || fail "rollback did not restore the copy taken before the update"
failed_db="$(sudo -u postgres psql -qtAc "SELECT datname FROM pg_database WHERE datname LIKE 'nhill\_failed\_%' LIMIT 1")"
[[ -n "$failed_db" ]] || fail "database of the update not kept"
[[ "$(sudo -u postgres psql -qtAX -d "$failed_db" -c "SELECT count(*) FROM nhill.audit_log WHERE action = 'ci.after_update'")" == 1 ]] || fail "data written after the update lost"
pass "way back: database copy restored (row written after the update set aside in $failed_db), API back"
/usr/sbin/nhill-healthcheck >/dev/null || fail "health check after the way back"
pass "health check after the way back"

# --- an older server refuses a database migrated by a newer version
sql "INSERT INTO nhill.schema_migrations (version, checksum) VALUES ('9999_newer_version', 'x')" >/dev/null
systemctl restart nhill-server || true
sleep 8
! health_ok || fail "server started on a database migrated by a newer version"
journalctl -u nhill-server -n 30 --no-pager | grep -q 'migrated by a newer version' || fail "no explanation in the journal"
sql "DELETE FROM nhill.schema_migrations WHERE version = '9999_newer_version'" >/dev/null
systemctl reset-failed nhill-server || true
systemctl restart nhill-server
wait_health 60 || fail "server not back"
pass "older server refuses a newer database (explained in the journal), starts again once fixed"

# --- first administrator token
out="$(sudo -u nhill /usr/bin/nhill-server --config /etc/nhill/server.env admin-bootstrap ci-admin@example.com CI 2>&1)" || fail "token reissue: $out"
grep -q 'First administrator pending' <<<"$out" || fail "token reissue output"
! sudo -u nhill /usr/bin/nhill-server --config /etc/nhill/server.env admin-bootstrap other@example.com X >/dev/null 2>&1 || fail "token issued for another address"
pass "first administrator: new token for the pending address only"

echo
echo "VPS ACCEPTANCE PASSED: $n checks"
