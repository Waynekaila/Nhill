#!/usr/bin/env bash
# Local development stack: throwaway PostgreSQL 16 + nhill-server on
# http://127.0.0.1:8080. NOT for production (trust authentication, test keys).
#   scripts/dev/dev-stack.sh start  [build-dir]   (default build dir: build)
#   scripts/dev/dev-stack.sh stop
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
STATE="${NH_DEV_STATE:-/tmp/nhill-dev}"
PGBIN="${NH_PG_BIN:-$(ls -d /usr/lib/postgresql/*/bin 2>/dev/null | sort -V | tail -n1)}"
PGBIN="${PGBIN:-/usr/lib/postgresql/16/bin}"
PORT_PG="${NH_DEV_PG_PORT:-55433}"
BUILD="${2:-$ROOT/build}"
AS=""; [ "$(id -u)" = "0" ] && AS="runuser -u postgres --"

start() {
  mkdir -p "$STATE"; [ "$(id -u)" = "0" ] && chown postgres:postgres "$STATE"
  if [ ! -d "$STATE/data" ]; then
    $AS "$PGBIN/initdb" -D "$STATE/data" -A trust -U postgres -E UTF8 --locale=C.UTF-8 >/dev/null
  fi
  $AS "$PGBIN/pg_ctl" -D "$STATE/data" -o "-p $PORT_PG -k $STATE -c listen_addresses=''" -l "$STATE/pg.log" -w start >/dev/null
  psql "host=$STATE port=$PORT_PG user=postgres dbname=postgres" -q -v ON_ERROR_STOP=1 \
       -v owner_password="'dev-owner'" -v app_password="'dev-app'" -f "$ROOT/db/bootstrap/00-roles.sql" >/dev/null
  cat > "$STATE/server.env" <<CONF
NH_DATABASE_URL=host=$STATE port=$PORT_PG user=nhill_app password=dev-app dbname=nhill
NH_MIGRATION_DATABASE_URL=host=$STATE port=$PORT_PG user=nhill_owner password=dev-owner dbname=nhill
NH_SECRET_KEY=$( [ -f "$STATE/secret" ] || openssl rand -hex 32 > "$STATE/secret"; cat "$STATE/secret")
NH_LISTEN_ADDRESS=127.0.0.1
NH_LISTEN_PORT=8080
NH_PUBLIC_BASE_URL=http://127.0.0.1:8080
NH_PROFILE_DIR=$ROOT/server/profiles
NH_I18N_DIR=$ROOT/server/i18n
NH_FONT_DIR=$ROOT/assets/fonts
NH_MIGRATIONS_DIR=$ROOT/db/migrations
NH_JOBS_INTERVAL_SECONDS=60
NH_WEBHOOK_SECRET_GENERIC=dev-webhook-secret-0123456789abcdefghij
CONF
  chmod 600 "$STATE/server.env"
  "$BUILD/server/nhill-server" --config "$STATE/server.env" migrate
  nohup "$BUILD/server/nhill-server" --config "$STATE/server.env" serve > "$STATE/server.log" 2>&1 &
  echo $! > "$STATE/server.pid"
  for i in $(seq 1 50); do curl -sf http://127.0.0.1:8080/health >/dev/null && break; sleep 0.1; done
  echo "Nhill dev server: http://127.0.0.1:8080  (logs: $STATE/server.log)"
}

stop() {
  [ -f "$STATE/server.pid" ] && kill "$(cat "$STATE/server.pid")" 2>/dev/null || true
  $AS "$PGBIN/pg_ctl" -D "$STATE/data" -m fast stop >/dev/null 2>&1 || true
}

case "${1:-}" in
  start) start ;;
  stop) stop ;;
  *) echo "usage: $0 start|stop [build-dir]"; exit 2 ;;
esac
