# Nhill 3.0 — User guide

*Français : [../fr/guide-utilisateur.md](../fr/guide-utilisateur.md)*

Since version 3.0, each application has its own guide:

| Application | For whom | Platforms | Guide |
|---|---|---|---|
| **Nhill Business** | the business: sales, customers, stock, purchases, expenses, reports, team, settings | Windows, Linux, Android | [nhill-business.md](nhill-business.md) |
| **Nhill POS** | a business's cashiers | Windows, Linux | [nhill-pos.md](nhill-pos.md) |
| **Nhill Console** | the platform operator (private application) | Windows, Linux | [nhill-console.md](nhill-console.md) |

Real screenshots of version 3.0 (demo business, French interface): `docs/assets/v3/`.

## 1. What is new in version 3.0

- **Plans**: each business only sees the features included in its plan; a feature outside
  the plan appears nowhere and the server refuses it. Limits (users, tills, warehouses,
  products, documents per month, storage) and their usage are shown in Settings ›
  Subscription.
- **Server address**: entered and checked at the first launch of each application,
  changeable afterwards (no address is built into the applications any more).
- **Till invoices**: Nhill POS can issue an A4 invoice in the name of an identified
  customer, with the same numbering and template as Nhill Business. All documents (A4 or
  58/80 mm receipt) use a single template.
- **Notices on documents**: the business writes the notices printed on every document
  (Settings › Establishment).
- **Full export**: the owner downloads a ZIP archive of all the data, at any time, even in
  read-only mode.
- **Android**: **Share** button on a document.
- **End of contract**: 90 days of read-only to export, then permanent erasure by the
  operator.
- **Searches**: `%` and `_` are searched as typed.

Previous versions: [changelog.md](changelog.md).

## 2. Common rules

- Passwords: at least 8 characters, not a too-common password; no periodic change imposed.
  The eye button shows or hides what you type.
- "Remember me" keeps a session token in the system's secure storage, never the password.
- An operation interrupted by a cut is retried without being recorded twice.
- Language (French, English) and theme (light, dark, automatic): Settings › My account.

## 3. What Nhill does and does not do

| Topic | What Nhill does | What it does not do |
|---|---|---|
| Tax obligations | tax rates, legal identifiers and notices on every document; `CD` profile (DRC, to be validated by a tax adviser) and generic `INTL` profile; reference of an official document (for example the DGI's DEF) recorded on the invoice | **certified in no country**: no certified till, no regulatory e-invoice, no fiscal device |
| Accounting | journals and CSV/PDF exports (sales, payments, expenses, receivables, payables) | does not keep the general ledger |
| Payments | cash, card, transfer, cheque, Mobile Money recorded with their reference | not connected to Mobile Money operators or card terminals |
| Sending documents | PDF to save or share (Share button on Android) | no direct sending by e-mail or SMS from the application |
| Connection | a mobile connection is enough; an interrupted operation is resent without duplicates | without a connection, you can neither sell nor invoice |
| Devices | Nhill Business: Windows, Linux, Android; Nhill POS: Windows, Linux | no till on Android or iOS |
