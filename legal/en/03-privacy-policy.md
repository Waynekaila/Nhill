# 03 — Privacy policy

*Français (version de référence) : [../fr/03-politique-confidentialite.md](../fr/03-politique-confidentialite.md)*

> **TEMPLATE — DO NOT PUBLISH AS IS.** To be completed and validated (lawyer, data
> protection officer). The version published on the website
> (`website/src/pages/en/privacy.html`) must be aligned with this text once validated.

| Header | |
|---|---|
| Jurisdiction | DRC — Digital Code, Book III |
| Assumptions | H-01, H-03, H-04, H-05, H-08, H-13, H-15 |
| Sources | S01, S02, S03, S04, S07, S16 — see [00-legal-research.md](00-legal-research.md) § 7 |
| Clauses to validate | V-05, V-07, V-08, V-13 |
| Version | [NUMBER] of [DATE] |

## 1. Who is responsible

[COMPANY NAME], [LEGAL FORM], RCCM [RCCM], registered office [ADDRESS] ("Nhill", "we").
Data protection contact: info@nhill.online. Data protection officer: [NAME or "not appointed —
reason"] **[TO VALIDATE: V-13]**.

This policy covers: website visitors, people who fill in a form, customers and their users
(accounts), people who write to support.

It does not cover the data each business records in Nhill about its own customers,
suppliers and employees: the business is responsible for it and Nhill processes it on the
business's behalf (data processing agreement, template 06).

## 2. Data, purposes, legal bases, retention

| Processing | Data | Purpose | Legal basis **[TO VALIDATE]** | Retention |
|---|---|---|---|---|
| Website forms (demo, pilot, partnership, question) | name, organisation, e-mail, phone, city, sector, message, consent choices | answer the request | pre-contractual steps / consent | [12] months after the last exchange (setting `SITE_RETENTION_DAYS`) |
| Protecting the website against abuse | non-reversible HMAC fingerprint of the IP address, browser type | security, rate limiting | legitimate interest | [30] days for the fingerprint |
| News and offers (if box ticked) | e-mail, name, date and wording of consent | prospecting | **prior consent** (Digital Code art. 68–72) | until withdrawal, then [3] years as proof of withdrawal |
| User accounts | name, e-mail, phone, language, role, hashed password (Argon2id), sessions | provide the Service, security | performance of the contract | account lifetime + [12] months |
| Security and audit log | actions, date, IP address, device | security, evidence, fraud prevention | legal obligation / legitimate interest | [12] months **[TO VALIDATE: V-07]** |
| Subscription billing | business identity, legal identifiers, payments | invoicing, accounting | legal obligation | 10 years (accounting records, OHADA) |
| Support | messages, attachments, masked technical errors | answer, fix | performance of the contract | [24] months after closing |
| Operating statistics | aggregates per business (counts, volumes per currency, active users) | run and improve the Service | legitimate interest | [24] months |

Nhill does not sell personal data and does no advertising profiling. The website sets **no
cookie** and uses no third-party analytics (template 04).

## 3. Recipients

- authorised Nhill staff, according to their role (support, billing);
- technical processors: hosting [HOST, COUNTRY], e-mail delivery [PROVIDER or "none"],
  payment providers [STRIPE / PAYPAL / OTHER, COUNTRY] for payments only;
- authorities, upon lawful request.

## 4. Transfers outside the DRC

Data is hosted [in the DRC / in [COUNTRY] by [HOST]]. Hosting outside the DRC is an
international transfer regulated by the Digital Code; Nhill carries it out after
[authorisation No. [No.] of [DATE] from the data-protection authority] and with
contractual safeguards. **[TO VALIDATE: V-05, H-04, H-05.]**

## 5. Security

Encrypted communications (HTTPS), hashed passwords, isolation of each business's data at
database level, role-based access control, audit log, encrypted backups. Summary:
template 13. If a breach presents a risk, Nhill informs the authority and the people
concerned within the legal deadlines (template 14).

## 6. Your rights

You can ask for access to your data, its rectification or erasure, object to processing
based on legitimate interest, and withdraw your marketing consent at any time (unsubscribe
link or message to info@nhill.online). Form: template 19. Answer within [30] days. Proof of
identity may be requested if there is doubt.

You can contact the data-protection authority: [NAME AND CONTACT OF THE COMPETENT AUTHORITY
— ARPTC on a provisional basis or ARPTIC, to be checked] **[TO VALIDATE: H-05]**.

## 7. Businesses' employees

Nhill's activity log records each user's actions in the application. The employing
business is responsible for this processing and must inform its staff (template 17).

## 8. Children

The Service is intended for businesses; it is not designed for children.

## 9. Changes

Any substantial change is announced on the website and in the application [30] days before
it takes effect. Version history: [LINK].
