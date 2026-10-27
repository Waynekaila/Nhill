# 18 — Record of processing activities (Publisher)

*Français (version de référence) : [../fr/18-registre-traitements.md](../fr/18-registre-traitements.md)*

> **TEMPLATE — INTERNAL DOCUMENT.** To be kept up to date by the data protection officer
> and shown to the authority on request. Formalities (declaration, authorisation) are given
> per processing.

| Header | |
|---|---|
| Jurisdiction | DRC — Digital Code, Book III; Decision No. 039/ARPTC/CLG/2025 (formalities, to reconfirm) |
| Assumptions | H-03, H-04, H-05, H-06 |
| Sources | S01, S02, S07 — see [00-legal-research.md](00-legal-research.md) § 7 |
| Clauses to validate | V-05, V-07, V-13 |
| Version | [NUMBER] of [DATE] |

Controller: [COMPANY NAME], [ADDRESS], [RCCM]. DPO: [NAME, CONTACT].

| No. | Processing | Nhill's role | Purpose | People | Data | Legal basis [TO VALIDATE] | Recipients | Hosting / transfer | Retention | Security | Formality (type, No., date) |
|---|---|---|---|---|---|---|---|---|---|---|---|
| T1 | Website forms | controller | answer requests | prospects | identity, contact, organisation, message | pre-contractual / consent | sales team | [COUNTRY] | [365] d | dedicated database, insert-only web role, HMAC of IP | [ ] |
| T2 | Prospecting | controller | commercial messages | consenting prospects | e-mail, phone, consent | consent | sales team | [ ] | until withdrawal | register (template 16) | [ ] |
| T3 | User accounts | controller | authentication, security | users | identity, e-mail, hashed password, sessions | contract | support | [COUNTRY] | account + [12] months | Argon2id, short tokens | [ ] |
| T4 | Subscription billing | controller | invoice, collect | customers' representatives | business identity, payments | legal obligation | accounting, payment provider | [ ] | 10 years | restricted access | [ ] |
| T5 | Support | controller | answer, fix | users | messages, masked errors | contract | support | [ ] | [24] months | automatic masking | [ ] |
| T6 | Platform audit log | controller | security, evidence | administrators, users | actions, IP | legitimate interest | security | [ ] | [12] months | append-only | [ ] |
| T7 | Businesses' data | **processor** | provide the Service | businesses' customers, suppliers, employees | see template 06 | the customer's | — | [COUNTRY] → **transfer outside the DRC if hosted abroad** | contract + [90] d | forced RLS, TLS, encrypted backups | transfer authorisation: [ ] |
| T8 | Operating statistics | controller | run the service | businesses (aggregates) | counts, volumes per currency | legitimate interest | management | [ ] | [24] months | aggregates without personal data | [ ] |
| T9 | Publisher's human resources | controller | staff management | employees | [to complete] | legal obligation | [ ] | [ ] | [ ] | [ ] | [ ] |

Change history:

| Date | Change | Author |
|---|---|---|
| [DATE] | creation | [NAME] |
