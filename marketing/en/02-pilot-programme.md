# Nhill pilot programme — Kinshasa

*Français : [../fr/02-programme-pilote.md](../fr/02-programme-pilote.md)*

Goal: confirm, with **8 to 12 real businesses**, that Nhill solves a problem they are
willing to pay for, and set prices from measurements, not assumptions. Agreement:
[`legal/en/09-assisted-trial-terms.md`](../../legal/en/09-assisted-trial-terms.md).
Public page: `website/src/pages/en/free-trial.html` ("Free assisted trial").

## 1. Offer to pilots (same as the website)

| We provide | We ask |
|---|---|
| Nhill Business (and Nhill POS if there is a till) free for 30 days | use Nhill for a real part of the business, every working day if possible |
| installation on the devices, catalogue import | a contact person, 20–30 minutes a week |
| initial training for each user | report problems, answer 2 short questionnaires |
| weekly check-in, support during working hours | an Internet connection on the devices |
| data export (CSV, PDF) at the end | — |

No price is promised during the pilot: the follow-up conditions are presented in writing
before the end.

## 2. Selection criteria

| Criterion | Required |
|---|---|
| Segment | A (retail), B (hardware, depot) first; 1 or 2 from C or D |
| Size | 1 to 20 users |
| Equipment | at least one Windows/Linux computer or an Android 7+ tablet; Internet |
| Tax | preferably not VAT-registered; a VAT payer only if they keep their DGI device |
| Commitment | named contact person, available for onboarding (half a day) |
| Diversity | at least 3 different Kinshasa communes |

Scoring grid: [`04-icp-qualification.md`](04-icp-qualification.md) § 3.

## 3. Schedule (30 days)

The first assisted trials serve as the pilot: same offer as the website, with before and
after measurements.

| When | Step | Output |
|---|---|---|
| D0 | demo with the business's products; trial terms accepted (`legal/en/09-assisted-trial-terms.md`); baseline measurement (see § 4) | acceptance by e-mail, baseline sheet |
| D0 | onboarding: installation, catalogue import, accounts, training | business running |
| week 1 | close follow-up: 2 ten-minute calls, on-site visit if needed | problem log |
| week 2 | 20-minute check-in; **mid-trial questionnaire** ([`08-sales-process-and-tracking.md`](08-sales-process-and-tracking.md) § 4) | answers |
| week 3 | 20-minute check-in; fixes delivered | — |
| D30 | **review**: final measurements, end questionnaire, written follow-up proposal | trial report |

## 4. Measurements (before / after)

| Measure | How | Who |
|---|---|---|
| Daily till-closing time | stopwatch, 3 days before the trial, 3 days in the last week | contact person |
| Average till difference | pilot's records before; Nhill Z reports during | Nhill |
| Stock differences at stock-take | count of 20 items on D0 and D30 | contact person + Nhill |
| Share of sales entered in Nhill | Nhill sales / total sales reported by the pilot | contact person |
| Usage | days with at least one sale, active users (aggregated console statistics, visible in the pilot's log) | Nhill |
| Satisfaction | 0–10 "would you recommend Nhill?" + comments (with consent) | questionnaire |
| Willingness to pay | open end-of-pilot questions | questionnaire |

No result is published without the pilot's written consent, and only as measured.

## 5. Programme success criteria

| Indicator | Decision threshold |
|---|---|
| Pilots active in the last week (at least 4 days of use a week) | ≥ 60 % of pilots |
| Average recommendation score | ≥ 7 / 10 |
| Pilots ready to pay the proposed price | ≥ 40 % |
| Blocking incidents unresolved within 48 h | 0 |

If thresholds are not met: do not launch wide sales; fix, then a second wave of pilots.

## 6. Organisation

| Role | Workload |
|---|---|
| Pilot lead (1 person) | recruitment, trial terms, weekly check-ins, report |
| Installation technician (can be an integrator partner) | onboarding, on-site support |
| Development | fixes, follow-up of reported technical errors |

Capacity: one lead follows at most 6 pilots at once; beyond that, two waves.

## 7. Risks

| Risk | Mitigation |
|---|---|
| Power / Internet cuts at the pilot | connection test at selection; paper fallback; resume without double entry |
| Drop-out after 2 weeks | committed contact person, first-week calls, goals chosen by the pilot |
| VAT payer believing Nhill is certified | clear message from the demo and in the trial terms |
| Wrong imported data | all-or-nothing import with error list; four-eyes check |
| Version not built for a platform | check the available deliverables (`release/V3.0.0/checksums/MISSING.txt`) before promising Windows or Android |
