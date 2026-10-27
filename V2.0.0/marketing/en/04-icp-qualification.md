# Ideal customer profile (ICP) and qualification grid

*Français : [../fr/04-icp-qualification.md](../fr/04-icp-qualification.md)*

## 1. Ideal customer profile

| Dimension | Profile |
|---|---|
| Activity | retail, hardware store, depot, wholesaler, parapharmacy, service provider with quotes |
| Location | Kinshasa (Gombe, Limete, Ngaliema, Kintambo, Lingwala, Barumbu, Masina first, depending on the field), then Lubumbashi and Goma |
| Size | 2 to 15 people; 1 to 3 points of sale or tills |
| Decision-maker | owner or manager present in the shop, who does the till himself or herself in the evening |
| Equipment | at least one computer (Windows 10/11 or Linux) or an Android 7+ tablet; smartphone; mobile Internet |
| Tax | not VAT-registered (priority target); VAT payer only if already equipped with a DGI device for standardised invoices |
| Pain | till differences, theft or stock errors, doesn't know what sells, depends on a trusted seller, lost notebooks |
| Trigger | opening a 2nd point of sale, hiring sellers, disastrous stock-take, owner's long absence |

**Personas**

- **Mama Nzeba, owner of a grocery shop (4 people)**: spends an hour every evening on the
  notebook, suspects differences but can't prove them. Wants: to know how much she sold
  without being there.
- **Patrick, manager of a hardware store (8 people, credit sales)**: customers who pay in
  instalments, thousands of items. Wants: to know who owes how much and what is left in the
  depot.

*Personas invented to illustrate the profile: they are neither customers nor testimonials.*

## 2. Not a target (for now)

- Businesses requiring Nhill to produce a **standardised invoice** (not certified).
- Activities with no compatible device or connection.
- Pharmacies dispensing prescription medicines (regulatory traceability not covered).
- Large companies requiring ERP integration, full general accounting, payroll.

## 3. Qualification grid (score out of 20)

| Criterion | 0 points | 1 point | 2 points | Weight |
|---|---|---|---|---|
| Pain expressed (till, stock, seller control) | none | vague | precise, with a recent example | ×2 |
| Decision-maker met | no | influencer | decides alone | ×2 |
| Compatible equipment | none | smartphone only | computer or Android tablet + Internet | ×1 |
| Tax | VAT payer without DGI device | VAT payer with DGI device | not VAT-registered | ×1 |
| Availability for onboarding | none | within the month | this week | ×1 |
| Size | 1 person alone | 2 to 3 | 4 to 15 | ×1 |
| Referral potential (network, association) | none | a few contacts | active member of an association or market | ×1 |

Maximum: 2×2 + 2×2 + 2 + 2 + 2 + 2 + 2 = 18, plus **2 bonus points** if the prospect has
already tried a spreadsheet or software (knows what they want) → 20.

| Score | Decision |
|---|---|
| ≥ 14 | offer the pilot |
| 9 to 13 | demo, then follow up in 2 weeks |
| ≤ 8 | keep in the list, inform (with consent) |

## 4. Discovery questions (10 minutes)

1. How do you record your sales today? Who records them?
2. How do you do the till in the evening? How long does it take?
3. The last time the till didn't add up, what did you do?
4. How do you know an item is about to run out?
5. Do you sell on credit? How do you track what you are owed?
6. How many people sell? Does each have their own access?
7. What devices do you have in the shop? Is the Internet stable?
8. Are you VAT-registered? Do you use a DGI device?
9. What have you already tried (notebook, Excel, software)? What didn't work?
10. If it worked, what would change for you in a month?

Record the answers in the prospect tracker (`trackers/suivi-prospects.csv`).
