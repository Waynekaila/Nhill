# Legal research — Nhill in the Democratic Republic of the Congo

*Français : [../fr/00-recherche-juridique.md](../fr/00-recherche-juridique.md)*

| | |
|---|---|
| Version | 2.0.0 — 27 October 2026 |
| Main jurisdiction | Democratic Republic of the Congo (Congolese law and OHADA uniform law) |
| Purpose | framework applicable to publishing and operating the Nhill online software (business management and point-of-sale SaaS) and its marketing website; basis of templates `01` to `19` |
| Method | reading official texts when available online, otherwise secondary sources identified as such (legal commentary, specialised press); sources consulted on 26–27 October 2026 |
| Status | **working document, not validated by a lawyer**. It is not legal advice |

> **Warning.** This file was prepared without access to the DRC Official Journal and
> without consulting a lawyer admitted to a Congolese bar. Several recent texts
> (2024–2026) are only known through commentary or press reports; their exact scope,
> validity and interplay must be confirmed. Nothing specific to the operating company
> (legal form, RCCM trade-register number, national identification, tax number, address,
> capital, managers) is known: these appear in square brackets in the templates and must
> never be invented. French is the reference language of the templates; the English
> versions are translations for information.

## 1. Executive summary

| Topic | What is known | Consequence for Nhill | Certainty |
|---|---|---|---|
| Personal data | Digital Code (Ordinance-law No. 23/010 of 13 March 2023), Book III: principles, data-subject rights, security, data protection officer, prior formalities, controlled transfers outside the DRC | Nhill is **controller** for its prospects, paying customers and administrators, and **processor** for the data businesses enter | High on the framework; medium on details |
| Supervisory authority | Data-protection authority tasks given provisionally to ARPTC (2024); press and commentary then report its replacement by a new authority, **ARPTIC** (2026) | Identify the authority **competent on the filing date** and the procedure in force | **Low: check first** |
| Formalities and fees | Decision No. 039/ARPTC/CLG/2025 of 11 September 2025 (declaration, authorisation, transfer, fees, validity periods); its legality is disputed by commentators | Budget and timeline of the formalities; hosting outside the DRC (e.g. Supabase) a priori requires a **transfer authorisation** | Medium (secondary source), validity disputed |
| Digital activities and services | Digital Code, art. 13–17 (regimes); orders No. 004 and No. 005 of 11 March 2026 (authorisation / declaration); 2026 interministerial order on fees, start-ups exempt according to the press | Determine whether Nhill's activity falls under declaration or authorisation ("application hosting") and whether the operator can be exempt | Low to medium |
| E-commerce | Digital Code, art. 48–72: pre-contractual information, content of online offers, withdrawal, advertising, direct marketing (prior consent) | Website and terms disclosures; opt-in for commercial messages | Medium |
| Consumer law | No adopted consumer code identified; a consumer-protection bill is under discussion; older sector texts and Law No. 18/020 of 9 July 2018 (prices and competition) | Nhill's customers are businesses: mainly B2B regime; care with sole traders | Medium |
| Invoicing and VAT | 16 % VAT (Ordinance-law No. 10/001 of 20 August 2010); standardised invoice produced by an electronic tax device, only certified software allowed for VAT payers, penalties from 15 May 2026 according to the press | Nhill **is not certified**: management documents only; target non-VAT businesses first, or use alongside the DGI device | Medium to high |
| Company law and accounting | DRC is an OHADA member (accession deposited on 13 July 2012); uniform acts apply (companies, general commercial law, accounting: 10-year retention) | Operator's company and trade registration to be created; document retention periods | High |
| Intellectual property | Law No. 82-001 of 7 January 1982 (industrial property, including trademarks); Ordinance-law No. 86-033 of 5 April 1986 (copyright, including software depending on its wording, to be confirmed) | Registration of the "Nhill" trademark to check (availability not checked); licence clauses | Medium |
| Employment (activity log) | The named log of employees' actions is a processing of employee data | Staff information notice provided to businesses (template 17) | Medium |

## 2. Assumptions register

Each template refers to the assumptions it relies on. An invalidated assumption requires
reviewing the templates listed.

