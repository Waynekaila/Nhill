# Nhill 2.1 — Installation guide

*Français : [../fr/guide-installation.md](../fr/guide-installation.md)*

This guide covers installing the **applications** (Linux, Windows, Android), the
**server** (Linux) and upgrading from 2.0 or 0.1.0. Building the packages:
[build-guide.md](build-guide.md). Settings: [configuration-guide.md](configuration-guide.md).
Marketing website: [`website/README.en.md`](../../website/README.en.md).

> **State of the deliverables.** Every file shipped in `release/V<version>/` was
> actually built; a missing platform is listed in `release/V<version>/checksums/MISSING.txt`.
> The validation report ([validation-report.md](validation-report.md)) says what was
> tested and on which machine. The Windows installers and the APK are **not signed** with
> a publisher certificate until the creators acquire one.

## 1. Verify a downloaded file

Before installing, compare the file's SHA-256 digest with the one in
`checksums/SHA256SUMS` (published separately from the file, for example on the website):

```bash
sha256sum -c SHA256SUMS --ignore-missing          # Linux
```
```powershell
Get-FileHash .\nhill-business-2.1.0-setup.exe -Algorithm SHA256   # Windows: compare by eye
```

If the digest differs, do not install the file.

## 2. Minimum configuration

| Component | System | Memory | Notes |
|---|---|---|---|
| Nhill Business, POS, Console (desktop) | Windows 10/11 64-bit; Debian 13 or Ubuntu 24.04 (amd64) | to be measured on the target computer | 1280×720 screen or larger recommended; POS works from 860×560 |
| Nhill Business (Android) | Android 7.0 (API 24) or later, arm64 processor | — | Internet connection required |
| Server | Debian 13 or Ubuntu 24.04, amd64 | starting point: 2 vCPU, 4 GB, 40 GB SSD | PostgreSQL 15+ (Supabase or self-hosted), synchronised clock (NTP) |

The client computers' memory needs have not been measured: test on the hardware actually
used (see the validation report).

## 3. Applications

### 3.1 Linux (Debian 13, Ubuntu 24.04)

Packages are built **for one specific distribution**: take those from your system's
folder (`linux/debian-13/` or `linux/ubuntu-24.04/`).

```bash
sudo apt install ./nhill-business_2.1.0-1_amd64.deb ./nhill-pos_2.1.0-1_amd64.deb
sudo apt install ./nhill-console_2.1.0-1_amd64.deb     # for the creators of Nhill only
```

`apt` installs the required libraries from the official repositories. The applications
appear in the menu (Office). Remembered sessions need a Secret Service keychain (GNOME
Keyring or KWallet), present on usual desktops.

Removal: `sudo apt remove nhill-business nhill-pos nhill-console`.

### 3.2 Windows 10/11

1. Check the digest (§ 1), then run `nhill-business-2.1.0-setup.exe` (or `nhill-pos-…`,
   `nhill-console-…`). Installation asks for administrator rights (it installs into
   `Program Files`).
2. As long as the installer is unsigned, Windows SmartScreen may show "Windows protected
   your PC". Bypass this warning **for this file only** and **only if the digest
   matches**: "More info" → "Run anyway". Never turn off SmartScreen or the antivirus.
3. Start-menu shortcuts are created; remove through Settings → Apps.

Silent installation (fleet of computers): `nhill-pos-2.1.0-setup.exe /S`.

### 3.3 Android

1. Check the digest of `nhill-business-2.1.0-arm64-v8a.apk`.
2. Open the file on the phone; Android asks you to allow **this app** (file manager or
   browser) to install apps. Allow it for this installation, then withdraw the permission
   (Settings → Apps → Special access → Install unknown apps).
3. An APK signed with the **debug** key (built without a release key) can be installed for
   trials but must not be distributed to customers; it cannot be upgraded by an APK signed
   with the release key (uninstall first).

Nhill POS and Nhill Console are not offered on Android (desktop applications).

### 3.4 First start

The server address is compiled into the application (`NHILL_SERVER_URL`). To try another
server, set the `NHILL_SERVER` environment variable (desktop only). Then: user guide,
Part A § 1.

## 4. Server (production)

Target: a Linux server for the API, a PostgreSQL 15+ database (**Supabase** or
self-hosted), a domain name and a TLS proxy (Caddy). The full procedure has not yet been
run on a public production server with TLS: run it on a staging server first.

### 4.1 Prerequisites

- Synchronised clock (NTP) — essential for TOTP and webhooks.
- Firewall: only 22 (administration, restricted) and 80/443 (Caddy) open.
- DNS: `api.<your-domain>` → server address.

### 4.2 Package

```bash
sudo apt install ./nhill-server_2.1.0-1_amd64.deb
curl -s http://127.0.0.1:8080/health     # {"database":false,"status":"setup"}
```

The package creates the system users `nhill` and `nhill-backup`, the file
`/etc/nhill/server.env` (mode 600) with an `NH_SECRET_KEY` **generated on the server**
and no database, then starts the service in **setup mode**. Copy `NH_SECRET_KEY` to a
vault immediately (without it, establishment codes and the administrators' TOTP secrets
become unreadable).

