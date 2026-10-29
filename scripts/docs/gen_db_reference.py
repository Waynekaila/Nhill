#!/usr/bin/env python3
"""Generate the Nhill database reference (French, English) from a migrated database.

  python3 scripts/docs/gen_db_reference.py --server build/server/nhill-server          # temporary database
  python3 scripts/docs/gen_db_reference.py --server build/server/nhill-server --check  # exit 1 if out of date
  python3 scripts/docs/gen_db_reference.py --dsn "host=… dbname=nhill user=postgres"     # existing database

With --server, a throwaway PostgreSQL cluster is created (initdb from NH_PG_BIN or the newest
/usr/lib/postgresql/*/bin), db/bootstrap/00-roles.sql is run, the migrations of db/migrations are
applied by `nhill-server migrate`, then the schema is read and the cluster deleted.
Output: docs/fr/reference-base-de-donnees.md and docs/en/database-reference.md.
"""
import argparse
import glob
import json
import os
import re
import secrets
import shutil
import socket
import subprocess
import sys
import tempfile

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from db_descriptions import GROUPS, MIGRATIONS, T  # noqa: E402

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
OUT = {"fr": os.path.join(ROOT, "docs", "fr", "reference-base-de-donnees.md"),
       "en": os.path.join(ROOT, "docs", "en", "database-reference.md")}

Q_TABLES = """
SELECT coalesce(json_agg(t ORDER BY t.name), '[]') FROM (
  SELECT c.relname AS name, c.relrowsecurity AS rls, c.relforcerowsecurity AS force_rls,
         pg_get_userbyid(c.relowner) AS owner,
         (SELECT coalesce(json_agg(json_build_object('name', a.attname, 'type', format_type(a.atttypid, a.atttypmod),
                  'notnull', a.attnotnull, 'default', pg_get_expr(d.adbin, d.adrelid),
                  'identity', a.attidentity <> '') ORDER BY a.attnum), '[]')
            FROM pg_attribute a LEFT JOIN pg_attrdef d ON d.adrelid = a.attrelid AND d.adnum = a.attnum
           WHERE a.attrelid = c.oid AND a.attnum > 0 AND NOT a.attisdropped) AS columns,
         (SELECT coalesce(json_agg(json_build_object('type', k.contype, 'def', pg_get_constraintdef(k.oid))
                  ORDER BY k.contype, pg_get_constraintdef(k.oid)), '[]')
            FROM pg_constraint k WHERE k.conrelid = c.oid AND k.contype IN ('p', 'u', 'f')) AS constraints,
         (SELECT count(*) FROM pg_constraint k WHERE k.conrelid = c.oid AND k.contype = 'c') AS checks,
         (SELECT coalesce(json_agg(json_build_object('name', p.polname, 'cmd', p.polcmd) ORDER BY p.polname), '[]')
            FROM pg_policy p WHERE p.polrelid = c.oid) AS policies,
         (SELECT coalesce(json_agg(t.tgname ORDER BY t.tgname), '[]')
            FROM pg_trigger t WHERE t.tgrelid = c.oid AND NOT t.tgisinternal) AS triggers,
         (SELECT coalesce(json_agg(p ORDER BY p), '[]') FROM unnest(ARRAY['SELECT','INSERT','UPDATE','DELETE']) p
           WHERE has_table_privilege('nhill_app', c.oid, p)) AS app_privileges
    FROM pg_class c JOIN pg_namespace n ON n.oid = c.relnamespace
   WHERE n.nspname = 'nhill' AND c.relkind IN ('r', 'p')) t;
"""

Q_FUNCS = """
SELECT coalesce(json_agg(f ORDER BY f.name, f.args), '[]') FROM (
  SELECT p.proname AS name, pg_get_function_identity_arguments(p.oid) AS args,
         pg_get_function_result(p.oid) AS result, p.prosecdef AS definer,
         CASE p.provolatile WHEN 'i' THEN 'immutable' WHEN 's' THEN 'stable' ELSE 'volatile' END AS volatility,
         has_function_privilege('nhill_app', p.oid, 'EXECUTE') AS app_exec,
         coalesce(array_to_string(p.proconfig, ', '), '') AS config
    FROM pg_proc p JOIN pg_namespace n ON n.oid = p.pronamespace
   WHERE n.nspname = 'nhill' AND p.prokind = 'f') f;
"""

Q_MIGR = "SELECT coalesce(json_agg(m ORDER BY m.version), '[]') FROM (SELECT * FROM nhill.schema_migrations) m;"


