# Nhill 2.1

*Français : [README.md](README.md)*

*Named in tribute to Napoleon Hill.* Multi-business commercial management suite for traders
and SMEs: three applications on one secure server.

| Application | For | Platforms |
|---|---|---|
| **Nhill Business** | traders, entrepreneurs, SMEs: sales (quotes, orders, invoices, credit notes), customers and suppliers, products and stock, purchases, expenses, payments, reports, team and "who did what" log, subscription | Windows, Linux, Android |
| **Nhill POS** | cashiers: till sessions, split-payment sales, controlled discounts, returns, 58/80 mm receipts, closing with difference, X/Z reports | Windows, Linux |
| **Nhill Console** | Nhill's creators (private): server setup, businesses, subscriptions, plans and prices, payment providers, support, audit | Windows, Linux |

- **Server**: C++17, `/api/v1` API, PostgreSQL 15+ (Supabase or self-hosted), isolation of
  businesses through forced row-level security, no database credentials in the applications.
- **Applications**: C++17 / Qt 6 Quick (QML), one code base; French and English; CDF, USD,
  EUR… exact amounts, never added across currencies.
- **Nhill is not invoicing software certified by the DRC tax authority (DGI)**: its
  documents are management documents (see [docs/en/legal-parameters.md](docs/en/legal-parameters.md)).

## State of version 2.1.0 (27 October 2026, branch `release/2.1.0`)

| Item | Real state | Evidence |
|---|---|---|
| Build + tests (common 22, server 100 including 16 for the forgotten password, setup, client) | passed on Ubuntu 24.04 and Debian 13 | GitHub Actions, branches `ci-results/ci`, `ci-results/deb-*` |
| "Remember me" (QtKeychain) | real test passed: Windows Credential Manager (CI Windows Server 2022), Keystore of an Android 11 emulator, Secret Service on Linux; not yet on a real phone or user computer | `ci-results/windows`, `ci-results/android-keychain`, `ci-results/ci` |
| `.deb` packages (4 per distribution) | built for **Ubuntu 24.04** and native **Debian 13**; installed then removed in clean containers | `ci-results/deb-*`, `ci-results/install-*` |
| Windows installers (NSIS, 3 applications) | built (MSVC 2022, Qt 6.5.3, static QtKeychain); silent install and uninstall of Nhill POS and Nhill Business tested on Windows Server 2022 (CI runner) | `ci-results/windows` |
| Android APK (Nhill Business, arm64-v8a) | built with QtKeychain; **signed with a debug key** (installable, not publishable); not tested on a real device | `ci-results/android` |
| Website + form backend | 24 pages checked; backend tests on SQLite and PostgreSQL 16 | `ci-results/site-backend-postgresql` |
| Code signing (Authenticode, Android release key) | **not done** — certificates to acquire | — |
| External audit, penetration test | **not done** | [security audit](docs/en/03-security-audit.md) |

Details, limitations and human actions: [validation report](docs/en/validation-report.md).

## Quick start (development, Linux)

```bash
scripts/install-deps-linux.sh --with-packaging --with-docs --yes
scripts/build-linux.sh                                          # build + tests
NH_DEV_STATE=/tmp/nhdev scripts/dev/dev-stack.sh start build    # throwaway PostgreSQL + API 127.0.0.1:8080
python3 scripts/dev/seed-demo.py --email demo@example.com        # DEMO business
NHILL_SERVER=http://127.0.0.1:8080 build/client/nhill-business
```

Building every deliverable: [build guide](docs/en/build-guide.md) (`scripts/build-all.sh`,
or GitHub Actions on a `release/**` branch).

## Documentation

| Document | EN | FR |
|---|---|---|
| User guide | [docs/en/user-guide.md](docs/en/user-guide.md) | [docs/fr/guide-utilisateur.md](docs/fr/guide-utilisateur.md) |
| Installation | [docs/en/installation-guide.md](docs/en/installation-guide.md) | [docs/fr/guide-installation.md](docs/fr/guide-installation.md) |
| Configuration | [docs/en/configuration-guide.md](docs/en/configuration-guide.md) | [docs/fr/guide-configuration.md](docs/fr/guide-configuration.md) |
| Development | [docs/en/developer-guide.md](docs/en/developer-guide.md) | [docs/fr/guide-developpement.md](docs/fr/guide-developpement.md) |
| Build and release | [docs/en/build-guide.md](docs/en/build-guide.md) | [docs/fr/guide-compilation.md](docs/fr/guide-compilation.md) |
| Security | [docs/en/security.md](docs/en/security.md) | [docs/fr/securite.md](docs/fr/securite.md) |
| Legal parameters | [docs/en/legal-parameters.md](docs/en/legal-parameters.md) | [docs/fr/parametres-legaux.md](docs/fr/parametres-legaux.md) |
| Changelog | [docs/en/changelog.md](docs/en/changelog.md) | [docs/fr/changelog.md](docs/fr/changelog.md) |
| Technical, UI/UX, security audits | [docs/en/01…03](docs/en/) | [docs/fr/01…03](docs/fr/) |
| Validation report | [docs/en/validation-report.md](docs/en/validation-report.md) | [docs/fr/rapport-validation.md](docs/fr/rapport-validation.md) |
| Website | [website/README.en.md](website/README.en.md) | [website/README.md](website/README.md) |
| Legal file (DRC, 19 templates) | [legal/en/](legal/en/00-legal-research.md) | [legal/fr/](legal/fr/00-recherche-juridique.md) |
| Commercial file (Kinshasa) | [marketing/en/](marketing/README.md) | [marketing/fr/](marketing/README.md) |

Every Markdown document has its generated PDF (`docs/pdf/`, `legal/pdf/`, `marketing/pdf/`,
`README.pdf`, `README.en.pdf`) produced by `scripts/build-docs-pdf.sh`. The 0.1.0 reference
documents are kept in French in `docs/*.md` (technical, API, backup, compliance…).

## Repository layout

```
common/      shared C++ library (amounts, codes, validation, portable 128-bit integer)
server/      C++17 API (http, services, setup, jobs, pdf), country profiles, translations
client/      Qt 6 applications: src/, qml/Nhill/{Ui,Business,Console,Pos}, i18n/, android/
db/          migrations 0001–0013 (digests checked)
tests/       common, server (throwaway PostgreSQL), client, standalone test tool
packaging/   Debian, Windows (NSIS, MinGW), Android
scripts/     build, verification, release, QA, i18n, development
website/     FR/EN marketing website and form backend
docs/ legal/ marketing/   documentation, legal and commercial files (FR/EN, MD + PDF)
.github/     CI workflows (tests, Linux packages, Windows, Android, documentation and website)
```

## Contact

Nhill Solution — head office in Kinshasa (Democratic Republic of the Congo).
General contact: info@nhill.online · Customer support: support@nhill.online.

## Licence

Project code: proprietary, Nhill Solution, all rights reserved. Dependencies:
`docs/licences-dependances.md` (Qt under LGPL v3: dynamic linking and notice required).
