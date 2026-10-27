# Sales process, prospect tracking and questionnaires

*Français : [../fr/08-suivi-et-processus.md](../fr/08-suivi-et-processus.md)*

## 1. Process stages (conversion)

The `statut` values are kept in French in the tracker so that both teams share one file.

| No. | Status (`statut` column) | Entry | Exit | Target time |
|---|---|---|---|---|
| 1 | `identifié` (identified) | business spotted (field, referral, form) | first contact made | — |
| 2 | `contacté` (contacted) | visit, call or message (with consent) | demo booked, or refusal | 7 days |
| 3 | `démo fixée` (demo booked) | date confirmed | demo done | 7 days |
| 4 | `démo faite` (demo done) | report filled in, score computed | pilot offered or follow-up | 2 days |
| 5 | `pilote proposé` (pilot offered) | agreement sent | signed, or refusal | 7 days |
| 6 | `pilote en cours` (pilot running) | onboarding done | W8 review | 8 weeks |
| 7 | `proposition` (proposal) | written follow-up proposal sent | accepts / declines | 14 days |
| 8 | `client` (customer) | first payment confirmed | — | — |
| — | `perdu` (lost) | refusal or no answer after 2 follow-ups | reason noted (`motif_perte`) | — |
| — | `plus tard` (later) | interest but not now | dated follow-up | — |

Conversion rates to track (KPIs `10`): contacted → demo, demo → pilot, pilot → customer.

## 2. Prospect tracker

`marketing/trackers/suivi-prospects.csv` (UTF-8, `;` separator, opens in a spreadsheet; an
English-header copy is `prospect-tracker.csv`) and the "Prospects" sheet of
`trackers/kpi-tableau-de-bord.xlsx`.

| Column | Content |
|---|---|
| `id` | P-0001… |
| `date_entree` | entry date, YYYY-MM-DD |
| `etablissement`, `secteur`, `commune` | business, sector, commune |
| `contact_nom`, `contact_role` | contact name and role |
| `telephone`, `email` | only if given by the person |
| `consentement_canal`, `consentement_date` | e.g. `WhatsApp`, `2026-11-03` (empty = no commercial message) |
| `source` | field / referral / website / partner:[ID] / event |
| `score` | grid `04` § 3 (/20) |
| `statut` | see § 1 |
| `derniere_action`, `prochaine_action`, `date_prochaine_action` | last action, next action, date |
| `motif_perte` | price / no device / VAT / not now / other |
| `notes` | no sensitive data |

The file holds personal data: access limited to the sales team, retention according to
`legal/en/15-retention-policy.md`, never shared publicly.

## 3. Weekly routine (Monday, 30 minutes)

1. Update statuses; delete the rows of people who asked for it.
2. List the week's actions (`date_prochaine_action`).
3. Compute the KPIs (`10`), compare with the plan's goal (`03`).
4. One decision: what to stop, what to reinforce.

## 4. Satisfaction questionnaire — mid-pilot (W4, 5 minutes)

1. How many days a week do you use Nhill? (0–1 / 2–3 / 4–6)
2. What helps you most? (till, stock, invoices, reports, activity log, other)
3. What bothers you most? (free answer)
4. Did you have a blocking problem? Was it solved? How fast?
5. Was the training enough for each user? (yes / partly / no)
6. From 0 to 10: would you recommend Nhill to another trader?
7. One thing to change first?

## 5. End-of-pilot questionnaire (W8, 10 minutes)

1. Questions 1, 2, 3 and 6 of the mid-pilot questionnaire again.
2. Compared with the start, closing the till takes: more / the same / less time. (Complete
   with the timed measurement.)
3. Till or stock differences have: increased / stayed stable / decreased / I don't know.
4. What do you spend today to manage your sales (notebooks, spreadsheet, software, a
   dedicated person)? (free answer, amount optional)
5. At what monthly price would Nhill seem: too expensive to consider / expensive but
   possible / a good deal / so low you'd doubt the quality? (free amounts, in CDF or USD)
6. Do you want to continue after the pilot? (yes / yes, if: … / no: why)
7. Do you agree that we quote your feedback? (no / yes, anonymously / yes, with name —
   separate consent form to sign, agreement article 7)

## 6. Follow-up and referral templates

See `06-message-templates.md` § 3 (follow-up), § 7 (referral), § 9 (end of pilot).
