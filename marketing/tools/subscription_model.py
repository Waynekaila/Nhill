#!/usr/bin/env python3
"""Modèle financier des abonnements Nhill (étude du 27 octobre 2026).

Source unique des chiffres de l'étude marketing/{fr,en}/16-*.md : toutes les hypothèses
sont dans HYP ci-dessous, chacune marquée V (donnée vérifiée, source citée dans l'étude)
ou H (hypothèse à valider). Le script :

  * simule 36 mois pour trois scénarios (pessimiste, central, optimiste), par cohortes
    mensuelles (abonnements mensuels et annuels, résiliation au renouvellement pour
    l'annuel) ;
  * calcule l'économie unitaire (ARPA, marge brute, CAC, LTV, délai de récupération),
    le seuil de rentabilité et le besoin de trésorerie ;
  * écrit marketing/data/etude-abonnements/*.csv|json, les graphiques SVG de
    marketing/visuals/etude-abonnements/ et remplace les blocs <!-- model:NOM --> des
    deux documents de l'étude.

  python3 marketing/tools/subscription_model.py          # tout régénérer
  python3 marketing/tools/subscription_model.py --check  # vérifier que les documents sont à jour

Les prix sont des PROPOSITIONS soumises à validation : ils ne sont ni publiés sur le
site ni chargés dans la base.
"""
import argparse
import csv
import json
import math
import os
import re
import sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DATA = os.path.join(ROOT, "marketing", "data", "etude-abonnements")
VIS = os.path.join(ROOT, "marketing", "visuals", "etude-abonnements")
DOCS = {"fr": os.path.join(ROOT, "marketing", "fr", "16-etude-financiere-abonnements.md"),
        "en": os.path.join(ROOT, "marketing", "en", "16-subscription-financial-study.md")}
MONTHS = 36
EUR_USD = 1 / 0.87          # V : 0,87 EUR pour 1 USD (relevé d'août 2026, cf. étude § 8)

# --------------------------------------------------------------------------- hypothèses
# Prix proposés (USD par mois, facturation mensuelle). Annuel = 10 mois payés pour 12.
PLANS = {
    "solo":     {"fr": "Solo", "en": "Solo", "zoneA": 9, "zoneB": 15,
                 "users": 1, "registers": 1, "warehouses": 1},
    "commerce": {"fr": "Commerce", "en": "Commerce", "zoneA": 25, "zoneB": 39,
                 "users": 5, "registers": 2, "warehouses": 1},
    "pme":      {"fr": "PME", "en": "SME", "zoneA": 59, "zoneB": 89,
                 "users": 15, "registers": 5, "warehouses": 3},
}
ANNUAL_MONTHS_PAID = 10     # H : deux mois offerts sur l'annuel

