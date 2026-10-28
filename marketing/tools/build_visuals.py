#!/usr/bin/env python3
"""Nhill — génère les sources HTML modifiables des visuels (FR/EN) puis leurs exports.

  python3 marketing/tools/build_visuals.py            # sources + exports PNG/PDF
  python3 marketing/tools/build_visuals.py --no-export

Sources : marketing/visuals/src/*.html (+ visuals.css) — modifiables dans un éditeur de
texte ou un navigateur. Exports : marketing/visuals/export/ (Playwright/Chromium).
Captures d'écran : écrans réels de Nhill 2.0 (docs/assets/v2/, CI, données de démonstration).
Coordonnées : nhill.online et info@nhill.online (aucun numéro de téléphone n'est publié).
Signature e-mail nominative : --sig-name "Prénom Nom" --sig-role "Fonction" (sinon signature d'équipe).
"""
import argparse, os, sys

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
SRC = os.path.join(ROOT, "marketing", "visuals", "src")
EXP = os.path.join(ROOT, "marketing", "visuals", "export")
R = "../../.."   # depuis src/ vers la racine du dépôt
DASH_PHONE = f"{R}/docs/assets/v2/business-telephone-05-tableau-de-bord.png"
DASH_DESK = f"{R}/docs/assets/v2/business-1366x768-05-tableau-de-bord.png"
JOURNAL = f"{R}/docs/assets/v2/business-1366x768-15b-journal-activite.png"
LOGO_REV = f"{R}/assets/brand/nhill-logo-reverse.svg"
LOGO = f"{R}/assets/brand/nhill-logo.svg"

