# Deploying Nhill Server on a Hostinger VPS (France)

*Français : [../fr/deploiement-vps-hostinger.md](../fr/deploiement-vps-hostinger.md)*

This guide installs Nhill Server and its PostgreSQL database on **a Hostinger VPS located
in France**, running Ubuntu 24.04, then covers operations: backups, restore, monitoring,
updates and rollback. Commands run on the VPS unless stated otherwise.

> **Verification status (28 October 2026).** The full procedure (`deploy/vps/provision.sh`
> then `deploy/vps/tests/acceptance.sh`) is run by the CI job `vps-test` on a fresh Ubuntu
> 24.04 machine, with a certificate from Caddy's local authority. It has **not** yet been
> run on the production Hostinger VPS, nor with a real Let's Encrypt certificate: run it on
> a staging VPS first.

## 1. Architecture

```
Applications (Business, POS, Console)
        │ HTTPS 443 (TLS 1.2+, Let's Encrypt certificate renewed by Caddy)
        ▼
Hostinger firewall (before the VPS)  →  ufw (on the VPS)
        ▼
Caddy :80/:443 ──► nhill-server 127.0.0.1:8080 ──► PostgreSQL 16 127.0.0.1:5432
                                                    │
        daily encrypted backup (age) ◄──────────────┘
        /var/backups/nhill/archives  ──►  off-site copy (outside Hostinger)
```

| Choice | Reason |
|---|---|
| One VPS, local database | modest initial volume; the database is never exposed to the network (listens on 127.0.0.1 and ::1 only) |
| Caddy in front of the API | automatic TLS (ACME), security headers, request size limit; the API listens locally only |
| Separate PostgreSQL roles | `nhill_owner` (migrations), `nhill_app` (API, cannot bypass row-level security), `nhill_backup` (read-only) |
| Backups encrypted with `age` | only the **public** key is on the VPS: stealing the VPS or the backups does not reveal the data |
| Database copy before every update | rollback in one command, without the backup private key |
| Data in France (European Union) | location chosen when the VPS is created; it cannot change afterwards |

**Hosting provider.** For a customer established in the DRC, Hostinger's contracting entity
is Hostinger International Ltd, 61 Lordou Vironos str., 6023 Larnaca, Cyprus (Hostinger's
terms of service, § 2, and their list of countries). Hostinger offers a data processing
agreement (DPA) listing its sub-processors; accepting and keeping it belongs to the
compliance file (record of processing, legal template 18).

## 2. Ports, flows and firewall

| Port | Listener | Open to the Internet | Role |
|---|---|---|---|
| 22/tcp (or your SSH port) | sshd | yes, preferably restricted to your addresses | administration (SSH keys) |
| 80/tcp | Caddy | yes | Let's Encrypt ACME challenge, redirect to HTTPS |
| 443/tcp, 443/udp | Caddy | yes | API over HTTPS (HTTP/2, HTTP/3) |
| 8080/tcp | nhill-server | **no** (127.0.0.1) | API behind Caddy |
| 5432/tcp | PostgreSQL | **no** (127.0.0.1, ::1) | database |

Two complementary firewalls:

1. **Hostinger managed firewall** (VPS panel → *Security* → *Firewall*). It filters before
   traffic reaches the VPS; an empty rule group **blocks everything**. Create a group with
   "accept" rules for 22/tcp (source: your addresses if possible), 80/tcp, 443/tcp and
   443/udp, then activate it. Changes apply to new connections within two minutes.
2. **ufw on the VPS**, set by `provision.sh`: all incoming traffic refused except SSH (rate
   limited), 80/tcp and 443/tcp+udp. `fail2ban` bans an address for one hour after 5 SSH
   failures within 10 minutes.

## 3. Before you start

1. **Create the VPS** at Hostinger: location **France**, **Ubuntu 24.04** plain OS (no
   panel), 2 vCPU and 4 GB RAM recommended (2 GB minimum), an SSH key added at creation.
