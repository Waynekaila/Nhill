# 15 — Data retention and deletion policy

*Français (version de référence) : [../fr/15-politique-conservation.md](../fr/15-politique-conservation.md)*

> **TEMPLATE — TO VALIDATE.** Periods in brackets are proposals; the matching technical
> settings are given where they exist.

| Header | |
|---|---|
| Jurisdiction | DRC — Digital Code (limited retention); OHADA (accounting records: 10 years) |
| Assumptions | H-03, H-14 |
| Sources | S01, S15 — see [00-legal-research.md](00-legal-research.md) § 7 |
| Clauses to validate | V-07 |
| Version | [NUMBER] of [DATE] |

## 1. Principle

Data is kept only as long as its purpose requires, then deleted or anonymised. Deletion
also covers backups, which expire on their own cycle.

## 2. Periods

| Data | Period | Implementation |
|---|---|---|
| Website submissions (not followed up) | [365] days | `SITE_RETENTION_DAYS`, daily purge (`nhill-site-purge.timer`) |
| IP fingerprint of form submissions | [30] days | `SITE_IP_RETENTION_DAYS` |
| Website anti-abuse counters | 2 days | daily purge |
| Marketing consent | until withdrawal; proof of withdrawal [3] years | template 16; `cli unsubscribe` |
| A business's data (customers, sales, stock…) | contract term + [90] days of reversibility | export by the customer; deletion by operations |
| The business's accounting records | 10 years **at the business's charge** (it exports its data) | CSV/PDF export |
| Subscription invoices issued by the Publisher | 10 years | Publisher's accounting |
| Inactive user accounts | [24] months without sign-in → warning then deletion [TO DECIDE] | manual procedure |
| Audit and security log | at least [12] months, at most [5] years **[TO VALIDATE: V-07]** | — |
| Support requests | [24] months after closing | — |
| Database backups | rotation: [7] daily, [4] weekly, [12] monthly | `deploy/backup/` |
| Server technical logs | [30] days | journald configuration |

## 3. Deletion at the end of the contract

1. End of subscription → business read-only; the owner is invited to export its data.
2. D+[90] → deletion of the business's data from the production database; deletion
   recorded (date, operator) in the register.
3. D+[90] + backup rotation period → the data has disappeared from backups.
4. On request, written confirmation of deletion.

## 4. Erasure requests

Handled according to template 19, within [30] days, unless a legal retention obligation
applies (then: limited retention, restricted access, and the person is informed).

## 5. Review

This policy is reviewed once a year and with every new processing (register, template 18).