def psql(dsn, sql):
    r = subprocess.run(["psql", dsn, "-X", "-At", "-v", "ON_ERROR_STOP=1", "-c", sql],
                       capture_output=True, text=True, check=True)
    return json.loads(r.stdout)


def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    p = s.getsockname()[1]
    s.close()
    return p


class TempDatabase:
    def __init__(self, server):
        self.server = os.path.abspath(server)
        b = os.environ.get("NH_PG_BIN") or sorted(glob.glob("/usr/lib/postgresql/*/bin"),
                                                  key=lambda p: [int(x) for x in re.findall(r"\d+", p)])[-1]
        self.bin = b
        self.root = os.geteuid() == 0
        self.dir = tempfile.mkdtemp(prefix="nhdbref-", dir="/tmp")
        self.port = free_port()

    def run(self, args, **kw):
        if self.root:
            args = ["runuser", "-u", "postgres", "--"] + args
        return subprocess.run(args, check=True, capture_output=True, text=True, **kw)

    def __enter__(self):
        if self.root:
            shutil.chown(self.dir, "postgres", "postgres")
        data = os.path.join(self.dir, "data")
        self.run([f"{self.bin}/initdb", "-D", data, "-A", "trust", "-U", "postgres", "-E", "UTF8", "--locale=C.UTF-8"])
        self.run([f"{self.bin}/pg_ctl", "-D", data, "-o", f"-p {self.port} -k {self.dir} -c listen_addresses=''",
                  "-l", os.path.join(self.dir, "log"), "-w", "start"])
        base = f"host={self.dir} port={self.port}"
        subprocess.run(["psql", f"{base} user=postgres dbname=postgres", "-X", "-q", "-v", "ON_ERROR_STOP=1",
                        "-v", "owner_password='owner'", "-v", "app_password='app'",
                        "-f", os.path.join(ROOT, "db", "bootstrap", "00-roles.sql")], check=True, capture_output=True)
        cfg = os.path.join(self.dir, "server.env")
        with open(cfg, "w") as f:
            f.write(f"NH_MIGRATION_DATABASE_URL={base} user=nhill_owner password=owner dbname=nhill\n"
                    f"NH_DATABASE_URL={base} user=nhill_app password=app dbname=nhill\n"
                    f"NH_SECRET_KEY={secrets.token_hex(32)}\n"
                    f"NH_MIGRATIONS_DIR={os.path.join(ROOT, 'db', 'migrations')}\n")
        os.chmod(cfg, 0o600)
        subprocess.run([self.server, "--config", cfg, "migrate"], check=True, capture_output=True)
        self.dsn = f"{base} user=postgres dbname=nhill"
        return self

    def __exit__(self, *a):
        try:
            self.run([f"{self.bin}/pg_ctl", "-D", os.path.join(self.dir, "data"), "-m", "immediate", "stop"])
        finally:
            shutil.rmtree(self.dir, ignore_errors=True)