HYP = {
    # coûts d'infrastructure (USD / mois)
    "app_server": {"v": round(5.99 * EUR_USD * 1.20, 2), "src": "V",
                   "fr": "Serveur d'application Hetzner CX23 (2 vCPU, 4 Go, 40 Go) 5,99 € + sauvegardes 20 %",
                   "en": "Hetzner CX23 application server (2 vCPU, 4 GB, 40 GB) €5.99 + 20% backups"},
    "database": {"v": 25.0, "src": "V", "fr": "Base PostgreSQL gérée Supabase Pro (8 Go, sauvegarde quotidienne 7 jours)",
                 "en": "Supabase Pro managed PostgreSQL (8 GB, daily backup kept 7 days)"},
    "offsite_backup": {"v": 1.0, "src": "H", "fr": "Sauvegardes chiffrées hors site (Backblaze B2, 6,95 $/To ; 10 Go gratuits)",
                       "en": "Encrypted off-site backups (Backblaze B2, $6.95/TB; first 10 GB free)"},
    "email": {"v": 1.0, "src": "H", "fr": "Envoi d'e-mails (Amazon SES, 0,10 $ / 1 000 e-mails)",
              "en": "E-mail delivery (Amazon SES, $0.10 per 1,000 e-mails)"},
    "domain": {"v": 1.67, "src": "H", "fr": "Nom de domaine (20 $ par an)", "en": "Domain name ($20 a year)"},
    "code_signing": {"v": 9.99, "src": "H", "fr": "Signature des installateurs Windows (Azure Artifact Signing, 9,99 $ annoncés, à confirmer)",
                     "en": "Windows installer signing (Azure Artifact Signing, $9.99 quoted, to be confirmed)"},
    "tools": {"v": 45.0, "src": "H", "fr": "Outils : assistance, CRM, visioconférence, bureautique",
              "en": "Tools: help desk, CRM, video calls, office suite"},
    "legal_accounting": {"v": 150.0, "src": "H", "fr": "Comptable, frais juridiques et administratifs",
                         "en": "Accountant, legal and administrative fees"},
    "telecom_transport": {"v": 80.0, "src": "H", "fr": "Données mobiles, téléphone, déplacements de l'équipe",
                          "en": "Mobile data, phone, team travel"},
    "marketing_fixed": {"v": 150.0, "src": "H", "fr": "Supports de démonstration, impressions, petits événements",
                        "en": "Demo material, printing, small events"},
    # équipe (USD / mois, Kinshasa)
    "founder": {"v": 1000.0, "src": "H", "fr": "Rémunération du fondateur (produit, développement)",
                "en": "Founder pay (product, development)"},
    "support_agent": {"v": 500.0, "src": "H", "fr": "Agent d'accompagnement et d'assistance (1 par tranche de 150 clients actifs)",
                      "en": "Onboarding and support agent (1 per 150 active customers)"},
    "customers_per_support": {"v": 150, "src": "H", "fr": "Clients actifs suivis par agent d'assistance",
                              "en": "Active customers per support agent"},
    "sales_agent": {"v": 400.0, "src": "H", "fr": "Commercial terrain, salaire fixe (1 de plus au-delà de 12 puis 24 nouveaux clients par mois)",
                    "en": "Field sales agent, fixed pay (1 more above 12, then 24 new customers a month)"},
    "sales_bonus": {"v": 15.0, "src": "H", "fr": "Prime du commercial par nouveau client payant",
                    "en": "Sales bonus per new paying customer"},
    # coûts variables
    "variable_marketing": {"v": 10.0, "src": "H", "fr": "Déplacement et supports par nouveau client",
                           "en": "Travel and material per new customer"},
    "payment_fee": {"v": 0.035, "src": "H", "fr": "Frais d'encaissement moyens (Mobile Money 2,5-3,2 % via agrégateur, virements, cartes)",
                    "en": "Average collection fees (Mobile Money 2.5-3.2% via an aggregator, transfers, cards)"},
    "partner_share": {"v": 0.20, "src": "H", "fr": "Part des nouveaux clients apportés par un partenaire",
                      "en": "Share of new customers brought by a partner"},
    "partner_commission": {"v": 0.20, "src": "H", "fr": "Commission partenaire sur la première année encaissée",
                           "en": "Partner commission on the first year collected"},
    "referral_share": {"v": 0.15, "src": "H", "fr": "Part des nouveaux clients venus par recommandation (1 mois offert au parrain)",
                       "en": "Share of new customers from referrals (1 month free for the referrer)"},
    "discount": {"v": 0.05, "src": "H", "fr": "Remises moyennes (codes promotionnels, gestes commerciaux)",
                 "en": "Average discounts (promo codes, commercial gestures)"},
    "customers_per_app_server": {"v": 250, "src": "H", "fr": "Établissements actifs par serveur d'application (aucun test de charge réalisé)",
                                 "en": "Active establishments per application server (no load test performed)"},
    "db_upgrade_above": {"v": 500, "src": "H", "fr": "Établissements actifs au-delà desquels la base passe à la puissance supérieure (+15 $/mois)",
                         "en": "Active establishments above which the database moves to larger compute (+$15/month)"},
}
ONE_OFF = [  # (mois, USD, source, fr, en)
    (1, 1500.0, "H", "Création de la société, conseil juridique et fiscal", "Company set-up, legal and tax advice"),
    (1, 1200.0, "H", "Matériel de démonstration (imprimante de tickets, lecteur, tablette Android)", "Demo hardware (receipt printer, scanner, Android tablet)"),
    (1, 25.0, "V", "Compte développeur Google Play (paiement unique)", "Google Play developer account (one-time fee)"),
    (1, 500.0, "H", "Identité visuelle imprimée, cartes, kakémono", "Printed brand material, cards, banner"),
    (6, 5000.0, "H", "Test d'intrusion externe avant un déploiement large", "External penetration test before a wide roll-out"),
]

