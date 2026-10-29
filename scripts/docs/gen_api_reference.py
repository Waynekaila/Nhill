#!/usr/bin/env python3
"""Generate the Nhill API reference (French and English) from the server's source code.

  python3 scripts/docs/gen_api_reference.py          # writes docs/fr/reference-api.md, docs/en/api-reference.md
  python3 scripts/docs/gen_api_reference.py --check  # exit 1 if the files are not up to date

Every column except "Description" comes from the code: route declarations
(server/src/services/*.cpp, server/src/setup.cpp), role permissions (server/src/http/app.cpp)
and Idempotency-Key handling (calls to idempotent()). Descriptions live in api_descriptions.py;
a route without a description, or a description without a route, is an error.
"""
import argparse
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import api_routes  # noqa: E402
from api_descriptions import D, SECTIONS  # noqa: E402

ROOT = api_routes.ROOT
OUT = {"fr": os.path.join(ROOT, "docs", "fr", "reference-api.md"),
       "en": os.path.join(ROOT, "docs", "en", "api-reference.md")}


def version():
    m = re.search(r"project\(nhill VERSION ([0-9.]+)", open(os.path.join(ROOT, "CMakeLists.txt"), encoding="utf-8").read())
    return m.group(1) if m else "?"


def key(r):
    return ("SETUP " if r["mode"] == "setup" else "") + f'{r["method"]} {r["path"]}'


def idempotent_routes():
    """Routes whose handler calls idempotent(): the text between two declarations."""
    out = set()
    sdir = os.path.join(ROOT, "server", "src", "services")
    for fn in sorted(os.listdir(sdir)):
        if not fn.endswith(".cpp"):
            continue
        text = open(os.path.join(sdir, fn), encoding="utf-8").read()
        decls = list(api_routes.DECL.finditer(text))
        for i, m in enumerate(decls):
            end = decls[i + 1].start() if i + 1 < len(decls) else len(text)
            if "idempotent(tx, ctx" in text[m.end():end]:
                out.add(f"{api_routes.METHODS[m.group(1)]} {m.group(2)}")
    return out


def role_permissions():
    """Role -> permissions, parsed from App::roleHas (owner and admin also get every pos.* permission)."""
    text = open(os.path.join(ROOT, "server", "src", "http", "app.cpp"), encoding="utf-8").read()
    body = text[text.index("bool App::roleHas"):text.index("std::string App::permissionFeature")]
    pos_all = re.findall(r'"(pos\.[a-z]+)"', body[body.index("posAll"):body.index(";", body.index("posAll"))])
    roles = {}
    for m in re.finditer(r'\{"([a-z_]+)",\s*\{([^{}]*)\}\}', body):
        roles[m.group(1)] = re.findall(r'"([a-z.]+)"', m.group(2))
    for r in ("owner", "admin"):
        roles[r] = roles[r] + [p for p in pos_all if p not in roles[r]]
    return roles


