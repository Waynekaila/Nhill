# Compliance matrix: marketing website and application (3.0.0)

*Français : [../fr/matrice-conformite-site.md](../fr/matrice-conformite-site.md)*

| | |
|---|---|
| Version | 3.0.0 — 28 October 2026 |
| Purpose | every statement of the marketing website (`website/src/pages/fr`, `website/src/pages/en`) checked against the code, the tests or a management decision |
| Method | reading of the server code, migrations and QML screens; automated tests (`tests/server`, 121 tests of which 120 passed and 1 skipped locally on 28/10/2026); end-to-end runs of the deployment tools on a fresh Ubuntu 24.04 machine in continuous integration |
| Key | **Verified**: present in the code and, where stated, covered by a test · **Tooled**: the tools exist and are tested, real operation has not started · **Commitment**: human service promise, not verifiable in the code, to be kept by the team · **Fixed**: gap found during this review and fixed |

## 1. Till (Nhill POS)

| Website statement | Status | Evidence |
|---|---|---|
| Each cashier has an account and sees only their till and session | Verified | roles `cashier` and `cash_manager` (`db/migrations/0009_pos.sql`), assignment `pos_register_users`; tests `tests/server/test_pos.cpp` |
| Opening with a cash float; sale by touch or with a USB barcode reader | Verified | `client/qml/Nhill/Pos/pages/PosHome.qml` (`openSession`), search field validated by the Enter key the reader sends (`PosSale.qml`) |
| Several payment methods on one sale (cash, card, Mobile Money, transfer, cheque), change due | Verified | `pos_payment_methods.kind` (`0009_pos.sql`), `PosPayDialog.qml` |
| Receipt or A4 invoice in the customer's name, same template as Nhill Business | Verified | `document: "invoice"` in `PosPayDialog.qml`; shared templates (task "POS invoices identical to Business"); tests `test_documents.cpp` |
| Discount above the cap, return, void: approval by a supervisor with their password, valid once | Verified | `pos_approvals` (`0009_pos.sql`), `PosApprovalDialog.qml`, `PosNav.sendApproved` |
| Cash in and out with a reason, X reading | Verified | `pos_cash_movements.kind` (`cash_in`, `cash_out`), X report (`server/src/services/pos.cpp`) |
| Blind close, difference calculated and kept in the Z report | Verified | the expected amount is removed from responses before closing (`pos.cpp`, fields `_cash_expected_minor`); closed session immutable (`nh_guard_pos_session`) |
| 58 or 80 mm receipts; F2 and F9 shortcuts | Verified | `receipt_width_mm IN (58, 80)` (`0009_pos.sql`); `Shortcut { sequence: "F2" }`, `"F9"` (`PosSale.qml`) |
| A sale interrupted by an outage can be validated again without being counted twice | Verified | idempotency key kept for the retry (`PosPayDialog.qml`, `idempotency_keys`) |

## 2. Invoices, stock, reports

| Website statement | Status | Evidence |
|---|---|---|
| Quotes, orders, invoices, credit notes numbered by the server with no gap or duplicate | Verified | `doc_type IN ('quote','order','invoice','credit_note')`, `document_sequences` (`0002_business.sql`) |
| Quote turned into an invoice in one click; a credit note corrects an invoice and restocks | Verified | `POST /api/v1/sales-documents/:id/convert`, `/credit-note` with `restock` (`server/src/services/sales.cpp`) |
| 36 currencies, exchange rate recorded with the document | Verified | 36 ISO 4217 codes inserted in `currencies` (migrations); `fx_rate`, `fx_rate_date`, `fx_rate_source` (`0002_business.sql`) |
| Configurable tax rates (DRC profile: 16 %, 0 %, exempt) | Verified | `tax_rates`, compliance profile `CD` |
| Field for the standardised invoice reference | Verified | column `fiscal_reference` (`sales_documents`) |
| Stock per warehouse, transfers, traced movements, threshold alerts, estimated value | Verified | `stock_movements.kind` (`transfer_in`, `transfer_out`, `adjustment`…), `server/src/services/stock.cpp`; dashboard |
| Catalogue CSV import and export | Verified | `GET /api/v1/products/export.csv`, `POST /api/v1/products/import` (`catalog.cpp`) |
| Reports: sales, payments, expenses, products with estimated margin, stock movements, receivables, payables; PDF and CSV export | Verified | `server/src/services/reports.cpp` (routes `/api/v1/reports/...`) |
| Expense receipts (photo or PDF) | Verified | table `objects`, `purpose = 'receipt'`, PNG, JPEG, PDF types |
| Full data archive for the account holder (CSV, JSON, attachments) | Verified | `GET /api/v1/establishment/export` (`export.cpp`) |

