"""Tests of the website form backend (SQLite, throwaway database per test).

  cd website/backend && python3 -m unittest discover -s tests -v

PostgreSQL: set NHILL_SITE_PG_TEST=1 and the SITE_DATABASE_* / SITE_MAINTENANCE_* variables
(database prepared with schema/postgresql/*.sql) to run the same scenarios on PostgreSQL.
"""
import datetime as dt
import io
import json
import os
import sqlite3
import sys
import tempfile
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from nhill_site import cli  # noqa: E402
from nhill_site.app import App  # noqa: E402
from nhill_site.config import Config  # noqa: E402
from nhill_site.storage import Storage  # noqa: E402

ORIGIN = "https://www.nhill.example"


def make_cfg(tmp, **kw):
    pg = os.environ.get("NHILL_SITE_PG_TEST") == "1"
    c = Config(engine="postgresql" if pg else "sqlite", sqlite_path=os.path.join(tmp, "site.sqlite3"),
               allowed_origins=[ORIGIN], ip_salt="x" * 40, rate_per_hour=5, rate_per_day_global=300,
               db_host=os.environ.get("SITE_DATABASE_HOST", "127.0.0.1"),
               db_port=int(os.environ.get("SITE_DATABASE_PORT", "5432")),
               db_name=os.environ.get("SITE_DATABASE_NAME", "nhill_site"),
               db_user=os.environ.get("SITE_DATABASE_USER", "nhill_site_app"),
               db_password=os.environ.get("SITE_DATABASE_PASSWORD", ""),
               db_sslmode=os.environ.get("SITE_DATABASE_SSLMODE", "disable"))
    for k, v in kw.items():
        setattr(c, k, v)
    return c


def valid(kind="demo", **over):
    d = {"name": "Mbuyi Kalala", "organisation": "Quincaillerie Test (démo)", "email": "Contact@Example.CD",
         "phone": "+243 81 234 5678", "city": "Kinshasa", "sector": "hardware", "message": "Bonjour",
         "partner_type": "referral", "consent_privacy": True, "consent_marketing": False,
         "elapsed_ms": 9000, "lang": "fr", "website": ""}
    d.update(over)
    return d


