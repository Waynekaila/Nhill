# Partner model

*Français : [../fr/09-modele-partenaires.md](../fr/09-modele-partenaires.md)*

Standard agreement: [`legal/en/10-partnership-agreement.md`](../../legal/en/10-partnership-agreement.md).
Public page: `website/src/pages/en/partners.html`.

## 1. Partner types

| Type | Who | What they do | Remuneration | What Nhill provides |
|---|---|---|---|---|
| Referrer | influential trader, independent accountant, field agent | presents Nhill, records the prospect before any contact by Nhill | commission on amounts actually collected; rate and duration set in the letter of agreement | partner sheet, form link, statement for each commission payment |
| Integrator | IT service provider, hardware reseller (receipt printers, tablets) | installs, imports the catalogue, first-level support | invoices its services to the customer; possible commission | 2-hour technical training, documentation, demo access |
| Trainer | business school, training centre, independent trainer | trains users | invoices its training; Nhill materials free | training materials, demo business |
| Support organisation | traders' association, incubator, SME support body, federation | makes Nhill known, hosts workshops | no commission unless stated in the letter of agreement; reserved places in the assisted free trial, free workshop for members | turnkey workshop, reserved places |
| Reference user | satisfied customer business | welcomes other traders, **only if they wish** | none, unless a consideration is written in the letter of agreement and mentioned next to any published testimonial ([`legal/en/20`](../../legal/en/20-testimonial-image-consent.md)) | — |

## 2. Rules

- No partner is announced publicly without a signed contract and agreement on the use of the
  name.
- A partner only promises what Nhill does (list "what Nhill does not do" in
  `07-demo-script.md`) and never presents Nhill as DGI-certified.
- A partner never asks a trader for a password; they create their own access if the business
  invites them.
- Contacts passed on have the people's consent (`legal/en/16`).

## 3. Partner journey

| Step | Timing |
|---|---|
| First conversation (30 min): network, motivation, partnership type | — |
| Demo and training (2 h) | within 1 week |
| Contract signed | within 2 weeks |
| First prospect recorded | within 3 weeks |
| Monthly review: prospects, conversions, feedback, commission statement | every month |

## 4. Tracking

`marketing/trackers/suivi-partenaires.csv` (English-header copy: `partner-tracker.csv`):
`id`, `nom` (name), `type`, `commune`, `date_contrat` (contract date),
`prospects_enregistres` (prospects recorded), `demos`, `pilotes` (pilots), `clients`
(customers), `encaisse_usd` (collected), `commission_due_usd`, `commission_payee_usd`
(paid), `derniere_rencontre` (last meeting), `notes`.

## 5. Partner KPIs

| Indicator | 90-day target |
|---|---|
| Partners signed | 3 |
| Share of pilots coming from partners | ≥ 30 % |
| Average commission payment time | ≤ 10 days after collection |