## 3. Team, security, accounts

| Website statement | Status | Evidence |
|---|---|---|
| Roles: owner, administrator, manager, sales, accountant, read-only, cash supervisor, cashier | Verified | constraint `memberships_role_check` (`0009_pos.sql`) |
| Business code renewed every 30 days by default | Verified | setting `code_rotation_days = 30` (`0004_reference_data.sql`), job `rotateCodes` |
| Activity log that cannot be changed, exportable | Verified | only `SELECT, INSERT` rights on `audit_log` (`0003_security.sql`); export (`activity.cpp`) |
| Disabled employee: their sessions are closed | Verified | `members.cpp` (revocation `member_disabled`) |
| Forgotten password: temporary password from the administrator; e-mailed code for owners and administrators | Verified | `password_resets.purpose IN ('email_code','forced_change')` (`0011_password_recovery.sql`), `auth.cpp`; tests `test_password.cpp` |
| Each business's data kept separate | Verified | row-level security forced on 30 tables (`row_level_security` check of `nhill-server setup`); isolation tests |
| The Nhill team sees only overall figures, each consultation recorded in the business's log | Verified | aggregate functions `0006_activity_and_platform_stats.sql`, audit of consultations |
| Functions limited to the business's plan | Verified | plan rights (`0012_plan_entitlements.sql`, `entitlements.cpp`); tests `test_entitlements.cpp` |
| Export possible even in read-only mode | Verified | `read_only` access = consultation and export (`nh_subscription_access`) |

## 4. Devices and languages

| Website statement | Status | Evidence |
|---|---|---|
| Nhill Business: Windows 10 and 11 (64-bit), Linux (Debian 13, Ubuntu 24.04), Android 7 and later | Verified | Windows builds, `.deb` packages for Ubuntu 24.04 and Debian 13, APK `minSdk 24` (`client/CMakeLists.txt`) in continuous integration |
| Nhill POS: Windows 10 and 11, Linux | Verified | same builds |
| Interface, documents and reports in French and English | Verified | `client/i18n/nhill_fr.ts` (0 untranslated string in `scripts/qa/qml-check.py`), bilingual PDF templates |
| "A mobile connection (3G, 4G) is enough" | Fixed | no bandwidth measurement: "3G, 4G" removed from the FAQ (FR and EN) |

## 5. Data, hosting, retention

| Website statement | Status | Evidence |
|---|---|---|
| Hosting on a Hostinger server located in France | Tooled | management decision; `deploy/vps` kit tested on a fresh machine; the production server is not deployed yet (no access provided) |
| Encrypted backup every day | Tooled | `nhill-backup.timer`, `age` encryption, trial restore off the server (`deploy/vps/tests/acceptance.sh`) |
| A business's data deleted 90 days after the end of the contract | **Fixed** | no deletion existed: migration `0014_establishment_erasure.sql`, the Nhill Console "Erase…" action and 6 tests (`tests/server/test_erasure.cpp`) added |
| Website requests deleted after 12 months | **Fixed** | the website said "12 months after the last exchange" while the purge counts from sending: wording aligned with the code (`SITE_RETENTION_DAYS`) |
| Technical logs: 90 days at most | Verified | journald `MaxRetentionSec=90day`, Caddy `roll_keep_for 2160h`; ended sessions deleted after 90 days (daily job) |
| No cookie, no tracker | Verified | no `Set-Cookie`, no browser storage, no external resource (`website/assets/js/site.js`; content security policy in `website/backend/deploy/Caddyfile.site`) |

## 6. Service commitments (to be kept by the team)

These promises appear on the website; they do not depend on the software and cannot be
checked in the code. Management confirms it can keep them before the website goes live.

| Statement | Pages |
|---|---|
| Demonstration of about 45 minutes, by video call or on site in Kinshasa | home, contact, get started |
| Onboarding in about half a day (installation, catalogue import, training) | home, free trial, FAQ |
| 30-day assisted trial: weekly check-in, support; limited places | free trial, home |
| Written proposal before the end of the trial | free trial, terms |
| At most one news message a month | forms, privacy policy |

## 7. Contacts and domain

| Item | Status on 28 October 2026 |
|---|---|
| `nhill.online` | the domain answers over HTTPS (Let's Encrypt certificate valid until 25 January 2027) but shows Hostinger's parked-domain page: the website is not published yet; no HTTP to HTTPS redirect and no HSTS header (`website/tools/check_live.py`, CI report `ci-results/domain-check`) |
| `info@nhill.online`, `support@nhill.online` | no MX, SPF or DMARC record: these addresses cannot receive mail until e-mail is set up |
