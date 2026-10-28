# Nhill 2.0 legal parameters

*Français : [../fr/parametres-legaux.md](../fr/parametres-legaux.md)*

This document lists **all the legal and tax information to provide** before commercial
launch, and where each item must be entered: legal templates, marketing website, Nhill
Console and Nhill Business settings. None of these values is known today; **none may be
invented**. Detailed analysis and sources:
[`legal/en/00-legal-research.md`](../../legal/en/00-legal-research.md).

> Nothing in this repository is legal advice. The templates must be reviewed by a lawyer
> admitted to a DRC bar and, for tax matters, by a tax adviser.

## 0. Values set by Nhill Solution (28 October 2026)

| Item | Value | Note |
|---|---|---|
| Trade name | Nhill Solution | no legal form, registration or certification is stated until it is confirmed |
| Head office | Kinshasa, Democratic Republic of the Congo | no postal address published |
| Official domain | `nhill.online` | public address `https://nhill.online`, to check before announcing it: `website/tools/check_live.py` |
| General e-mail | `info@nhill.online` | contact, data protection, marketing |
| Support e-mail | `support@nhill.online` | customer support, security or unlawful-content reports |
| Phone | none published | — |
| Service hosting (API and database) | Hostinger International Ltd (Larnaca, Cyprus), virtual private server in France (EU) | Hostinger's contracting entity for a customer outside the UK and outside groups 1 and 2 of its terms |

## 1. Operator identity (field → where it appears)

| Field | Format example | Templates | Website | Application |
|---|---|---|---|---|
| `[COMPANY NAME]` | exact name in the RCCM | 01, 02, 03, 05, 06, 09–12, 18 | `legal-notice`, `privacy`, `terms` (FR/EN) | subscription invoice (Nhill Console → platform settings, payment instructions) |
| `[LEGAL FORM]`, `[CAPITAL]` | SARL / SAS / SA (OHADA), amount and currency | 01, 02, 05 | `legal-notice` | — |
| `[RCCM]` | Trade and Personal Property Credit Register number | 01, 02, 03, 05, 09, 18 | `legal-notice` | — |
| `[NAT. ID]` | national identification | 01, 02, 05 | `legal-notice` | — |
| `[NIF]` | tax number | 01, 02, 05 | `legal-notice` | subscription invoices |
| `[ADDRESS]` | full registered office | all | `legal-notice`, `contact` | — |
| ~~`[PHONE]`~~, `[E-MAIL]` | **set**: `info@nhill.online` (no phone) | 01, 05 | `contact`, footer (`SITE_CONTACT_EMAIL`, `SITE_SUPPORT_EMAIL`) | Help and support (`support@nhill.online`) |
| `[NAME, TITLE]` | legal representative / publication director | 02, 05, 06, 09–11 | `legal-notice` | — |
| `[DPO E-MAIL]`, `[DPO NAME]` | **e-mail set**: `info@nhill.online`; no officer appointed to date | 03, 04, 14, 16, 18, 19 | `privacy` | — |
| `[SECURITY E-MAIL]` | **set**: `support@nhill.online` (subject "Security") | 05, 08, 13 | `legal-notice`, future `/.well-known/security.txt` | — |

## 2. Hosting and transfers

| Field | Decision to make | Where |
|---|---|---|
| API server `[HOST]`, `[COUNTRY / REGION]` | provider and country | templates 03, 05, 06, 18; `legal-notice` |
| Database host (Supabase or self-hosted PostgreSQL), region | if outside the DRC: **transfer authorisation** (assumption H-04) | templates 03, 06, 18 |
| Website host | — | template 05; `website/README.en.md` |
| Transfer authorisation reference | number, date, authority | templates 03, 06, 18 |
| Competent data-protection authority | ARPTC (provisional) or ARPTIC (H-05): **check first** | templates 03, 14, 19; `privacy` page (currently mentions ARPTC) |
| References of processing declarations / authorisations | number and date per processing | template 18 |
| Classification of the digital activity (orders No. 004 and 005/2026) | declaration, authorisation or exemption (H-07) | template 05 ("digital-activity authorisation / declaration") |

## 3. Commercial terms

| Parameter | Proposed value | Where to set it | To validate |
|---|---|---|---|
| Free trial length | 30 days (default) | Nhill Console → Platform settings | — |
| Grace period before read-only | 7 days (default) | Nhill Console → Platform settings | V-04 |
| Plans, limits, prices per term and currency | **no price preconfigured** | Nhill Console → Plans and prices; template 02 annex A | V-03 |
| Billing currency, VAT on the subscription | `[USD / CDF]`, `[excl./incl. tax]` | template 02 § 3 | tax adviser |
| Tacit or express renewal | `[TO CHOOSE]` | template 02 § 2.2 | lawyer |
| Reversibility after the end | 90 days | templates 02, 06, 15 | V-07 |
| Partner commission | `[N] %` over 12 months | template 10 | V-11 |
| "Pilot" discount | `[N] %` for `[N]` months | template 09; Nhill Console → Promo codes | — |
| Support hours, channels | `[DAYS AND HOURS]`, business WhatsApp, e-mail | templates 02, 07 | — |

## 4. Tax in the application

| Item | 2.0 status | Action |
|---|---|---|
| DRC tax profile (`server/profiles/CD.json`) | VAT 16 %, 0 %, exempt, not applicable | have rates and mentions checked by a tax adviser |
| Standardised invoice (DEF / certified SFE) | **Nhill is not certified**; "management document" mention on VAT payers' documents; DEF reference entry | do not present Nhill as compliant invoicing software; certification roadmap: `docs/conformite/conformite.md` (French) |
| So-called 8 % "social VAT" | not established, not enabled | to validate |

## 5. Texts displayed by the application

| Parameter | Current content | Where |
|---|---|---|
| `platform_stats_notice` | FR/EN text explaining the aggregated statistics viewed by the Nhill team (migration 0006) | Nhill Console → Platform settings; to reflect in the privacy policy (template 03, processing table) |
| "Management document" mention | printed on VAT payers' documents | profiles (`server/profiles/*.json`) |
| Staff information (activity log) | template 17, to be handed out by each business | to offer in Nhill Business help and when accounts are activated |

## 6. Website: pages to finalise

| FR / EN page | Source to align | 2.0 status |
|---|---|---|
| `mentions-legales` / `legal-notice` | template 05 | "draft" banner, bracketed fields |
| `confidentialite` / `privacy` | template 03 | "draft" banner |
| `conditions` / `terms` | templates 01 and 02 | "draft" banner |

The pages carry a "draft to complete and have validated by a lawyer — do not publish as
is" banner, to be removed only after validation.

## 7. Recommended order

1. Company, RCCM, tax number, national ID (section 1).
2. Hosting choice; check of the competent authority; filings (section 2).
3. Review of templates 01–19 by the lawyer (clauses V-01 to V-13).
4. Plans and prices entered in Nhill Console; annex A of template 02.
5. Website pages finalised; "draft" banners removed.
6. Website forms opened (`SITE_API_BASE_URL`), then sign-ups.
