# 15 — Data retention and deletion policy

*Français (version de référence) : [../fr/15-politique-conservation.md](../fr/15-politique-conservation.md)*

| Header | |
|---|---|
| Publisher | Nhill Solution, Kinshasa (Democratic Republic of the Congo) |
| Version | 3.0.0 — 28 October 2026 |
| Status | written without review by a lawyer; recommended review point: V-07 ([00](00-legal-research.md) § 5). The periods are those published in the privacy policy and applied by the settings shown. The French version is the reference text |
| Assumptions | H-03, H-14, H-19 ([00](00-legal-research.md) § 2) |

## 1. Principle

Data is kept only as long as its purpose requires, then deleted. Deletion also covers
backups, from which data disappears as they rotate.

## 2. Periods

| Data | Period | Implementation |
|---|---|---|
| Requests received through the website | 12 months after sending | `SITE_RETENTION_DAYS=365`, daily purge (`nhill-site-purge.timer`) |
| IP address fingerprint of the forms | 30 days | `SITE_IP_RETENTION_DAYS=30`, same purge |
| Website anti-abuse counters | 2 days | same purge |
| Agreement to receive news | until withdrawal, at the latest until the request is deleted | `python3 -m nhill_site.cli unsubscribe --email …`; register (document 16) for agreements collected outside the website |
| A business's data (accounts, customers, products, documents, stock, payments, activity log, support requests) | term of the contract, then 90 days of read-only export | permanent deletion from the administration console, possible only after these 90 days (article 3) |
| The business's accounting records | 10 years, **the business's responsibility** | CSV and PDF export, full data archive |
| Subscription invoices issued by Nhill Solution | 10 years | Nhill Solution's accounts |
| Platform administration log (team actions) | as long as the platform is operated; entries linked to a business deleted with it | append-only log |
| Encrypted database backups | 14 days on the server | `NH_BACKUP_KEEP_DAYS=14` (`/etc/nhill/backup.env`) |
| Copies of the server by the host | according to the host's cycle (2 weekly copies by default) | Hostinger panel |
| Off-site copy of the backups (if set up) | period chosen when it is set up, recorded in the register (document 18) | `NH_BACKUP_OFFSITE_CMD` |
| Technical server logs | 90 days at most | journald `MaxRetentionSec=90day`; Caddy logs `roll_keep_for 2160h` |

## 3. Deletion at the end of the contract

1. End of the subscription or the trial, then any grace period → the business switches to
   read-only; the account holder is invited to export the data.
2. 90 days after the end of full access → the team permanently deletes the business from
   the administration console; the deletion (date, author) is recorded in the
   administration log.
3. 14 days later → the data has disappeared from the backups kept on the server; it
   disappears from the host's copies and the off-site copy as they rotate.
4. On request, Nhill Solution confirms the deletion in writing.

## 4. Erasure requests

Handled according to document 19, within one month, unless the law requires keeping the
data (in that case: limited retention, restricted access, the person is informed).

## 5. Review

This policy is reviewed once a year and with every new processing (register, document 18).
