#!/usr/bin/env python3
"""Nhill — génère les pages HTML du site vitrine à partir de website/src/.

Les pages sources (website/src/pages/<langue>/<page>.html) ne contiennent que le contenu
principal, précédé d'un en-tête JSON dans un commentaire :

    <!--{"title": "…", "description": "…", "nav": "pilot", "pair": "pilot-programme"}-->

Le gabarit commun (en-tête, navigation, pied de page, balises SEO, hreflang) est ajouté
ici, pour que les 24 pages restent cohérentes. Résultat : HTML statique, sans framework.

  python3 website/tools/build_site.py [--base-url https://www.exemple.cd] [--check]

--check : échoue si les pages générées ne correspondent pas aux sources (CI).
L'adresse publique vient de --base-url, sinon de SITE_PUBLIC_BASE_URL, sinon de
config/site.json (« publicBaseUrl »).
"""
import argparse
import html
import json
import os
import re
import sys

SITE = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))

NAV = {
    "fr": [("features", "fonctionnalites", "Fonctionnalités"), ("uses", "cas-usage", "Cas d'usage"),
           ("pilot", "programme-pilote", "Programme pilote"), ("partners", "partenaires", "Partenaires"),
           ("access", "acces", "Accès et tarifs"), ("faq", "faq", "Questions"), ("contact", "contact", "Contact")],
    "en": [("features", "features", "Features"), ("uses", "use-cases", "Use cases"),
           ("pilot", "pilot-programme", "Pilot programme"), ("partners", "partners", "Partners"),
           ("access", "access", "Access and pricing"), ("faq", "faq", "Questions"), ("contact", "contact", "Contact")],
}
FOOT = {
    "fr": {"product": "Produit", "company": "Nhill", "legal": "Informations légales",
           "privacy": ("confidentialite", "Politique de confidentialité"), "terms": ("conditions", "Conditions d'utilisation"),
           "notice": ("mentions-legales", "Mentions légales"),
           "fine": "Nhill est un logiciel de gestion commerciale. Il n'est pas un système de facturation "
                   "normalisée homologué par la DGI. Les captures d'écran montrent l'application réelle avec des "
                   "données de démonstration.",
           "skip": "Aller au contenu", "menu": "Menu", "lang": "English", "home": "Accueil"},
    "en": {"product": "Product", "company": "Nhill", "legal": "Legal information",
           "privacy": ("privacy", "Privacy policy"), "terms": ("terms", "Terms of use"),
           "notice": ("legal-notice", "Legal notice"),
           "fine": "Nhill is business management software. It is not a standardised invoicing system "
                   "homologated by the DRC tax authority (DGI). Screenshots show the real application with demo data.",
           "skip": "Skip to content", "menu": "Menu", "lang": "Français", "home": "Home"},
}


def out_path(lang, slug):
    if lang == "fr":
        return "index.html" if slug == "index" else ("404.html" if slug == "404" else f"pages/{slug}.html")
    return f"en/{slug}.html"


def rel_root(path):
    depth = path.count("/")
    return "." if depth == 0 else "/".join([".."] * depth)


def href(from_path, to_path):
    return os.path.relpath(to_path, os.path.dirname(from_path) or ".").replace(os.sep, "/")


