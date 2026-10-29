# Nhill Business 3.0 — Application guide

*Français : [../fr/nhill-business.md](../fr/nhill-business.md)*

Nhill Business serves the business: sales, customers, products, stock, purchases,
expenses, payments, reports, team and settings. Platforms: Windows 10 and 11 (64-bit),
Linux (Ubuntu 24.04, Debian 13), Android 7 and later phones and tablets. Installation:
[installation-guide.md](installation-guide.md) § 3. Other applications:
[nhill-pos.md](nhill-pos.md) (till), [nhill-console.md](nhill-console.md) (operator).
The interface is in French or English (labels below: English interface).

> **What you see depends on your role and on the business's plan.** A menu, tab or button
> appears only if your role allows it **and** the plan includes the feature (invoices,
> quotes, till, purchases, expenses, several warehouses, several currencies, catalogue
> import, advanced reports). A feature outside the plan is never shown; the server refuses
> it too. When the plan changes, rights are updated at the next request.

Real screenshots of version 3.0 (demo business, French interface): `docs/assets/v3/`.

## 1. First launch

### 1.1 Server address

No address is built into the application. At first launch, **Server settings** (Welcome
screen) asks for the **Server address (API)** given by your provider, for example
`https://api.example.com`. **Test the connection** checks it (Nhill server, compatible
version, HTTPS required), then **Use this server** saves it on the device. It can be
changed from the same screen. If the server requires a newer version of the application,
install the update first.

### 1.2 "My business already exists" (join)

1. Enter the **business code** (`NH-XXXX-XXXX-XXXX`) given by the owner. The code changes
   regularly (every 30 days by default): if it is refused, ask for the new one. The error
   message is deliberately the same in every case.
2. Then **"I have an account"** (e-mail and password) or **"I received an invitation"**
   (invitation e-mail, activation code given by your manager, password of at least 8
   characters to choose).

### 1.3 "Create my business"

1. **Type of activity**: individual entrepreneur (a single user) or company or
   organisation (several users, roles, invitations).
2. **Identity**: trade name, legal name, sector, country, VAT registration, the legal
   identifiers that apply to you (RCCM, Id. Nat., NIF…).
3. **Contact details and preferences**: city, address, phone, e-mail, main currency,
   document language.
4. **Your account** (owner), then review and creation.

No payment method is asked for. The starting plan (usually the free trial, 30 days by
default) is assigned by the operator's rules; the dashboard shows the days left. Note the
**business code** shown at the end. If a company with the same legal identifier already
exists, Nhill never attaches you to it automatically: you can send a **join request**.

## 2. Main screen

Menu: Dashboard, Sales, Customers (and suppliers), Products & services, Stock, Purchases,
Expenses, Payments, Reports, Activity log (or My activity), Settings, Help & support —
each according to your role and the plan. At the top: global search, connection state,
subscription state, notifications.

| Shortcut | Action |
|---|---|
| `Ctrl+K` | global search |
| `Ctrl+N` | new invoice (if your role and the plan allow it) |
| `Ctrl+1` … `Ctrl+4` | Dashboard, Sales, Customers, Products |
| `F5` | refresh |
| `Alt+←` | previous screen |

The dashboard only shows the indicators of the features included in the plan; on a narrow
screen, indicator titles wrap onto two lines.

## 3. Sales

- **New invoice** (or quote, order): customer, currency, date, due date, lines (product or
  free line, quantity, price excl. tax, discount, tax). The server recomputes the totals
  and is authoritative.
- **Draft**: editable, without a number or fiscal value.
- **Issue**: final number (for example `FA-2026-000012`), stock reduced, SHA-256 hash of
  the PDF recorded; the document **can no longer be modified or deleted**. To correct:
  **Credit note** (full or partial, possible restock, reason).
- **Foreign currency**: the exchange rate is asked at issue, recorded with its date and
  source, never recomputed ("several currencies" feature of the plan).
- **Quote**: accepted or declined, then **Convert to invoice**. **Order**: converted to
  an invoice.
- **Payment**: "Record payment" (cash, Mobile Money, card, transfer, cheque…, reference),
  never more than the amount due; on a credit note, "Record refund".
- **PDF A4** or **Ticket** (58 or 80 mm): same document, same template as Nhill POS; the
  file is saved in the device's download folder, under `Nhill`. On Android, **Share** sends
  the PDF to messaging or e-mail.
- **Connection lost while issuing or paying**: **Retry**; the operation is never recorded
  twice.
- **DRC, VAT-registered**: Nhill documents are management documents, not standardised
  invoices. Issue the standardised invoice with your electronic fiscal device (DEF), then
  **Record DEF reference** (NID, authentication code, number) on the Nhill invoice for
  reconciliation. Nhill certifies nothing.
- The plan's monthly document limit, if any, never blocks a credit note.

## 4. Customers, products, stock, purchases, expenses

- **Customers and suppliers**: contact details, NIF, payment terms, notes; the record
  shows the balance and documents (suppliers: if the plan includes purchases or expenses).