T = {
    "fr": dict(
        title="Nhill — Référence de l'API v1",
        other="*English: [../en/api-reference.md](../en/api-reference.md)*",
        intro=("Document **généré** depuis le code du serveur par `scripts/docs/gen_api_reference.py` "
               "(ne pas modifier à la main : modifier le code ou `scripts/docs/api_descriptions.py`, puis relancer "
               "le script ; l'intégration continue vérifie que ce fichier est à jour). Version {v} : "
               "{n} points d'accès en fonctionnement normal et {s} en mode installation."),
        conv="Conventions",
        conv_items=[
            "Base : `https://<adresse du serveur>/api/v1` ; l'adresse est saisie dans les applications, aucune "
            "adresse n'y est compilée. JSON UTF-8. Langue des messages et des PDF : en-tête `Accept-Language` "
            "(`en…` → anglais, sinon français).",
            "Authentification : `Authorization: Bearer <jeton d'accès>` (15 minutes par défaut, "
            "`NH_ACCESS_TOKEN_MINUTES`), renouvelé par `POST /api/v1/auth/refresh`. La console utilise un jeton "
            "d'administration distinct, non interchangeable.",
            "Montants : chaînes décimales (`\"1234.50\"`) ; quantités : chaînes (`\"2.5\"`) ; taux : points de base "
            "entiers (`1600` = 16 %). Dates `AAAA-MM-JJ`, horodatages ISO 8601 UTC.",
            "Colonne « Idem. » : l'en-tête `Idempotency-Key` (UUID) est obligatoire "
            "(`400 idempotency_key_required`) ; un rejeu renvoie la même réponse avec `Idempotent-Replayed: true`.",
            "Pagination : `limit` (1 à 200), `offset`. Rapports : `from`, `to`, `format=json|csv|pdf`.",
            "Erreurs : `{\"error\": {\"code\", \"message\", \"details\", \"request_id\"}}`.",
        ],
        access="Contrôles d'accès",
        access_items=[
            "**Public** : sans jeton. **Utilisateur** : membre d'un établissement. **Console** : administrateur de la "
            "plateforme (colonne « Rôles console » ; `superadmin` a tous les droits). **Webhook** : signature du "
            "prestataire. **Installation** : serveur sans base, jeton d'installation affiché dans son journal.",
            "« Permission » : droit du rôle exigé (tableau ci-dessous) ; sinon `403 permission_denied`.",
            "« Formule » : fonction que la formule de l'établissement doit comprendre (`a|b` : l'une ou l'autre) ; "
            "sinon `403 feature_not_included`. Une permission `pos.*`, `purchases.*` ou `expenses.*` exige aussi la "
            "fonction correspondante. Certaines routes vérifient en plus une fonction selon le contenu : type de "
            "document (devis et commandes → `sales.quotes`, factures et avoirs → `sales.invoices`), devise autre que "
            "la devise principale (`multi_currency`), dépôt autre que le dépôt principal (`stock.multi_warehouse`). "
            "Les listes ne renvoient que les données des fonctions comprises.",
            "« Écrit. » : écriture permise seulement avec un accès complet ; abonnement en lecture seule → "
            "`402 subscription_suspended` ; établissement suspendu par la plateforme → `403 establishment_suspended`. "
            "Établissement fermé ou sans accès → `403 establishment_closed` sur toutes les routes utilisateur.",
            "Limites de la formule (utilisateurs, caisses, dépôts, produits, documents par mois, stockage) : "
            "dépassement → `409 plan_limit_reached` (`user_limit_reached` pour les utilisateurs, `plan_register_limit` "
            "pour les caisses), avec `limit_key`, `limit` et `used` dans `details`.",
        ],
        roles="Permissions par rôle",
        roles_head="| Rôle | Permissions |",
        role_names={"owner": "Propriétaire (`owner`)", "admin": "Administrateur (`admin`)", "manager": "Gérant (`manager`)",
                    "sales": "Vendeur (`sales`)", "accountant": "Comptable (`accountant`)", "viewer": "Lecture seule (`viewer`)",
                    "cash_manager": "Responsable de caisse (`cash_manager`)", "cashier": "Caissier (`cashier`)"},
        none="aucune écriture",
        head="| Méthode | Chemin | Accès | Permission | Formule | Écrit. | Idem. | Rôles console | Description |",
        access_names={"Public": "Public", "User": "Utilisateur", "Admin": "Console", "Webhook": "Webhook",
                      "Setup": "Installation"},
        yes="oui",
        counts="Récapitulatif",
        counts_head="| Section | Points d'accès |",
    ),
    "en": dict(
        title="Nhill — API v1 reference",
        other="*Français : [../fr/reference-api.md](../fr/reference-api.md)*",
        intro=("**Generated** from the server's code by `scripts/docs/gen_api_reference.py` (do not edit by hand: "
               "change the code or `scripts/docs/api_descriptions.py`, then run the script again; continuous "
               "integration checks that this file is up to date). Version {v}: {n} endpoints in normal operation "
               "and {s} in setup mode."),
        conv="Conventions",
        conv_items=[
            "Base: `https://<server address>/api/v1`; the address is entered in the applications, none is compiled "
            "into them. UTF-8 JSON. Language of messages and PDFs: `Accept-Language` header (`en…` → English, "
            "otherwise French).",
            "Authentication: `Authorization: Bearer <access token>` (15 minutes by default, "
            "`NH_ACCESS_TOKEN_MINUTES`), renewed by `POST /api/v1/auth/refresh`. The console uses a separate "
            "administration token that cannot be swapped.",
            "Amounts: decimal strings (`\"1234.50\"`); quantities: strings (`\"2.5\"`); rates: integer basis points "
            "(`1600` = 16 %). Dates `YYYY-MM-DD`, timestamps ISO 8601 UTC.",
            "\"Idem.\" column: the `Idempotency-Key` header (UUID) is required (`400 idempotency_key_required`); a "
            "replay returns the same answer with `Idempotent-Replayed: true`.",
            "Pagination: `limit` (1 to 200), `offset`. Reports: `from`, `to`, `format=json|csv|pdf`.",
            "Errors: `{\"error\": {\"code\", \"message\", \"details\", \"request_id\"}}`.",
        ],
        access="Access checks",
        access_items=[
            "**Public**: no token. **User**: member of a business. **Console**: platform administrator (\"Console "
            "roles\" column; `superadmin` has every right). **Webhook**: provider signature. **Setup**: server "
            "without a database, setup token printed in its log.",
            "\"Permission\": right the role must have (table below); otherwise `403 permission_denied`.",
            "\"Plan\": feature the business's plan must include (`a|b`: either); otherwise `403 feature_not_included`. "
            "A `pos.*`, `purchases.*` or `expenses.*` permission also requires the matching feature. Some routes also "
            "check a feature from the content: document type (quotes and orders → `sales.quotes`, invoices and credit "
            "notes → `sales.invoices`), a currency other than the main currency (`multi_currency`), a warehouse other "
            "than the main one (`stock.multi_warehouse`). Lists only return data of the included features.",
            "\"Write\": writing allowed only with full access; read-only subscription → `402 subscription_suspended`; "
            "business suspended by the platform → `403 establishment_suspended`. Closed business or no access → "
            "`403 establishment_closed` on every user route.",
            "Plan limits (users, tills, warehouses, products, documents per month, storage): exceeded → "
            "`409 plan_limit_reached` (`user_limit_reached` for users, `plan_register_limit` for tills), with "
            "`limit_key`, `limit` and `used` in `details`.",
        ],
        roles="Permissions per role",
        roles_head="| Role | Permissions |",
        role_names={"owner": "Owner (`owner`)", "admin": "Administrator (`admin`)", "manager": "Manager (`manager`)",
                    "sales": "Salesperson (`sales`)", "accountant": "Accountant (`accountant`)", "viewer": "Read-only (`viewer`)",
                    "cash_manager": "Till supervisor (`cash_manager`)", "cashier": "Cashier (`cashier`)"},
        none="no write access",
        head="| Method | Path | Access | Permission | Plan | Write | Idem. | Console roles | Description |",
        access_names={"Public": "Public", "User": "User", "Admin": "Console", "Webhook": "Webhook", "Setup": "Setup"},
        yes="yes",
        counts="Summary",
        counts_head="| Section | Endpoints |",
    ),
}


