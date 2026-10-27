# Nhill 2.1 — User guide

*Français : [../fr/guide-utilisateur.md](../fr/guide-utilisateur.md)*

| Part | Application | For | Platforms |
|---|---|---|---|
| A | **Nhill Business** | the business: sales, customers, stock, purchases, expenses, reports, team | Windows, Linux, Android |
| B | **Nhill Console** | the creators of Nhill (private application) | Windows, Linux |
| C | **Nhill POS** | the cashiers of a business | Windows, Linux |

Screenshots: `docs/captures/` (taken on version 0.1.0; the 2.0 screens keep the same
layout, with the improvements listed in § 0). The applications are available in French
and English; the labels below are the English ones.

---

## 0. What is new for users

### Version 2.1

- **Remember me** (Nhill Business and Nhill POS, on Windows, Linux and Android): when
  ticked at sign-in, the application reopens already signed in. Only a session token is
  kept, in the system's secure storage (Windows Credential Manager, Android Keystore, Linux
  keyring), **never the password**. Unticked, nothing is kept; signing out also erases the
  remembered session (§ 10).
- **Forgotten password**: the establishment's administrator gives an employee a temporary
  password; owners and administrators receive a code by e-mail (§ 9).

### Version 2.0

- **Passwords**: at least 8 characters everywhere (account creation, invitation, password
  change, reset, administration console). Very common passwords ("12345678", "password",
  "kinshasa"…) are refused. No periodic change is required; change it only if you think
  it was seen or guessed.
- **Show password**: an eye button in every password field shows or hides what you type
  (useful on phones).
- **Windows that fit the screen**: on start, each application takes a size suited to the
  screen (small laptop screens included) and is centred; on a screen that is too small it
  opens maximised. Windows can be resized freely.
- **Tables**: horizontal scrolling when the columns do not fit, tooltip on truncated text,
  keyboard navigation (arrows, Enter).
- **Accessibility**: stronger contrast (secondary text, field borders, disabled buttons),
  visible keyboard focus ring, tooltips on icon buttons, error messages shown longer (8 s)
  and announced to screen readers.
- **Nhill POS**: `F2` puts the cursor in the search / barcode field, `F9` opens payment;
  − / + buttons for quantities; layout adapted to narrow screens; closing requires the
  counted amount.

---

# Part A — Nhill Business

## 1. First start

The **Welcome** screen offers two choices.

### 1.1 "My business already exists" (join)

1. Enter the **establishment code** (`NH-XXXX-XXXX-XXXX`) given by the owner. The code
   changes automatically every 30 days: if it is refused, ask for the new one. The error
   message is deliberately the same in every case.
2. The code is not enough: either **"I have an account"** (your e-mail and password) or
   **"I received an invitation"** (the invitation e-mail, the 8-digit activation code given
   by your manager, and a password of at least 8 characters that you choose).

### 1.2 "Create my business"

1. **Type of activity**: *Sole trader* (one user, simplified screens) or *Company or
   organisation* (several users, roles, invitations).
2. **Identity**: trade name, legal name, sector, country, VAT registration, legal
   identifiers (RCCM, national ID, tax number… only those that apply to you).
3. **Contact details and preferences**: city, address, phone, e-mail, main currency,
   document language.
4. **Your account** (owner, password of at least 8 characters), then **review and create**.

The free trial (30 days by default) starts without any payment method. Write down the
**establishment code** shown at the end. If a business with the same legal identifier
already exists, Nhill never links you to it automatically: you can send a **link
request**, checked by the owner or by support.

## 2. Main screen

Side menu (drawer on phones, scrollable when the window is short): Dashboard, Sales,
Customers and suppliers, Products and services, Stock, Purchases, Expenses, Payments,
Reports, Activity log (or My activity), Settings. At the top: global search (`Ctrl+K`),
connection status (Online / Offline / Server problem), subscription status, notifications.