| No. | Assumption | Templates | How to check | Status |
|---|---|---|---|---|
| H-01 | The operator is a company under Congolese law (OHADA form to choose: SARL, SAS, SA…) registered with the Kinshasa RCCM | all | articles of association, RCCM extract | **open** — no company identified |
| H-02 | Nhill's customers are businesses (traders, companies, associations) acting for their trade; there is no sale to consumers | 01, 02, 07, 09 | business model; lawyer: status of a sole trader | to validate |
| H-03 | Nhill is controller for visitor, prospect, user-account and billing data, and processor for data entered by businesses | 03, 06, 15, 18 | lawyer; Digital Code, Book III | to validate |
| H-04 | The production database may be hosted outside the DRC (Supabase, region to choose): international transfer requiring prior authorisation | 03, 06, 14, 18 | choice of host; competent authority | **open** — host and region not chosen |
| H-05 | The competent data-protection authority is ARPTC on a provisional basis, or ARPTIC if its creation and takeover of these tasks are effective | 03, 06, 14, 18, 19 | founding texts; authority's website | **open — priority** |
| H-06 | The fees and procedures of Decision No. 039/ARPTC/CLG/2025 still apply despite challenges and the change of authority | checklist § 6 | authority; lawyer | open |
| H-07 | Nhill's activity (online software hosted for third parties) may fall under the **authorisation** regime of orders No. 004 and 005/2026 if it is classed as application hosting; a young company could fall under declaration or an exemption | checklist § 6 | text of the orders; ministry / authority | open |
| H-08 | Commercial prospecting by e-mail, SMS and WhatsApp requires the person's prior consent (opt-in); silence is not consent | 03, 16 | Digital Code, art. 68–72 | to validate |
| H-09 | Documents issued by Nhill are not standardised invoices; a VAT payer must use a device or software certified by the DGI | 01, 02 | DGI; tax adviser | high, to confirm |
| H-10 | Congolese law and the Kinshasa courts (Kinshasa/Gombe commercial court) are chosen in the contracts; OHADA CCJA arbitration is an option | 01, 02, 05, 09, 10, 11, 12 | lawyer | choice to make |
| H-11 | Prices are set in USD and/or CDF; conversion uses the rate stated on the invoice | 02, 09 | pricing policy; Law No. 18/020; foreign-exchange rules | to validate |
| H-12 | Availability of the name "Nhill" as a trademark and company name has not been checked | 01, 05, 12 | prior-rights search (DRC, international) | **open** |
| H-13 | The marketing website sets no cookie and uses no third-party analytics | 04 | technical check of the site | true in 2.0.0 (checked in the code) |
| H-14 | Business data is kept during the subscription, then 90 days after it ends to allow export, unless a legal obligation says otherwise | 02, 06, 15 | product policy; lawyer (10-year retention lies with the business) | proposal |
| H-15 | A data breach is notified to the authority and, where needed, to individuals within the Digital Code deadlines (72 hours according to a secondary summary) | 06, 14 | official text | to confirm |
| H-16 | The pilot programme is free, time-limited and without purchase commitment | 09 | pricing policy | proposal |
| H-17 | Applications are provided under a non-exclusive licence tied to the subscription; the code remains the publisher's property | 01, 12 | articles / assignment of developers' rights | to validate |
| H-18 | Partners (referrers, integrators, trainers) are independent, paid by commission, with no power to sign on the operator's behalf | 10 | lawyer; tax adviser (withholding) | to validate |

## 3. Findings by area

### 3.1 Personal data

- **Text**: Ordinance-law No. 23/010 of 13 March 2023 on the Digital Code, Book III
  (art. 183–270 in the version consulted) [S01]. Principles of lawfulness, purpose,
  proportionality, accuracy, limited retention, security; rights to information, access,
  rectification, objection, erasure; security obligation; data protection officer
  (art. 222); transfers outside the DRC controlled (art. 187, 201 and 202 according to the
  commentary available) [S01, S07].
