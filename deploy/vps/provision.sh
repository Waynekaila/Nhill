#!/usr/bin/env bash
# =============================================================================
# Nhill — installation of Nhill Server and its database on a NEW Ubuntu 24.04 VPS
# (target: Hostinger VPS in France, "plain OS" Ubuntu 24.04). Run as root ON the VPS:
#
#   sudo deploy/vps/provision.sh \
#     --deb nhill-server_3.0.0-1_amd64.deb --sha256sums SHA256SUMS \
#     --api-domain api.example.com --acme-email ops@example.com \
#     --admin-email you@example.com --admin-name "Your Name" \
#     --backup-recipient age1... [--offsite-cmd 'rclone copy {} remote:nhill-backups/'] \
#     [--support-email support@nhill.online] [--ssh-port 22] [--ssh-allow CIDR] \
#     [--harden-ssh] [--tls acme|internal] [--yes]
#
# What it does (each step checks its result and stops at the first failure):
#   1. checks the system (Ubuntu 24.04, amd64, memory, disk, clock) and the package checksum;
#   2. installs from the Ubuntu archive: PostgreSQL 16, Caddy, age, ufw, fail2ban,
#      unattended-upgrades (security updates, no automatic reboot);
#   3. firewall: incoming traffic refused except SSH, 80/tcp and 443/tcp+udp;
#   4. PostgreSQL listens on the loopback only (never reachable from the Internet);
#   5. installs the nhill-server package, then the database with `nhill-server setup`
#      through a temporary superuser that is deleted afterwards (its password is never
#      written to disk); roles nhill_owner / nhill_app / nhill_backup with random passwords;
#   6. encrypted daily backups (age, public key only on the VPS), optional off-site copy;
#   7. Caddy in front (TLS certificates from Let's Encrypt, renewed automatically);
#   8. health checks every 5 minutes, journald retention, logs;
#   9. first administrator of Nhill Console: one-time token printed at the end (30 min).
#
# Nothing here needs a Hostinger account or API: the Hostinger panel steps (firewall,
# snapshots, DNS) are described in docs/fr/deploiement-vps-hostinger.md. Re-running the
# script on an installed VPS is safe: installed steps are kept, settings are re-applied.
# Secrets (NH_SECRET_KEY, database passwords) are generated on the VPS and never printed,
# except the backup account password, written only to its .pgpass file.
# =============================================================================
set -euo pipefail
umask 022
HERE="$(cd "$(dirname "$0")" && pwd)"

DEB="" SUMS="" SHA="" API_DOMAIN="" ACME_EMAIL="" ADMIN_EMAIL="" ADMIN_NAME="" RECIPIENTS=""
OFFSITE="" SUPPORT_EMAIL="" SSH_PORT=22 SSH_ALLOW="" HARDEN_SSH=0 TLS=acme YES=0 FORCE_DISTRO=0
usage() { sed -n '2,33p' "$0" | sed 's/^# \{0,1\}//'; exit "${1:-0}"; }
while [[ $# -gt 0 ]]; do
  case "$1" in
    --deb) DEB="$2"; shift ;;
    --sha256sums) SUMS="$2"; shift ;;
    --sha256) SHA="$2"; shift ;;
    --api-domain) API_DOMAIN="$2"; shift ;;
    --acme-email) ACME_EMAIL="$2"; shift ;;
    --admin-email) ADMIN_EMAIL="$2"; shift ;;
    --admin-name) ADMIN_NAME="$2"; shift ;;
    --backup-recipient) RECIPIENTS="${RECIPIENTS:+$RECIPIENTS }$2"; shift ;;
    --offsite-cmd) OFFSITE="$2"; shift ;;
    --support-email) SUPPORT_EMAIL="$2"; shift ;;
    --ssh-port) SSH_PORT="$2"; shift ;;
    --ssh-allow) SSH_ALLOW="$2"; shift ;;
    --harden-ssh) HARDEN_SSH=1 ;;
    --tls) TLS="$2"; shift ;;
    --yes) YES=1 ;;
    --force-distro) FORCE_DISTRO=1 ;;
    -h|--help) usage 0 ;;
    *) echo "unknown option: $1" >&2; usage 2 ;;
  esac
  shift
