# 07 — Service level agreement (SLA)

*Français (version de référence) : [../fr/07-engagement-niveau-service.md](../fr/07-engagement-niveau-service.md)*

> **TEMPLATE — OPTIONAL.** Annex C of template 02. Only commit to values after measuring the
> real availability of the infrastructure for at least [3] months.

| Header | |
|---|---|
| Jurisdiction | DRC; OHADA contract law |
| Assumptions | H-01, H-02 |
| Sources | — (contract clauses) |
| Clauses to validate | V-01 (service credits as the sole compensation) |
| Version | [NUMBER] of [DATE] |

## 1. Definitions

- **Availability**: share of time, over a calendar month, during which the Nhill API
  answers the health probe correctly (`/health` → status `ok`) from [measurement point].
- **Excluded unavailability**: scheduled maintenance announced [48] hours in advance (at
  most [4] hours a month, preferably between [10 pm] and [6 am], Kinshasa time); incidents
  at the Customer (power, Internet, hardware); force majeure; suspension for non-payment or
  abuse.

## 2. Commitments

| Indicator | Commitment |
|---|---|
| Monthly availability | [99.0] % |
| First response — blocking incident (sales or till impossible) | [2] business hours |
| First response — inconvenient incident | [1] business day |
| First response — question | [2] business days |
| Business hours | [Monday–Saturday, 8 am – 6 pm, Kinshasa time] |
| Channels | "Help and support" menu, [E-MAIL], [BUSINESS WHATSAPP] |
| Backups | daily, encrypted, kept [30] days; targeted maximum data loss: [24] hours |

## 3. Service credits

| Monthly availability | Credit (on the following month) |
|---|---|
| < [99.0] % and ≥ [98.0] % | [5] % of the monthly price |
| < [98.0] % and ≥ [95.0] % | [10] % |
| < [95.0] % | [25] % |

The Customer requests the credit within [30] days of the end of the month concerned.
Credits are the only compensation due for the service level, without prejudice to gross
negligence. **[TO VALIDATE: V-01.]**

## 4. Degraded mode

When the connection is lost, the applications suspend writing without losing confirmed
operations; an interrupted operation can be retried without being recorded twice. The
Customer plans a fallback procedure (paper sales book) for long outages.

## 5. Report

On request, the Publisher shares the measured availability of the past month and the list
of incidents.
