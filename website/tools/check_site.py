#!/usr/bin/env python3
"""Contrôles automatiques du site vitrine Nhill (sans dépendance externe).

  python3 website/tools/check_site.py [website/]

Vérifie :
  * pages générées à jour par rapport à website/src (build_site.py --check) ;
  * une seule balise <h1>, <title> et meta description présents, attribut lang ;
  * liens internes et ressources (images, CSS, JS) existants, ancres présentes ;
  * attribut alt sur chaque image ; libellé <label for> pour chaque champ de formulaire ;
  * paires de langue (hreflang) existantes ;
  * aucun secret dans les fichiers publics (motifs de mots de passe, clés, .env) ;
  * config/site.json sans clé secrète ;
  * aucune grille ni mention de tarifs sur les pages publiques (choix commercial).
Code de sortie 1 au premier défaut trouvé (tous les défauts sont listés).
"""
import html.parser
import json
import os
import re
import subprocess
import sys
import urllib.parse

SITE = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(os.path.dirname(__file__), ".."))
PUBLIC_EXT = (".html", ".css", ".js", ".json", ".txt", ".xml", ".svg")
PRICING_RE = re.compile(r"(?i)\btarifs?\b|\btarifaire|\bpricing\b|\bprice list\b|"
                        r"\d[\d\s.,]*\s?(?:CDF|USD|FC|EUR|\$|€)\s?(?:/|par|per|a|an)\s?(?:mois|month|an|year|utilisateur|user)\b")
SECRET_RE = re.compile(r"(?i)(password\s*[:=]\s*['\"][^'\"\[]{4,}|secret\s*[:=]\s*['\"][^'\"\[]{4,}|sk_(live|test)_|AKIA[0-9A-Z]{16}|-----BEGIN [A-Z ]*PRIVATE KEY|SITE_DATABASE_PASSWORD\s*=\s*\S+)")


class Page(html.parser.HTMLParser):
    def __init__(self):
        super().__init__()
        self.h1 = 0
        self.title = False
        self.desc = False
        self.lang = None
        self.links, self.ids, self.imgs_noalt, self.inputs, self.labels = [], set(), [], [], set()

    def handle_starttag(self, tag, attrs):
        a = dict(attrs)
        if "id" in a:
            self.ids.add(a["id"])
        if tag == "html":
            self.lang = a.get("lang")
        elif tag == "h1":
            self.h1 += 1
        elif tag == "title":
            self.title = True
        elif tag == "meta" and a.get("name") == "description" and a.get("content"):
            self.desc = True
        elif tag == "a" and a.get("href"):
            self.links.append(a["href"])
        elif tag in ("img", "script") and a.get("src"):
            self.links.append(a["src"])
            if tag == "img" and "alt" not in a:
                self.imgs_noalt.append(a["src"])
        elif tag == "link" and a.get("href") and a.get("rel") in ("stylesheet", "icon", "preload"):
            self.links.append(a["href"])
        elif tag in ("input", "select", "textarea") and a.get("type") != "hidden":
            self.inputs.append(a.get("id"))
        elif tag == "label" and a.get("for"):
            self.labels.add(a["for"])


def main():
    problems = []
    gen = subprocess.run([sys.executable, os.path.join(SITE, "tools", "build_site.py"), "--check"], capture_output=True, text=True)
    if gen.returncode != 0:
        problems.append("pages générées périmées : lancez python3 website/tools/build_site.py\n" + gen.stdout)
    pages = []
    for d, dirs, files in os.walk(SITE):
        dirs[:] = [x for x in dirs if x not in ("src", "backend", "tools", "node_modules")]
        pages += [os.path.join(d, f) for f in files if f.endswith(".html")]
    parsed = {}
    for p in sorted(pages):
        rel = os.path.relpath(p, SITE)
        pg = Page()
        pg.feed(open(p, encoding="utf-8").read())
        parsed[p] = pg
        if pg.h1 != 1:
            problems.append(f"{rel}: {pg.h1} balise(s) <h1> (1 attendue)")
        if not pg.title or not pg.desc:
            problems.append(f"{rel}: <title> ou meta description manquant")
        if pg.lang not in ("fr", "en"):
            problems.append(f"{rel}: attribut lang absent ou inattendu ({pg.lang})")
        for src in pg.imgs_noalt:
            problems.append(f"{rel}: image sans alt ({src})")
        for i in pg.inputs:
            if not i or i not in pg.labels:
                problems.append(f"{rel}: champ de formulaire sans <label for> ({i})")
    for p, pg in parsed.items():
        rel = os.path.relpath(p, SITE)
        if rel.endswith("404.html"):
            continue  # liens absolus depuis la racine, vérifiés ci-dessous
        for link in pg.links:
            u = urllib.parse.urlparse(link)
            if u.scheme in ("http", "https", "mailto", "tel") or link.startswith("#") and not link[1:]:
                continue
            target = os.path.normpath(os.path.join(os.path.dirname(p), urllib.parse.unquote(u.path))) if u.path else p
            if not os.path.exists(target):
                problems.append(f"{rel}: lien cassé {link}")
                continue
            if u.fragment and target.endswith(".html"):
                tp = parsed.get(target)
                if tp and u.fragment not in tp.ids:
                    problems.append(f"{rel}: ancre absente {link}")
    for p, pg in parsed.items():
        if p.endswith("404.html"):
            for link in pg.links:
                path = urllib.parse.urlparse(link).path
                if link.startswith("/") and not os.path.exists(os.path.join(SITE, path.lstrip("/"))):
                    problems.append(f"{os.path.relpath(p, SITE)}: lien absolu cassé {link}")
    # Commercial choice: no price list on the public site (offers are discussed with the sales team).
    for p in parsed:
        text = open(p, encoding="utf-8").read()
        m = PRICING_RE.search(re.sub(r"<[^>]+>", " ", text))
        if m:
            problems.append(f"{os.path.relpath(p, SITE)}: mention de tarifs interdite ({m.group(0)})")
    for d, dirs, files in os.walk(SITE):
        dirs[:] = [x for x in dirs if x not in ("backend", "node_modules", "__pycache__")]
        for f in files:
            path = os.path.join(d, f)
            rel = os.path.relpath(path, SITE)
            if f == ".env" or f.endswith(".sqlite3"):
                problems.append(f"{rel}: fichier privé dans l'arborescence publique")
            if f.endswith(PUBLIC_EXT):
                text = open(path, encoding="utf-8", errors="replace").read()
                m = SECRET_RE.search(text)
                if m:
                    problems.append(f"{rel}: secret possible : {m.group(0)[:40]}")
    cfg = json.load(open(os.path.join(SITE, "config", "site.json"), encoding="utf-8"))
    for k in cfg:
        if re.search(r"(?i)pass|secret|token|key$", k):
            problems.append(f"config/site.json : clé interdite côté navigateur : {k}")
    print(f"{len(pages)} pages contrôlées, {len(problems)} problème(s)")
    for p in problems:
        print("  " + p)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