def cell(s):
    return s.replace("|", "\\|") if s else "—"


def code(s):
    return f"`{s}`" if s else "—"


def render(lang, rs, idem, roles):
    t = T[lang]
    i = 0 if lang == "fr" else 1
    normal = [r for r in rs if r["mode"] == "normal"]
    setup = [r for r in rs if r["mode"] == "setup"]
    out = [f"# {t['title']}", "", t["other"], "",
           t["intro"].format(v=version(), n=len(normal), s=len(setup)), "",
           f"## {t['conv']}", ""]
    out += [f"* {x}" for x in t["conv_items"]] + ["", f"## {t['access']}", ""]
    out += [f"* {x}" for x in t["access_items"]] + ["", f"## {t['roles']}", "", t["roles_head"], "|---|---|"]
    for role in ("owner", "admin", "manager", "sales", "accountant", "viewer", "cash_manager", "cashier"):
        perms = roles.get(role, [])
        out.append(f"| {t['role_names'][role]} | " + (", ".join(f"`{p}`" for p in perms) if perms else t["none"]) + " |")
    out.append("")
    counts = []
    placed = set()
    for sid, fr, en, pred in SECTIONS:
        sec = [r for r in rs if key(r) not in placed and pred(r["mode"], r["path"])]
        if not sec:
            continue
        title = fr if lang == "fr" else en
        out += [f"## {title}", "", t["head"], "|---|---|---|---|---|---|---|---|---|"]
        for r in sec:
            placed.add(key(r))
            k = key(r)
            out.append("| " + " | ".join([
                r["method"], f'`{r["path"]}`', t["access_names"][r["access"]], code(r["permission"]),
                cell(code(r["feature"])), t["yes"] if r["write"] else "—",
                t["yes"] if f'{r["method"]} {r["path"]}' in idem and r["mode"] == "normal" else "—",
                ", ".join(f"`{x}`" for x in r["roles"]) if r["roles"] else "—",
                cell(D[k][i].replace("\n", " ")),
            ]) + " |")
        out.append("")
        counts.append((title, len(sec)))
    missing = [key(r) for r in rs if key(r) not in placed]
    if missing:
        raise SystemExit("routes outside every section: " + ", ".join(missing))
    out += [f"## {t['counts']}", "", t["counts_head"], "|---|---|"]
    out += [f"| {a} | {b} |" for a, b in counts]
    out += [f"| **Total** | **{len(rs)}** |", ""]
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    rs = api_routes.routes()
    keys = {key(r) for r in rs}
    no_desc = sorted(keys - set(D))
    stale = sorted(set(D) - keys)
    if no_desc or stale:
        for k in no_desc:
            print(f"route sans description : {k}", file=sys.stderr)
        for k in stale:
            print(f"description sans route : {k}", file=sys.stderr)
        return 1
    idem = idempotent_routes()
    roles = role_permissions()
    bad = 0
    for lang, path in OUT.items():
        text = render(lang, rs, idem, roles)
        if a.check:
            cur = open(path, encoding="utf-8").read() if os.path.exists(path) else ""
            if cur != text:
                print(f"périmé : {os.path.relpath(path, ROOT)} (relancer scripts/docs/gen_api_reference.py)")
                bad = 1
        else:
            open(path, "w", encoding="utf-8").write(text)
            print(f"{os.path.relpath(path, ROOT)} : {len(rs)} points d'accès")
    return bad


if __name__ == "__main__":
    sys.exit(main())
