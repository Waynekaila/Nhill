# 18 — Record of processing activities (Nhill Solution)

*Français (version de référence) : [../fr/18-registre-traitements.md](../fr/18-registre-traitements.md)*

| Header | |
|---|---|
| Publisher | Nhill Solution, Kinshasa (Democratic Republic of the Congo) — contact: info@nhill.online |
| Version | 3.0.0 — 28 October 2026 |
| Status | internal document, kept up to date by management and shown to the authority on request; written without review by a lawyer; recommended review points: V-05, V-07, V-13 ([00](00-legal-research.md) § 5). The French version is the reference text |
| Assumptions | H-03, H-04, H-05, H-06 ([00](00-legal-research.md) § 2) |
| Formalities | declarations or authorisations with the data protection authority: to be determined with the lawyer (checklist, [00](00-legal-research.md) § 6); each formality completed is added to the history below with its reference |

| No. | Processing | Role | Purpose | People | Data | Basis | Recipients | Location | Period | Security |
|---|---|---|---|---|---|---|---|---|---|---|
| T1 | Website forms | controller | answer requests | prospects | identity, contact, organisation, message, consent choices | pre-contractual steps | sales team | website server | 12 months after sending | dedicated database, web role limited to inserting, IP fingerprint (HMAC) erased after 30 days |
| T2 | News from Nhill | controller | commercial messages | people who agreed | e-mail, phone, agreement | consent | sales team, sending provider | website server | until withdrawal | register (document 16), unsubscribe |
| T3 | User accounts | controller | authentication, security | users | identity, e-mail, password fingerprint, sessions | contract | support | France (Hostinger) | term of the business's contract + 90 days | Argon2id, short tokens, device keychain |
| T4 | Subscriptions and payments | controller | manage the subscription, invoice | account holders and representatives of businesses | business, plan, payments | contract; accounting obligations | management, payment provider (for that payment only) | France (Hostinger) | term of the contract; accounting records 10 years | restricted access, verified payment notifications |
| T5 | Support | controller | answer, fix | users | messages, technical details without password | contract | support | France (Hostinger) | term of the business's contract + 90 days | role-based access |
| T6 | Platform administration log | controller | security, proof | platform administrators | actions, date, IP address | legitimate interest | management | France (Hostinger) | as long as operated; entries linked to a business deleted with it | append-only, administrators' two-factor authentication |
| T7 | Businesses' data | **processor** | provide the Service | customers, suppliers, employees of businesses | see document 06 | the business's | — | France (Hostinger): **transfer outside the DRC** | term of the contract + 90 days | row-level security enforced by the database, HTTPS, encrypted backups |
| T8 | Operating statistics | controller | operate the Service | businesses (overall figures) | counts, volumes per currency | legitimate interest | management, support | France (Hostinger) | calculated on request, not stored | no personal data; consultation recorded in the business's log |
| T9 | Technical server logs | controller | security, diagnosis | users, visitors | IP address, date, request | legitimate interest | operations | France (Hostinger) | 90 days at most | server administrator access only |

History of changes and formalities:

| Date | Change or formality (reference) | Author |
|---|---|---|
| 28 October 2026 | version 3.0.0: Hostinger hosting in France, periods aligned with the privacy policy | Nhill Solution |