class Base(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.cfg = make_cfg(self.tmp.name)
        self.store = Storage(self.cfg)
        if self.cfg.engine == "sqlite":
            self.store.init_schema()
        else:
            self.store.query("DELETE FROM submissions"); self.store.query("DELETE FROM rate_limits")
        self.app = App(self.cfg, self.store)

    def tearDown(self):
        self.tmp.cleanup()

    def call(self, method, path, body=None, origin=ORIGIN, ctype="application/json", ip="198.51.100.7", raw=None, xff=None):
        payload = raw if raw is not None else (json.dumps(body).encode() if body is not None else b"")
        env = {"REQUEST_METHOD": method, "PATH_INFO": path, "CONTENT_TYPE": ctype, "CONTENT_LENGTH": str(len(payload)),
               "wsgi.input": io.BytesIO(payload), "REMOTE_ADDR": ip, "HTTP_USER_AGENT": "unittest"}
        if origin:
            env["HTTP_ORIGIN"] = origin
        if xff:
            env["HTTP_X_FORWARDED_FOR"] = xff
        out = {}

        def start(status, headers):
            out["status"] = int(status.split()[0]); out["headers"] = dict(headers)
        res = b"".join(self.app(env, start))
        return out["status"], (json.loads(res) if res else {}), out["headers"]

    def rows(self, where="1=1", args=()):
        names, rows, _ = self.store.query(f"SELECT * FROM submissions WHERE {where}", args)
        return [dict(zip(names, r)) for r in rows]


class FormTests(Base):
    def test_valid_demo_is_stored_minimised_with_retention(self):
        st, body, h = self.call("POST", "/api/forms/demo", valid())
        self.assertEqual(st, 201, body)
        self.assertEqual(h.get("Access-Control-Allow-Origin"), ORIGIN)
        r = self.rows()
        self.assertEqual(len(r), 1)
        r = r[0]
        self.assertEqual(r["email"], "contact@example.cd")
        self.assertIsNone(r.get("sector"))           # not a field of the demo form: not kept
        self.assertIsNone(r.get("partner_type"))
        self.assertNotIn("198.51.100.7", json.dumps({k: str(v) for k, v in r.items()}))
        self.assertEqual(len(r["ip_hash"]), 64)
        created = dt.datetime.fromisoformat(str(r["created_at"]))
        delete_after = dt.datetime.fromisoformat(str(r["delete_after"]))
        self.assertEqual((delete_after - created).days, self.cfg.retention_days)

    def test_every_form_kind(self):
        for kind in ("demo", "pilot", "partner", "contact"):
            st, body, _ = self.call("POST", f"/api/forms/{kind}", valid(kind), ip=f"203.0.113.{len(kind)}")
            self.assertEqual(st, 201, (kind, body))
        self.assertEqual(len(self.rows()), 4)

    def test_validation_errors_name_the_field(self):
        cases = [({"consent_privacy": False}, "consent_privacy"), ({"email": "pas-un-email"}, "email"),
                 ({"name": ""}, "name"), ({"phone": "abc"}, "phone"), ({"elapsed_ms": 500}, "elapsed_ms"),
                 ({"name": "x" * 121}, "name"), ({"meeting_mode": "teleport"}, "meeting_mode")]
        for i, (over, field) in enumerate(cases):
            st, body, _ = self.call("POST", "/api/forms/demo", valid(**over), ip=f"192.0.2.{i}")
            self.assertEqual((st, body.get("field")), (422, field), over)
        self.assertEqual(self.rows(), [])

    def test_honeypot_is_accepted_but_not_stored(self):
        st, _, _ = self.call("POST", "/api/forms/contact", valid("contact", website="http://spam.example"))
        self.assertEqual(st, 201)
        self.assertEqual(self.rows(), [])

    def test_rate_limit_per_ip_and_form(self):
        for i in range(5):
            self.assertEqual(self.call("POST", "/api/forms/contact", valid("contact"))[0], 201)
        st, body, h = self.call("POST", "/api/forms/contact", valid("contact"))
        self.assertEqual((st, body["error"]), (429, "rate_limited"))
        self.assertEqual(h.get("Retry-After"), "3600")
        self.assertEqual(self.call("POST", "/api/forms/contact", valid("contact"), ip="198.51.100.8")[0], 201)

    def test_proxy_header_only_trusted_when_configured(self):
        self.cfg.trust_proxy = True
        for i in range(5):
            self.call("POST", "/api/forms/contact", valid("contact"), ip="127.0.0.1", xff=f"6.6.6.{i}, 198.51.100.50")
        # the client-controlled left part changes, the proxy-added right part does not: limited
        self.assertEqual(self.call("POST", "/api/forms/contact", valid("contact"), ip="127.0.0.1", xff="9.9.9.9, 198.51.100.50")[0], 429)

    def test_protocol_errors(self):
        self.assertEqual(self.call("POST", "/api/forms/demo", valid(), origin="https://evil.example")[0], 403)
        self.assertEqual(self.call("POST", "/api/forms/demo", valid(), ctype="text/plain")[0], 415)
        self.assertEqual(self.call("POST", "/api/forms/demo", raw=b"{not json")[0], 400)
        self.assertEqual(self.call("POST", "/api/forms/demo", raw=b"[1,2]")[0], 400)
        self.assertEqual(self.call("POST", "/api/forms/demo", raw=b"{" + b" " * 17000 + b"}")[0], 413)
        self.assertEqual(self.call("POST", "/api/forms/unknown", valid())[0], 404)
        self.assertEqual(self.call("GET", "/api/forms/demo")[0], 405)
        self.assertEqual(self.call("GET", "/api/health")[0], 200)
        st, _, h = self.call("OPTIONS", "/api/forms/demo")
        self.assertEqual(st, 204)
        self.assertIn("POST", h["Access-Control-Allow-Methods"])
        _, _, h = self.call("OPTIONS", "/api/forms/demo", origin="https://evil.example")
        self.assertNotIn("Access-Control-Allow-Origin", h)
        self.assertEqual(self.rows(), [])

    def test_control_characters_are_removed(self):
        self.call("POST", "/api/forms/contact", valid("contact", message="Bon\x00jour\x07 !"))
        self.assertEqual(self.rows()[0]["message"], "Bonjour !")


class MaintenanceTests(Base):
    def test_purge_applies_retention_and_clears_ip_fingerprints(self):
        self.call("POST", "/api/forms/demo", valid())
        future = dt.datetime.now(dt.timezone.utc) + dt.timedelta(days=self.cfg.ip_retention_days + 1)
        d, c, _ = self.store.purge(now=future)
        self.assertEqual((d, c), (0, 1))
        self.assertIsNone(self.rows()[0]["ip_hash"])
        far = dt.datetime.now(dt.timezone.utc) + dt.timedelta(days=self.cfg.retention_days + 1)
        d, _, _ = self.store.purge(now=far)
        self.assertEqual(d, 1)
        self.assertEqual(self.rows(), [])

    def test_access_and_erasure_by_email(self):
        self.call("POST", "/api/forms/demo", valid(email="a@example.cd"))
        self.call("POST", "/api/forms/pilot", valid("pilot", email="a@example.cd"), ip="192.0.2.99")
        self.call("POST", "/api/forms/contact", valid("contact", email="b@example.cd"), ip="192.0.2.98")
        n = self.store.query("UPDATE submissions SET consent_marketing = 0 WHERE email = ?", ("a@example.cd",))[2]
        self.assertEqual(n, 2)
        n = self.store.query("DELETE FROM submissions WHERE email = ?", ("a@example.cd",))[2]
        self.assertEqual(n, 2)
        self.assertEqual([r["email"] for r in self.rows()], ["b@example.cd"])

    @unittest.skipIf(os.environ.get("NHILL_SITE_PG_TEST") == "1", "SQLite file permissions only")
    def test_sqlite_file_is_private(self):
        self.call("POST", "/api/forms/demo", valid())
        self.assertEqual(os.stat(self.cfg.sqlite_path).st_mode & 0o077, 0)

    def test_cli_commands(self):
        self.call("POST", "/api/forms/demo", valid(email="c@example.cd"))
        env = {"SITE_DATABASE_ENGINE": self.cfg.engine, "SITE_SQLITE_PATH": self.cfg.sqlite_path,
               "SITE_IP_HASH_SALT": "x" * 40, "SITE_ALLOWED_ORIGINS": ORIGIN}
        old = {k: os.environ.get(k) for k in env}
        os.environ.update(env)
        try:
            out = io.StringIO()
            sys.stdout, saved = out, sys.stdout
            try:
                self.assertEqual(cli.main(["check"]), 0)
                self.assertEqual(cli.main(["stats"]), 0)
                self.assertEqual(cli.main(["export", "--email", "C@example.cd"]), 0)
                self.assertEqual(cli.main(["delete", "--email", "c@example.cd"]), 0)
            finally:
                sys.stdout = saved
            self.assertIn("c@example.cd", out.getvalue())
            self.assertEqual(self.rows(), [])
        finally:
            for k, v in old.items():
                if v is None:
                    os.environ.pop(k, None)
                else:
                    os.environ[k] = v


class ConfigTests(unittest.TestCase):
    def test_config_refuses_weak_or_missing_settings(self):
        with self.assertRaises(SystemExit):
            Config(ip_salt="short", allowed_origins=[ORIGIN]).validate()
        with self.assertRaises(SystemExit):
            Config(ip_salt="x" * 40, allowed_origins=[]).validate()
        with self.assertRaises(SystemExit):
            Config(engine="postgresql", ip_salt="x" * 40, allowed_origins=[ORIGIN], db_password="").validate()
        Config(ip_salt="x" * 40, allowed_origins=[ORIGIN]).validate()


if __name__ == "__main__":
    unittest.main()