done

say()  { printf '\n==> %s\n' "$*"; }
ok()   { printf '  ok  %s\n' "$*"; }
warn() { printf '  !!  %s\n' "$*" >&2; }
die()  { printf '\nERROR: %s\n' "$*" >&2; exit 1; }
# KEY=VALUE in an env file: replaced if present (commented or not), appended otherwise.
set_kv() {
  local file="$1" key="$2" value="$3" tmp
  tmp="$(mktemp "$file.XXXX")"
  awk -v k="$key" -v v="$value" 'BEGIN{done=0}
    $0 ~ "^#? *"k"=" { if (!done) { print k"="v; done=1 } ; next } { print }
    END { if (!done) print k"="v }' "$file" >"$tmp"
  chown --reference="$file" "$tmp"; chmod --reference="$file" "$tmp"; mv "$tmp" "$file"
}
wait_http() { # wait_http URL SECONDS [curl options...]
  local url="$1" t="$2"; shift 2
  for _ in $(seq 1 "$t"); do curl -fsS --max-time 5 "$@" "$url" >/dev/null 2>&1 && return 0; sleep 1; done
  return 1
}

# ----------------------------------------------------------------- 1. checks
say "1/9 System and package checks"
[[ $EUID -eq 0 ]] || die "run as root (sudo)"
[[ -n "$DEB" && -f "$DEB" ]] || die "--deb: nhill-server package file required"
[[ -n "$API_DOMAIN" ]] || die "--api-domain required (for example api.nhill.online, pointing to this VPS)"
[[ "$API_DOMAIN" =~ ^[A-Za-z0-9.-]+$ ]] || die "--api-domain: host name only, without https://"
[[ -n "$ADMIN_EMAIL" && -n "$ADMIN_NAME" ]] || die "--admin-email and --admin-name required (first Nhill Console administrator)"
[[ -n "$RECIPIENTS" ]] || die "--backup-recipient required: age PUBLIC key; create the key pair OFF this server (age-keygen)"
for r in $RECIPIENTS; do [[ "$r" =~ ^age1[0-9a-z]{58}$ ]] || die "invalid age public key: $r"; done
[[ "$TLS" == acme || "$TLS" == internal ]] || die "--tls acme|internal"
[[ "$TLS" == internal || -n "$ACME_EMAIL" ]] || die "--acme-email required with Let's Encrypt (certificate expiry notices)"
[[ "$SSH_PORT" =~ ^[0-9]+$ ]] || die "--ssh-port: number"
. /etc/os-release
if [[ "${ID:-}" != ubuntu || "${VERSION_ID:-}" != 24.04 ]]; then
  [[ $FORCE_DISTRO == 1 ]] || die "Ubuntu 24.04 expected (found ${PRETTY_NAME:-unknown}); --force-distro to continue at your own risk"
  warn "distribution ${PRETTY_NAME:-unknown} not tested"
fi
[[ "$(dpkg --print-architecture)" == amd64 ]] || die "amd64 expected (package built for amd64)"
mem_mb=$(( $(awk '/MemTotal/ {print $2}' /proc/meminfo) / 1024 ))
(( mem_mb >= 1800 )) || die "at least 2 GB of RAM expected (found ${mem_mb} MB)"
disk_gb=$(( $(df --output=avail -k / | tail -1) / 1024 / 1024 ))
(( disk_gb >= 10 )) || die "at least 10 GB free on / expected (found ${disk_gb} GB)"
ok "Ubuntu ${VERSION_ID:-?}, ${mem_mb} MB RAM, ${disk_gb} GB free"
pkg_name="$(dpkg-deb -f "$DEB" Package)"; pkg_ver="$(dpkg-deb -f "$DEB" Version)"
[[ "$pkg_name" == nhill-server ]] || die "$DEB is $pkg_name, not nhill-server"
if [[ -n "$SHA" ]]; then
  echo "$SHA  $DEB" | sha256sum -c --quiet || die "SHA-256 of $DEB does not match --sha256"