T = {
 "fr": dict(
  head="Vos ventes, votre stock et votre caisse, au clair.",
  sub="Nhill, le logiciel de gestion pour les commerçants et PME de Kinshasa.",
  b1="Chaque vente enregistrée au nom du caissier, le stock suit.",
  b2="Clôture de caisse : le caissier compte, Nhill calcule l'écart.",
  b3="Depuis votre téléphone, vous voyez tout, même absent.",
  cta="Programme pilote gratuit — 8 semaines",
  cta2="Installation, formation et suivi inclus. Sans engagement.",
  contact="nhill.online · info@nhill.online",
  legal="Nhill est un logiciel de gestion ; il ne remplace pas la facture normalisée de la DGI.",
  shot="Capture réelle de Nhill 2.0 — établissement de démonstration, données fictives.",
  sq_head="La caisse du soir, <span class='gold'>juste</span>.",
  sq_sub="Ventes, stock et caisse au clair — même quand vous n'êtes pas au magasin.",
  pilot_title="Programme pilote Nhill",
  pilot_sub="Huit semaines pour tester Nhill dans votre commerce. Gratuit, accompagné, sans engagement d'achat.",
  give="Ce que nous apportons", ask="Ce que nous vous demandons",
  give_l=["Nhill Business, et Nhill POS si vous avez une caisse","Installation sur vos appareils et import de vos produits","Formation de chaque utilisateur","Un point par semaine et une assistance aux heures de travail","À la fin, l'export de vos données (CSV, PDF)"],
  ask_l=["Utiliser Nhill pour une partie réelle de votre activité","Une personne référente, 20 à 30 minutes par semaine","Signaler les problèmes, répondre à deux courts questionnaires","Un ordinateur Windows ou Linux, ou une tablette Android 7+, et internet"],
  steps="Comment ça se passe", steps_l=["Candidature","Démonstration sur vos produits et convention de pilote","Mise en route : une demi-journée","Huit semaines d'utilisation, un point par semaine","Bilan, et conditions écrites pour continuer si vous le souhaitez"],
  apply="Candidater", 
  part_title="Devenir partenaire de Nhill",
  part_sub="Vous connaissez les commerçants et les PME ? Aidez-les à mieux suivre leurs ventes, leur stock et leur caisse.",
  part_rows=[("Recommandation","Vous présentez Nhill à votre réseau.","Commission convenue par écrit sur les abonnements des clients apportés."),
             ("Installation et assistance","Vous installez, importez les données, assurez le premier niveau.","Vos prestations, facturées à vos clients ; commission éventuelle."),
             ("Formation","Vous formez les utilisateurs avec nos supports.","Votre formation ; supports Nhill gratuits."),
             ("Accompagnement d'entrepreneurs","Vous proposez Nhill aux entreprises que vous suivez.","Places au programme pilote, atelier gratuit pour vos membres.")],
  part_cols=("Forme","Votre rôle","Ce que vous y gagnez"),
  part_rules="Nos règles communes", part_rules_l=["Présenter Nhill tel qu'il est : un logiciel de gestion, non homologué pour la facture normalisée DGI","Ne transmettre un contact qu'avec l'accord de la personne","Ne jamais demander de mot de passe à un commerçant","Aucun partenariat annoncé sans contrat signé"],
  sig_role="", sig_name="L'équipe Nhill Solution",
  lang_label="Français"),
 "en": dict(
  head="Your sales, stock and till, clear.",
  sub="Nhill, management software for traders and SMEs in Kinshasa.",
  b1="Every sale recorded under the cashier's name; stock follows.",
  b2="Till closing: the cashier counts, Nhill works out the difference.",
  b3="From your phone, you see everything, even when away.",
  cta="Free pilot programme — 8 weeks",
  cta2="Installation, training and follow-up included. No commitment.",
  contact="nhill.online · info@nhill.online",
  legal="Nhill is management software; it does not replace the DGI standardised invoice.",
  shot="Real Nhill 2.0 screenshot (French interface) — demo business, fictitious data.",
  sq_head="The evening till, <span class='gold'>right</span>.",
  sq_sub="Sales, stock and till, clear — even when you're not in the shop.",
  pilot_title="Nhill pilot programme",
  pilot_sub="Eight weeks to test Nhill in your business. Free, supported, no purchase commitment.",
  give="What we provide", ask="What we ask",
  give_l=["Nhill Business, and Nhill POS if you have a till","Installation on your devices and import of your products","Training for each user","A weekly check-in and support during working hours","At the end, the export of your data (CSV, PDF)"],
  ask_l=["Use Nhill for a real part of your business","A contact person, 20 to 30 minutes a week","Report problems, answer two short questionnaires","A Windows or Linux computer, or an Android 7+ tablet, and Internet"],
  steps="How it works", steps_l=["Application","Demo with your products and pilot agreement","Onboarding: half a day","Eight weeks of use, a weekly check-in","Review, and written terms to continue if you wish"],
  apply="Apply",
  part_title="Become a Nhill partner",
  part_sub="You know traders and SMEs? Help them keep better track of their sales, stock and till.",
  part_rows=[("Referral","You introduce Nhill to your network.","Commission agreed in writing on referred customers' subscriptions."),
             ("Installation and support","You install, import data, provide first-level support.","Your services, invoiced to your customers; possible commission."),
             ("Training","You train users with our materials.","Your training; Nhill materials free."),
             ("Entrepreneur support","You offer Nhill to the businesses you support.","Pilot places, free workshop for your members.")],
  part_cols=("Type","Your role","What you get"),
  part_rules="Our shared rules", part_rules_l=["Present Nhill as it is: management software, not certified for the DGI standardised invoice","Only pass on a contact with the person's consent","Never ask a trader for a password","No partnership announced without a signed contract"],
  sig_role="", sig_name="The Nhill Solution team",
  lang_label="English"),
}

ASCENT = """<svg class="ascent" style="{style}" viewBox="0 0 400 300" preserveAspectRatio="xMinYMax meet" aria-hidden="true">
<path d="M10 290 L150 290 L250 190 L390 40" stroke-width="{w}"/><polygon class="flag" points="372,10 400,22 372,34"/></svg>"""

def page(title, size_css, body, lang):
    return f"""<!doctype html>
<html lang="{lang}"><head><meta charset="utf-8"><title>{title}</title>
<meta name="viewport" content="width=device-width, initial-scale=1">
<link rel="stylesheet" href="visuals.css">
<style>@page {{ size: {size_css}; margin: 0; }} {{}}</style>
</head><body>
{body}
</body></html>
"""

