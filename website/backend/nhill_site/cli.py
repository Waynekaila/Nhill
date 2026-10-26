"""Maintenance commands for the website form database (run on the server, never exposed).

  python3 -m nhill_site.cli init                 create the SQLite schema (PostgreSQL: see docs)
  python3 -m nhill_site.cli check                test the database connection
  python3 -m nhill_site.cli list [--kind K]      list recent submissions (no message bodies)
  python3 -m nhill_site.cli show ID              show one submission
  python3 -m nhill_site.cli export --email E     everything stored about one person (JSON, access requests)
  python3 -m nhill_site.cli delete --email E     delete everything about one person (erasure requests)
  python3 -m nhill_site.cli unsubscribe --email E    withdraw the marketing consent
  python3 -m nhill_site.cli purge                apply the retention rules (cron / systemd timer, daily)
  python3 -m nhill_site.cli stats                counts per form and status
  python3 -m nhill_site.cli set-status ID STATUS (new, contacted, qualified, closed)

PostgreSQL: maintenance commands use SITE_MAINTENANCE_DATABASE_USER/PASSWORD (role with
SELECT/UPDATE/DELETE); the web application role can only insert.
"""
import argparse
import json
import sys

from .config import Config
from .storage import Storage

STATUSES = ("new", "contacted", "qualified", "closed")


def main(argv=None):
    ap = argparse.ArgumentParser(prog="nhill_site.cli")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("init"); sub.add_parser("check"); sub.add_parser("purge"); sub.add_parser("stats")
    l = sub.add_parser("list"); l.add_argument("--kind"); l.add_argument("--limit", type=int, default=50)
    s = sub.add_parser("show"); s.add_argument("id")
    for name in ("export", "delete", "unsubscribe"):
        x = sub.add_parser(name); x.add_argument("--email", required=True)
    st = sub.add_parser("set-status"); st.add_argument("id"); st.add_argument("status", choices=STATUSES)
    a = ap.parse_args(argv)
    cfg = Config.from_env()
    db = Storage(cfg)
    if a.cmd == "init":
        db.init_schema(); print("schéma prêt"); return 0
    if a.cmd == "check":
        _, rows, _ = db.query("SELECT count(*) FROM submissions")
        print(f"connexion OK ({cfg.engine}), {rows[0][0]} demande(s) enregistrée(s)"); return 0
    if a.cmd == "purge":
        d, c, r = db.purge()
        print(f"{d} demande(s) supprimée(s) (durée de conservation dépassée), {c} empreinte(s) IP effacée(s), {r} compteur(s) supprimé(s)")
        return 0
    if a.cmd == "stats":
        _, rows, _ = db.query("SELECT kind, status, count(*) FROM submissions GROUP BY kind, status ORDER BY kind, status")
        for r in rows: print(f"{r[0]:8} {r[1]:10} {r[2]}")
        return 0
    if a.cmd == "list":
        sql = "SELECT id, created_at, kind, status, name, organisation, city FROM submissions"
        args = ()
        if a.kind:
            sql += " WHERE kind = ?"; args = (a.kind,)
        _, rows, _ = db.query(sql + " ORDER BY created_at DESC LIMIT ?", args + (a.limit,))
        for r in rows: print(" | ".join(str(x) for x in r))
        return 0
    if a.cmd == "show":
        names, rows, _ = db.query("SELECT * FROM submissions WHERE id = ?", (a.id,))
        if not rows: print("introuvable", file=sys.stderr); return 1
        print(json.dumps(dict(zip(names, [str(x) if x is not None else None for x in rows[0]])), ensure_ascii=False, indent=1)); return 0
    email = a.email.strip().lower()
    if a.cmd == "export":
        names, rows, _ = db.query("SELECT * FROM submissions WHERE email = ?", (email,))
        print(json.dumps([dict(zip(names, [str(x) if x is not None else None for x in r])) for r in rows], ensure_ascii=False, indent=1))
        return 0
    if a.cmd == "delete":
        _, _, n = db.query("DELETE FROM submissions WHERE email = ?", (email,))
        print(f"{n} demande(s) supprimée(s) pour {email}"); return 0
    if a.cmd == "unsubscribe":
        _, _, n = db.query("UPDATE submissions SET consent_marketing = 0 WHERE email = ?", (email,))
        print(f"consentement retiré sur {n} demande(s)"); return 0
    if a.cmd == "set-status":
        _, _, n = db.query("UPDATE submissions SET status = ? WHERE id = ?", (a.status, a.id))
        print("mis à jour" if n else "introuvable"); return 0 if n else 1
    return 2


if __name__ == "__main__":
    sys.exit(main())