elif [[ -n "$SUMS" ]]; then
  expected="$(awk -v f="$(basename "$DEB")" '$2==f || $2=="*"f {print $1}' "$SUMS")"
  [[ -n "$expected" ]] || die "$(basename "$DEB") is not listed in $SUMS"
  echo "$expected  $DEB" | sha256sum -c --quiet || die "SHA-256 of $DEB does not match $SUMS"
else
  die "--sha256sums FILE or --sha256 HEX required: the package is installed only after its checksum is verified"
fi
ok "package $pkg_name $pkg_ver, checksum verified"
if [[ $YES != 1 ]]; then
  read -r -p "Install Nhill Server $pkg_ver on this VPS for https://$API_DOMAIN ? [y/N] " a
  [[ "$a" == [yYoO]* ]] || die "cancelled"
fi

# ----------------------------------------------------------------- 2. packages
say "2/9 System packages (Ubuntu archive) and automatic security updates"
export DEBIAN_FRONTEND=noninteractive
apt-get update -q
apt-cache policy caddy | grep -q 'Candidate: [0-9]' || die "caddy not available: enable the 'universe' component (add-apt-repository universe)"
apt-get install -y -q --no-install-recommends postgresql postgresql-client caddy age ufw fail2ban \
  unattended-upgrades curl jq ca-certificates openssl >/dev/null
cat >/etc/apt/apt.conf.d/52nhill-unattended-upgrades <<'EOF'
// Nhill: security updates are installed automatically; the server is never rebooted
// automatically (planned reboot: see docs/fr/deploiement-vps-hostinger.md).
Unattended-Upgrade::Automatic-Reboot "false";
Unattended-Upgrade::Remove-Unused-Dependencies "true";
EOF
cat >/etc/apt/apt.conf.d/20auto-upgrades <<'EOF'
APT::Periodic::Update-Package-Lists "1";
APT::Periodic::Unattended-Upgrade "1";
EOF
timedatectl set-ntp true >/dev/null 2>&1 || warn "could not enable time synchronisation (timedatectl)"
ok "postgresql $(psql --version | awk '{print $3}'), caddy $(caddy version 2>/dev/null | awk '{print $1}'), age, ufw, fail2ban"