2. **DNS**: an `A` record (and `AAAA` if the VPS has IPv6) for the API name, for example
   `api.<your-domain>`, pointing to the VPS. The certificate is issued only once that name
   resolves to the VPS.
3. **Backup key, on your computer** (never on the VPS): `age-keygen -o nhill-backup-key.txt`;
   note the public key it prints (`age1…`); keep the file in your password safe and an
   offline copy. Without it, no backup can be read.
4. **Package**: `nhill-server_<version>-1_amd64.deb` and its `SHA256SUMS`
   (`dist/linux/ubuntu-24.04/` or the release), plus the repository's `deploy/vps/` folder
   (also installed by the package in `/usr/share/nhill/vps/`).
5. **Off-site copy** (recommended): storage outside Hostinger, in the European Union, and a
   copy command (for example `rclone` configured for the `nhill-backup` user).

## 4. Installation

```bash
scp -r nhill-server_3.0.0-1_amd64.deb SHA256SUMS deploy/vps root@<vps-address>:/root/
ssh root@<vps-address>
cd /root && sudo ./vps/provision.sh \
  --deb nhill-server_3.0.0-1_amd64.deb --sha256sums SHA256SUMS \
  --api-domain api.<your-domain> --acme-email <address-for-certificate-notices> \
  --admin-email <your-address> --admin-name "<Your name>" \
  --backup-recipient age1… \
  --support-email support@nhill.online \
  --offsite-cmd 'rclone copy {} remote:nhill-backups/' \
  --harden-ssh
```

The script stops at the first error; re-run, it keeps what is installed. It:

1. checks the system (Ubuntu 24.04, amd64, memory, disk) and **the package SHA-256**;
2. installs from the Ubuntu archive: PostgreSQL 16, Caddy, age, ufw, fail2ban and automatic
   security updates (no automatic reboot);
3. configures ufw and fail2ban; with `--harden-ssh`, accepts SSH keys only (ignored while no
   key is installed, so that you are not locked out);
4. restricts PostgreSQL to the loopback and creates the `nhill` database;
5. installs the package, then the database with `nhill-server setup` through a temporary
   superuser **deleted immediately** (its password only goes through standard input);
   checks the isolation (10 checks);
6. sets up the encrypted backups and takes a first one;
7. writes `/etc/caddy/Caddyfile` for `api.<your-domain>` (Let's Encrypt);
8. installs the health checks, log retention and the `nhill-update`, `nhill-rollback` tools;
9. prints the **one-time token** of the first Nhill Console administrator (30 minutes).

Then:

- **Back up `NH_SECRET_KEY`** (`sudo grep '^NH_SECRET_KEY=' /etc/nhill/server.env`) in your
  password safe, apart from the backups: without it, establishment codes and administrators'
  second factors can no longer be read.
- **Nhill Console**: address `https://api.<your-domain>`, "First access", token, password
  (14 characters minimum) and TOTP. Token expired or lost, while nobody has completed the
  first access: `sudo -u nhill nhill-server --config /etc/nhill/server.env admin-bootstrap <same-address> "<Name>"`.
- **E-mails**: set the SMTP relay (`NH_EMAIL_TRANSPORT=smtp`, `NH_SMTP_*`,
  [configuration guide](configuration-guide.md)) with a sender address of the domain
  authorised by SPF and DKIM, then `sudo systemctl restart nhill-server`. While
  `NH_EMAIL_TRANSPORT=log`, no e-mail (recovery codes, reminders) is sent.

## 5. Secrets

| Secret | Where | Permissions | Backup |
|---|---|---|---|
| `NH_SECRET_KEY` (master key) | `/etc/nhill/server.env` | 600, `nhill` | password safe, kept apart |
| `nhill_owner`, `nhill_app` passwords | `/etc/nhill/server.env` | 600, `nhill` | not needed (regenerated by `nhill-server setup`) |
| `nhill_backup` password | `/var/backups/nhill/.pgpass` | 600, `nhill-backup` | not needed |
| age **public** key | `/etc/nhill/backup.env` | 600, `nhill-backup` | — |
| age **private** key | **outside the VPS** | — | safe + offline copy |
| SMTP password | `/etc/nhill/smtp-password` (`NH_SMTP_PASSWORD_FILE`) | 600, `nhill` | safe |
| Payment provider secrets | database, encrypted with `NH_SECRET_KEY` | — | backups |

No secret is in the repository, the package or the logs.

## 6. Certificates

Caddy obtains the certificate of `api.<your-domain>` from Let's Encrypt (HTTP-01 challenge on
port 80) and renews it by itself, about 30 days before expiry. The health check alerts when
fewer than 14 days remain. On failure: `journalctl -u caddy`, DNS resolution of the name,
ports 80 and 443 open in **both** firewalls.

