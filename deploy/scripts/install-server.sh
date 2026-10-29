#!/usr/bin/env bash
# First installation of the Nhill server on a Linux host (Ubuntu 24.04 tested
# for the build; this script was exercised step by step, not on a production host).
# Run as root from a build directory produced by scripts/build-linux.sh:
#   sudo deploy/scripts/install-server.sh BUILD_DIR
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
BUILD="${1:?build directory}"
[ "$(id -u)" = 0 ] || { echo "run as root" >&2; exit 1; }

cmake --install "$BUILD" --component server --prefix /usr
id nhill >/dev/null 2>&1 || useradd --system --home /nonexistent --shell /usr/sbin/nologin nhill
id nhill-backup >/dev/null 2>&1 || useradd --system --home /var/backups/nhill --shell /usr/sbin/nologin nhill-backup
install -d -m 0750 -o nhill -g nhill /etc/nhill
install -d -m 0700 -o nhill-backup -g nhill-backup /var/backups/nhill
if [ ! -f /etc/nhill/server.env ]; then
  install -m 0600 -o nhill -g nhill "$ROOT/config/server.env.example" /etc/nhill/server.env
  sed -i -e "s/^NH_SECRET_KEY=$/NH_SECRET_KEY=$(openssl rand -hex 32)/" \
         -e 's/^NH_DATABASE_URL=/#NH_DATABASE_URL=/' -e 's/^NH_MIGRATION_DATABASE_URL=/#NH_MIGRATION_DATABASE_URL=/' \
         /etc/nhill/server.env
  echo ">> /etc/nhill/server.env created with a new NH_SECRET_KEY and no database (setup mode),"
  echo ">> and keep a copy of NH_SECRET_KEY in your password safe (not with the backups)."
fi
install -m 0644 "$ROOT/deploy/systemd/nhill-server.service" /etc/systemd/system/
install -m 0644 "$ROOT/deploy/systemd/nhill-backup.service" "$ROOT/deploy/systemd/nhill-backup.timer" /etc/systemd/system/
install -m 0755 "$ROOT/deploy/backup/nhill-backup.sh" "$ROOT/deploy/backup/nhill-restore.sh" /usr/sbin/
[ -f /etc/nhill/backup.env ] || install -m 0600 -o nhill-backup -g nhill-backup "$ROOT/deploy/backup/backup.env.example" /etc/nhill/backup.env
systemctl daemon-reload
cat <<MSG
Recommended (docs/archive/deploiement-production.md § 4): leave the database settings empty,
  systemctl enable --now nhill-server, read the setup token (journalctl -u nhill-server)
  and install the database from Nhill Console ("Install a new server").
Manual alternative:
  1. PostgreSQL roles:  sudo -u postgres psql -v owner_password="'…'" -v app_password="'…'" -f $ROOT/db/bootstrap/00-roles.sql
                        sudo -u postgres psql -d nhill -v backup_password="'…'" -f $ROOT/db/bootstrap/01-backup-role.sql
  2. Edit /etc/nhill/server.env and /etc/nhill/backup.env
  3. sudo -u nhill nhill-server --config /etc/nhill/server.env check
  4. systemctl enable --now nhill-server nhill-backup.timer
  5. Reverse proxy: deploy/caddy/Caddyfile (TLS), then the first administrator:
     sudo -u nhill nhill-server --config /etc/nhill/server.env admin-bootstrap you@example.com "Your Name"
MSG
