# Key performance indicators (KPIs) and dashboard

*Français : [../fr/10-indicateurs.md](../fr/10-indicateurs.md)*

Tracking file: `marketing/trackers/kpi-tableau-de-bord.xlsx` (sheets "Tableau de bord"
(dashboard), "Hebdo" (weekly), "Prospects", "Partenaires" (partners), "Budget", "Taux"
(rates)) — formulas included, to fill in every Monday. Targets are those of the 90-day plan
(`03`).

## 1. Indicators

| Family | Indicator | Exact definition | Source | 90-day target |
|---|---|---|---|---|
| Acquisition | Prospects identified | rows added to the prospect tracker | tracker | 150 |
| | Contacts made | status ≥ `contacté` | tracker | 90 |
| | Contact → demo rate | demos done / contacts made | tracker | ≥ 25 % |
| | Cost per demo | marketing spend / demos done | budget + tracker | to measure |
| Activation | Demos done | status ≥ `démo faite` | tracker | 25 |
| | Demo → pilot rate | pilots signed / demos done | tracker | ≥ 40 % |
| | Active pilots | pilots with ≥ 4 days of use last week | console (aggregates) | 10 |
| | Time to onboard | days between signature and first sale in Nhill | tracker | ≤ 3 d |
| Satisfaction | Average recommendation score | mean of 0–10 answers | questionnaires | ≥ 7 |
| | Blocking incidents > 48 h | count | support | 0 |
| Revenue | Pilot → customer rate | customers / completed pilots | tracker | ≥ 40 % |
| | Monthly recurring revenue (MRR) | sum of active subscriptions brought to a month, **per currency** | console (transactions) | to set with prices |
| | Collected | confirmed payments, per currency | console | — |
| Partners | Partners signed | contracts signed | partner tracker | 3 |
| | Share of pilots via partners | pilots with `source=partner` / pilots | tracker | ≥ 30 % |
| Budget | Cumulative spend vs envelope | spend / scenario envelope | budget | ≤ 100 % |

Rules: amounts are never added across USD and CDF (as in Nhill); a conversion for comparison
uses the dated BCC rate of the "Taux" sheet.

## 2. Weekly reading

| Signal | Likely meaning | Action |
|---|---|---|
| Many contacts, few demos | message or target | review script `05`, refocus target `04` |
| Demos without pilots | demo or doubts (price, VAT, devices) | review `07`, list objections |
| Inactive pilots | onboarding or training | support call, on-site visit |
| Satisfaction < 7 | product | escalate problems to development first |