L = {
    "fr": dict(
        title="Nhill — Référence de la base de données",
        other="*English: [../en/database-reference.md](../en/database-reference.md)*",
        intro=("Document **généré** par `scripts/docs/gen_db_reference.py` à partir d'une base PostgreSQL migrée "
               "avec les migrations du dépôt (ne pas modifier à la main ; l'intégration continue vérifie qu'il est à "
               "jour). Version {v} : {t} tables et {f} fonctions dans le schéma `nhill`, {m} migrations."),
        principles="Principes",
        principles_items=[
            "Deux rôles (`db/bootstrap/00-roles.sql`) : `nhill_owner` possède le schéma et applique les migrations, "
            "il n'est jamais utilisé par l'API ; `nhill_app` est le rôle du serveur en fonctionnement : il ne possède "
            "rien, n'a aucun droit de définition (DDL) et reste soumis à la sécurité au niveau des lignes.",
            "Isolement des établissements : les tables de gestion des établissements ont la sécurité au niveau "
            "des lignes **activée et forcée** ; le serveur fixe `app.establishment_id` dans chaque transaction et les "
            "politiques ne laissent voir que les lignes de cet établissement. Les tables de la plateforme sont lues "
            "par les requêtes du serveur avec les seuls droits nécessaires au rôle applicatif (voir chaque table).",
            "Montants : `bigint` en unités mineures avec une devise `char(3)` ; taux : entiers en points de base ; "
            "quantités : `bigint` en millièmes ; horodatages `timestamptz` (UTC).",
            "Migrations : fichiers `db/migrations/NNNN_nom.sql`, appliqués dans l'ordre par `nhill-server migrate` ; "
            "chaque migration appliquée est enregistrée avec son empreinte et une migration modifiée après coup est "
            "refusée ; un serveur refuse une base migrée par une version plus récente.",
            "Suppression d'un établissement : fonction `nh_erase_establishment` (propriétaire des tables, 90 jours "
            "après la fin de l'accès ; voir la console).",
        ],
        migrations="Migrations", mig_head="| Migration | Objet |",
        tables="Tables", table_rls="Sécurité au niveau des lignes", forced="activée et forcée", enabled="activée",
        off="non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)",
        app="Droits de `nhill_app`", none="aucun", policies="Politiques", triggers="Déclencheurs",
        cols_head="| Colonne | Type | Non nul | Défaut |", yes="oui", constraints="Contraintes",
        checks="contrôle(s) `CHECK`", ident="identité",
        functions="Fonctions", func_head="| Fonction | Résultat | Propriétés | Exécutable par `nhill_app` |",
        definer="`SECURITY DEFINER`", summary="Récapitulatif", group_head="| Domaine | Tables |",
    ),
    "en": dict(
        title="Nhill — Database reference",
        other="*Français : [../fr/reference-base-de-donnees.md](../fr/reference-base-de-donnees.md)*",
        intro=("**Generated** by `scripts/docs/gen_db_reference.py` from a PostgreSQL database migrated with the "
               "repository's migrations (do not edit by hand; continuous integration checks that it is up to date). "
               "Version {v}: {t} tables and {f} functions in the `nhill` schema, {m} migrations."),
        principles="Principles",
        principles_items=[
            "Two roles (`db/bootstrap/00-roles.sql`): `nhill_owner` owns the schema and applies the migrations, it is "
            "never used by the API; `nhill_app` is the running server's role: it owns nothing, has no definition (DDL) "
            "rights and remains subject to row-level security.",
            "Business isolation: the business-management tables have row-level security **enabled and forced**; the "
            "server sets `app.establishment_id` in each transaction and the policies only show that business's rows. "
            "Platform tables are read by the server's queries with only the rights the application role needs (see "
            "each table).",
            "Amounts: `bigint` in minor units with a `char(3)` currency; rates: integer basis points; quantities: "
            "`bigint` in thousandths; timestamps `timestamptz` (UTC).",
            "Migrations: files `db/migrations/NNNN_name.sql`, applied in order by `nhill-server migrate`; each applied "
            "migration is recorded with its checksum and a migration modified afterwards is refused; a server refuses "
            "a database migrated by a newer version.",
            "Erasing a business: function `nh_erase_establishment` (table owner, 90 days after access ends; see the "
            "console).",
        ],
        migrations="Migrations", mig_head="| Migration | Purpose |",
        tables="Tables", table_rls="Row-level security", forced="enabled and forced", enabled="enabled",
        off="no (platform table, access limited by the `nhill_app` role's rights)",
        app="`nhill_app` rights", none="none", policies="Policies", triggers="Triggers",
        cols_head="| Column | Type | Not null | Default |", yes="yes", constraints="Constraints",
        checks="`CHECK` constraint(s)", ident="identity",
        functions="Functions", func_head="| Function | Result | Properties | Executable by `nhill_app` |",
        definer="`SECURITY DEFINER`", summary="Summary", group_head="| Domain | Tables |",
    ),
}


def esc(s):
    return (s or "").replace("|", "\\|").replace("\n", " ")


def version():
    m = re.search(r"project\(nhill VERSION ([0-9.]+)", open(os.path.join(ROOT, "CMakeLists.txt"), encoding="utf-8").read())
    return m.group(1) if m else "?"