- **Authority**: tasks entrusted provisionally to ARPTC [S03]; several 2026 sources report a
  new authority, ARPTIC, presented as the single authority for digital regulation,
  electronic certification and data protection, and the disappearance of ARPTC [S04].
  **The exact status could not be read in an official text** (H-05).
- **Formalities and fees**: Decision No. 039/ARPTC/CLG/2025 of 11 September 2025,
  according to a law firm's commentary [S02]: declaration (USD 250 file fee + USD 250),
  authorisation (USD 500 + 500), transfer authorisation (USD 750 + 750); valid 3, 2 and
  1 years respectively; fines of 8 to 200 million Congolese francs. The decision's legality
  and constitutionality are disputed by commentators [S02b]. **Amounts and periods to be
  reconfirmed with the authority** (H-06).
- **Nhill's role**: controller for its own files (website, prospects, accounts, billing,
  support); processor for businesses' data (customers, sales, employees) — hence the data
  processing agreement (template 06).

### 3.2 Digital activities and services

- Digital Code, art. 13–17: regimes for digital activities and services (declaration,
  authorisation) [S01].
- Ministerial orders No. 004 and No. 005 of 11 March 2026: conditions for authorising and
  declaring digital activities and services; according to commentary, application hosting
  requires authorisation and start-ups a declaration, with a transition period until
  30 June 2026 [S05].
- 2026 interministerial order on fees: kept in force, start-ups exempt according to the
  press; different amounts are quoted for local and foreign hosting [S06]. **Text not
  read** (H-07).

### 3.3 E-commerce and prospecting

- Digital Code, art. 48–72 [S01]: information to provide (provider identity, price, terms),
  content of online offers (art. 53–54), right of withdrawal (art. 61, mainly for
  consumers), identifiable advertising (art. 66–67), direct marketing subject to prior
  consent, silence not being consent (art. 68–72).
- Consequences: legal notice and terms accessible on the website; separate, unticked
  marketing consent box (already in place on the site); unsubscribe link in every message;
  consent register.

### 3.4 Consumers, prices and competition

- No adopted consumer code was identified; a consumer-protection bill has been reviewed
  [S11]. Reference texts cited by commentators: Law No. 18/020 of 9 July 2018 on freedom of
  prices and competition [S10] and older trade texts.
- Nhill targets businesses (H-02); for a sole trader, use balanced clauses (no obviously
  unfair clause, simple termination).

### 3.5 Tax and invoicing

- VAT at 16 % [S09]. Standardised invoice: electronic tax device, certification of
  invoicing systems, only certified software allowed since 1 July 2025, penalties announced
  from 15 May 2026 [S08].
- Consequence: the terms of service must state clearly that Nhill is **not** a certified
  invoicing system (already displayed in the software for VAT payers).
- Billing of the Nhill subscription by the operator: if the operator is VAT-registered, its
  own invoices must be standardised (tax adviser).

### 3.6 Companies, accounting, contracts

- The DRC applies OHADA law since its accession (instruments deposited on 13 July 2012)
  [S14]: Uniform Acts on commercial companies, general commercial law, accounting
  (retention of records: 10 years) [S15].
- Choice of forum: Kinshasa commercial court; arbitration option (CCJA) to discuss (H-10).

### 3.7 Intellectual property

- Trademarks: Law No. 82-001 of 7 January 1982 on industrial property [S12].
- Copyright: Ordinance-law No. 86-033 of 5 April 1986 [S13]; protection of software and
  ownership of code written by developers or contractors: assignment clause to include in
  their contracts (outside this file).
- Digital Code, art. 49 (cited in earlier research on e-commerce): to be reread.
- Availability of "Nhill": **not checked** (H-12).

### 3.8 Employment

- The named activity log processes the business's employees' data: prior information of
  staff (purpose, data, retention, rights). Template 17 provided to businesses; interplay
  with the Labour Code to validate.

## 4. List of templates