def render(lang, slug, meta, body, base_url):
    path = out_path(lang, slug)
    root = rel_root(path)
    if slug == "404":
        # Served for any missing URL, at any depth: links must be absolute from the site root.
        root = ""
    # {{root}} -> relative path to the site root; {{p:slug}} -> link to a page of the same language
    body = body.replace("{{root}}", root)
    def L(target_path):
        return "/" + target_path if slug == "404" else href(path, target_path)
    link = lambda target: L(out_path(lang, target))
    body = re.sub(r"\{\{p:([a-z0-9-]+)\}\}", lambda m: link(m.group(1)), body)
    f = FOOT[lang]
    other = "en" if lang == "fr" else "fr"
    pair = meta.get("pair", slug)
    pair_path = out_path(other, pair)
    canonical = f"{base_url}/{path}".replace("/index.html", "/")
    alt_url = f"{base_url}/{pair_path}".replace("/index.html", "/")
    nav_items = []
    for key, s, label in NAV[lang]:
        cur = ' aria-current="page"' if meta.get("nav") == key else ""
        nav_items.append(f'<a href="{L(out_path(lang, s))}"{cur}>{html.escape(label)}</a>')
    nav_items.append(f'<a class="lang" href="{L(pair_path)}" lang="{other}" hreflang="{other}">{f["lang"]}</a>')
    foot_product = "".join(f'<li><a href="{L(out_path(lang, s))}">{html.escape(label)}</a></li>' for _, s, label in NAV[lang])
    legal = "".join(f'<li><a href="{L(out_path(lang, f[k][0]))}">{html.escape(f[k][1])}</a></li>' for k in ("privacy", "terms", "notice"))
    title = meta["title"] if slug == "index" else f'{meta["title"]} — Nhill'
    robots = '<meta name="robots" content="noindex">' if meta.get("noindex") else ""
    ld = ""
    if slug == "index":
        ld = ('<script type="application/ld+json">' + json.dumps({
            "@context": "https://schema.org", "@type": "SoftwareApplication", "name": "Nhill",
            "applicationCategory": "BusinessApplication", "operatingSystem": "Windows, Linux, Android",
            "softwareVersion": "2.0.0", "inLanguage": ["fr", "en"],
            "description": meta["description"], "url": canonical}, ensure_ascii=False) + "</script>")
    return f"""<!doctype html>
<html lang="{lang}">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{html.escape(title)}</title>
<meta name="description" content="{html.escape(meta['description'])}">
{robots}
<link rel="canonical" href="{canonical}">
<link rel="alternate" hreflang="{lang}" href="{canonical}">
<link rel="alternate" hreflang="{other}" href="{alt_url}">
<meta property="og:type" content="website">
<meta property="og:title" content="{html.escape(meta['title'])}">
<meta property="og:description" content="{html.escape(meta['description'])}">
<meta property="og:url" content="{canonical}">
<meta property="og:image" content="{base_url}/assets/images/capture-caisse-ticket.webp">
<meta property="og:locale" content="{'fr_CD' if lang == 'fr' else 'en'}">
<meta name="theme-color" content="#0E2236">
<link rel="icon" href="{root}/assets/images/nhill-mark.svg" type="image/svg+xml">
<link rel="preload" href="{root}/assets/fonts/IBMPlexSans-SemiBold.ttf" as="font" type="font/ttf" crossorigin>
<link rel="stylesheet" href="{root}/assets/css/site.css">
{ld}
</head>
<body data-root="{root}">
<a class="skip" href="#contenu">{f['skip']}</a>
<header class="site-head">
  <div class="wrap">
    <a class="brand" href="{L(out_path(lang, 'index'))}"><img src="{root}/assets/images/nhill-mark-reverse.svg" alt="" width="30" height="30"><span>Nhill</span></a>
    <button class="nav-toggle" type="button" aria-expanded="false" aria-controls="nav">{f['menu']}</button>
    <nav id="nav" class="nav" aria-label="{'Navigation principale' if lang == 'fr' else 'Main navigation'}">
      {chr(10).join('      ' + n for n in nav_items).strip()}
    </nav>
  </div>
</header>
<main id="contenu">
{body.strip()}
</main>
<footer class="site-foot">
  <div class="wrap">
    <div class="cols">
      <div><h2>Nhill</h2><p>{html.escape(meta.get('footer_tagline') or ('Gestion commerciale et caisse pour les commerçants et les PME.' if lang == 'fr' else 'Business management and point of sale for shops and SMEs.'))}</p></div>
      <div><h2>{f['product']}</h2><ul>{foot_product}</ul></div>
      <div><h2>{f['legal']}</h2><ul>{legal}</ul></div>
    </div>
    <p class="fine">{html.escape(f['fine'])}</p>
  </div>
</footer>
<script src="{root}/assets/js/site.js" defer></script>
</body>
</html>
"""


def parse(src_path):
    text = open(src_path, encoding="utf-8").read()
    m = re.match(r"\s*<!--(\{.*?\})-->\s*", text, re.S)
    if not m:
        raise SystemExit(f"{src_path}: en-tête JSON manquant")
    return json.loads(m.group(1)), text[m.end():]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--base-url")
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    cfg = json.load(open(os.path.join(SITE, "config", "site.json"), encoding="utf-8"))
    base = (a.base_url or os.environ.get("SITE_PUBLIC_BASE_URL") or cfg.get("publicBaseUrl") or "").rstrip("/")
    if not base:
        raise SystemExit("adresse publique du site inconnue : --base-url, SITE_PUBLIC_BASE_URL ou config/site.json")
    outputs = {}
    for lang in ("fr", "en"):
        d = os.path.join(SITE, "src", "pages", lang)
        for fn in sorted(os.listdir(d)):
            if fn.endswith(".html"):
                meta, body = parse(os.path.join(d, fn))
                slug = fn[:-5]
                outputs[out_path(lang, slug)] = render(lang, slug, meta, body, base)
    urls = [p for p in outputs if not p.endswith("404.html")]
    outputs["sitemap.xml"] = ('<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n'
                              + "".join(f"  <url><loc>{base}/{p}</loc></url>\n".replace("/index.html<", "/<") for p in sorted(urls))
                              + "</urlset>\n")
    outputs["robots.txt"] = f"User-agent: *\nAllow: /\nDisallow: /api/\nSitemap: {base}/sitemap.xml\n"
    stale = []
    for rel, content in outputs.items():
        p = os.path.join(SITE, rel)
        if a.check:
            if not os.path.exists(p) or open(p, encoding="utf-8").read() != content:
                stale.append(rel)
            continue
        os.makedirs(os.path.dirname(p), exist_ok=True)
        open(p, "w", encoding="utf-8").write(content)
    if a.check:
        for s in stale:
            print("  page à régénérer :", s)
        print(f"{len(outputs)} fichiers, {len(stale)} à régénérer")
        return 1 if stale else 0
    print(f"{len(outputs)} fichiers générés (adresse publique : {base})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