def poster(t, lang):  # A3 portrait 297 x 420 mm
    b = f"""<div class="sheet dark" style="width:297mm;height:420mm;padding:22mm 20mm">
<img src="{LOGO_REV}" alt="Nhill" style="height:20mm">
<h1 style="font-weight:600;font-size:23mm;line-height:1.02;letter-spacing:-.02em;margin-top:22mm;max-width:240mm">{t['head']}</h1>
<p class="muted" style="font-size:7.5mm;margin-top:9mm;max-width:220mm">{t['sub']}</p>
<div style="display:flex;gap:14mm;margin-top:16mm;align-items:flex-start">
  <ul class="ticks" style="flex:1;font-size:7mm;line-height:1.35;display:grid;gap:7mm;margin-top:6mm">
    <li>{t['b1']}</li><li>{t['b2']}</li><li>{t['b3']}</li></ul>
  <div class="phone" style="width:92mm;flex:none"><img src="{DASH_PHONE}" alt=""></div>
</div>
<div style="position:absolute;left:20mm;right:20mm;bottom:20mm">
  <p style="font-size:11mm;font-weight:600" class="gold">{t['cta']}</p>
  <p style="font-size:6mm;margin-top:3mm">{t['cta2']}</p>
  <p class="mono" style="font-size:6mm;margin-top:6mm">{t['contact']}</p>
  <hr class="rule" style="margin:7mm 0 4mm">
  <p class="note muted" style="font-size:3.6mm">{t['legal']} {t['shot']}</p>
</div></div>"""
    return page("Nhill — affiche A3" if lang == "fr" else "Nhill — A3 poster", "297mm 420mm", b, lang)

def square(t, lang):  # 1080 x 1080 px
    b = f"""<div class="sheet dark" style="width:1080px;height:1080px;padding:72px">
<img src="{LOGO_REV}" alt="Nhill" style="height:64px">
<h1 style="font-weight:600;font-size:92px;line-height:1.02;letter-spacing:-.02em;margin-top:56px;max-width:560px">{t['sq_head']}</h1>
<p class="muted" style="font-size:30px;line-height:1.35;margin-top:28px;max-width:500px">{t['sq_sub']}</p>
<div class="phone" style="position:absolute;right:72px;top:150px;width:330px"><img src="{DASH_PHONE}" alt=""></div>
{ASCENT.format(style="left:0;bottom:190px;width:560px;height:150px", w=4)}
<div style="position:absolute;left:72px;bottom:64px;right:72px">
<p class="gold" style="font-size:36px;font-weight:600">{t['cta']}</p>
<p class="note muted" style="font-size:16px;margin-top:14px">{t['shot']}</p></div></div>"""
    return page("Nhill — visuel 1080" if lang == "fr" else "Nhill — 1080 visual", "1080px 1080px", b, lang)

def story(t, lang):  # 1080 x 1920 px
    b = f"""<div class="sheet dark" style="width:1080px;height:1920px;padding:96px 84px">
<img src="{LOGO_REV}" alt="Nhill" style="height:72px">
<h1 style="font-weight:600;font-size:104px;line-height:1.02;letter-spacing:-.02em;margin-top:80px">{t['sq_head']}</h1>
<p class="muted" style="font-size:36px;line-height:1.35;margin-top:32px">{t['sq_sub']}</p>
<div class="phone" style="margin:56px auto 0;width:420px"><img src="{DASH_PHONE}" alt=""></div>
<div style="position:absolute;left:84px;right:84px;bottom:96px">
<p class="gold" style="font-size:46px;font-weight:600">{t['cta']}</p>
<p style="font-size:30px;margin-top:16px">{t['cta2']}</p>
<p class="note muted" style="font-size:20px;margin-top:28px">{t['shot']}</p></div></div>"""
    return page("Nhill — statut 1080x1920" if lang == "fr" else "Nhill — 1080x1920 status", "1080px 1920px", b, lang)

def li(items): return "".join(f"<li>{x}</li>" for x in items)

