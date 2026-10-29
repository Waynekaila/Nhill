# Nhill 3.0

*Français : [README.md](README.md)*

*Named in tribute to Napoleon Hill.* Multi-business commercial management suite for traders
and SMEs: three applications on one secure server.

| Application | For | Platforms |
|---|---|---|
| **Nhill Business** | traders, entrepreneurs, SMEs: sales (quotes, orders, invoices, credit notes), customers and suppliers, products and stock, purchases, expenses, payments, reports, team and "who did what" log, subscription | Windows, Linux, Android |
| **Nhill POS** | cashiers: till sessions, split-payment sales, controlled discounts, returns, 58/80 mm receipts or A4 invoice in the customer's name, closing with difference, X/Z reports | Windows, Linux |
| **Nhill Console** | the platform operator (private): server setup, businesses, plans, rights and prices, assignment rules, payment providers, support, erasure of data at the end of the contract, audit | Windows, Linux |

- **Server**: C++17, `/api/v1` API, PostgreSQL 15+ (Supabase or self-hosted), isolation of
  businesses through forced row-level security, no database credentials in the applications.
- **Applications**: C++17 / Qt 6 Quick (QML), one code base; French and English; CDF, USD,
  EUR… exact amounts, never added across currencies.
- **Nhill is not invoicing software certified by the DRC tax authority (DGI)**: its
  documents are management documents (see [docs/en/legal-parameters.md](docs/en/legal-parameters.md)).

## State of version 3.0.0 (29 October 2026, branch `release/3.0.0`)

| Item | Actual state | Evidence |
|---|---|---|
| Build + tests (common 22; server 122: 121 passed, 1 skipped; setup 6; client 15; real Linux keyring 3) | passed on Ubuntu 24.04 | GitHub Actions, branch `ci-results/ci` |
| Plans and rights (features hidden and refused by the server, limits) | 10 server tests (`Plans.*`, every plan and every role); hiding in the applications through `Session.has()` (no automatic interface test for each plan) | `ci-results/ci` |
| "Remember me" (QtKeychain) | passed: Windows Credential Manager (Windows Server 2022), Keystore of an Android 11 emulator, Secret Service on Linux; not yet on a real phone or user machine | `ci-results/windows`, `ci-results/android-keychain`, `ci-results/ci` |
| `.deb` packages (server, Business, POS, Console) | built for **Ubuntu 24.04** and **Debian 13**; installed then removed in clean containers | `ci-results/deb-*`, `ci-results/install-*` |
| VPS set-up kit (`deploy/vps`) | 12-check acceptance passed on a fresh CI Ubuntu 24.04 machine; **no real deployment on the Hostinger VPS** (no access) | `ci-results/vps-test` |
| Windows installers (NSIS, 3 applications) | built (MSVC 2022, Qt 6.5.3); silent installation tested on Windows Server 2022 | `ci-results/windows` |
| Android APK (Nhill Business, arm64-v8a) | built, third-party notices included; **signed with a debug key** (installable, not publishable); not tested on a real device | `ci-results/android` |
| API (203 endpoints) and database (71 tables) references | generated from the code and a migrated database; checked up to date by CI | `docs/en/api-reference.md`, `docs/en/database-reference.md` |
| Marketing website + forms backend | 24 pages checked; backend tests on SQLite and PostgreSQL; **not online yet**: `nhill.online` shows Hostinger's parking page, no mail configured (MX, SPF, DMARC) | `ci-results/site-backend-postgresql`, `ci-results/domain-check` |
| Code signing (Authenticode, Android release key) | **not done** — certificates to acquire | — |
| External audit, penetration test | **not done**; open risks R1 (Ubuntu 24.04 cpp-httplib) and R2 (Qt 6.5.3) awaiting a decision | [security audit](docs/en/03-security-audit.md) |

Details, limits and human actions: [validation report](docs/en/validation-report.md).

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
| **Index per component** | [docs/en/index.md](docs/en/index.md) | [docs/fr/index.md](docs/fr/index.md) |
| Nhill Business · Nhill POS · Nhill Console | [business](docs/en/nhill-business.md) · [pos](docs/en/nhill-pos.md) · [console](docs/en/nhill-console.md) | [business](docs/fr/nhill-business.md) · [pos](docs/fr/nhill-pos.md) · [console](docs/fr/nhill-console.md) |
| Server · database | [server](docs/en/server.md) · [database](docs/en/database.md) | [serveur](docs/fr/serveur.md) · [base](docs/fr/base-de-donnees.md) |
| Generated references (API, database) | [API](docs/en/api-reference.md) · [database](docs/en/database-reference.md) | [API](docs/fr/reference-api.md) · [base](docs/fr/reference-base-de-donnees.md) |
| Production (Hostinger VPS) | [docs/en/vps-deployment-hostinger.md](docs/en/vps-deployment-hostinger.md) | [docs/fr/deploiement-vps-hostinger.md](docs/fr/deploiement-vps-hostinger.md) |
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
| Legal file (DRC, 20 templates) | [legal/en/](legal/en/00-legal-research.md) | [legal/fr/](legal/fr/00-recherche-juridique.md) |
| Commercial file (Kinshasa) | [marketing/en/](marketing/README.md) | [marketing/fr/](marketing/README.md) |

Every Markdown document has its generated PDF (`docs/pdf/`, `legal/pdf/`, `marketing/pdf/`,
`README.pdf`, `README.en.pdf`) produced by `scripts/build-docs-pdf.sh`. The documents of
versions 0.1 to 2.x are archived in `docs/archive/` (French, no longer maintained).

## Repository layout

```
common/      shared C++ library (amounts, codes, validation, portable 128-bit integer)
server/      C++17 API (http, services, setup, jobs, pdf), country profiles, translations
client/      Qt 6 applications: src/, qml/Nhill/{Ui,Business,Console,Pos}, i18n/, android/
db/          migrations 0001–0015 (digests checked)
tests/       common, server (throwaway PostgreSQL), client, standalone test tool
packaging/   Debian, Windows (NSIS), Android, third-party notices
deploy/      server: systemd, Caddy, backups, Hostinger VPS kit
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
