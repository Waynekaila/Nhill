# Market study — Nhill in Kinshasa (DRC)

*Français : [../fr/01-etude-marche.md](../fr/01-etude-marche.md)*

| | |
|---|---|
| Version | 2.0.0 — 27 October 2026 |
| Area | Kinshasa first, then Lubumbashi and Goma |
| Method | desk research (sources § 8), no field survey specific to Nhill; figures are those of the sources, with their dates |
| Limitation | no market figure was invented; where data is missing, the document says how to obtain it |

## 1. Summary

- Congolese micro, small and medium enterprises (MSMEs) are getting equipped: in a survey of
  914 MSMEs (Kinshasa, Goma, Lubumbashi), **51 %** use digital tools, **46 %** of
  micro-enterprises and **71 %** of small ones [S2].
- Among equipped MSMEs, **94 %** use a smartphone, **86 %** a laptop, **72 %** accounting
  software, but only **19 %** stock management software [S2]: sales, stock and till
  management is the poorly equipped link — Nhill's space.
- **Cost** is the first barrier cited (54 % of micro-enterprises) [S2]; connectivity and
  energy follow (regular power cuts for 16 % of MSMEs, 33 % without backup) [S2]. Nhill must
  be affordable, light on data and tolerant of outages.
- Country digital access: 114 million people, 34.7 million Internet users (30.5 %),
  64.7 million mobile connections (56.9 % of the population), 10.4 million social-media
  identities, 48.8 % urban (October 2025 data) [S1].
- **Decisive regulatory constraint**: VAT payers must issue standardised invoices with
  software or a device certified by the DGI (penalties announced from 15 May 2026) [S4].
  Nhill is not certified. The first market is therefore **businesses not registered for VAT**
  and **management** uses (stock, till, per-employee tracking) alongside the tax device for
  VAT payers.

## 2. The problem observed

| Finding | Source | What Nhill brings |
|---|---|---|
| Low use of stock management (19 % of equipped MSMEs) | S2 | stock through movements, threshold alerts, stock-take, history with author |
| Cost, the first barrier | S2 | subscription per business, free trial, free pilot; prices set after the pilot |
| Power and Internet cuts | S2 | resume without double entry after a cut, light desktop application, Android |
| Hiring by word of mouth (79 % of MSMEs): trusted, informal teams | S2 | one account per person, non-modifiable activity log, "blind" till closing |
| 31 % of sampled MSMEs not registered with the RCCM | S2 | can start without legal identifiers, add them later |
| Massive mobile use | S1, S2 | Nhill Business on Android, phone-friendly screens |

## 3. Segments

| Segment | Description | Interest for Nhill | Priority |
|---|---|---|---|
| A. Non-VAT retail shops (Gombe, Limete, Ngaliema, Kintambo, organised markets) | 1–5 people, one till, a few hundred items | till + stock + staff tracking; quick decision by the owner | **1** |
| B. Hardware stores, building-material depots, wholesalers | large stock, credit sales, several sellers | stock, customer receivables, supplier purchases | **1** |
| C. Pharmacies and parapharmacies (no prescriptions) | many items, till control | till, stock, thresholds | 2 |
| D. Service providers (workshops, printers, agencies) | quotes and invoices, little stock | quotes → invoices, payments | 2 |
| E. VAT-registered SMEs | must use the DGI device | Nhill alongside (stock, till, reports), not for the standardised invoice | 3 — after tax clarification |

## 4. Competition and alternatives

| Alternative | Strengths | Weaknesses for the target | Nhill's position |
|---|---|---|---|
| Notebook, receipt book, calculator | free, familiar, no equipment | no reliable stock, no seller control, errors | same simplicity, with control |
| Spreadsheet (Excel) | flexible, already installed | no traceability, lost files, not multi-user | data on a server, one account per person |
| International management software (online ERP) | feature-rich, integrations | prices in foreign currency, complexity, little adapted to local French and CDF | simple, CDF and USD, French and English, local support |
| Local till software | local presence, installers | often single-computer, little remote control | multi-device, remote viewing, activity log |
| DGI-certified software and devices | compliant for the standardised invoice | focused on tax compliance | complementary (Nhill does not replace the standardised invoice) |