| Shortcut | Action |
|---|---|
| `Ctrl+K` | global search |
| `Ctrl+N` | new invoice (if your role allows it) |
| `Ctrl+1` … `Ctrl+4` | Dashboard, Sales, Customers, Products |
| `F5` | refresh the screen |
| `Alt+←` (Back key) | previous screen |
| `Tab` / `Shift+Tab` | next / previous field (also inside text areas) |

Light, dark or automatic theme and language (French/English): Settings › My account.

## 3. Sales

- **New invoice** (or quote, order): customer, currency, date, due date, lines (product
  or free line, quantity, price before tax, discount, tax). The total shown is an
  estimate; the server recalculates and its figure is authoritative.
- **Save draft**: editable, no number, "DRAFT" watermark on the PDF.
- **Issue**: the invoice gets its **final number** (e.g. `FA-2026-000012`), stock goes
  down, and it **can no longer be edited or deleted**. To correct it: **Credit note**
  (full or partial, with optional return to stock).
- **Foreign-currency document**: an exchange rate is asked for at issue; it is stored
  with its date and source and never recalculated.
- **Connection lost while issuing or recording a payment**: a banner offers **Retry**.
  The operation will never be recorded twice.
- **Quote**: Send → Accepted / Declined → Convert to invoice. **Order**: Confirm →
  Convert to invoice.
- **Payment**: from the invoice, "Record a payment" (cash, Mobile Money, card, transfer,
  cheque…, reference). The amount cannot exceed the balance due.
- **PDF**: PDF button; the file is saved in `Downloads/Nhill` and opened.
- **DRC, VAT-registered**: Nhill documents are **management documents**, not standardised
  invoices. Issue the standardised invoice with the electronic tax device required by
  the DGI, then use "Record DEF reference" on the Nhill invoice for reconciliation (see
  `docs/en/legal-parameters.md`).

## 4. Customers, products, stock, purchases, expenses

- **Customers and suppliers**: contact details, tax number, payment terms, internal
  notes; the record shows the balance and documents.
- **Products and services**: reference, name, unit, sale and purchase price, tax, alert
  threshold. CSV import/export (`;`): import is **all or nothing** (errors are listed per
  line and nothing is imported while any remain).
- **Stock**: quantities change **only through movements** (sale, purchase, credit note,
  adjustment with a mandatory reason, transfer between warehouses depending on the plan).
  Movements tab: full history with the author.
- **Purchases**: draft → **Receive goods** (stock in, purchase price updated) → payment.
- **Expenses**: amount including tax, deductible tax, category, supplier, paid or not,
  **receipt** (PDF/PNG/JPEG, 5 MB).

## 5. Reports

Sales journal (with "Issued by"), activity per employee, payments (with "Recorded by"),
expenses, sales per product (margin **estimated** at the current purchase price), stock
movements, receivables and payables to date. Any period, export to **CSV** or **PDF**.
Amounts are always shown **per currency**, never converted or added across currencies.

## 6. Who did what — activity log

- **Owner, administrator, accountant**: *Activity log* — every action in the business
  (sign-ins, documents, payments, stock, products, contacts, team, settings, till, Nhill
  support interventions), with date, author, object and details. Filters by
  **employee**, **action type** (including "Till" and "Nhill support" in 2.0) and
  **period**; **CSV/PDF** export. Clicking a line opens the document concerned.
- **"Activity per employee" report**: for each person and currency, documents created,
  invoices and credit notes issued, amounts collected, expenses entered, stock corrections
  and last activity.
- **Other roles**: *My activity* — only their own actions.
- The log is kept by the server and **can be neither edited nor erased**, not even by the
  owner. Tell your staff about it.

## 7. Settings

| Tab | Content | Who |
|---|---|---|
| Business | Identity, legal identifiers, **logo**, document language, payment terms, tax rounding, payment instructions, footer, negative stock | Owner, administrator |
| Establishment code | Show/hide, copy, **renew now** | Owner, administrator, manager (read) |
| Team | Members, roles, deactivation, **invitations** (activation code shown **only once**: hand it over in person, by phone or SMS), **password reset** of a member (§ 9) | Owner, administrator |
| Subscription | Plan, end date, plans by duration and currency, promo code, payment, history | Owner, administrator |
| Taxes and accounts | Tax rates (cannot be edited: create a new code), cash and bank accounts, **exchange rates** | Owner, administrator |
| Point of sale | Tills and cashiers, payment methods, maximum discount, receipt, till promo codes, sessions and X/Z reports | Owner, administrator; manager and accountant: sessions and reports |
| My account | Name, phone, language, theme, **password** (changing it signs out your other devices), **Remember me on this device**, sign out | Everyone |

