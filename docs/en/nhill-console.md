# Nhill Console 3.0 — Operator guide

*Français : [../fr/nhill-console.md](../fr/nhill-console.md)*

Nhill Console is the private application of the platform operator (Nhill Solution):
server installation, businesses, plans and rights, billing, support, operations.
Platforms: Windows 10 and 11, Linux. It is not distributed to businesses.

The console gives **no access to the businesses' invoices, customers, products or stock**:
only their platform record, their subscription and **aggregated** statistics, each
consultation of which appears in the business's activity log. Every console action is
recorded in the platform audit log.

## 1. Installing a new server

1. Start the server without a database: it enters **setup mode** and writes a **setup
   token** in its log (`journalctl -u nhill-server`), valid 60 minutes, renewed at each
   restart.
2. Console › **Install a new server**: (1) server address (HTTPS except a local server) and
   token; (2) database: self-hosted PostgreSQL or Supabase (Session pooler), host, port,
   database, administrator user, password (used once, not stored), TLS; PostgreSQL 15 or
   later; (3) first administrator; (4) verification.
3. The verification shows the checks: migrations, application role without privileges and
   without objects, forced row-level security, dedicated `nhill` schema, closed to public
   roles, functions with a fixed `search_path`, no data visible and no write without a
   business, no schema change by the application. The server then restarts in normal
   operation.

On the production VPS, `deploy/vps/provision.sh` performs this installation on the command
line ([vps-deployment-hostinger.md](vps-deployment-hostinger.md)).

## 2. Access

- **Server address**: asked at first launch, checked before it is saved; **Change server**
  modifies it.
- **First access**: e-mail, **setup token** (given only once, valid 30 minutes), password
  (8 characters minimum, not a too-common password), then a TOTP key to add to an
  authenticator app and a 6-digit code. Two-factor authentication is **mandatory**. A lost
  or expired token of the first administrator is reissued with `nhill-server
  admin-bootstrap` as long as the first access is not complete.
- **Sign-in**: e-mail, password and TOTP code (a code is used only once). The session is
  not remembered.
- **Roles**: `superadmin` (everything, including settings, providers, erasure of
  businesses, administrator invitations), `billing` (plans, prices, rights, rules,
  transactions, promo codes), `support` (businesses, suspensions, codes, join requests,
  support requests, errors, announcements), `auditor` (read-only: log, transactions,
  provider events, reports). Detail per route: [api-reference.md](api-reference.md)
  ("Console roles" column).

## 3. Screens

| Screen | Use |
|---|---|
| Overview | businesses, subscriptions by state, active users, activity, confirmed revenue |
| Notifications | support, payments received or mismatched, technical errors; mark all as read |
| Establishments | search; filter **To erase (90 days over)**; record: identity, subscription, effective rights and usage, codes (without their value), aggregated statistics; **Suspend**, **Reactivate**, **Reissue the code**, **Subscription…** (extend, suspend, reactivate, cancel, change plan), **Erase…** — reason required |
| Reports | report per business and per currency (CSV, PDF); monthly report of a business |
| Attachment requests | check the identity, accept or refuse with a note |
| Support tickets | take on, reply, internal note (invisible to the business), status, severity, assignee |
| Transactions | subscription payment requests, detail and provider references, **manual confirmation** of a transfer (reference and reason) |
| Plans & prices | § 5 |
| Assignment rules | § 6 |
| Promo codes | percentage or fixed amount in a currency, dates, number of uses, uses; code and discount cannot change after creation |
| Payment providers | Stripe, PayPal, generic signed (HMAC); Payoneer configurable without a payment adapter; secrets **encrypted, never shown again**; test or live mode |
| Provider events | notifications received and their processing |
| Technical errors | grouped errors of the applications and the server, to mark as resolved |
| Audit log | platform and security events; details of business actions masked |
| Announcements | messages published to businesses |
| Platform settings | trial, grace period, reminders, code rotation, attempt limits, invitation validity, users during the trial, forgotten password, payment instructions (FR/EN); **invite an administrator** (token shown only once) |

## 4. Erasing a business

Commitment of the terms of service: at the end of the contract, the data stays readable
and exportable for 90 days, then it is deleted.

1. Establishments › filter **To erase (90 days over)**. The record shows when access ended
   and from which date erasure is due.
2. **Erase…** (super administrator): reason, exact trade name typed. Before the 90 days,
   only if the customer asked for it (checkbox), and never while the business has full
   access.
3. Deleted: documents, customers, suppliers, products, stock, payments, files, activity
   log, codes, sessions and accounts used only in that business. Paid or refunded
   subscription orders are archived for 10 years for accounting. The erasure is recorded in
   the platform log. It is permanent (backups disappear as they rotate).

## 5. Plans and rights (Plans & prices)

- **Identity**: code, FR/EN names and descriptions, kind (paid, trial, free, custom),
  audience (everyone, individual entrepreneurs, companies), visibility (public or private:
  a private plan is only assigned from the console), tier (higher = immediate pro-rata
  upgrade; lower = change at the end of the period), display order.
- **Conditions**: self-service, automatic renewal allowed (with the customer's express
  consent), grace period, reminders (days before the end), duration when assigned, plan
  applied at expiry instead of read-only.
- **Features**: invoices and credit notes, quotes and orders, Nhill POS till, purchases,
  expenses, several warehouses and transfers, several currencies, catalogue import,
  advanced reports. **A feature that is off is invisible in the applications and refused by
  the server.**
- **Limits**: users, tills, warehouses, products, documents per month, storage (empty =
  unlimited).
- **Prices per duration** (1, 3, 6, 12, 24 or 36 months) **and per currency**, excluding
  tax, with a tax rate; no price is preconfigured. Withdraw or offer a price again; changes
  apply only to future payment requests.
- **Published** (visible to businesses) or **archived** (no new subscription; current ones
  continue).

After saving, the plan's businesses get their new rights at their next request.

## 6. Assignment rules

Plan given automatically to a new business: the first active rule that matches (type of
business, country), by ascending priority, applies; as a **trial** or as a **granted
period** (duration in days, or open-ended for a free or custom plan). Without a rule, new
businesses get the standard trial of the platform settings.