### 4.3 Database

- **Supabase**: Connect → **Session pooler** (port 5432, user `postgres.<reference>`);
  not the *transaction pooler* (6543). No API key (`anon`, `service_role`) is needed or
  accepted; Nhill creates a `nhill` schema closed to Supabase's API roles.
- **Self-hosted PostgreSQL**: empty `nhill` database, an account able to create roles,
  `scram-sha-256` in `pg_hba.conf`, TLS if the database is remote.

### 4.4 Installation from Nhill Console

1. Setup token (60 minutes, single use):
   `journalctl -u nhill-server | grep -i -A2 "setup mode"`.
2. Nhill Console → **Install a new server** → address and token → database type, host,
   port, user, password → **Test the connection** → first administrator → **Initialise
   the database**.
3. The server creates `nhill_owner`, `nhill_app`, `nhill_backup` (random passwords),
   applies the migrations, checks isolation, writes `/etc/nhill/server.env` and restarts.
   The database administration password is not kept; the backup account's password is
   shown **only once**.

Check: `/health` → `{"database":true,"status":"ok"}`. Command line:
`NH_SETUP_DB_PASSWORD=… sudo -u nhill nhill-server --config /etc/nhill/server.env setup --host … --user … --admin-email … --admin-name "…"`.

### 4.5 TLS and settings

```bash
sudo apt install caddy
sudo cp /usr/share/nhill/examples/Caddyfile.example /etc/caddy/Caddyfile   # replace api.example.com
sudo systemctl reload caddy
curl -sI https://api.<domain>/health       # 200 + Strict-Transport-Security
```

In `/etc/nhill/server.env`: `NH_PUBLIC_BASE_URL=https://api.<domain>`,
`NH_TRUST_PROXY=1`; then `sudo systemctl restart nhill-server` and
`sudo -u nhill nhill-server --config /etc/nhill/server.env check`.

### 4.6 Backups, first access, plans

- Encrypted backups (age): `docs/sauvegarde-restauration.md` (French);
  `sudo systemctl enable --now nhill-backup.timer`, then **a restore test**.
- Nhill Console → **First access** (30-minute token, password of at least 8 characters,
  mandatory TOTP).
- **Plans and prices**: no price is preconfigured.
- **Mail** (2.1): SMTP relay with TLS (`NH_EMAIL_TRANSPORT=smtp`, `NH_SMTP_*`); without it,
  owners and administrators cannot recover a forgotten password.
- **Payment providers**: stay in test mode until a successful test payment with the
  provider's account.

## 5. Upgrading to 2.1.0

From 2.0.x:

1. **Back up** the database: `sudo systemctl start nhill-backup.service`.
2. Server: `sudo apt install ./nhill-server_2.1.0-1_amd64.deb`; at start-up the service
   applies migration **0011** (forgotten password: two columns in `users`, the
   `password_resets` table, three platform settings). Schema: 0001–0011.
3. Mail: set `NH_EMAIL_TRANSPORT=smtp` and the `NH_SMTP_*` variables
   ([configuration-guide.md](configuration-guide.md) § 1 and § 5), then
   `sudo -u nhill nhill-server --config /etc/nhill/server.env check` and try
   "Forgot your password?" with a test account.
4. Applications: install the new versions **before** using the reset by an administrator.
   A 2.0 client is still accepted by the 2.1 server (API `v1`), but it cannot show the
   required change of a temporary password: the member could not sign in from that device.
5. Remembered session: a user whose session was already kept by 2.0 (Linux) keeps it;
   elsewhere, "Remember me" takes effect at the next sign-in.

From 0.1.0: same steps; 2.0 added no migration (the 8-character rule applies to new
passwords, with no forced change for existing ones). Upgrade server and clients together
(0.1.x clients reject passwords shorter than 10 characters locally).

Rollback to 2.0: restoring the backup from step 1 is the safe method. Reinstalling only the
2.0 package leaves migration 0011 in place (the 2.0 server ignores the added columns and
table); this case has not been tested, and a temporary password already given would work
there as an ordinary password.

## 6. Troubleshooting

| Symptom | Action |
|---|---|
| "Server problem" in the application | `curl https://api.<domain>/health`; `journalctl -u nhill-server` |
| Session not remembered on Linux | install/unlock GNOME Keyring or KWallet |
| Session not remembered on Windows or Android | check that "Remember me" is ticked (Settings › My account › This device); Android: set a screen lock; a "secure storage unavailable" message appears if the system refuses to save |
| "Forgot your password?": no code received | server: `NH_EMAIL_TRANSPORT=smtp`? relay error in `journalctl -u nhill-server`; user: spam folder; an employee of a single establishment receives the instruction to contact their administrator, not a code |
| "Update required" | install the version requested by the server |
| Android: "App not installed" | an APK with another signature is present: uninstall it |
| Windows: application does not start | reinstall; check that the antivirus has not quarantined a file |
| Server refuses to start, `server.env` readable | `chmod 600 /etc/nhill/server.env` |
| Server: modified migration refused | never edit a published migration; restore the original |