- **Products and services**: reference, name, unit, sale and purchase prices, tax, barcode,
  alert threshold. CSV export; **all-or-nothing CSV import** (errors listed per line) if
  the plan includes it. The plan may limit the number of products.
- **Stock**: quantities change only through movements (sale, received purchase, credit
  note, adjustment with a reason, transfer between warehouses if the plan allows several
  warehouses), each with its author.
- **Purchases**: draft → **receipt** (stock entry, purchase price updated) → payment.
- **Expenses**: amount, tax, category, supplier, paid or not, **receipt** (PDF, PNG or
  JPEG, 5 MB), within the plan's storage limit.

## 5. Reports and activity log

- **Reports** ("reports" permission): sales journal ("Issued by"), payments ("Recorded
  by"), expenses, stock movements, receivables and payables to date; with the plan's
  advanced reports: sales per product (**estimated** margin) and activity per employee.
  Any period, **CSV** or **PDF** export. Amounts are always shown **per currency**, never
  converted or added across currencies.
- **Activity log** (owner, administrator, accountant): every action of the business,
  filters by employee, action type and period, CSV/PDF export. Other roles see **My
  activity**. The log is kept by the server and can be neither modified nor deleted, not
  even by the owner; consultations of statistics by the operator also appear in it.

## 6. Settings

| Tab | Content | Who |
|---|---|---|
| Establishment | identity, time zone (dates of documents and sessions), VAT, legal identifiers, logo (PNG or JPEG, 2 MB), document language, payment terms, tax rounding, payment instructions, **notices printed on every document** (you write them; Nhill neither writes nor checks them), footer, selling below zero stock; **Export all my data** (owner) | owner, administrator |
| Establishment code | show, copy, renew now | owner, administrator; manager read-only |
| Team | members, roles, deactivation, invitations (activation code shown **only once**), temporary password for a member (§ 8), number of users allowed by the plan | owner, administrator |
| Subscription | § 7 | owner, administrator |
| Taxes & accounts | tax rates (a rate cannot be changed: create another one), cash and bank accounts, exchange rates | owner, administrator |
| Point of sale | tills and cashiers, payment methods, maximum discount, receipt, till promo codes, sessions and X/Z reports ([nhill-pos.md](nhill-pos.md) § 1) | depending on the role, if the plan includes the till |
| My account | name, phone, language, theme, password, "Remember me on this device", sign out | everyone |

**Export all my data**: one ZIP archive with all the business's data (CSV and JSON files),
the stored files (logo, receipts) and a list of contents with hashes. Available at any time
to the owner, including in read-only mode. The archive contains personal data: keep it
safe.

Roles: owner, administrator, manager, salesperson, accountant, read-only, till supervisor,
cashier. Permissions in detail: [api-reference.md](api-reference.md) ("Permissions per
role").

## 7. Subscription (owner and administrators)

- **Current subscription**: plan, period and days left, features **included in your plan**
  and usage of the limits (for example "3 of 5 used").
- **Plans**: plans published by the operator, prices per duration and currency. If no plan
  is published, the screen says so.
- **Choose this plan**: summary computed by the server (price, promo code, credit for
  unused days, tax, total). Upgrade: immediate, unused part deducted. Downgrade: paid now,
  applied at the end of the period. Renewal: the new period starts at the end of the
  current one.
- **Payment**: through an online provider when the operator has enabled one, otherwise
  bank transfer or Mobile Money with the reference shown, confirmed by the billing team.
  Nothing is charged before confirmation; an unpaid request expires.
- **Automatic renewal**: only with your express consent (checkbox), when the plan allows
  it and a compatible provider is connected.
- **Reminders** before the due date, then a **grace period** if payment is not confirmed;
  then, depending on the plan, a fallback plan or **read-only** (viewing and export remain
  possible).
- **End of the contract**: 90 days of read-only to export your data, then permanent erasure
  by the operator.

## 8. Password, sign-in and security

- **Employee**: if you forget it, the administrator or owner creates a **temporary
  password** (Settings › Team › Reset the password), shown only once and given in person or
  by phone; your sessions are closed; at the next sign-in, you choose a new password (at
  least 8 characters). An administrator cannot reset the owner or themselves; only the
  owner resets an administrator; an account used in another business goes through the
  e-mail procedure.
- **Owner, administrator, account of several businesses**: **Forgot your password?** on
  the sign-in screen sends an 8-digit code by e-mail (single use, limited validity); the
  message shown is the same whether the address exists or not; every session is closed
  after the change.
- **Remember me**: session token kept in the system's secure storage (Windows Credential
  Manager, Android Keystore, Linux keyring), never the password; tick it only on a
  personal device; signing out erases it.
- Without a connection, you can neither sell nor invoice; nothing is lost.
- Never put a password, business code or card number in **Help & support**.

## 9. What Nhill does not do

See [user-guide.md](user-guide.md) § 3 (tax, general ledger, payments, sending documents,
connection, devices).
