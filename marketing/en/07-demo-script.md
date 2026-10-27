# Demo script (15 to 30 minutes)

*Français : [../fr/07-script-demonstration.md](../fr/07-script-demonstration.md)*

Equipment: computer or tablet with Nhill Business and Nhill POS installed, connected to a
**demo business** (fictitious data, all marked "(démo)", never a real customer's data).
Before the visit: add 10 to 20 of the prospect's items (names and prices they gave), check
the connection, charge the device.

Only show what exists in the installed version. Never enter a third party's real personal
data during the demo.

## Flow

| Min. | Step | What you do | What you say |
|---|---|---|---|
| 0–2 | Restate the pain | recall what the prospect said (till, stock, sellers) | "You told me the evening till takes you an hour. Let's see what it looks like with Nhill." |
| 2–6 | A sale at the till | Nhill POS: open the till with a float, scan or tap 3 of the prospect's items, `F9`, cash payment with change, PDF receipt | "Stock has already gone down, and the sale is under the cashier's name." |
| 6–9 | Control | try a discount above the limit → supervisor approval request; closing: the cashier enters the count without seeing the expected amount; Z report with the difference | "The cashier can't adjust the count to make it match." |
| 9–12 | Stock | Nhill Business → Stock: quantities, alert threshold, movements with author; a received purchase | "You know what will run out before it does." |
| 12–14 | Owner away | on a second device (or phone): dashboard, "activity per employee" report, activity log | "From home, you see what happened in the shop." |
| 14–15 | Outage | explain (without faking an outage): an interrupted operation is retried without duplicates; backup notebook for a long outage | "Nhill needs the Internet to record; it never counts twice." |
| 15–20 | Questions | answer; note objections | see standard answers in `05` § 1 |
| 20–25 | Option: invoices and credit | invoice to a customer, partial payment, balance due | "You know who owes you what." |
| 25–30 | Close | propose the pilot, dates, agreement | "We can install on [day]. It takes half a day and the list of your items." |

## Watch-outs

- **VAT**: if the prospect is VAT-registered, say it before the end: Nhill does not produce
  the DGI standardised invoice.
- **Price**: not yet set; free pilot; prices published before any invoicing.
- **Platforms**: check what is actually delivered (Windows, Linux, Android) in
  `release/V2.0.0/` before mentioning it.
- **What Nhill 2.0 does not do**: sending e-mails, resetting a forgotten password, built-in
  Mobile Money payment of sales, general accounting, payroll, working without Internet.

## Demo report (fill in the same day)

| Field | Answer |
|---|---|
| Date, place, duration | |
| Business, sector, commune | |
| People present (role) | |
| Confirmed pains | |
| Positive reactions (exact quotes) | |
| Objections and answers given | |
| Unanswered questions → to whom, by when | |
| Equipment observed (devices, Internet, power) | |
| Qualification score (`04` § 3) | /20 |
| Decision | pilot / follow up on [DATE] / not interested |
| Consent to messages (channel, date) | |
| Next action and date | |

Copy into `trackers/suivi-prospects.csv` ("score", "statut", "prochaine_action" columns).