| No. | Template (FR, reference) | Template (EN) | Use |
|---|---|---|---|
| 01 | [Conditions générales d'utilisation](../fr/01-conditions-generales-utilisation.md) | [Terms of use](01-terms-of-use.md) | use of the website and applications |
| 02 | [Conditions de service SaaS](../fr/02-conditions-service-saas.md) | [SaaS terms of service](02-saas-terms-of-service.md) | subscription contract with the business |
| 03 | [Politique de confidentialité](../fr/03-politique-confidentialite.md) | [Privacy policy](03-privacy-policy.md) | information to individuals |
| 04 | [Politique cookies](../fr/04-politique-cookies.md) | [Cookie policy](04-cookie-policy.md) | marketing website |
| 05 | [Mentions légales](../fr/05-mentions-legales.md) | [Legal notice](05-legal-notice.md) | marketing website |
| 06 | [Accord de traitement des données](../fr/06-accord-traitement-donnees.md) | [Data processing agreement](06-data-processing-agreement.md) | contract annex (processing) |
| 07 | [Engagement de niveau de service](../fr/07-engagement-niveau-service.md) | [Service level agreement](07-service-level-agreement.md) | availability, support |
| 08 | [Politique d'utilisation acceptable](../fr/08-politique-utilisation-acceptable.md) | [Acceptable use policy](08-acceptable-use-policy.md) | prohibited uses |
| 09 | [Convention de programme pilote](../fr/09-convention-programme-pilote.md) | [Pilot programme agreement](09-pilot-programme-agreement.md) | supervised trial |
| 10 | [Contrat de partenariat](../fr/10-contrat-partenariat.md) | [Partnership agreement](10-partnership-agreement.md) | referrers, integrators, trainers |
| 11 | [Accord de confidentialité](../fr/11-accord-confidentialite.md) | [Non-disclosure agreement](11-non-disclosure-agreement.md) | exchanges with partners, contractors |
| 12 | [Licence des applications](../fr/12-licence-applications.md) | [Application licence](12-application-licence.md) | installed applications (Windows, Linux, Android) |
| 13 | [Politique de sécurité](../fr/13-politique-securite.md) | [Security policy (public summary)](13-security-policy.md) | trust, tenders |
| 14 | [Procédure de violation de données](../fr/14-procedure-violation-donnees.md) | [Data breach procedure](14-data-breach-procedure.md) | internal |
| 15 | [Politique de conservation](../fr/15-politique-conservation.md) | [Retention and deletion policy](15-retention-policy.md) | internal and contractual |
| 16 | [Consentement à la prospection](../fr/16-consentement-prospection.md) | [Marketing consent form and register](16-marketing-consent.md) | prospecting, WhatsApp, e-mail |
| 17 | [Information du personnel](../fr/17-information-personnel.md) | [Staff information notice](17-staff-notice.md) | given to businesses (activity log) |
| 18 | [Registre des traitements](../fr/18-registre-traitements.md) | [Record of processing activities](18-processing-register.md) | internal |
| 19 | [Demande d'exercice des droits](../fr/19-demande-exercice-droits.md) | [Data subject request form](19-data-subject-request.md) | access, rectification, erasure |

Every template starts with a header: jurisdiction, assumptions, sources, clauses to
validate. Fields to complete are in square brackets `[…]`; clauses to have validated carry
the mark **[TO VALIDATE]**.

## 5. Clauses requiring legal validation

| Ref. | Clause | Templates | Question for the lawyer |
|---|---|---|---|
| V-01 | Limitation of liability (cap = amounts paid over 12 months) | 02, 07, 12 | validity and enforceability under Congolese / OHADA law; exceptions (gross negligence, fraud) |
| V-02 | Governing law and forum (Kinshasa) or arbitration | 01, 02, 09, 10, 11, 12 | best forum, arbitration clause |
| V-03 | Prices in USD, payment in CDF at the day's rate | 02, 10 | foreign-exchange rules, Law 18/020 |
| V-04 | Suspension for non-payment after a grace period, read-only mode | 02 | formal-notice periods |
| V-05 | Transfer of data outside the DRC and safeguards | 03, 06 | prior authorisation, competent authority (H-04, H-05) |
| V-06 | Breach notification deadline (72 h) | 06, 14 | exact Digital Code deadline |
| V-07 | Retention periods | 03, 15 | limitation periods, accounting obligations |
| V-08 | Marketing consent (opt-in) and proof | 16 | art. 68–72 |
| V-09 | Staff information about the activity log | 17 | Labour Code, staff representation |
| V-10 | Withdrawal for sole traders | 02 | applicability of art. 61 |
| V-11 | Partner commission, non-compete, non-solicitation | 10 | validity, tax (withholding) |
| V-12 | Tax disclaimer (management documents) | 01, 02 | sufficiency under the standardised-invoice reform |
| V-13 | Appointment of a data protection officer | 03, 18 | obligation (art. 222) and thresholds |

## 6. Validation checklist before commercial launch

| # | Action | Owner | Before |
|---|---|---|---|
| 1 | Set up the operating company (OHADA form), obtain RCCM, national identification, tax number | founders + lawyer / one-stop shop | any contract |
| 2 | Check name availability and register the "Nhill" trademark | lawyer / IP adviser | public communication |
| 3 | Identify the competent data-protection authority (ARPTC / ARPTIC) and the procedure in force | lawyer | filings |
| 4 | Choose the host and region; if outside the DRC, apply for the transfer authorisation | technical lead + lawyer | production |
| 5 | File the processing declarations / authorisations (prospects, customers, operator's staff, processing on behalf of customers) | DPO / lawyer | data collection |
| 6 | Classify the activity under orders No. 004 and 005/2026; check eligibility for a start-up exemption | lawyer | billing subscriptions |
| 7 | Appoint a data protection officer or document why not | management | filings |
| 8 | Complete all `[…]` fields of templates 01 to 19 and have clauses V-01 to V-13 reviewed | lawyer | publication |
| 9 | Validate the tax position: Nhill = management documents; operator's own invoicing (VAT, standardised invoice) | tax adviser | first payment |
| 10 | Publish legal notice, terms, privacy policy on the website; remove the "draft" banners | management | opening the forms |
| 11 | Set up the record of processing (template 18) and the breach procedure (template 14) | DPO | production |
| 12 | Provide template 17 to businesses when the activity log is enabled | product team | first customers |
| 13 | Review this file every 6 months and with every new text | DPO / lawyer | — |

## 7. Sources

Consulted on 26 and 27 October 2026. "Secondary": commentary, press or legal writing, not
the official text. The sources are in French.

| Ref. | Source | Type |
|---|---|---|
| S01 | Ordinance-law No. 23/010 of 13 March 2023 on the Digital Code — [PDF published by Droit-Numérique.cd](https://droitnumerique.cd/wp-content/uploads/2024/08/Code-du-numerique-I-RD-Congo.pdf); [presentation page](https://droitnumerique.cd/code-du-numerique-congolais-rdc-pdf/) | text (unofficial copy) |
| S02 | [Avocats.cd — business compliance with personal-data rules (decision 039/ARPTC/CLG/2025)](https://avocats.cd/blog/donnees-personnelles-en-rdc-comment-les-entreprises-doivent-se-conformer) | secondary |
| S02b | [Droit-Numérique.cd — on the illegality of the ARPTC decision](https://droitnumerique.cd/inconstitutionnalite-de-la-decision-de-larptc/); [PDF "Illégalité de la décision de l'ARPTC"](https://droitnumerique.cd/wp-content/uploads/2026/05/Illegalite-de-la-decision-de-lARPTC-Brozeck-Kandolo-1.pdf) | commentary |
| S03 | [Droit-Numérique.cd — ARPTIC/ARPTC entrusted with the data-protection authority's tasks](https://droitnumerique.cd/arptic-arptc-investie-des-missions-de-lautorite-de-protection-des-donnees-en-rd-congo/) | secondary |
| S04 | [Droit-Numérique.cd — ARPTIC as single authority](https://droitnumerique.cd/regulation-du-numerique-en-rdc-larptic-investie-autorite-unique-pour-la-regulation-du-numerique-la-certification-electronique-et-la-protection-des-donnees/); [Ouragan.cd — "ARPTC no longer exists" (January 2026)](https://ouragan.cd/2026/01/deja-abrogee-larptc-nexiste-plus); [Rights Empower — ARPTIC replaces ARPTC](https://rightsempower.org/fr/reforme-dans-le-secteur-de-telecommunications-une-nouvelle-autorite-de-regulation-des-postes-des-telecommunications-et-des-technologies-de-linformation-et-de-la-communication-arptic-remplace-larptc) | secondary |
| S05 | [Droit-Numérique.cd — orders No. 004 and 005 of 11 March 2026](https://droitnumerique.cd/arretes-n-004-et-n-005-du-11-mars-2026-relatifs-aux-activites-et-services-numeriques-en-rdc/); [Leganews — order No. 005/2026 (declaration)](https://leganews.pro/articles/019cfd07-43b2-7af3-914b-4d345972970d); [Numerico.cd — limits of the order](https://numerico.cd/2026/03/25/encadrement-des-services-numeriques-honore-mvula-pointe-les-limites-du-nouvel-arrete/) | secondary |
| S06 | [Mediacongo — analysis of the new order (taxes, approvals, penalties)](https://www.mediacongo.net/article-actualite-166525_taxes_agrements_sanctions_decryptage_du_nouvel_arrete_qui_encadre_le_numerique_en_rdc.html); [Journal de Kinshasa — start-ups exempt](https://www.journaldekinshasa.com/rdc-fiscalite-numerique-startups-exonerees-arrete-2026/); [Afrique IT News](https://afriqueitnews.com/news/rdc-nouvelles-taxes-numeriques-entrent-vigueur-start-up-exonerees/) | press |
| S07 | [Droit-Numérique.cd — transfer of personal data outside the DRC](https://droitnumerique.cd/le-transfert-des-donnees-personnelles-hors-de-la-rd-congo/) | secondary |
| S08 | [DGI — standardised invoice reform](https://dgi.gouv.cd/reforme-de-la-facture-normalisee/); [Bankable — only certified software allowed](https://bankable.africa/en/climat-des-affaires/1906-1335-facturation-seuls-les-logiciels-homologues-autorises-des-le-1er-juillet-en-rdc); [La Prospérité — end of the moratorium, penalties from 15 May 2026](https://www.laprosperite.cd/rdc-le-ministere-des-finances-met-fin-au-moratoire-sur-la-facture-normalisee-les-sanctions-debutent-le-15-mai-2026/) | official + press |
| S09 | [Ordinance-law No. 10/001 of 20 August 2010 (VAT), consolidated text — Leganet](https://www.leganet.be/Legislation/Dfiscal/TVA/OL.10.001.20.2010.htm) | text |
| S10 | [Law No. 18/020 of 9 July 2018 on freedom of prices and competition — FAOLEX](https://faolex.fao.org/docs/pdf/Cng189580.pdf) | text |
| S11 | [ACP — review of the consumer-protection bill](https://acp.cd/economie/rdc-passage-en-revue-du-projet-de-loi-portant-protection-des-consommateurs/) | official press agency |
| S12 | [Law No. 82-001 of 7 January 1982 on industrial property — WIPO Lex](https://www.wipo.int/wipolex/en/legislation/details/7499) | text |
| S13 | [Ordinance-law No. 86-033 of 5 April 1986 (copyright and related rights) — WIPO Lex](https://www.wipo.int/wipolex/en/legislation/details/7511) | text |
| S14 | [OHADA — deposit of the DRC's accession instruments, Dakar, 13 July 2012](https://www.ohada.com/actualite/1614/depot-des-instruments-dadhesion-de-la-rdc-a-lohada-dakar-13-juillet-2012.html) | official |
| S15 | [OHADA Uniform Act on accounting and financial reporting (2017)](https://www.vie-publique.sn/documents/11026/acte-uniforme-droit-comptable-information-financiere-ohada-2017) | text |
| S16 | [DLA Piper — Data protection laws of the world: Democratic Republic of the Congo](https://www.dlapiperdataprotection.com/index.html?t=law&c=CD) | secondary |

Limitation: the Digital Code articles cited come from the S01 PDF copy and commentary; the
2024–2026 texts (orders, decision, creation of ARPTIC) were not read in their official
version published in the Official Journal.