def pilot(t, lang):  # A5 portrait 148 x 210 mm
    steps = "".join(f"<li style='margin-bottom:1.6mm'><span class='mono' style='color:var(--gold-text)'>{i}</span>&nbsp; {s}</li>" for i, s in enumerate(t['steps_l'], 1))
    b = f"""<div class="sheet" style="width:148mm;height:210mm;padding:11mm 11mm 9mm">
<div style="display:flex;justify-content:space-between;align-items:center">
<img src="{LOGO}" alt="Nhill" style="height:9mm"></div>
<h1 style="font-size:9.5mm;line-height:1.05;margin-top:7mm;color:var(--night-700);font-weight:600">{t['pilot_title']}</h1>
<p style="font-size:3.6mm;line-height:1.4;margin-top:2.5mm" class="muted">{t['pilot_sub']}</p>
<div style="display:grid;grid-template-columns:1fr 1fr;gap:6mm;margin-top:6mm;font-size:3.2mm;line-height:1.35">
<div><h2 style="font-size:3.8mm;margin-bottom:2mm;color:var(--night-700)">{t['give']}</h2><ul class="ticks" style="display:grid;gap:1.6mm">{li(t['give_l'])}</ul></div>
<div><h2 style="font-size:3.8mm;margin-bottom:2mm;color:var(--night-700)">{t['ask']}</h2><ul class="ticks" style="display:grid;gap:1.6mm">{li(t['ask_l'])}</ul></div></div>
<h2 style="font-size:3.8mm;margin:7mm 0 2.5mm;color:var(--night-700)">{t['steps']}</h2>
<ol style="list-style:none;font-size:3.2mm;line-height:1.3">{steps}</ol>
<div style="position:absolute;left:11mm;right:11mm;bottom:9mm">
<p style="font-size:3.8mm;font-weight:600;color:var(--night-700)">{t['apply']} : <span class="mono" style="font-weight:400">{t['contact']}</span></p>
<p class="note muted" style="font-size:2.5mm;margin-top:2mm">{t['legal']} {t['shot']}</p></div></div>"""
    return page("Nhill — fiche pilote A5" if lang == "fr" else "Nhill — A5 pilot sheet", "148mm 210mm", b, lang)

def partner(t, lang):  # A4 portrait
    rows = "".join(f"<tr><th style='text-align:left;padding:3mm;color:var(--night-700);width:44mm'>{a}</th><td style='padding:3mm'>{b}</td><td style='padding:3mm'>{c}</td></tr>" for a, b, c in t['part_rows'])
    c1, c2, c3 = t['part_cols']
    b = f"""<div class="sheet" style="width:210mm;height:297mm;padding:16mm 16mm 12mm">
<div class="dark" style="margin:-16mm -16mm 0;padding:14mm 16mm 12mm;position:relative">
<img src="{LOGO_REV}" alt="Nhill" style="height:11mm">
<h1 style="font-size:12mm;line-height:1.05;margin-top:9mm;font-weight:600">{t['part_title']}</h1>
<p class="muted" style="font-size:4.6mm;line-height:1.4;margin-top:3mm;max-width:125mm">{t['part_sub']}</p>
{ASCENT.format(style="right:10mm;bottom:8mm;width:42mm;height:32mm", w=3)}</div>
<table style="width:100%;border-collapse:collapse;margin-top:9mm;font-size:3.6mm;line-height:1.4">
<thead><tr style="background:var(--night-700);color:#fff"><th style="text-align:left;padding:3mm">{c1}</th><th style="text-align:left;padding:3mm">{c2}</th><th style="text-align:left;padding:3mm">{c3}</th></tr></thead>
<tbody>{rows}</tbody></table>
<h2 style="font-size:4.6mm;margin:9mm 0 3mm;color:var(--night-700)">{t['part_rules']}</h2>
<ul class="ticks" style="font-size:3.6mm;line-height:1.45;display:grid;gap:2mm">{li(t['part_rules_l'])}</ul>
<div class="screen" style="margin-top:7mm;height:44mm"><img src="{JOURNAL}" alt=""></div>
<div style="position:absolute;left:16mm;right:16mm;bottom:12mm">
<p class="mono" style="font-size:4mm">{t['contact']}</p>
<p class="note muted" style="font-size:2.8mm;margin-top:2mm">{t['legal']} {t['shot']}</p></div></div>"""
    return page("Nhill — fiche partenaire A4" if lang == "fr" else "Nhill — A4 partner sheet", "210mm 297mm", b, lang)

