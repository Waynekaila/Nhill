# Validation report — Nhill 3.0.0

*Français : [../fr/rapport-validation.md](../fr/rapport-validation.md)*

## Version 3.0.0 — plans, documents, data, deployment

| | |
|---|---|
| Date | 29 October 2026 |
| Version | 3.0.0 (`project(nhill VERSION 3.0.0)`), branch `release/3.0.0`, not published |
| Validated code commit | `18d7054` (later commits only change documentation) |

### Tests run (continuous integration of commit `18d7054`)

| Suite | Where | Result | Evidence |
|---|---|---|---|
| `common` | CI Ubuntu 24.04 | **22/22** | [run 40826508382](https://github.com/Waynekaila/Nhill/actions/runs/40826508382), `ci-results/ci` |
| `server` (plans and rights for every plan and every role, limits, computed access, documents, till, erasure of a business, retention, searches, forgotten password…) | CI Ubuntu 24.04 (PostgreSQL 16); workspace | **121 passed, 1 skipped** (`Samples.GenerateDocumentSamples`) out of 122 | same |
| `setup`, `client` | CI | **6/6**, **15/15** | same |
| `keychain` — real Secret Service | CI Ubuntu 24.04 | **3/3** (+ init/cleanup) | `ci-results/ci`, `logs/keychain-linux.txt` |
| `keychain` — real Windows Credential Manager | CI `windows-2022` | **3/3** | [run 40826508391](https://github.com/Waynekaila/Nhill/actions/runs/40826508391) |
| `keychain` — Android Keystore | Android 11 emulator (CI) | phases `write`, `read`, `logout`, `forget`: **OK** | [run 40826508406](https://github.com/Waynekaila/Nhill/actions/runs/40826508406), `ci-results/android-keychain` |
| Ubuntu 24.04 and Debian 13 `.deb` packages, `apt` installation then removal | CI (clean containers) | **passed** | [run 40826508393](https://github.com/Waynekaila/Nhill/actions/runs/40826508393) |
| VPS kit: `provision.sh` on a fresh Ubuntu 24.04 machine, then acceptance | CI | **12/12 checks** (local ports, firewall, secrets, TLS and HSTS through Caddy, monitoring, encrypted backup restored off the server, update, rollback, refusal of a newer database, first administrator) | `ci-results/vps-test` |
| Windows installers, silent installation | CI `windows-2022` | passed | `ci-results/windows` |
| APK: third-party notices present | CI | `assets/THIRD-PARTY-NOTICES.txt` present | `ci-results/android` |
| API and database references up to date | CI | passed | `ci-results/ci`, `ci-results/docs-website` |
| PDFs up to date, website, forms backend (SQLite, PostgreSQL) | CI | passed; 24 pages, 0 problems | [run 40826508436](https://github.com/Waynekaila/Nhill/actions/runs/40826508436) |
| QML check | workspace, CI | 100 files, 0 problems, 0 untranslated texts | `scripts/qa/qml-check.py` |
| `nhill.online` domain | CI | **expected failure**: Hostinger parking page, no MX, SPF or DMARC | `ci-results/domain-check` |

SHA-256 of the deliverables built by CI (`18d7054`):

```
fe68846cf61ff330f5ca8f332916d38685417bba272f4213c14f6d82cc19da9b  windows/nhill-business-3.0.0-setup.exe
9d5d55a841850ee9dc3a8d8911fd84398f0a01b10c472384e1dbb65b899be763  windows/nhill-pos-3.0.0-setup.exe
eb654f3bfa0ef7e39785f03c8bacc8f466e281556744686f8ca6d3da8aa498e6  windows/nhill-console-3.0.0-setup.exe
500d06e310fe7a68d5b29f5a2a5ddab5c6055b54274300eb10f671fde573c0e8  android/nhill-business-3.0.0-arm64-v8a.apk
456ca24392c57fd860b3096b05d564c0e828b1afad97df6c61be2bd2d542757f  linux/ubuntu-24.04/nhill-server_3.0.0-1_amd64.deb
f9cddb2f63854b65423757d521dd2eea97d4fc3a287df904b8ae23c9d4366c0f  linux/debian-13/nhill-server_3.0.0-1_amd64.deb
cb60e13fa18e09f1430f9d983bb86bdd6fee141c4fe5e48adeae67fa30d34c87  website/nhill-website-3.0.0.tar.gz
```

Full list: `release/V3.0.0/checksums/SHA256SUMS`.

### Defects found and fixed during the 3.0 validation

| Defect | Fix |
|---|---|
| Erasure of a business promised by the terms but impossible (documents and log immutable for the application) | migration 0014, controlled erasure function, console screen, tests |
| `%` and `_` interpreted in searches | migration 0015, `nh_like_contains` |
| VPS acceptance: inactive PostgreSQL umbrella unit, internal certificate at 0 days, restore test unreadable for `nobody`, backup checksum with absolute path | `pg_isready` check, adapted threshold, script copy, checksum on the file name |
| POS 1366×768 screenshots missing (Xvfb display not ready) | wait for the X socket before launching |
| Android Keystore test: "Can't find service: activity" right after boot | wait for the service and retry `am` commands |
| APK without QtKeychain's BSD notice (statically linked) | notices included and checked at build time |
| Sales file: bracket fields, fictitious tracker rows, outdated "8-week pilot" offer | declared merge fields, empty files, 30-day trial everywhere |

### Not tested

- Real deployment on the Hostinger VPS (no access); `nhill.online` website not published.
- Real Android phone, user Windows 10/11 machine, e-mails through a real SMTP relay.
- Upgrade of a 2.1 database holding real data to 0012–0015 (tests apply 0001–0015 to an empty database).
- Hiding of features in the interface for each plan (checked server-side by the `Plans.*` tests; in the applications by code reading and static check).

### Pending decisions and human actions

- R1 (cpp-httplib of the Ubuntu 24.04 package) and R2 (Qt 6.5.3 on Windows and Android): choice of the fix ([03-security-audit.md](03-security-audit.md) § 4).
- Website host (legal notice), sender address of automatic e-mails, mail DNS records (MX, SPF, DMARC).
- Signing certificates (Authenticode, Android release key), legal review (points V-01 to V-14), external audit.

---

## Version 2.1.0 — remembered session and forgotten password

| | |
|---|---|
| Date | 27 October 2026 |
| Version | 2.1.0 (`project(nhill VERSION 2.1.0)`), branch `release/2.1.0`, unreleased |
| Validated code commit | `f8524fdd048c7f8462165925aae30d9b4d4357f1` (later commits change only the documentation, the marketing website and the commercial documents) |

### Tests run

| Suite | Where | Result | Evidence |
|---|---|---|---|
| `common` | CI Ubuntu 24.04, workspace | **22/22** | [run 40541503872](https://github.com/Waynekaila/Nhill/actions/runs/40541503872) |
| `server` (including 16 forgotten-password tests: reset by the administrator, e-mailed code, SMTP delivery to a local fake relay, SMTP configuration) | CI Ubuntu 24.04 (PostgreSQL 16); workspace | **99 passed, 1 skipped** (`Samples.GenerateDocumentSamples`) out of 100 | same |
| `setup`, `client` | CI | **6/6**, **13/13** | same |
| `keychain` — real Secret Service (gnome-keyring, two processes) | CI Ubuntu 24.04 | **5/5** (write then read back in a second process, sign-out, nothing stored without "Remember me") | `ci-results/ci`, `logs/keychain-linux.txt` |
| `keychain` — real Windows Credential Manager, `NHILL_KEYCHAIN_REQUIRED=1` | CI `windows-2022` | **5/5** | [run 40541504066](https://github.com/Waynekaila/Nhill/actions/runs/40541504066), `keychain-test-log.txt` |
| QtKeychain linked into each Windows application | CI `windows-2022` | `CredReadW`, `CredWriteW`, `CredDeleteW` imported from ADVAPI32; no QtKeychain DLL | same |
| Silent install and uninstall (Nhill POS, Nhill Business) | CI `windows-2022` | passed; installed executable identical to the checked one (SHA-256) | same |
| `keychain` — real Android Keystore | Android 11 x86_64 emulator (CI) | phases `write`, `read` (new launch), `logout`, `forget`: **OK** | [run 40541503866](https://github.com/Waynekaila/Nhill/actions/runs/40541503866), `ci-results/android-keychain` |
| Nhill Business APK | CI | built; QtKeychain Keystore code present in the library | same |
| Ubuntu 24.04 and Debian 13 `.deb` packages, clean install with `apt` then removal | CI (`ubuntu:24.04`, `debian:trixie` containers) | **passed** | [run 40541504028](https://github.com/Waynekaila/Nhill/actions/runs/40541504028), `ci-results/deb-*`, `ci-results/install-*` |
| QML static check | workspace | 98 files, 0 problems, 0 untranslated text | `scripts/qa/qml-check.py` |
| Marketing website | workspace | 24 pages, 0 problems; contrast 0 failures (16 pages × 2 widths) | `check_site.py`, accessibility probes |
| Financial model | workspace | documents up to date | `subscription_model.py --check` |

Digests of the Windows installers built by the CI (`f8524fd`):

```
e7b12542145330b7f9f5afc1131bd1ba6ed66660773969462552a88a37108b28  nhill-business-2.1.0-setup.exe
fd7048f0b4d6ec1031019b21242ea21b16cd6c3a8bded5a53e8a01b3ff2837b0  nhill-console-2.1.0-setup.exe
64e0493dc921090209b6a459c021da9c76f11ec4835159711a442d5c843278d3  nhill-pos-2.1.0-setup.exe
```

### Defects found and fixed during the 2.1 validation

| Defect | Fix |
|---|---|
| `windeployqt` treated `qt6keychain.dll` as a Qt module and looked for it in Qt's folder | QtKeychain linked statically on Windows; imports checked |
| Windows test program without a console (`qt_add_executable` defaults to the GUI subsystem): results lost | `WIN32_EXECUTABLE FALSE`; QtTest log written to a file |
| Imported targets (OpenGL, DBus, Qt6Keychain) missing in `tests/keychain` | same `find_package` calls as `client/` |
| `QSKIP` called from a helper; "sign-out" phase without a stored session | macro in the test function; phase skipped unless storage is required |
| Android emulator: AVD not found | explicit AVD folder |

### Not tested

- Real Android phone (arm64) and a user's Windows 10/11 computer (only the emulator and the CI's Windows Server 2022).
- E-mail delivery through a real SMTP provider (local test relay only).
- Upgrading a 2.0 database holding real data to migration 0011 (tests apply 0001–0011 to an empty database); rollback to 2.0.
- 2.0 client against the 2.1 server; complete on-screen flows (build and static check only for the new dialogs).

### Known limits of 2.1

- Windows Credential Manager keeps the session after uninstalling as long as the user has not signed out.
- The MinGW cross-build does not include QtKeychain: the session is never remembered.
- QtKeychain licence notice installed with the Windows applications, not yet shown in the APK.
- `log` transport: no code is delivered; `NH_EMAIL_TRANSPORT=smtp` is needed in production.
- Resetting a console account's password: out of scope (existing creators' procedure).

# Validation 2.0.0

| | |
|---|---|
| Date | 27 October 2026 |
| Version | 2.0.0 (`project(nhill VERSION 2.0.0)`) |
| Repository | `Waynekaila/Nhill` (GitHub, private), branches `main` and `release/2.0.0` |
| Deliverables commit | `1c0f30c2fd33d6ac37b852f1f68adb9b154b2e97` |
| Principle | this report only mentions what was executed; everything else is under "Not tested" (§ 5) |

## 1. Where checks took place

| Environment | Role | Limits |
|---|---|---|
| Session workspace (Linux, without Qt or libpqxx development packages) | code changes, standalone `common` tests, static QML check, website, form backend (SQLite), local PostgreSQL 16 schema, documentation, PDFs | the server and Qt applications cannot be built there |
| GitHub Actions (`ubuntu-24.04`, `ubuntu:24.04` and `debian:trixie` containers, `windows-2022`) | full build, tests, packages, test installation | CI virtual machines, not a real user computer |

Each job's results are published in `ci-results/*` branches of the repository
(`RUN-INFO.txt`: workflow, status, commit, run link).

## 2. Tests executed and results

| Suite | Where | Result | Evidence |
|---|---|---|---|
| `common` (amounts, taxes, codes, passwords, 128-bit integer) | CI Ubuntu 24.04 and Debian 13; workspace (standalone tool) | **22/22** passed; also 22/22 in `NHILL_PORTABLE_INT128` mode (Windows path) in the workspace | [run 40464011312](https://github.com/Waynekaila/Nhill/actions/runs/40464011312), `Testing/Temporary/LastTest.log` |
| `server` (API + throwaway PostgreSQL) | CI Ubuntu 24.04 (PostgreSQL 16), Debian 13 (PostgreSQL 17) | **83 passed, 1 skipped** (`Samples.GenerateDocumentSamples`, sample generation off by default) out of 84 | same |
| `setup` (setup mode) | CI | **6/6** | same |
| `client` (Qt Test) | CI | **13/13** | same |
| Static QML check | workspace + CI | 95 files, 0 problems, 0 untranslated texts | `scripts/qa/qml-check.py` |
| Website backend (SQLite) | workspace + CI | **13/13** | `website/backend/tests` |
| Website backend (PostgreSQL 16, real roles) | CI | **passed** (13 tests + read refused for the application role) | [run 40461610891](https://github.com/Waynekaila/Nhill/actions/runs/40461610891) |
| Website pages | workspace | 24 pages, 0 problems (links, `alt`, titles, FR/EN pairs, secrets) | `website/tools/check_site.py` |
| KPI workbook | workspace | 91 formulas, 0 errors; values checked with test data | LibreOffice recalculation |
| Website server (smoke) | workspace | `/api/health` 200, form submission 201 | session log |

## 3. Deliverables built

| Deliverable | Build | Checks | SHA-256 |
|---|---|---|---|
| `linux/ubuntu-24.04/nhill-business_2.0.0-1_amd64.deb` | CI, `ubuntu:24.04` container | lintian 0 errors; `apt` install in a clean container, headless start, removal | `d46cadeb0701b6ff823ca65e8533116c6b1a372cd6a8bbbe078536cfa06d1c89` |
| `linux/ubuntu-24.04/nhill-console_2.0.0-1_amd64.deb` | same | same | `f26aa9f414343b8a80156b44f9891cce3e58614a060783caa4f9ccb438103e63` |
| `linux/ubuntu-24.04/nhill-pos_2.0.0-1_amd64.deb` | same | same | `c67f88b6af4c99bcf1ba3a8a3e6921e21c17e8ced11e41f0d6dbe2d2e1c34d3d` |
| `linux/ubuntu-24.04/nhill-server_2.0.0-1_amd64.deb` | same | lintian 0 errors, 7 warnings; installed in the clean container | `997c29c8c44cfe444ddc02bb21f2e2a0bcb6224339e76ab0222ecd2f8cba77ae` |
| `linux/debian-13/nhill-business_2.0.0-1_amd64.deb` | CI, `debian:trixie` container (Debian's Qt 6.8) | install, start, removal in a clean container | `63ce71aad7a5b67208384aaf1e740d4915561d978659183732e4ab949fe1d1d4` |
| `linux/debian-13/nhill-console_2.0.0-1_amd64.deb` | same | same | `579f363770d21e3c5660e920551d8f3864e03b084e3aea5f2ce8433a37f2bf87` |
| `linux/debian-13/nhill-pos_2.0.0-1_amd64.deb` | same | same | `ce804cd61b037b515a50d83ff1f513ca204798dea876b9ff58fc35d2a69f55b9` |
| `linux/debian-13/nhill-server_2.0.0-1_amd64.deb` | same | installed | `dc05552506a6b7cfcfe787d6f7abc9ffd8fb078e0efa4500b58939f154cc13d8` |
| `windows/nhill-business-2.0.0-setup.exe` | CI `windows-2022`, MSVC 2022, Qt 6.5.3, NSIS | PE format checked | `5a224a1b21f439cf6a63aba074830de8a3df56d6845e1ac80604ada7aeac2a1b` |
| `windows/nhill-console-2.0.0-setup.exe` | same | PE format checked | `e1f1c768b593a4097598e22e1c1a0e523b82c09de9c30b536638318b6b7e81b9` |
| `windows/nhill-pos-2.0.0-setup.exe` | same | PE format; **silent install and uninstall succeeded** on the runner | `b95c1dd8fae02f48157606637b1b185486abf8d04e9a5153bb4039027c5cdd07` |
| `android/nhill-business-2.0.0-arm64-v8a.apk` | CI, Qt 6.5.3 for Android, NDK r25c, platform 34 | content (manifest, arm64-v8a libraries), **debug** signature verified by `apksigner` | `70c9664ad39baeadb61885d8d2ead5a6de19bb33d13d856f50d475e75f2fdf01` |
| Website (sources + archive) | workspace | checks of § 2 | `release/V2.0.0/checksums/SHA256SUMS` |
| Documentation, legal and marketing PDFs | workspace (pandoc + Chromium) | `scripts/build-docs-pdf.sh --check` | `docs/pdf/manifest.json` |

## 4. Issues found and fixed during validation

| Issue | Fix |
|---|---|
| Windows build impossible: `__int128` unknown to MSVC in money computations | portable 128-bit integer (`common/include/nhill/int128.hpp`), compared with `__int128` on 20,000 operations; `ExchangeRate::scaled` as `int64_t` |
| `.deb` packages: build folder inside the source refused; logs counted in the source digest | build outside the source; `build/`, `dist/`, `release/` excluded from the digest |
| Debian 13: SVG plugin in a separate package, blocking package lookup | `qt6-svg-plugins` installed and declared, tolerant lookup |
| Windows: `Get-FileHash` unavailable in PowerShell 5.1 launched from pwsh; garbled accents | digests through .NET; script saved as UTF-8 with BOM |
| Android: CMake install rule invalid for the target; runner's platform 36 unreadable by Gradle 7 | rule limited to desktop; platforms > 34 removed in CI |
| Console: no password check before sending at first access | check with the shared rule (8 characters) |

## 5. Not tested (to be done by people, on real hardware)

| Item | Why | Action |
|---|---|---|
| Windows installers on a user's Windows 10/11 computer (SmartScreen, antivirus, 150 % HiDPI, receipt printing) | CI runner only | manual acceptance (`docs/en/installation-guide.md` § 3.2) |
| Nhill Business and Nhill Console installed through their Windows installers | only Nhill POS is installed in CI | manual acceptance |
| APK on Android 7 and 14 phones/tablets | no device | manual acceptance; then release signing |
| Remembered sessions on Windows and Android | QtKeychain not included in the CI Windows/Android builds: sessions are not remembered there | add QtKeychain (vcpkg / Android build) then test |
| Production deployment with public TLS, production Supabase, backups, restore | no production infrastructure in the session | `docs/en/installation-guide.md` § 4 |
| Real payments (Stripe, PayPal) | adapters tested against simulated responses only | test payment with a provider account |
| Nhill POS 2.0 screenshots | the automatic tour now reaches the choice of the demo till (created by `seed-demo.py`) but does not open the session: 2.0 sales screens are not captured automatically | adapt the tour (`PosMain.qml`) or capture by hand; Business 2.0 screenshots in `docs/assets/v2/` |
| Accessibility with a screen reader (NVDA, Orca, TalkBack) | not available | manual test |
| Legal | no validation by a lawyer | `legal/en/00-legal-research.md` § 6 |
| External audit, penetration test | not ordered | before a wide opening |

## 6. Known limitations of the release

- No forgotten-password reset (no e-mail sending).
- cpp-httplib 0.14 (dynamically linked to the distribution package): CVE-2025-46728 and
  CVE-2025-53629; mitigated by Caddy; upgrade to ≥ 0.23.0 recommended.
- Activity log: some detail values are shown raw (e.g. "Montant : 95000.00",
  "type : invoice") — seen in the 2.0 screenshots.
- Nhill is not certified for the DGI standardised invoice.
- The 8-character requirement is below the NIST recommendation (15) for a password without
  a second factor: accepted risk, to be recorded (`docs/en/security.md` § 3).

## 7. Human actions required before commercial launch

1. Create the operating company; complete every `[…]` field (`docs/en/legal-parameters.md`).
2. Have the 19 legal templates validated by a lawyer; check the competent data-protection
   authority (ARPTC / ARPTIC); formalities and transfers.
3. Acquire a Windows code-signing certificate and create the Android release key
   (`ANDROID_KEYSTORE_*` secrets); sign, then republish the digests.
4. Manual acceptance on real Windows and Linux computers and Android devices.
5. Choose production hosting; deploy server, Caddy, backups; test a restore.
6. Set prices after the pilot programme; enter them in Nhill Console.
7. Commission an external penetration test before a wide roll-out.
8. Check the availability of the "Nhill" trademark and register it.