## 7. Backups

| Level | Frequency | Retention | Protects against |
|---|---|---|---|
| Encrypted Nhill backup (`nhill-backup.timer`) | daily around 02:30 UTC (+0 to 15 min) | 14 days on the VPS | mistakes, deletion, database corruption |
| Off-site copy (`NH_BACKUP_OFFSITE_CMD`) | every backup | per your storage (30 days or more advised) | loss of the VPS or of the Hostinger account |
| Hostinger backups of the whole VPS | weekly by default; daily as a paid option; 2 weekly and 2 daily kept | — | whole-system recovery; **a restore overwrites the whole VPS** |
| Hostinger snapshot | on demand (before an update) | one, deleted after 1 day | whole-system rollback right after an operation |
| Database copy before update (`nhill_pre_…`) | every `nhill-update` | last 2 | rollback of an update |

Hostinger facts from its support article "How to back up or restore a VPS", read on
28 October 2026. Hostinger backups **complement** the Nhill backups and do not replace
them: they are neither encrypted by you nor kept outside Hostinger.

Check: `systemctl list-timers nhill-backup.timer`, `ls -lh /var/backups/nhill/archives`,
`journalctl -u nhill-backup`.

## 8. Restore

**Monthly test, outside the VPS** (administrator's computer or staging VPS, with the private key):

```bash
scp root@<vps>:/var/backups/nhill/archives/nhill-<date>.dump.age* .
deploy/vps/nhill-restore-test.sh nhill-<date>.dump.age nhill-backup-key.txt
```

The script starts a throw-away PostgreSQL (local socket, nothing on the network), restores,
counts migrations, establishments, users, documents and audit entries, checks forced
row-level security and the isolation of the application role, then deletes everything.

**Real restore on the VPS** (after data loss):

1. Copy the backup (with its `.sha256`) and, **temporarily**, the private key into
   `/var/lib/postgresql/restore/` (owner `postgres`, mode 700).
2. `cd /var/lib/postgresql/restore && sudo -u postgres env PGHOST=/var/run/postgresql nhill-restore.sh nhill-<date>.dump.age key.txt nhill_restored`
   (restores into a **new** database, then shows migrations, establishments, row-level security).
3. `sudo systemctl stop nhill-server`, then rename:
   `ALTER DATABASE nhill RENAME TO nhill_before_restore; ALTER DATABASE nhill_restored RENAME TO nhill;`
   and give the database privileges again: `GRANT CONNECT, CREATE ON DATABASE nhill TO nhill_owner; GRANT CONNECT ON DATABASE nhill TO nhill_app, nhill_backup;`
4. `sudo systemctl start nhill-server`, check `nhill-healthcheck`, then **delete the private
   key from the VPS** (`sudo shred -u /var/lib/postgresql/restore/key.txt`).

Data entered between the backup and the incident is lost: at most one day with the
default frequency.

## 9. Monitoring and logs

`nhill-healthcheck.timer` checks every 5 minutes: services `nhill-server`, `postgresql`,
`caddy`; `/health` locally and over HTTPS through Caddy; certificate expiry; ports 5432 and
8080 on loopback only; ufw active; disk (< 85 %); last backup younger than 26 hours.
Result: `/var/lib/nhill/health.json`. A failure starts `nhill-alert`: a message in the system
journal (priority `err`) and, when `NH_ALERT_CMD` is set in `/etc/nhill/alert.env`, your
alert command (e-mail, chat, notification).

A VPS cannot report its own total outage: add an **external probe** on
`https://api.<your-domain>/health` (uptime monitoring service of your choice).

| Log | Where | Retention |
|---|---|---|
| API (JSON per line) | `journalctl -u nhill-server` | 90 days, 1 GB at most (journald) |
| HTTPS access (IP addresses) | `/var/log/caddy/nhill-access.log` | 10 × 50 MB, 90 days at most |
| PostgreSQL | `/var/log/postgresql/` | Ubuntu rotation (logrotate) |
| Backups, health | `journalctl -u nhill-backup -u nhill-healthcheck` | 90 days |
| Application errors | Nhill Console → Technical errors | database |

## 10. Updates and rollback

```bash
sudo nhill-update nhill-server_3.0.1-1_amd64.deb --sha256sums SHA256SUMS
```

1. checks the package (name, checksum, newer version);
2. takes an encrypted backup (and its off-site copy);
3. stops the API and copies the database (`nhill_pre_<version>_<date>`);
4. installs the package; at start, the server applies the new migrations;
5. waits for `/health`; on failure, **automatic rollback**.

Downtime: from step 3 to the end of step 5, usually under a minute for a small database.
Before an important update, also take a **Hostinger snapshot**.

Manual rollback (for example after a defect found later): `sudo nhill-rollback` reinstalls
the previous package and the database copy; the database used since the update is kept as
`nhill_failed_<date>` (entries made since then are there).

**Migrations**: versioned (`db/migrations/NNNN_*.sql`), applied in order, one transaction
each, under a lock; a migration changed after being applied is refused (checksum). They only
go forward: an older server **refuses to start** on a database migrated by a newer version
(message in `journalctl -u nhill-server`); going back therefore uses the database copy, never
a "down" migration.

System updates: security fixes are automatic; schedule a reboot when
`/var/run/reboot-required` exists (`sudo reboot`, the API restarts by itself).

## 11. Reinstalling on a new VPS (recovery plan)

1. Create a VPS (France, Ubuntu 24.04), DNS, Hostinger firewall: §§ 2-3.
2. Run `provision.sh` (§ 4) with the same domain.
3. Put the original `NH_SECRET_KEY` back in `/etc/nhill/server.env` (safe), then restore the
   latest off-site backup (§ 8, real restore).
4. `sudo systemctl restart nhill-server`, `nhill-healthcheck`, test sign-in from Nhill
   Console and Nhill Business.

## 12. Personal data and compliance

- The data of the businesses and of their customers is hosted in France, in the European
  Union; Hostinger's contract and DPA govern this processor.
- Businesses located outside the European Union (for example in the DRC) transfer their data
  to France: the obligations of their country (declaration, transfer authorisation) are
  listed in `docs/en/legal-parameters.md` and `legal/en/00-legal-research.md`. No compliance
  is claimed without review by a lawyer.
- Limited log retention (90 days); encrypted backups; SSH key access to the VPS.

## 13. Left to the operator

| Action | Where |
|---|---|
| Accept Hostinger's DPA and file it | Hostinger account |
| Configure the Hostinger firewall | VPS panel |
| Choose and configure the off-site copy | `--offsite-cmd`, `rclone` |
| Configure the SMTP relay and SPF/DKIM/DMARC of the sender domain | `server.env`, DNS zone |
| Add an external uptime probe | service of your choice |
| Set `NH_ALERT_CMD` | `/etc/nhill/alert.env` |
| Run the first restore test outside the VPS, then monthly | § 8 |