SCENARIOS = {
    "pessimiste": {"fr": "Pessimiste", "en": "Pessimistic", "color": "#eb6834",
                   "start": 2, "step_every": 4, "step": 1, "cap": 10, "churn": 0.05,
                   "mix": {"solo": 0.45, "commerce": 0.45, "pme": 0.10}, "annual": 0.25},
    "central": {"fr": "Central", "en": "Central", "color": "#2a78d6",
                "start": 4, "step_every": 2, "step": 1, "cap": 25, "churn": 0.03,
                "mix": {"solo": 0.35, "commerce": 0.45, "pme": 0.20}, "annual": 0.35},
    "optimiste": {"fr": "Optimiste", "en": "Optimistic", "color": "#1baf7a",
                  "start": 6, "step_every": 2, "step": 2, "cap": 45, "churn": 0.02,
                  "mix": {"solo": 0.30, "commerce": 0.45, "pme": 0.25}, "annual": 0.45},
}
FUNNEL = {"demo_to_trial": 0.5, "trial_to_paid": 0.4}   # H : pour traduire les objectifs en démonstrations


def h(k):
    return HYP[k]["v"]


def new_customers(sc, m):
    """Nouveaux clients payants au mois m (1..36)."""
    return min(sc["cap"], sc["start"] + ((m - 1) // sc["step_every"]) * sc["step"])


def simulate(key, zone="zoneA"):
    sc = SCENARIOS[key]
    churn = sc["churn"]
    annual_churn = 1 - (1 - churn) ** 12
    cohorts = []          # [start_month, plan, billing, count]
    rows, cum_cash = [], 0.0
    for m in range(1, MONTHS + 1):
        n_new = new_customers(sc, m)
        for plan, share in sc["mix"].items():
            cohorts.append([m, plan, "annual", n_new * share * sc["annual"]])
            cohorts.append([m, plan, "monthly", n_new * share * (1 - sc["annual"])])
        mrr = cash_in = active = 0.0
        partner_cost = 0.0
        for c in cohorts:
            start, plan, billing, count = c
            age = m - start
            price = PLANS[plan][zone] * (1 - h("discount"))
            if billing == "monthly":
                if age > 0:
                    count *= (1 - churn)
                    c[3] = count
                cash_in += count * price
                mrr += count * price
                first_year = age < 12
            else:
                if age > 0 and age % 12 == 0:
                    count *= (1 - annual_churn)
                    c[3] = count
                if age % 12 == 0:
                    cash_in += count * price * ANNUAL_MONTHS_PAID
                mrr += count * price * ANNUAL_MONTHS_PAID / 12
                first_year = age < 12
            active += count
            if first_year:
                partner_cost += count * price * (ANNUAL_MONTHS_PAID / 12 if billing == "annual" else 1) \
                    * h("partner_share") * h("partner_commission")
        arpa = mrr / active if active else 0.0
        # coûts
        servers = max(1, math.ceil(active / h("customers_per_app_server")))
        infra = servers * h("app_server") + h("database") + h("offsite_backup") + h("email") + h("domain") \
            + h("code_signing") + (15.0 if active > h("db_upgrade_above") else 0.0)
        support_agents = max(1, math.ceil(active / h("customers_per_support")))
        sales_agents = 1 + (1 if n_new > 12 else 0) + (1 if n_new > 24 else 0)
        team = h("founder") + support_agents * h("support_agent") + sales_agents * h("sales_agent")
        overhead = h("tools") + h("legal_accounting") + h("telecom_transport") + h("marketing_fixed")
        acquisition = n_new * (h("sales_bonus") + h("variable_marketing") + h("referral_share") * arpa)
        fees = cash_in * h("payment_fee")
        one_off = sum(v for (mm, v, *_r) in ONE_OFF if mm == m)
        costs = infra + team + overhead + acquisition + partner_cost + fees
        operating = mrr - (costs - fees) - mrr * h("payment_fee")     # résultat d'exploitation (base MRR)
        cash_flow = cash_in - costs - one_off
        cum_cash += cash_flow
        rows.append({"scenario": key, "zone": zone, "month": m, "new": round(n_new, 2), "active": round(active, 2),
                     "mrr": round(mrr, 2), "arr": round(mrr * 12, 2), "arpa": round(arpa, 2),
                     "cash_in": round(cash_in, 2), "infra": round(infra, 2), "team": round(team, 2),
                     "support_agents": support_agents, "sales_agents": sales_agents, "overhead": round(overhead, 2),
                     "acquisition": round(acquisition, 2), "partner": round(partner_cost, 2), "fees": round(fees, 2),
                     "one_off": round(one_off, 2), "costs": round(costs, 2), "operating": round(operating, 2),
                     "cash_flow": round(cash_flow, 2), "cum_cash": round(cum_cash, 2)})
    return rows


def summary(rows):
    key = rows[0]["scenario"]
    sc = SCENARIOS[key]
    be = next((r["month"] for i, r in enumerate(rows)
               if all(x["operating"] >= 0 for x in rows[i:])), None)
    min_cash = min(r["cum_cash"] for r in rows)
    min_month = next(r["month"] for r in rows if r["cum_cash"] == min_cash)
    total_new = sum(r["new"] for r in rows)
    acq_costs = sum(r["acquisition"] + r["partner"] + r["sales_agents"] * h("sales_agent") + h("marketing_fixed") for r in rows)
    cac = acq_costs / total_new
    last = rows[-1]
    revenue = sum(r["mrr"] for r in rows)
    direct = sum(r["fees"] + r["infra"] + r["support_agents"] * h("support_agent") for r in rows)
    gross_margin = 1 - direct / revenue
    arpa = last["arpa"]
    ltv = arpa * gross_margin / sc["churn"]
    payback = cac / (arpa * gross_margin) if arpa else None
    at = {m: next(r for r in rows if r["month"] == m) for m in (12, 24, 36)}
    return {"scenario": key, "break_even_month": be, "min_cum_cash": round(min_cash), "min_cash_month": min_month,
            "cac": round(cac, 1), "gross_margin": round(gross_margin, 3), "ltv": round(ltv), "ltv_cac": round(ltv / cac, 1),
            "payback_months": round(payback, 1) if payback else None, "arpa_m36": round(arpa, 2),
            "active": {m: round(at[m]["active"]) for m in at}, "mrr": {m: round(at[m]["mrr"]) for m in at},
            "arr": {m: round(at[m]["arr"]) for m in at}, "cum_cash": {m: round(at[m]["cum_cash"]) for m in at},
            "new_m12": at[12]["new"], "new_m36": at[36]["new"],
            "demos_needed_m12": math.ceil(at[12]["new"] / (FUNNEL["demo_to_trial"] * FUNNEL["trial_to_paid"]))}


def break_even_customers(zone, mix):
    """Clients actifs nécessaires pour couvrir les coûts fixes du mois type (1 commercial, 1 agent d'assistance)."""
    arpa = sum(PLANS[p][zone] * s for p, s in mix.items()) * (1 - h("discount"))
    fixed = h("app_server") + h("database") + h("offsite_backup") + h("email") + h("domain") + h("code_signing") \
        + h("founder") + h("support_agent") + h("sales_agent") + h("tools") + h("legal_accounting") \
        + h("telecom_transport") + h("marketing_fixed")
    contribution = arpa * (1 - h("payment_fee"))
    return arpa, fixed, contribution, math.ceil(fixed / contribution)


# --------------------------------------------------------------------------- sorties
def fmt(x, lang, dec=0):
    s = f"{x:,.{dec}f}"
    if lang == "fr":
        s = s.replace(",", " ").replace(".", ",")
    return s


def money(x, lang):
    return (fmt(x, lang) + " $") if lang == "fr" else ("$" + fmt(x, lang)).replace("$-", "-$")


def tables(results, sums, lang):
    L = lambda fr, en: fr if lang == "fr" else en
    out = {}
    # plans
    hdr = L("| Formule | Pour qui | Utilisateurs | Caisses POS | Dépôts | Prix mensuel, zone A | Annuel, zone A (10 mois) | Prix mensuel, zone B | Annuel, zone B |",
            "| Plan | For whom | Users | POS tills | Warehouses | Monthly price, zone A | Annual, zone A (10 months) | Monthly price, zone B | Annual, zone B |")
    who = {"solo": L("indépendant, solopreneur", "freelancer, solopreneur"),
           "commerce": L("petite entreprise, boutique", "small business, shop"),
           "pme": L("PME, plusieurs sites", "SME, several sites")}
    lines = [hdr, "|---|---|---|---|---|---|---|---|---|"]
    for k, p in PLANS.items():
        lines.append(f"| {p[lang]} | {who[k]} | {p['users']} | {p['registers']} | {p['warehouses']} | "
                     f"{money(p['zoneA'], lang)} | {money(p['zoneA'] * ANNUAL_MONTHS_PAID, lang)} | "
                     f"{money(p['zoneB'], lang)} | {money(p['zoneB'] * ANNUAL_MONTHS_PAID, lang)} |")
    lines.append(L("| Sur mesure | au-delà de 15 utilisateurs, 5 caisses ou 3 dépôts | sur devis | sur devis | sur devis | sur devis | sur devis | sur devis | sur devis |",
                   "| Custom | beyond 15 users, 5 tills or 3 warehouses | on quote | on quote | on quote | on quote | on quote | on quote | on quote |"))
    out["plans"] = "\n".join(lines)
    # hypotheses
    lines = [L("| Hypothèse | Valeur | Nature |", "| Assumption | Value | Type |"), "|---|---|---|"]
    for k, v in HYP.items():
        val = v["v"]
        if isinstance(val, float) and val < 1:
            sval = fmt(val * 100, lang, 1) + " %" if lang == "fr" else fmt(val * 100, lang, 1) + "%"
        elif k in ("customers_per_support", "customers_per_app_server", "db_upgrade_above"):
            sval = fmt(val, lang)
        else:
            mv = money(val, lang) if float(val).is_integer() else \
                ((fmt(val, lang, 2) + "\u00a0$") if lang == "fr" else "$" + fmt(val, lang, 2))
            sval = mv + L(" / mois", " / month") if k not in ("sales_bonus", "variable_marketing") \
                else mv + L(" / nouveau client", " / new customer")
        lines.append(f"| {v[lang]} | {sval} | {L('vérifiée', 'verified') if v['src'] == 'V' else L('hypothèse', 'assumption')} |")
    out["hypotheses"] = "\n".join(lines)
    lines = [L("| Mois | Dépense ponctuelle | Montant | Nature |", "| Month | One-off expense | Amount | Type |"), "|---|---|---|---|"]
    for (m, v, src, fr, en) in ONE_OFF:
        lines.append(f"| M{m} | {fr if lang == 'fr' else en} | {money(v, lang)} | {L('vérifiée', 'verified') if src == 'V' else L('hypothèse', 'assumption')} |")
    out["one_off"] = "\n".join(lines)
    # scenarios inputs
    lines = [L("| Paramètre | Pessimiste | Central | Optimiste |", "| Parameter | Pessimistic | Central | Optimistic |"), "|---|---|---|---|"]
    def row(label, f):
        lines.append(f"| {label} | " + " | ".join(f(SCENARIOS[k]) for k in SCENARIOS) + " |")
    row(L("Nouveaux clients payants au mois 1", "New paying customers in month 1"), lambda s: str(s["start"]))
    row(L("Progression", "Growth"), lambda s: L(f"+{s['step']} tous les {s['step_every']} mois", f"+{s['step']} every {s['step_every']} months"))
    row(L("Plafond mensuel de nouveaux clients", "Monthly cap on new customers"), lambda s: str(s["cap"]))
    row(L("Résiliation mensuelle", "Monthly churn"), lambda s: fmt(s["churn"] * 100, lang, 0) + L(" %", "%"))
    row(L("Répartition Solo / Commerce / PME", "Solo / Commerce / SME split"),
        lambda s: " / ".join(fmt(s["mix"][p] * 100, lang, 0) for p in PLANS) + L(" %", "%"))
    row(L("Part d'abonnements annuels", "Share of annual subscriptions"), lambda s: fmt(s["annual"] * 100, lang, 0) + L(" %", "%"))
    out["scenario_inputs"] = "\n".join(lines)
    # results
    lines = [L("| Indicateur | Pessimiste | Central | Optimiste |", "| Indicator | Pessimistic | Central | Optimistic |"), "|---|---|---|---|"]
    def r2(label, f):
        lines.append(f"| {label} | " + " | ".join(f(sums[k]) for k in SCENARIOS) + " |")
    for m in (12, 24, 36):
        r2(L(f"Clients actifs, mois {m}", f"Active customers, month {m}"), lambda s, m=m: fmt(s["active"][m], lang))
    for m in (12, 24, 36):
        r2(L(f"Revenu mensuel récurrent (MRR), mois {m}", f"Monthly recurring revenue (MRR), month {m}"), lambda s, m=m: money(s["mrr"][m], lang))
    r2(L("Revenu annuel récurrent (ARR), mois 36", "Annual recurring revenue (ARR), month 36"), lambda s: money(s["arr"][36], lang))
    r2(L("Revenu moyen par client (ARPA), mois 36", "Average revenue per account (ARPA), month 36"), lambda s: money(s["arpa_m36"], lang) + L(" / mois", " / month"))
    r2(L("Mois d'équilibre d'exploitation durable", "Month of lasting operating break-even"),
       lambda s: ("M" + str(s["break_even_month"])) if s["break_even_month"] else L("non atteint en 36 mois", "not reached in 36 months"))
    r2(L("Besoin de trésorerie maximal (creux cumulé)", "Maximum cash need (cumulative low point)"),
       lambda s: money(-s["min_cum_cash"], lang) + L(f" au mois {s['min_cash_month']}", f" in month {s['min_cash_month']}"))
    r2(L("Trésorerie cumulée, mois 36", "Cumulative cash, month 36"), lambda s: money(s["cum_cash"][36], lang))
    r2(L("Coût d'acquisition moyen (CAC)", "Average acquisition cost (CAC)"), lambda s: money(s["cac"], lang))
    r2(L("Marge brute (encaissement, serveurs, assistance)", "Gross margin (collection, servers, support)"), lambda s: fmt(s["gross_margin"] * 100, lang, 0) + L(" %", "%"))
    r2(L("Valeur vie client (LTV)", "Customer lifetime value (LTV)"), lambda s: money(s["ltv"], lang))
    r2(L("LTV / CAC", "LTV / CAC"), lambda s: fmt(s["ltv_cac"], lang, 1))
    r2(L("Récupération du CAC", "CAC payback"), lambda s: fmt(s["payback_months"], lang, 1) + L(" mois", " months"))
    r2(L("Démonstrations nécessaires au mois 12", "Demos needed in month 12"), lambda s: fmt(s["demos_needed_m12"], lang))
    out["results"] = "\n".join(lines)
    # break-even customers
    lines = [L("| Zone de prix | ARPA (mix central, remises déduites) | Coûts fixes du mois type | Contribution par client | Clients actifs pour l'équilibre |",
               "| Price zone | ARPA (central mix, after discounts) | Fixed costs of a typical month | Contribution per customer | Active customers to break even |"),
             "|---|---|---|---|---|"]
    for zone, label in (("zoneA", L("A (lancement)", "A (launch)")), ("zoneB", "B")):
        arpa, fixed, contrib, n = break_even_customers(zone, SCENARIOS["central"]["mix"])
        m2 = lambda x: (fmt(x, lang, 2) + "\u00a0$") if lang == "fr" else "$" + fmt(x, lang, 2)
        lines.append(f"| {label} | {m2(arpa)} | {money(fixed, lang)} | {m2(contrib)} | {fmt(n, lang)} |")
    out["break_even"] = "\n".join(lines)
    # monthly costs central at M1 / M12 / M36
    rows = results["central"]
    lines = [L("| Poste (scénario central) | Mois 1 | Mois 12 | Mois 24 | Mois 36 |", "| Item (central scenario) | Month 1 | Month 12 | Month 24 | Month 36 |"), "|---|---|---|---|---|"]
    pick = [r for r in rows if r["month"] in (1, 12, 24, 36)]
    for fld, label in (("infra", L("Infrastructure et signature", "Infrastructure and signing")),
                       ("team", L("Équipe", "Team")), ("overhead", L("Outils, comptable, télécoms, marketing fixe", "Tools, accountant, telecoms, fixed marketing")),
                       ("acquisition", L("Acquisition variable (primes, déplacements, parrainage)", "Variable acquisition (bonuses, travel, referrals)")),
                       ("partner", L("Commissions partenaires", "Partner commissions")), ("fees", L("Frais d'encaissement", "Collection fees")),
                       ("costs", L("Total des coûts récurrents", "Total recurring costs")), ("mrr", L("Revenu mensuel récurrent", "Monthly recurring revenue")),
                       ("operating", L("Résultat d'exploitation", "Operating result"))):
        lines.append(f"| {label} | " + " | ".join(money(r[fld], lang) for r in pick) + " |")
    out["costs"] = "\n".join(lines)
    # coût de service par client actif (scénario central)
    lines = [L("| Coût de service par client actif et par mois (central) | Mois 12 | Mois 36 |",
               "| Cost to serve per active customer per month (central) | Month 12 | Month 36 |"), "|---|---|---|"]
    pk = [r for r in rows if r["month"] in (12, 36)]
    for label, f in ((L("Serveurs, base, sauvegardes, e-mails, signature", "Servers, database, backups, e-mail, signing"), lambda r: r["infra"] / r["active"]),
                     (L("Assistance (agents)", "Support (agents)"), lambda r: r["support_agents"] * h("support_agent") / r["active"]),
                     (L("Frais d'encaissement", "Collection fees"), lambda r: r["fees"] / r["active"]),
                     (L("Total", "Total"), lambda r: (r["infra"] + r["support_agents"] * h("support_agent") + r["fees"]) / r["active"]),
                     (L("Revenu moyen par client (ARPA)", "Average revenue per account (ARPA)"), lambda r: r["arpa"])):
        lines.append(f"| {label} | " + " | ".join((fmt(f(r), lang, 2) + "\u00a0$") if lang == "fr" else "$" + fmt(f(r), lang, 2) for r in pk) + " |")
    out["unit_costs"] = "\n".join(lines)
    # sensitivity: price ±20 %, churn ±1 pt
    lines = [L("| Variante du scénario central | Mois d'équilibre | Besoin de trésorerie maximal | Trésorerie cumulée, mois 36 |",
               "| Variant of the central scenario | Break-even month | Maximum cash need | Cumulative cash, month 36 |"), "|---|---|---|---|"]
    for label, mutate in sensitivity_variants(lang):
        s = run_variant(mutate)
        lines.append(f"| {label} | " + (("M" + str(s["break_even_month"])) if s["break_even_month"] else L("non atteint", "not reached"))
                     + f" | {money(-s['min_cum_cash'], lang)} | {money(s['cum_cash'][36], lang)} |")
    out["sensitivity"] = "\n".join(lines)
    return out


def sensitivity_variants(lang):
    L = lambda fr, en: fr if lang == "fr" else en
    return [
        (L("Scénario central tel quel", "Central scenario as is"), lambda: None),
        (L("Prix −20 %", "Prices −20%"), lambda: [PLANS[p].__setitem__("zoneA", PLANS[p]["zoneA"] * 0.8) for p in PLANS]),
        (L("Prix +20 %", "Prices +20%"), lambda: [PLANS[p].__setitem__("zoneA", PLANS[p]["zoneA"] * 1.2) for p in PLANS]),
        (L("Résiliation 4 % par mois", "Churn 4% a month"), lambda: SCENARIOS["central"].__setitem__("churn", 0.04)),
        (L("Fondateur non rémunéré pendant 36 mois", "Founder unpaid for 36 months"), lambda: HYP["founder"].__setitem__("v", 0.0)),
        (L("Base de données auto-hébergée (2e CX23 au lieu de Supabase)", "Self-hosted database (2nd CX23 instead of Supabase)"),
         lambda: HYP["database"].__setitem__("v", HYP["app_server"]["v"])),
    ]


def run_variant(mutate):
    import copy
    saved = (copy.deepcopy(PLANS), copy.deepcopy(SCENARIOS), copy.deepcopy(HYP))
    try:
        mutate()
        return summary(simulate("central"))
    finally:
        PLANS.clear(); PLANS.update(saved[0]); SCENARIOS.clear(); SCENARIOS.update(saved[1]); HYP.clear(); HYP.update(saved[2])


def charts(results):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 9, "svg.fonttype": "none",
                         "axes.edgecolor": "#8a8f98", "axes.labelcolor": "#52514e", "xtick.color": "#52514e",
                         "ytick.color": "#52514e", "axes.spines.top": False, "axes.spines.right": False})
    specs = [("mrr", {"fr": "Revenu mensuel récurrent (USD)", "en": "Monthly recurring revenue (USD)"}),
             ("active", {"fr": "Clients actifs", "en": "Active customers"}),
             ("cum_cash", {"fr": "Trésorerie cumulée (USD)", "en": "Cumulative cash (USD)"})]
    for lang in ("fr", "en"):
        for fld, title in specs:
            fig, ax = plt.subplots(figsize=(6.4, 3.0), dpi=100)
            fig.patch.set_facecolor("#fcfcfb"); ax.set_facecolor("#fcfcfb")
            ax.grid(axis="y", color="#e4e4e0", linewidth=0.8); ax.set_axisbelow(True)
            if fld == "cum_cash":
                ax.axhline(0, color="#8a8f98", linewidth=0.8)
            for key, sc in SCENARIOS.items():
                xs = [r["month"] for r in results[key]]; ys = [r[fld] for r in results[key]]
                ax.plot(xs, ys, color=sc["color"], linewidth=2, label=sc[lang], solid_capstyle="round")
                ax.annotate(sc[lang], (xs[-1], ys[-1]), xytext=(6, 0), textcoords="offset points",
                            va="center", fontsize=8.5, color="#0b0b0b")
            ax.set_xlim(1, 36); ax.set_xticks([1, 6, 12, 18, 24, 30, 36])
            ax.set_xlabel("Mois" if lang == "fr" else "Month")
            ax.set_title(title[lang], loc="left", fontsize=10, color="#0b0b0b")
            sep = " " if lang == "fr" else ","
            ax.yaxis.set_major_formatter(matplotlib.ticker.FuncFormatter(lambda v, _p, sep=sep: f"{v:,.0f}".replace(",", sep)))
            ax.legend(frameon=False, loc="upper left", fontsize=8.5)
            fig.subplots_adjust(right=0.86, left=0.12, bottom=0.17, top=0.88)
            fig.savefig(os.path.join(VIS, f"{fld}-{lang}.svg"), format="svg")
            plt.close(fig)


