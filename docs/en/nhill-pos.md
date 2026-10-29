# Nhill POS 3.0 — Till guide

*Français : [../fr/nhill-pos.md](../fr/nhill-pos.md)*

Nhill POS is the till of a Nhill business: same products, prices, taxes, stock and
numbering as Nhill Business. Platforms: Windows 10 and 11 (64-bit), Linux (Ubuntu 24.04,
Debian 13); a USB barcode reader can be used (it behaves like a keyboard). No till on
Android or iOS. The till is available only if the business's plan includes it.

Each cashier signs in with **their own account**, created in Nhill Business (Cashier or
Till supervisor role; owner, administrator and manager can also sell).

## 1. Preparing the till (Nhill Business › Settings › Point of sale)

1. **New till**: code printed on receipts (for example `C1`), name, location, warehouse
   whose stock is used. The number of tills is limited by the plan.
2. **Authorised cashiers**: tick the members who can open this till.
3. **Payment methods**: Cash (always present), card, Mobile Money, etc. Nhill records the
   amount and reference; it drives neither card terminals nor operators.
4. **Rules and receipt**: maximum discount without approval, whether prices can be
   changed, receipt width (58 or 80 mm), receipt footer message.
5. **Till promo codes**: percentage, minimum amount, validity, number of uses.
6. **Sessions and reports**: list of sessions, X readings and Z reports.

## 2. Signing in and opening the session

1. First launch: **Connect this till to your Nhill server** — enter the server address given
   by your provider (HTTPS), checked before it is saved on the machine. **Change server**
   remains possible; the till's current session then ends.
2. Sign in (e-mail, password). "Forgot your password?" and "Remember me" work as in Nhill
   Business ([nhill-business.md](nhill-business.md) § 8); on a machine shared by several
   cashiers, leave "Remember me" unticked.
3. **Choose your register** (a cashier only sees the tills assigned to them; a supervisor —
   till supervisor, manager, owner, administrator — sees them all), count the **opening
   float**, then **Open the cash session**.

## 3. Selling

| Key or button | Action |
|---|---|
| `F2` | back to search / barcode |
| Enter (after a barcode or search) | add the product |
| − / + | decrease / increase a line's quantity |
| Discount | percentage discount on the line |
| `F9` or **Pay** | open payment |

- The stock of the till's warehouse is shown on each product; an item out of stock is
  flagged.
- **Customer**: walk-in customer by default, or a customer searched by name or phone.
- **Promo code**: checked and applied by the server at payment.
- **Payment**: one or several payment methods; for cash, enter the **amount tendered**: the
  **change to give back** is shown. **Validate**.
- **Invoice instead of a receipt**: for an identified customer, tick "Issue an invoice for
  … instead of a receipt" (if the plan includes invoices). The invoice has the same type,
  numbering and template as in Nhill Business; **Print the invoice (A4)**. Otherwise
  **Print the receipt** (roll-format PDF).
- **Above the allowed discount** (or for a changed price, a return, a void, a cash
  withdrawal, closing another cashier's session without the permission): a supervisor types
  **their own** e-mail and password; the approval is used once and recorded in the log with
  their name.
- **Network cut while validating**: press **Validate** again; the sale is not recorded
  twice.

## 4. Tickets & cash

**Tickets & cash** screen: sales, returns and voids, net sales, discounts, expected cash
(supervisor view), cash movements, list of the session's receipts and invoices (date and
time, number, type, customer, total, status).

- On a receipt: **Print**, **Return the quantities** (enter the quantity to return on each
  line concerned; a return creates a credit note, refunds in cash and puts the goods back
  in stock) or **Void the ticket** (whole receipt, same session). Reason required.
- **Cash in / out** (change, deposit, bank deposit, expense) with a reason; a withdrawal
  cannot exceed the cash present.
- **X reading**: interim report without closing.

## 5. Closing

**Close the register**: count the drawer (amount **required**, 0 if empty) and enter the
total of each terminal or statement. The cashier counts without seeing the expected amount;
the server computes the difference, reviewed afterwards by a supervisor. The **Z report**
can be downloaded after closing; a closed session can no longer be changed.

Receipts are management documents: Nhill POS is neither a certified till nor an approved
fiscal invoicing device.