# ----------------------------------------------------------------- 3. firewall, SSH
say "3/9 Firewall (ufw) and SSH"
ufw --force default deny incoming >/dev/null
ufw --force default allow outgoing >/dev/null
if [[ -n "$SSH_ALLOW" ]]; then ufw allow from "$SSH_ALLOW" to any port "$SSH_PORT" proto tcp >/dev/null
else ufw limit "$SSH_PORT/tcp" >/dev/null; fi
ufw allow 80/tcp >/dev/null      # ACME HTTP-01 challenge and redirection to HTTPS
ufw allow 443/tcp >/dev/null
ufw allow 443/udp >/dev/null     # HTTP/3
ufw --force enable >/dev/null
ok "incoming: SSH ${SSH_PORT}/tcp${SSH_ALLOW:+ from $SSH_ALLOW}, 80/tcp, 443/tcp+udp; everything else refused"
cat >/etc/fail2ban/jail.d/nhill-sshd.local <<EOF
[sshd]
enabled = true
port = $SSH_PORT
backend = systemd
maxretry = 5
findtime = 10m
bantime = 1h
EOF
systemctl enable --now fail2ban >/dev/null 2>&1 && systemctl restart fail2ban
ok "fail2ban: SSH jail active"
if [[ $HARDEN_SSH == 1 ]]; then
  keys=0
  for f in /root/.ssh/authorized_keys /home/*/.ssh/authorized_keys; do
    [[ -s "$f" ]] && grep -qE '^(ssh-|ecdsa-|sk-)' "$f" && keys=1
  done
  if [[ $keys == 1 ]]; then
    cat >/etc/ssh/sshd_config.d/50-nhill.conf <<EOF
# Nhill: key-based SSH only (password logins refused).
PasswordAuthentication no
KbdInteractiveAuthentication no
PermitRootLogin prohibit-password
EOF
    sshd -t && systemctl reload ssh && ok "SSH: keys only, passwords refused"
  else
    warn "--harden-ssh ignored: no SSH public key installed (it would lock you out); add a key first"
  fi
else
  warn "SSH passwords still accepted: add an SSH key, then re-run with --harden-ssh"
fi

# ----------------------------------------------------------------- 4. PostgreSQL
say "4/9 PostgreSQL (loopback only)"
PGV="$(ls /etc/postgresql | sort -V | tail -1)"
[[ -n "$PGV" && -d "/etc/postgresql/$PGV/main" ]] || die "PostgreSQL cluster not found in /etc/postgresql"
shared=$(( mem_mb / 4 )); (( shared > 2048 )) && shared=2048
cat >"/etc/postgresql/$PGV/main/conf.d/50-nhill.conf" <<EOF
# Nhill — local database: never reachable from the network.
listen_addresses = 'localhost'
password_encryption = scram-sha-256
shared_buffers = ${shared}MB
effective_cache_size = $(( mem_mb / 2 ))MB
max_connections = 100
log_min_duration_statement = 2000
log_lock_waits = on
EOF
systemctl enable postgresql >/dev/null 2>&1 || true
systemctl enable --now "postgresql@$PGV-main" >/dev/null 2>&1 || systemctl enable --now postgresql >/dev/null
systemctl restart "postgresql@$PGV-main"
for _ in $(seq 1 30); do sudo -u postgres pg_isready -q && break; sleep 1; done
sudo -u postgres pg_isready -q || die "PostgreSQL does not answer"
if ss -ltnH '( sport = :5432 )' | awk '{print $4}' | grep -vqE '^(127\.0\.0\.1|\[::1\]):5432$'; then
  die "PostgreSQL listens on a non-loopback address: check /etc/postgresql/$PGV/main/postgresql.conf"
fi
sudo -u postgres psql -qtAc "SELECT 1 FROM pg_database WHERE datname='nhill'" | grep -q 1 \
  || sudo -u postgres createdb --encoding=UTF8 --template=template0 nhill
ok "PostgreSQL $PGV on 127.0.0.1/::1 only, database nhill present"

# ----------------------------------------------------------------- 5. Nhill Server
say "5/9 Nhill Server $pkg_ver and its database"
install -d -m 0750 /var/lib/nhill/releases
cp -f "$DEB" "/var/lib/nhill/releases/$(basename "$DEB")"
apt-get install -y -q "/var/lib/nhill/releases/$(basename "$DEB")" >/dev/null
ENVF=/etc/nhill/server.env
[[ -f $ENVF ]] || die "$ENVF missing after package installation"
if ! grep -q '^NH_DATABASE_URL=' "$ENVF"; then
  systemctl stop nhill-server
  setup_pw="$(openssl rand -hex 24)"
  # Temporary superuser for the installation only. Its password (hexadecimal) goes through
  # standard input, never through a command line visible in the process list.
  sudo -u postgres psql -v ON_ERROR_STOP=1 -q <<SQL
\\set pw '$setup_pw'
SELECT 'CREATE ROLE nhill_setup' WHERE NOT EXISTS (SELECT FROM pg_roles WHERE rolname = 'nhill_setup')\\gexec
ALTER ROLE nhill_setup WITH LOGIN SUPERUSER PASSWORD :'pw';
SQL
  report="$(mktemp /root/nhill-setup-XXXX.json)"; chmod 600 "$report"
  set +e
  # shellcheck disable=SC2024  # the script runs as root: root writes the report, not nhill
  printf '%s\n' "$setup_pw" | sudo -u nhill /usr/bin/nhill-server --config "$ENVF" setup \
    --host 127.0.0.1 --port 5432 --database nhill --user nhill_setup >"$report" 2>/root/nhill-setup.log
  rc=$?
  set -e
  unset setup_pw
  sudo -u postgres psql -q -d nhill -c "DROP OWNED BY nhill_setup" >/dev/null 2>&1 || true
  sudo -u postgres psql -q -c "DROP ROLE IF EXISTS nhill_setup" >/dev/null
  if [[ $rc -ne 0 ]] || [[ "$(jq -r '.ok' "$report" 2>/dev/null)" != true ]]; then
    jq 'del(.backup_account)' "$report" >&2 2>/dev/null || cat /root/nhill-setup.log >&2
    shred -u "$report"
    die "database installation failed (details above and in /root/nhill-setup.log)"
  fi
  bpw="$(jq -r '.backup_account.password // empty' "$report")"
  [[ -n "$bpw" ]] || { shred -u "$report"; die "backup account not created (see /root/nhill-setup.log)"; }
  install -m 0600 -o nhill-backup -g nhill-backup /dev/null /var/backups/nhill/.pgpass
  printf '127.0.0.1:5432:nhill:nhill_backup:%s\n' "$bpw" >/var/backups/nhill/.pgpass
  unset bpw
  jq '{ok, migrations: .migrations.applied | length, checks: [.verification.checks[] | {id, ok}]}' "$report"
  shred -u "$report"
  ok "database installed (roles nhill_owner, nhill_app, nhill_backup; isolation checks passed)"
else
  ok "database already configured: kept"
fi
set_kv "$ENVF" NH_LISTEN_ADDRESS 127.0.0.1
set_kv "$ENVF" NH_TRUST_PROXY 1
set_kv "$ENVF" NH_PUBLIC_BASE_URL "https://$API_DOMAIN"
[[ -n "$SUPPORT_EMAIL" ]] && set_kv "$ENVF" NH_SUPPORT_EMAIL "$SUPPORT_EMAIL"
[[ "$(stat -c '%a %U' "$ENVF")" == "600 nhill" ]] || die "$ENVF must be mode 600, owner nhill"
systemctl enable nhill-server >/dev/null 2>&1
systemctl restart nhill-server
wait_http http://127.0.0.1:8080/health 60 || { journalctl -u nhill-server -n 50 --no-pager >&2; die "Nhill Server does not answer on 127.0.0.1:8080"; }
curl -fsS http://127.0.0.1:8080/health | jq -e '.database == true' >/dev/null || die "Nhill Server answers without its database"
sudo -u nhill /usr/bin/nhill-server --config "$ENVF" check
ok "Nhill Server $pkg_ver running on 127.0.0.1:8080 (local only)"

# ----------------------------------------------------------------- 6. backups
say "6/9 Encrypted backups"
install -d -m 0700 -o nhill-backup -g nhill-backup /var/backups/nhill/archives
BKF=/etc/nhill/backup.env
[[ -f $BKF ]] || install -m 0600 -o nhill-backup -g nhill-backup /usr/share/nhill/examples/backup.env.example "$BKF"
set_kv "$BKF" PGHOST 127.0.0.1
set_kv "$BKF" PGPORT 5432
set_kv "$BKF" PGDATABASE nhill
set_kv "$BKF" PGUSER nhill_backup
set_kv "$BKF" PGPASSFILE /var/backups/nhill/.pgpass
set_kv "$BKF" NH_BACKUP_RECIPIENTS "\"$RECIPIENTS\""
set_kv "$BKF" NH_BACKUP_DIR /var/backups/nhill/archives
set_kv "$BKF" NH_BACKUP_KEEP_DAYS 14
if [[ -n "$OFFSITE" ]]; then set_kv "$BKF" NH_BACKUP_OFFSITE_CMD "\"$OFFSITE\""
else warn "no off-site copy (--offsite-cmd): the backups stay on this VPS only; add a copy outside Hostinger"; fi
systemctl daemon-reload
systemctl enable --now nhill-backup.timer >/dev/null
systemctl start nhill-backup.service || { journalctl -u nhill-backup -n 30 --no-pager >&2; die "first backup failed"; }
last="$(ls -1t /var/backups/nhill/archives/nhill-*.dump.age 2>/dev/null | head -1)"
[[ -n "$last" && -s "$last" ]] || die "first backup not found in /var/backups/nhill/archives"
head -c 21 "$last" | grep -q 'age-encryption.org/v1' || die "backup file is not age-encrypted"
ok "first backup: $last ($(du -h "$last" | cut -f1)), encrypted; daily at 02:30 (+0-15 min)"

# ----------------------------------------------------------------- 7. Caddy (TLS)
say "7/9 Caddy (HTTPS)"
install -d -o caddy -g caddy /var/log/caddy
{
  echo "# Generated by deploy/vps/provision.sh — Nhill API at https://$API_DOMAIN"
  if [[ "$TLS" == acme ]]; then printf '{\n\temail %s\n}\n\n' "$ACME_EMAIL"; fi
  sed -e "s/^api\.example\.com {/$API_DOMAIN {/" -e '/^# certificate automatically/d' \
      -e "s/^# Nhill — TLS reverse proxy (Caddy 2).*/# Nhill — TLS reverse proxy (Caddy 2)./" \
      /usr/share/nhill/examples/Caddyfile.example
} >/etc/caddy/Caddyfile.new
if [[ "$TLS" == internal ]]; then
  # Tests without a public name: certificate from Caddy's local authority.
  sed -i "0,/^$API_DOMAIN {/s//$API_DOMAIN {\n\ttls internal/" /etc/caddy/Caddyfile.new