def render(lang, tables, funcs, migrations):
    t = L[lang]
    i = 0 if lang == "fr" else 1
    out = [f"# {t['title']}", "", t["other"], "",
           t["intro"].format(v=version(), t=len(tables), f=len(funcs), m=len(migrations)), "",
           f"## {t['principles']}", ""]
    out += [f"* {x}" for x in t["principles_items"]]
    out += ["", f"## {t['migrations']}", "", t["mig_head"], "|---|---|"]
    for m in migrations:
        out.append(f"| `{m['version']}` | {esc(MIGRATIONS[m['version']][i])} |")
    out.append("")
    by_name = {x["name"]: x for x in tables}
    summary = []
    for gid, fr, en, names in GROUPS:
        gtitle = fr if lang == "fr" else en
        out += [f"## {gtitle}", ""]
        summary.append((gtitle, len(names)))
        for name in names:
            x = by_name[name]
            out += [f"### `{name}`", "", T[name][i] + ".", ""]
            rls = t["forced"] if x["rls"] and x["force_rls"] else (t["enabled"] if x["rls"] else t["off"])
            out.append(f"* {t['table_rls']} : {rls}" if lang == "fr" else f"* {t['table_rls']}: {rls}")
            privs = ", ".join(x["app_privileges"]) or t["none"]
            out.append(f"* {t['app']} : {privs}" if lang == "fr" else f"* {t['app']}: {privs}")
            if x["policies"]:
                pol = ", ".join(f"`{p['name']}`" for p in x["policies"])
                out.append(f"* {t['policies']} : {pol}" if lang == "fr" else f"* {t['policies']}: {pol}")
            if x["triggers"]:
                trg = ", ".join(f"`{p}`" for p in x["triggers"])
                out.append(f"* {t['triggers']} : {trg}" if lang == "fr" else f"* {t['triggers']}: {trg}")
            out += ["", t["cols_head"], "|---|---|---|---|"]
            for c in x["columns"]:
                d = t["ident"] if c["identity"] else (f"`{esc(c['default'])}`" if c["default"] else "—")
                out.append(f"| `{c['name']}` | `{esc(c['type'])}` | {t['yes'] if c['notnull'] else '—'} | {d} |")
            cons = [f"`{esc(k['def'])}`" for k in x["constraints"]]
            if x["checks"]:
                cons.append(f"{x['checks']} {t['checks']}")
            if cons:
                out += ["", f"{t['constraints']} : " if lang == "fr" else f"{t['constraints']}: "]
                out[-1] += " ; ".join(cons) if lang == "fr" else "; ".join(cons)
            out.append("")
    out += [f"## {t['functions']}", "", t["func_head"], "|---|---|---|---|"]
    for f in funcs:
        props = [f["volatility"]]
        if f["definer"]:
            props.append(t["definer"])
        if f["config"]:
            props.append(f"`{esc(f['config'])}`")
        out.append(f"| `{f['name']}({esc(f['args'])})` | `{esc(f['result'])}` | {', '.join(props)} | "
                   f"{t['yes'] if f['app_exec'] else '—'} |")
    out += ["", f"## {t['summary']}", "", t["group_head"], "|---|---|"]
    out += [f"| {a} | {b} |" for a, b in summary]
    out += [f"| **Total** | **{len(tables)}** |", ""]
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    g = ap.add_mutually_exclusive_group(required=True)
    g.add_argument("--server", help="nhill-server binary: create a temporary migrated database")
    g.add_argument("--dsn", help="libpq connection string of a migrated database (superuser)")
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()

    def collect(dsn):
        return psql(dsn, Q_TABLES), psql(dsn, Q_FUNCS), psql(dsn, Q_MIGR)

    if a.server:
        with TempDatabase(a.server) as db:
            tables, funcs, migrations = collect(db.dsn)
    else:
        tables, funcs, migrations = collect(a.dsn)
    names = {x["name"] for x in tables}
    grouped = [n for _, _, _, ns in GROUPS for n in ns]
    errors = [f"table sans description : {n}" for n in sorted(names - set(T))]
    errors += [f"description sans table : {n}" for n in sorted(set(T) - names)]
    errors += [f"table hors des domaines : {n}" for n in sorted(names - set(grouped))]
    errors += [f"table classée deux fois : {n}" for n in sorted({n for n in grouped if grouped.count(n) > 1})]
    applied = {m["version"] for m in migrations}
    errors += [f"migration sans description : {n}" for n in sorted(applied - set(MIGRATIONS))]
    errors += [f"description sans migration : {n}" for n in sorted(set(MIGRATIONS) - applied)]
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    bad = 0
    for lang, path in OUT.items():
        text = render(lang, tables, funcs, migrations)
        if a.check:
            cur = open(path, encoding="utf-8").read() if os.path.exists(path) else ""
            if cur != text:
                print(f"périmé : {os.path.relpath(path, ROOT)} (relancer scripts/docs/gen_db_reference.py)")
                bad = 1
        else:
            open(path, "w", encoding="utf-8").write(text)
            print(f"{os.path.relpath(path, ROOT)} : {len(tables)} tables, {len(funcs)} fonctions")
    return bad


if __name__ == "__main__":
    sys.exit(main())
