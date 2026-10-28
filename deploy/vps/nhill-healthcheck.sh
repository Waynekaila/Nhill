#!/usr/bin/env bash
# Nhill — health of the VPS (run every 5 minutes by nhill-healthcheck.timer, or by hand).
# Prints one line per check; exit code 1 when a check fails (the timer then starts
# nhill-alert.service). The last result is kept in /var/lib/nhill/health.json.
# Settings: /etc/nhill/health.env (written by provision.sh).
set -uo pipefail
[[ -f /etc/nhill/health.env ]] && . /etc/nhill/health.env
PUBLIC_URL="${NH_HEALTH_PUBLIC_URL:-}"
PUBLIC_HOST="${NH_HEALTH_PUBLIC_HOST:-}"
CAFILE="${NH_HEALTH_CAFILE:-}"
BACKUP_DIR="${NH_HEALTH_BACKUP_DIR:-/var/backups/nhill/archives}"
MAX_BACKUP_AGE_H="${NH_HEALTH_MAX_BACKUP_AGE_HOURS:-26}"
DISK_MAX="${NH_HEALTH_DISK_MAX_PERCENT:-85}"
CERT_MIN_DAYS="${NH_HEALTH_CERT_MIN_DAYS:-14}"

failed=() results=()
check() { # check ID OK(0/1) DETAIL
  local id="$1" ok="$2" detail="$3"
  if [[ "$ok" == 0 ]]; then printf 'ok    %-22s %s\n' "$id" "$detail"
  else printf 'FAIL  %-22s %s\n' "$id" "$detail"; failed+=("$id: $detail"); fi
  results+=("$(jq -cn --arg id "$id" --argjson ok "$([[ $ok == 0 ]] && echo true || echo false)" --arg d "$detail" '{id:$id, ok:$ok, detail:$d}')")
}

for unit in nhill-server postgresql caddy; do
  st="$(systemctl is-active "$unit" 2>/dev/null)"; check "service $unit" "$([[ $st == active ]]; echo $?)" "$st"
done

body="$(curl -sS --max-time 5 http://127.0.0.1:8080/health 2>&1)"
check "api local" "$(jq -e '.status == "ok" and .database == true' <<<"$body" >/dev/null 2>&1; echo $?)" "${body:0:120}"

if [[ -n "$PUBLIC_URL" && -n "$PUBLIC_HOST" ]]; then
  opts=(--max-time 10 --resolve "$PUBLIC_HOST:443:127.0.0.1")
  [[ -n "$CAFILE" ]] && opts+=(--cacert "$CAFILE")
  code="$(curl -sS -o /dev/null -w '%{http_code}' "${opts[@]}" "$PUBLIC_URL" 2>&1)"
  check "api https (Caddy)" "$([[ $code == 200 ]]; echo $?)" "HTTP $code via $PUBLIC_URL"
  end="$(echo | openssl s_client -connect 127.0.0.1:443 -servername "$PUBLIC_HOST" 2>/dev/null | openssl x509 -noout -enddate 2>/dev/null | cut -d= -f2)"
  if [[ -n "$end" ]]; then
    days=$(( ( $(date -d "$end" +%s) - $(date +%s) ) / 86400 ))
    check "certificate" "$([[ $days -ge $CERT_MIN_DAYS ]]; echo $?)" "$days days left ($end)"
  else
    check "certificate" 1 "no certificate served for $PUBLIC_HOST"
  fi
fi

bad="$(ss -ltnH '( sport = :5432 or sport = :8080 )' | awk '{print $4}' | grep -vE '^(127\.0\.0\.1|\[::1\]):(5432|8080)$' || true)"
check "local-only ports" "$([[ -z $bad ]]; echo $?)" "${bad:-PostgreSQL 5432 and API 8080 on loopback only}"
ufw_state="$(ufw status 2>/dev/null | head -1)"
check "firewall" "$([[ $ufw_state == 'Status: active' ]]; echo $?)" "$ufw_state"

for mp in / /var/backups; do
  [[ -d "$mp" ]] || continue
  used="$(df --output=pcent "$mp" | tail -1 | tr -dc 0-9)"
  check "disk $mp" "$([[ $used -lt $DISK_MAX ]]; echo $?)" "${used}% used (alert at ${DISK_MAX}%)"
done

last="$(ls -1t "$BACKUP_DIR"/nhill-*.dump.age 2>/dev/null | head -1)"
if [[ -n "$last" ]]; then
  age_h=$(( ( $(date +%s) - $(stat -c %Y "$last") ) / 3600 ))
  check "last backup" "$([[ $age_h -lt $MAX_BACKUP_AGE_H && -s $last ]]; echo $?)" "$(basename "$last"), ${age_h} h old"
else
  check "last backup" 1 "no backup in $BACKUP_DIR"
fi
st="$(systemctl show -p Result --value nhill-backup.service 2>/dev/null)"
check "backup service" "$([[ $st == success || -z $st ]]; echo $?)" "last run: ${st:-never}"

install -d -m 0755 /var/lib/nhill 2>/dev/null || true
printf '%s\n' "${results[@]}" | jq -s --arg at "$(date -u +%FT%TZ)" '{checked_at:$at, ok:(all(.[]; .ok)), checks:.}' \
  >/var/lib/nhill/health.json 2>/dev/null || true
if (( ${#failed[@]} )); then
  printf '%s\n' "${failed[@]}" >/var/lib/nhill/health-failed.txt 2>/dev/null || true
  exit 1
fi
rm -f /var/lib/nhill/health-failed.txt
exit 0