def inject(path, blocks, check):
    text = open(path, encoding="utf-8").read()
    new = text
    for name, content in blocks.items():
        # le bloc s'arrête au premier marqueur de fin (jamais au-delà d'un autre bloc)
        pat = re.compile(r"<!-- model:%s -->\n(?:(?!<!-- /model -->).)*?<!-- /model -->" % re.escape(name), re.S)
        if not pat.search(new):
            raise SystemExit(f"{os.path.relpath(path, ROOT)} : bloc <!-- model:{name} --> absent")
        new = pat.sub(lambda m: f"<!-- model:{name} -->\n{content}\n<!-- /model -->", new)
    if check:
        return new == text
    if new != text:
        open(path, "w", encoding="utf-8").write(new)
    return True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    results = {k: simulate(k) for k in SCENARIOS}
    sums = {k: summary(results[k]) for k in SCENARIOS}
    ok = True
    for lang, path in DOCS.items():
        if os.path.exists(path):
            ok &= inject(path, tables(results, sums, lang), a.check)
    if a.check:
        print("documents à jour" if ok else "documents périmés : relancer marketing/tools/subscription_model.py")
        return 0 if ok else 1
    os.makedirs(DATA, exist_ok=True); os.makedirs(VIS, exist_ok=True)
    with open(os.path.join(DATA, "scenarios-mensuels.csv"), "w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(results["central"][0].keys()))
        w.writeheader()
        for k in SCENARIOS:
            w.writerows(results[k])
    json.dump({"hypotheses": HYP, "plans": PLANS, "scenarios": SCENARIOS, "one_off": ONE_OFF, "funnel": FUNNEL,
               "summary": sums}, open(os.path.join(DATA, "resultats.json"), "w", encoding="utf-8"),
              indent=1, ensure_ascii=False)
    charts(results)
    for k, s in sums.items():
        print(k, json.dumps(s, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
