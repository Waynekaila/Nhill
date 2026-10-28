# Nhill 3.0 legal parameters

*Français : [../fr/parametres-legaux.md](../fr/parametres-legaux.md)*

This document shows where Nhill's legal and commercial information lives: legal documents
(`legal/`), marketing website and Nhill Console settings. Detailed analysis, decisions and
sources: [`legal/en/00-legal-research.md`](../../legal/en/00-legal-research.md).

> Nothing in this repository is legal advice. The documents were written without review by
> a lawyer; the points to have reviewed are listed in § 5 of the legal file.

## 1. Values set by Nhill Solution (28 October 2026)

| Item | Value | Where it appears |
|---|---|---|
| Publisher | Nhill Solution (trade name), head office in Kinshasa (Democratic Republic of the Congo); no other identification data | documents 01 to 19, legal notice, website footer, installer metadata |
| Publication director | Wayne TSHINGI, founder | legal notice (document 05 and website) |
| Official domain | `nhill.online` | website (`config/site.json`), documents |
| General e-mail | `info@nhill.online` | website, documents, data protection |
| Support e-mail | `support@nhill.online` | website, applications ("Help and support"), automatic e-mails (`NH_SUPPORT_EMAIL`), security reports, `/.well-known/security.txt` |
| Phone | none published | — |
| Hosting of the service | Hostinger International Ltd, 61 Lordou Vironos str., 6023 Larnaca, Cyprus; virtual private server located in France | legal notice, privacy policy, data processing agreement, record |
| Governing law | DRC law; competent courts of Kinshasa, subject to the mandatory rules of the user's country | documents 01, 02, 09 to 12, "Terms" page |
| Liability | capped at the amounts paid during the 12 months before the event, except gross negligence or wilful misconduct and personal injury | document 02, "Terms" page |
| Service level | no availability rate or response time guaranteed | documents 02 and 07, "Terms" page |
| Prices | no price published; written proposal per business | website, documents 02 and 09 |
| End of contract | 90 days of read-only export, then deletion | documents 02, 06, 15; Nhill Console ("Erase…") |
| Good-practice reference | European standards (GDPR), without any claim of compliance | documents 03, 06, 13, 14, 15, 19 |

## 2. Nhill Console settings

| Parameter | Default | Where |
|---|---|---|
| Free trial length | 30 days (`trial_days`) | Platform settings; per plan |
| Grace period before read-only | 7 days (`grace_days`) | Platform settings; per plan |
| Reminders before the due date | 14, 7, 3 and 1 days | Platform settings; per plan |
| Automatic renewal | only with the account holder's express agreement, where the plan allows it | Plans ("Automatic renewal allowed (with consent)") |
| Plans, rights, limits, prices per term and currency | no price preconfigured | Plans and prices |
| Business code validity | 30 days (`code_rotation_days`) | Platform settings |

## 3. Tax in the application

| Item | 3.0 status | Action |
|---|---|---|
| DRC tax profile (`server/profiles/CD.json`) | VAT 16 %, 0 %, exempt, not applicable | have rates and notices checked by a tax adviser |
| DRC standardised invoice | **Nhill is not an approved invoicing device**; "management document" notice on VAT payers' documents; field for the reference of the official invoice | never present Nhill as approved invoicing software |
| International profile (`INTL`) | rates, identifiers and notices entered by the business | the business has its notices confirmed by its accountant |

## 4. Texts shown by the application

| Parameter | Content | Where |
|---|---|---|
| `platform_stats_notice` | FR/EN text explaining the overall figures consulted by the Nhill team | Platform settings; reused in the privacy policy |
| "Management document" notice | printed on VAT payers' documents | profiles (`server/profiles/*.json`) |
| Staff information (activity log) | document 17, to be handed out by each business | to offer when accounts open |

## 5. Legal pages of the website

| FR / EN page | Reference document | 3.0 status |
|---|---|---|
| `mentions-legales` / `legal-notice` | document 05 | complete, no field to fill; the "website host" line will be added once the website host is chosen |
| `confidentialite` / `privacy` | document 03 | complete, periods aligned with the code |
| `conditions` / `terms` | documents 01 and 02 | complete |

## 6. Actions left to the operator

1. Have documents 01 to 19 reviewed by a lawyer admitted to a DRC bar (points V-01 to
   V-13) and the tax position by a tax adviser.
2. Check the availability of the name "Nhill" and decide on a trademark filing.
3. Identify the competent data protection authority and complete the formalities
   (processing, transfer to France).
4. Set up e-mail for `nhill.online` (MX, SPF, DKIM, DMARC) before publishing the addresses.
5. Publish the website, then check the domain (`website/tools/check_live.py --require-site
   --require-mail`).
