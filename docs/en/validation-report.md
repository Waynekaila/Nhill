# Validation report — Nhill 2.0.0

*Français : [../fr/rapport-validation.md](../fr/rapport-validation.md)*

| | |
|---|---|
| Date | 27 October 2026 |
| Version | 2.0.0 (`project(nhill VERSION 2.0.0)`) |
| Repository | `Waynekaila/Nhill` (GitHub, private), branches `main` and `release/2.0.0` |
| Deliverables commit | `1389b46a5b5f97adc5b7e15f7906a470d43f81b2` |
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
| `common` (amounts, taxes, codes, passwords, 128-bit integer) | CI Ubuntu 24.04 and Debian 13; workspace (standalone tool) | **22/22** passed; also 22/22 in `NHILL_PORTABLE_INT128` mode (Windows path) in the workspace | [run 40461610927](https://github.com/Waynekaila/Nhill/actions/runs/40461610927), `Testing/Temporary/LastTest.log` |
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
| `linux/ubuntu-24.04/nhill-business_2.0.0-1_amd64.deb` | CI, `ubuntu:24.04` container | lintian 0 errors; `apt` install in a clean container, headless start, removal | `0cf37139440533d5961749118536e0163ab7f018d47db1653b254510988e1995` |
| `linux/ubuntu-24.04/nhill-console_2.0.0-1_amd64.deb` | same | same | `7545b6883e88cf19dcac418339428df956133fb393b8aa9c5b70a594a5414dc0` |
| `linux/ubuntu-24.04/nhill-pos_2.0.0-1_amd64.deb` | same | same | `453c3d554b1457a3315e7f5a7487921778dcfb3ab965532d1529708c93a99f77` |
| `linux/ubuntu-24.04/nhill-server_2.0.0-1_amd64.deb` | same | lintian 0 errors, 7 warnings; installed in the clean container | `a0c75e986e6614b887e9b61cc9356971ca8c652896e572a422d93d5839832c5c` |
| `linux/debian-13/nhill-business_2.0.0-1_amd64.deb` | CI, `debian:trixie` container (Debian's Qt 6.8) | install, start, removal in a clean container | `5f45dbcf7a730ce20313cb4b1f58f0dfed16b11cf3941847230b794985020fd4` |
| `linux/debian-13/nhill-console_2.0.0-1_amd64.deb` | same | same | `f7f87e521ac45c284b18db5cf3a330e8640f004d6ed0d410c9b0c2aa96f936f5` |
| `linux/debian-13/nhill-pos_2.0.0-1_amd64.deb` | same | same | `9f934b74718736a81334c1fd7f6979109afd37977fa3445106b681262c04678f` |
| `linux/debian-13/nhill-server_2.0.0-1_amd64.deb` | same | installed | `13b582cfcd6e89e9b094cc7125d815194acd6e422834d9baa6319617bed162bb` |
| `windows/nhill-business-2.0.0-setup.exe` | CI `windows-2022`, MSVC 2022, Qt 6.5.3, NSIS | PE format checked | `1d65aaa1da100550c09d020af2e94a06a99e78a4913e6ef5a862e792892e53e1` |
| `windows/nhill-console-2.0.0-setup.exe` | same | PE format checked | `48c2434afe25fb48678a6af7c247596d7d669af26595d05c180b4ece0dbcfe26` |
| `windows/nhill-pos-2.0.0-setup.exe` | same | PE format; **silent install and uninstall succeeded** on the runner | `bc812f9fcb4bcc47d94d69e98544941b91bd4f3c3c3a505ee56c350965d76872` |
| `android/nhill-business-2.0.0-arm64-v8a.apk` | CI, Qt 6.5.3 for Android, NDK r25c, platform 34 | content (manifest, arm64-v8a libraries), **debug** signature verified by `apksigner` | `a8156401b2a0cc45fddfad7f6be34da419337bcb6fc8f756e85ac5c1c8b9fba8` |
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
| Nhill POS 2.0 screenshots | the automatic tour stopped for lack of a demo till (fixed in `seed-demo.py`, to reconfirm in CI) | rerun CI; Business 2.0 screenshots available in `docs/assets/v2/` |
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