fi
caddy validate --adapter caddyfile --config /etc/caddy/Caddyfile.new >/dev/null 2>&1 \
  || { caddy validate --adapter caddyfile --config /etc/caddy/Caddyfile.new >&2 || true; die "invalid Caddyfile"; }
mv /etc/caddy/Caddyfile.new /etc/caddy/Caddyfile
systemctl enable caddy >/dev/null 2>&1
systemctl reload caddy 2>/dev/null || systemctl restart caddy
CURL_TLS=()
if [[ "$TLS" == internal ]]; then
  for _ in $(seq 1 30); do [[ -f /var/lib/caddy/.local/share/caddy/pki/authorities/local/root.crt ]] && break; sleep 1; done
  CURL_TLS=(--cacert /var/lib/caddy/.local/share/caddy/pki/authorities/local/root.crt)
fi
if wait_http "https://$API_DOMAIN/health" 120 --resolve "$API_DOMAIN:443:127.0.0.1" "${CURL_TLS[@]}"; then
  ok "https://$API_DOMAIN/health answers through Caddy (certificate verified)"
else
  warn "https://$API_DOMAIN not reachable yet: the DNS record must point to this VPS and ports 80/443 must be open"
  warn "in the Hostinger firewall; Caddy retries automatically (journalctl -u caddy)"
