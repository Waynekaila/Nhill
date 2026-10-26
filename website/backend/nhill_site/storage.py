"""Storage of form submissions. Two engines, same schema:

* SQLite (standard library, default): a single file outside the public web root.
* PostgreSQL (psycopg 3, optional: `apt install python3-psycopg`): a dedicated database
  and least-privilege roles, distinct from the Nhill application database.

The application role can only INSERT submissions and maintain rate-limit counters;
reading, exporting and deleting submissions are done by the maintenance CLI.
"""
import datetime as dt
import os
import sqlite3
import uuid

HERE = os.path.dirname(os.path.abspath(__file__))
SCHEMA_SQLITE = os.path.join(HERE, "..", "schema", "sqlite.sql")

COLUMNS = ("id", "kind", "lang", "name", "organisation", "email", "phone", "city", "sector", "team_size",
           "partner_type", "meeting_mode", "message", "consent_privacy_at", "consent_marketing", "ip_hash",
           "user_agent", "created_at", "delete_after", "status")


def utcnow():
    return dt.datetime.now(dt.timezone.utc).replace(microsecond=0)


class Storage:
    def __init__(self, cfg):
        self.cfg = cfg
        self.pg = cfg.engine == "postgresql"
        if self.pg:
            try:
                import psycopg  # noqa: F401
            except ImportError as e:
                raise SystemExit("PostgreSQL choisi mais psycopg 3 absent : sudo apt install python3-psycopg") from e

    # ------------------------------------------------------------------ connections
    def connect(self, maintenance=False):
        if self.pg:
            import psycopg
            user = os.environ.get("SITE_MAINTENANCE_DATABASE_USER") if maintenance else None
            password = os.environ.get("SITE_MAINTENANCE_DATABASE_PASSWORD") if maintenance else None
            return psycopg.connect(host=self.cfg.db_host, port=self.cfg.db_port, dbname=self.cfg.db_name,
                                   user=user or self.cfg.db_user, password=password or self.cfg.db_password,
                                   sslmode=self.cfg.db_sslmode, connect_timeout=10, autocommit=False)
        path = self.cfg.sqlite_path
        d = os.path.dirname(os.path.abspath(path))
        os.makedirs(d, mode=0o700, exist_ok=True)
        new = not os.path.exists(path)
        con = sqlite3.connect(path, timeout=10, isolation_level=None)
        con.execute("PRAGMA journal_mode=WAL")
        con.execute("PRAGMA foreign_keys=ON")
        if new:
            os.chmod(path, 0o600)
        return con

    def ts(self, value):
        """Timestamps: datetime objects for PostgreSQL (timestamptz), ISO text for SQLite."""
        if isinstance(value, str):
            value = dt.datetime.fromisoformat(value)
        return value if self.pg else value.isoformat()

    def p(self, sql):
        """SQLite uses ?, psycopg uses %s."""
        return sql.replace("?", "%s") if self.pg else sql

    def init_schema(self):
        if self.pg:
            raise SystemExit("PostgreSQL : appliquez schema/postgresql/*.sql avec le rôle propriétaire (voir docs).")
        con = self.connect()
        with open(SCHEMA_SQLITE, encoding="utf-8") as f:
            con.executescript(f.read())
        con.close()

    # ------------------------------------------------------------------ writes (application role)
    def hit(self, bucket, window_start):
        """Counts one hit in a rate-limit bucket; returns the new count."""
        window_start = self.ts(window_start)
        con = self.connect()
        try:
            cur = con.cursor()
            if self.pg:
                cur.execute("INSERT INTO rate_limits (bucket, window_start, hits) VALUES (%s, %s, 1) "
                            "ON CONFLICT (bucket, window_start) DO UPDATE SET hits = rate_limits.hits + 1 RETURNING hits",
                            (bucket, window_start))
                n = cur.fetchone()[0]
                con.commit()
            else:
                cur.execute("BEGIN IMMEDIATE")
                cur.execute("INSERT INTO rate_limits (bucket, window_start, hits) VALUES (?, ?, 1) "
                            "ON CONFLICT (bucket, window_start) DO UPDATE SET hits = hits + 1", (bucket, window_start))
                n = cur.execute("SELECT hits FROM rate_limits WHERE bucket = ? AND window_start = ?",
                                (bucket, window_start)).fetchone()[0]
                cur.execute("COMMIT")
            return n
        finally:
            con.close()

    def insert(self, row):
        row = dict(row)
        row.setdefault("id", str(uuid.uuid4()))
        row.setdefault("status", "new")
        for c in ("consent_privacy_at", "created_at", "delete_after"):
            if c in row:
                row[c] = self.ts(row[c])
        cols = [c for c in COLUMNS if c in row]
        sql = f"INSERT INTO submissions ({', '.join(cols)}) VALUES ({', '.join('?' for _ in cols)})"
        con = self.connect()
        try:
            cur = con.cursor()
            cur.execute(self.p(sql), [row[c] for c in cols])
            if self.pg:
                con.commit()
        finally:
            con.close()
        return row["id"]

    # ------------------------------------------------------------------ maintenance (CLI)
    def query(self, sql, args=(), maintenance=True):
        con = self.connect(maintenance=maintenance)
        try:
            cur = con.cursor()
            cur.execute(self.p(sql), args)
            rows = cur.fetchall() if cur.description else []
            names = [d[0] for d in cur.description] if cur.description else []
            if self.pg:
                con.commit()
            return names, rows, cur.rowcount
        finally:
            con.close()

    def purge(self, now=None):
        """Applies the retention rules: deletes expired submissions, clears old IP hashes and
        old rate-limit counters. Returns (deleted, ip_cleared, counters_deleted)."""
        now = now or utcnow()
        ip_limit = self.ts(now - dt.timedelta(days=self.cfg.ip_retention_days))
        _, _, deleted = self.query("DELETE FROM submissions WHERE delete_after < ?", (self.ts(now),))
        _, _, cleared = self.query("UPDATE submissions SET ip_hash = NULL WHERE ip_hash IS NOT NULL AND created_at < ?", (ip_limit,))
        _, _, counters = self.query("DELETE FROM rate_limits WHERE window_start < ?", (self.ts(now - dt.timedelta(days=2)),))
        return deleted, cleared, counters