Competitors' public prices change and depend on regions: record them from their websites on
the day of the pricing decision (watch table in `trackers/`), never from memory.

## 5. Value proposition

> **Nhill: your sales, stock and till, clear — even when you're not in the shop.**

Three benefits, verifiable in the software:

1. **Know what sells and what's left**: every sale lowers stock; alerts warn before
   running out.
2. **Know who did what**: one account per employee, a log nobody can erase, a till closing
   where the cashier doesn't see the expected amount.
3. **Work despite outages**: an interrupted operation can be retried without being recorded
   twice.

Never say: "DGI-compliant", "standardised invoice", "certified", "fully offline" (Nhill needs
a connection to record), "bug-free", unmeasured gains.

## 6. Pricing (to set after the pilot)

No price is set or published on the website; costed proposals in the financial study (document 16). Proposed method:

1. During the pilot, measure time saved and differences avoided at each pilot.
2. Ask pilots about their current spending (stationery, spreadsheet, software) and an
   acceptable price (end-of-pilot questionnaire).
3. Record the public prices of alternatives (§ 4) on the same date.
4. Set 2 or 3 plans (users, tills, warehouses) in CDF and USD, with a published, dated
   conversion rate (official BCC rate).

Exchange reference for the budgets in this file: **1 USD = 2,264.0000 CDF**, official rate of
the Central Bank of the Congo on **26/10/2026** [S3].

## 7. Estimating market size (to do, without inventing)

The number of target businesses in Kinshasa was not found in a reliable source. Method:

| Step | Source to consult | Expected result |
|---|---|---|
| 1 | Business-creation one-stop shop, Kinshasa RCCM: registrations by form and sector | formal businesses |
| 2 | Fédération des entreprises du Congo (FEC), traders' associations | breakdown by sector and size |
| 3 | Field count on 3 shopping streets per segment (A, B, C) | shops per street |
| 4 | Share equipped with a computer or Android tablet (prospecting questionnaire) | addressable market |
| 5 | Pilot conversion rate | reachable market at 12 months |

## 8. Sources

| Ref. | Source | Date |
|---|---|---|
| S1 | [DataReportal — Digital 2026: The Democratic Republic of the Congo](https://datareportal.com/reports/digital-2026-democratic-republic-of-the-congo) | published 8 November 2025, October 2025 data |
| S2 | [Malabar, Innogence Consulting, King Baudouin Foundation — "Transformation digitale en RDC : état des lieux des MPME"](https://www.microsave.net/fr/wp-content/uploads/2025/09/Transformation-digitale-en-RDC-E%CC%81tat-des-lieux-des-MPME.pdf); presentation: [King Baudouin Foundation](https://kbs-frb.be/en/study-digital-transformation-smes-drc) | April 2025; fieldwork 16 October 2024 – 14 January 2025; 914 MSMEs (Kinshasa 270, Goma 334, Lubumbashi 270), **non-representative sample** according to the authors |
| S3 | [Central Bank of the Congo — official exchange rate](https://www.bcc.cd/marche-des-changes/cours-de-change) | 26/10/2026: 1 USD = 2,264.0000 CDF |
| S4 | [DGI — standardised invoice reform](https://dgi.gouv.cd/reforme-de-la-facture-normalisee/); [La Prospérité — penalties from 15 May 2026](https://www.laprosperite.cd/rdc-le-ministere-des-finances-met-fin-au-moratoire-sur-la-facture-normalisee-les-sanctions-debutent-le-15-mai-2026/) | consulted 26/10/2026 |
| S5 | [GSMA — State of the Industry Report on Mobile Money 2026](https://www.gsma.com/sotir/wp-content/plugins/plugin_gsma_sotir/reports/The-State-of-the-Industry-Report-2026_French.pdf): Sub-Saharan Africa 2025, 1.2 billion registered accounts, 341 million 30-day active accounts, USD 1.4 trillion transactions (no DRC-specific figure) | 2026 |

Data consulted on 26 and 27 October 2026.