def signature(t, lang, name=None, role=None):
    # Courriel : styles en ligne, sans image distante (souvent bloquée par les messageries) ;
    # le domaine est écrit en clair : le lien https:// n'est ajouté qu'une fois le site vérifié
    # (website/tools/check_live.py).
    import html as _h
    name = _h.escape(name or t['sig_name'])
    role = _h.escape(role or t['sig_role'])
    role_line = f'<span style="color:#56606B">{role} — Nhill Solution</span><br>\n' if role else ""
    support_label = "assistance&nbsp;:" if lang == "fr" else "support:"
    return f"""<!doctype html>
<html lang="{lang}"><head><meta charset="utf-8"><title>Nhill — {'signature e-mail' if lang == 'fr' else 'e-mail signature'}</title></head>
<body style="margin:24px;background:#F7F5F0">
<!-- {'Copier le tableau ci-dessous dans le client de messagerie.' if lang == 'fr' else 'Copy the table below into the e-mail client.'} -->
<table cellpadding="0" cellspacing="0" role="presentation" style="font-family:Arial,Helvetica,sans-serif;color:#14181F;font-size:13px;line-height:1.45">
<tr><td style="padding-right:14px;border-right:3px solid #B7862A;vertical-align:top">
<strong style="font-size:20px;letter-spacing:-.01em;color:#173F5F">Nhill</strong></td>
<td style="padding-left:14px;vertical-align:top">
<strong style="font-size:14px;color:#173F5F">{name}</strong><br>
{role_line}<a href="mailto:info@nhill.online" style="color:#173F5F">info@nhill.online</a> · {support_label} <a href="mailto:support@nhill.online" style="color:#173F5F">support@nhill.online</a><br>
<span style="color:#8E6519">nhill.online</span></td></tr>
<tr><td colspan="2" style="padding-top:10px;font-size:11px;color:#56606B">{t['cta']}</td></tr>
</table></body></html>
"""

VISUALS = [("affiche-a3", "a3-poster", poster, ("297mm", "420mm")),
           ("visuel-lancement-1080", "launch-visual-1080", square, (1080, 1080)),
           ("statut-whatsapp-1080x1920", "whatsapp-status-1080x1920", story, (1080, 1920)),
           ("fiche-pilote-a5", "pilot-sheet-a5", pilot, ("148mm", "210mm")),
           ("fiche-partenaire-a4", "partner-sheet-a4", partner, ("210mm", "297mm"))]

def main():
    ap = argparse.ArgumentParser(); ap.add_argument("--no-export", action="store_true")
    ap.add_argument("--sig-name"); ap.add_argument("--sig-role"); a = ap.parse_args()
    os.makedirs(SRC, exist_ok=True); os.makedirs(EXP, exist_ok=True)
    jobs = []
    for lang in ("fr", "en"):
        t = T[lang]
        for fr_name, en_name, fn, size in VISUALS:
            name = f"{fr_name if lang == 'fr' else en_name}-{lang}"
            p = os.path.join(SRC, name + ".html")
            open(p, "w", encoding="utf-8").write(fn(t, lang)); jobs.append((p, name, size))
        sp = os.path.join(SRC, f"signature-email-{lang}.html")
        open(sp, "w", encoding="utf-8").write(signature(t, lang, a.sig_name, a.sig_role))
    # Les modèles de messages renvoient vers marketing/visuals/signature-email-<lang>.html
    for lang in ("fr", "en"):
        open(os.path.join(ROOT, "marketing", "visuals", f"signature-email-{lang}.html"), "w", encoding="utf-8").write(
            open(os.path.join(SRC, f"signature-email-{lang}.html"), encoding="utf-8").read())
    print(f"{len(jobs)} sources de visuels + 2 signatures écrites dans {SRC}")
    if a.no_export:
        return 0
    from playwright.sync_api import sync_playwright
    with sync_playwright() as pw:
        br = pw.chromium.launch()
        for path, name, size in jobs:
            pg = br.new_page(device_scale_factor=1)
            if isinstance(size[0], int):
                pg.set_viewport_size({"width": size[0], "height": size[1]})
                pg.goto("file://" + path); pg.wait_for_load_state("networkidle")
                pg.screenshot(path=os.path.join(EXP, name + ".png"), full_page=False)
            else:
                pg.goto("file://" + path); pg.wait_for_load_state("networkidle")
                pg.pdf(path=os.path.join(EXP, name + ".pdf"), width=size[0], height=size[1], print_background=True,
                       margin={"top": "0", "right": "0", "bottom": "0", "left": "0"})
                # aperçu PNG (96 dpi) pour le partage et la relecture
                mm = lambda v: int(float(v[:-2]) / 25.4 * 96)
                pg.set_viewport_size({"width": mm(size[0]), "height": mm(size[1])})
                pg.screenshot(path=os.path.join(EXP, name + "-apercu.png" if "-fr" in name else name + "-preview.png"))
            pg.close()
        br.close()
    print("exports :", EXP)
    return 0

if __name__ == "__main__":
    sys.exit(main())