fi

# ----------------------------------------------------------------- 8. monitoring, logs
say "8/9 Health checks and logs"
install -m 0755 "$HERE/nhill-healthcheck.sh" /usr/sbin/nhill-healthcheck
install -m 0755 "$HERE/nhill-alert.sh" /usr/sbin/nhill-alert
install -m 0755 "$HERE/nhill-update.sh" /usr/sbin/nhill-update
install -m 0755 "$HERE/nhill-rollback.sh" /usr/sbin/nhill-rollback
install -m 0644 "$HERE"/systemd/nhill-healthcheck.service "$HERE"/systemd/nhill-healthcheck.timer \
  "$HERE"/systemd/nhill-alert.service /etc/systemd/system/
HEF=/etc/nhill/health.env
[[ -f $HEF ]] || install -m 0640 -o root -g nhill /dev/null "$HEF"
set_kv "$HEF" NH_HEALTH_PUBLIC_URL "https://$API_DOMAIN/health"
set_kv "$HEF" NH_HEALTH_PUBLIC_HOST "$API_DOMAIN"
set_kv "$HEF" NH_HEALTH_CAFILE "$([[ $TLS == internal ]] && echo /var/lib/caddy/.local/share/caddy/pki/authorities/local/root.crt)"
set_kv "$HEF" NH_HEALTH_BACKUP_DIR /var/backups/nhill/archives
set_kv "$HEF" NH_HEALTH_CERT_MIN_DAYS "$([[ $TLS == internal ]] && echo 0 || echo 14)"
ALF=/etc/nhill/alert.env
[[ -f $ALF ]] || cat >"$ALF" <<'EOF'
# Command run when a health check fails (the failed checks are in $NH_ALERT_TEXT).
# Empty: the failure is only written to the system journal (priority "err").
# Example: NH_ALERT_CMD="curl -fsS -m 10 -d \"$NH_ALERT_TEXT\" https://ntfy.sh/your-private-topic"
NH_ALERT_CMD=
EOF
chmod 600 "$ALF"
mkdir -p /etc/systemd/journald.conf.d
cat >/etc/systemd/journald.conf.d/50-nhill.conf <<'EOF'
# Nhill: journal kept 90 days at most (it contains IP addresses), 1 GB at most.
[Journal]
Storage=persistent
SystemMaxUse=1G
MaxRetentionSec=90day
EOF
systemctl restart systemd-journald
systemctl daemon-reload
systemctl enable --now nhill-healthcheck.timer >/dev/null
if /usr/sbin/nhill-healthcheck; then ok "health check passed (every 5 min: nhill-healthcheck.timer)"
else warn "health check reports a problem (details above): fix it before opening the service"; fi