Roles: **Owner**, **Administrator**, **Manager**, **Sales / cashier**, **Accountant**,
**Till supervisor**, **Cashier**, **Read only**.

## 8. Subscription (owner and administrators)

- Free trial (30 days by default, once per business).
- **Choose a plan**: duration and currency offered by Nhill, then **Choose this plan**.
  The summary computed by the server shows the price, any discount, the prorated credit
  and the total.
- **Upgrade**: immediate, unused part deducted. **Downgrade**: applied at the end of the
  period. **Renewal**: no day is lost.
- **Payment**: card or PayPal when Nhill has enabled them, otherwise bank transfer /
  Mobile Money with the reference shown, confirmed by the billing team. The subscription
  changes when the payment is confirmed, never before. An unpaid request expires after
  48 hours.
- After the grace period without payment: **read only** (viewing and export remain
  possible).

## 9. Help and support

**Help and support** menu: send a question or a bug report to the Nhill team and follow
the answers. Never include a password, an establishment code or a card number.

### Forgotten password

The Nhill team does not know your password and will never ask for it. Depending on your
role:

**You are an employee (sales, cashier, accountant, manager…).** Ask the administrator or
the owner of your establishment. In **Settings › Team**, they choose your name, then
**Reset the password**: Nhill creates a temporary password (12 characters, for example
`k7Rm-x2Qa-Hp9w`), shown **only once**, which they give you **in person or by phone**
(never by e-mail or in a group chat). Your open sessions are closed immediately. At your
next sign-in with this temporary password, Nhill asks you to choose a new one (at least
8 characters, different from the temporary one) before opening the application. The
temporary password expires after 72 hours: after that, ask for another one.

An administrator can only reset accounts of **their** establishment, never the owner's
nor their own; only the owner can reset an administrator. If your account is also used in
**another** business, none of its administrators can reset it: use the e-mail procedure
below.

**You are the owner or an administrator** (or your account is used in several
businesses). On the sign-in screen, choose **Forgot your password?**, enter your e-mail
address, then **Send the code**. You receive an 8-digit code (`1234-5678`), valid for
**20 minutes** and usable **once**; enter it with your new password. After 5 wrong codes
the code is cancelled: ask for a new one. A new request cancels the previous code. The
message shown is the same whether or not the address matches an account (so nobody can
find out who uses Nhill); if yours is an employee account, the e-mail you receive tells
you whom to contact. Once the password is changed, **all your sessions are closed** and an
e-mail informs you of the change.

Changing your password while signed in remains possible (Settings › My account).

## 10. Sign-in and security

- **Remember me**: tick it at sign-in, only on a device you do not share. The application
  then reopens signed in; the session token is kept in the system's secure storage
  (Windows Credential Manager, Android Keystore, Secret Service or KWallet keyring on
  Linux), never in plain text and never the password. **Signing out** erases this session;
  unticking the option (at sign-in or in Settings › My account) erases it too. If the
  system refuses to keep it (for example Linux without a keyring), the application says so
  and will ask you to sign in again.
- Lost or stolen device: change your password from another device (every other session is
  closed) or ask your administrator to reset your access.
- Offline: viewing and writing are suspended; nothing is lost.
- Several businesses with the same account: choose at sign-in.
- After several wrong sign-in attempts, the server slows further attempts down.

---

# Part B — Nhill Console (creators)

Separate application, reserved for the creators of Nhill. Every action is written to the
audit log. The console gives **no access to the invoices, customers or stock** of the
businesses.

## 1. Access

- **Install a new server** (once): see the installation guide § 4.
- **First access**: e-mail, **initialisation token** (valid 30 minutes), password of **at
  least 8 characters**, then add the key to an authenticator app (TOTP) and enter a
  6-digit code. Two-factor authentication remains **mandatory** for the console (it gives
  access to the whole platform); a long passphrase is recommended.
- **Sign-in**: e-mail + password + TOTP code (each code works once). The session is not
  remembered.
- Roles: **superadmin**, **billing** (plans, payments, reports), **support** (businesses,
  suspensions, codes, link requests, announcements), **auditor** (read only: log,
  statistics, provider events).

## 2. Screens

| Screen | Use |
|---|---|
| Overview | Businesses, subscriptions by state, active users, 30-day activity, confirmed revenue |
| Businesses | Search; record: identity, subscription, codes (without their value), statistics; **Suspend / Reactivate / Reissue code**, **Subscription…** — reason required |
| Reports | Global report per business and currency — CSV/PDF |
| Link requests | Check identity, accept/decline with a note |
| Notifications | Support, payments received or mismatched, technical errors |
| Support requests | Take, answer, internal note, change state |
| Transactions | Payment requests, reconciliation, **manual confirmation** of a transfer (reference + reason) |
| Plans and prices | Plans, limits and **prices per duration and currency** (no hard-coded price) |
| Promo codes | Percentage or amount, dates, uses, eligible plans |
| Payment providers | Stripe, PayPal, Payoneer (configuration only), generic; secrets **encrypted, never shown again**; test/production mode |
| Technical errors | Grouped errors, to mark as resolved |
| Audit log | Platform and security events |
| Announcements | Messages to businesses |
| Platform settings | Trial, grace, code rotation, limits; invite an administrator |

A business's **statistics** are aggregates computed by the database; no customer name,
product or document number is transmitted, and every view appears in the business's log.

---

# Part C — Nhill POS (till)

Linked to a Nhill Business establishment: same products, prices, taxes and stock. Each
cashier signs in with **their own account** (Cashier or Till supervisor role).

## 1. Prepare (Nhill Business → Settings → Point of sale)

1. **New till**: code printed on receipts (e.g. `C1`), name, location, warehouse.
2. **Who can open this till**: tick the cashiers.
3. **Payment methods**: Cash (automatic), Card, M-Pesa… Nhill records the amount; it does
   not drive card terminals or mobile operators.
4. **Rules and receipt**: maximum discount without approval, width (58/80 mm), message.
5. **Till promo codes**.

## 2. Open the till

Sign in, choose the till, **count the opening float**, **Open the session**.
"Remember me" and "Forgot your password?" work as in Nhill Business (Part A, § 9 and § 10);
on a till computer shared by several cashiers, leave "Remember me" unticked.

## 3. Sell

| Key / button | Action |
|---|---|
| `F2` | go to search / barcode |
| Enter (after a barcode) | add the product |
| − / + | decrease / increase a line's quantity |
| `%` | line discount |
| `F9` or **Pay** | open payment |

- One or several payment methods; for cash, enter the **amount tendered**: the change is
  shown. **Print the receipt** (roll-format PDF).
- Above the allowed discount, a supervisor enters **their own** e-mail and password; the
  approval works once and is logged.
- Network cut during validation: press Validate again; the sale will not be recorded
  twice.
- Narrow screen: the ticket moves below the product grid.

## 4. Receipts, returns, cancellations, cash

**Receipts and till**: list of the session's receipts ("Date / time" column), totals,
sales per payment method.

- On a receipt: **Print**, **Return quantities** or **Cancel the receipt** (same
  session). Reason required; supervisor approval if needed.
- **Cash in / out**; cash out cannot exceed the cash present.
- **X reading**: interim report without closing.

## 5. Close

**Close the till**: count the drawer (amount **required**) and enter the terminal totals.
The cashier does not see the expected amount; the server computes the difference. The
**Z report** can then be downloaded; a closed session can no longer be changed.
Differences are notified to supervisors.

Receipts are management documents; Nhill POS is not a certified till.