# ----------------------------------------------------------------- 9. first administrator
say "9/9 First administrator of Nhill Console"
secret_line="$(grep -c '^NH_SECRET_KEY=.\{64\}$' "$ENVF" || true)"
[[ "$secret_line" == 1 ]] || die "NH_SECRET_KEY missing in $ENVF"
if out="$(sudo -u nhill /usr/bin/nhill-server --config "$ENVF" admin-bootstrap "$ADMIN_EMAIL" "$ADMIN_NAME" 2>&1)"; then
  echo "$out"
else
  echo "$out" >&2
  warn "no new token: an administrator is already active (invite others from Nhill Console)"
fi

cat <<EOF

Nhill Server $pkg_ver is installed for https://$API_DOMAIN.

To do now (not automated):
  1. Hostinger panel → VPS → Security → Firewall: accept ${SSH_PORT}/tcp, 80/tcp, 443/tcp, 443/udp only.
  2. Copy NH_SECRET_KEY from $ENVF into your password safe, apart from the backups
     (without it, establishment codes and administrator second factors are lost):
       sudo grep '^NH_SECRET_KEY=' $ENVF
  3. Keep the age PRIVATE key (backups) offline; test a restore from another machine:
       deploy/vps/nhill-restore-test.sh BACKUP.dump.age age-identity.txt
  4. Nhill Console → server address https://$API_DOMAIN → "First access" with the token above
     (30 minutes; a new token: sudo -u nhill nhill-server --config $ENVF admin-bootstrap EMAIL NAME).
  5. E-mails: configure the SMTP relay in $ENVF (NH_EMAIL_TRANSPORT=smtp ...), then
     systemctl restart nhill-server.
Updates: sudo nhill-update NEW.deb --sha256sums SHA256SUMS   ·   back: sudo nhill-rollback
EOF
