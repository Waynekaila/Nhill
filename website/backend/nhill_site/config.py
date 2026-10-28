"""Configuration of the form backend, read from environment variables only.

Secrets (database password, IP hashing salt, SMTP password) are read here, on the
server; nothing of this module is ever sent to a browser. See website/backend/.env.example.
"""
import os
from dataclasses import dataclass, field


def _env(name, default=None):
    v = os.environ.get(name)
    return v if v not in (None, "") else default


def load_env_file(path):
    """Loads KEY=VALUE lines into os.environ (development convenience, never required).
    Existing environment variables win. The file must not be readable by others."""
    if not path or not os.path.exists(path):
        return
    if os.stat(path).st_mode & 0o077:
        raise SystemExit(f"{path} est lisible par d'autres utilisateurs : chmod 600 {path}")
    for line in open(path, encoding="utf-8"):
        line = line.strip()
        if not line or line.startswith("#") or "=" not in line:
            continue
        k, v = line.split("=", 1)
        os.environ.setdefault(k.strip(), v.strip().strip('"').strip("'"))


@dataclass
class Config:
    engine: str = "sqlite"                  # sqlite | postgresql
    sqlite_path: str = "data/site.sqlite3"
    db_host: str = "127.0.0.1"
    db_port: int = 5432
    db_name: str = "nhill_site"
    db_user: str = "nhill_site_app"
    db_password: str = ""
    db_sslmode: str = "prefer"
    listen_host: str = "127.0.0.1"
    listen_port: int = 8090
    allowed_origins: list = field(default_factory=list)
    trust_proxy: bool = False
    ip_salt: str = ""
    retention_days: int = 365
    ip_retention_days: int = 30
    min_fill_ms: int = 3000
    rate_per_hour: int = 5
    rate_per_day_global: int = 300
    public_base_url: str = ""
    contact_email: str = ""
    notify_email: str = ""
    smtp_host: str = ""
    smtp_port: int = 587
    smtp_user: str = ""
    smtp_password: str = ""
    smtp_from: str = ""

    @staticmethod
    def from_env():
        load_env_file(_env("SITE_ENV_FILE"))
        c = Config(
            engine=_env("SITE_DATABASE_ENGINE", "sqlite"),
            sqlite_path=_env("SITE_SQLITE_PATH", "data/site.sqlite3"),
            db_host=_env("SITE_DATABASE_HOST", "127.0.0.1"),
            db_port=int(_env("SITE_DATABASE_PORT", "5432")),
            db_name=_env("SITE_DATABASE_NAME", "nhill_site"),
            db_user=_env("SITE_DATABASE_USER", "nhill_site_app"),
            db_password=_env("SITE_DATABASE_PASSWORD", ""),
            db_sslmode=_env("SITE_DATABASE_SSLMODE", "prefer"),
            listen_host=_env("SITE_LISTEN_HOST", "127.0.0.1"),
            listen_port=int(_env("SITE_LISTEN_PORT", "8090")),
            allowed_origins=[o.strip().rstrip("/") for o in _env("SITE_ALLOWED_ORIGINS", "").split(",") if o.strip()],
            trust_proxy=_env("SITE_TRUST_PROXY", "0") == "1",
            ip_salt=_env("SITE_IP_HASH_SALT", ""),
            retention_days=int(_env("SITE_RETENTION_DAYS", "365")),
            ip_retention_days=int(_env("SITE_IP_RETENTION_DAYS", "30")),
            min_fill_ms=int(_env("SITE_MIN_FILL_MS", "3000")),
            rate_per_hour=int(_env("SITE_RATE_PER_HOUR", "5")),
            rate_per_day_global=int(_env("SITE_RATE_PER_DAY_GLOBAL", "300")),
            public_base_url=_env("SITE_PUBLIC_BASE_URL", ""),
            contact_email=_env("SITE_CONTACT_EMAIL", ""),
            notify_email=_env("SITE_NOTIFY_EMAIL", ""),
            smtp_host=_env("SITE_SMTP_HOST", ""),
            smtp_port=int(_env("SITE_SMTP_PORT", "587")),
            smtp_user=_env("SITE_SMTP_USER", ""),
            smtp_password=_env("SITE_SMTP_PASSWORD", ""),
            smtp_from=_env("SITE_SMTP_FROM", ""),
        )
        c.validate()
        return c

    def validate(self):
        if self.engine not in ("sqlite", "postgresql"):
            raise SystemExit("SITE_DATABASE_ENGINE doit valoir sqlite ou postgresql")
        if len(self.ip_salt) < 32:
            raise SystemExit("SITE_IP_HASH_SALT doit contenir au moins 32 caractères (openssl rand -hex 32)")
        if self.engine == "postgresql" and not self.db_password:
            raise SystemExit("SITE_DATABASE_PASSWORD est requis pour PostgreSQL")
        if self.engine == "postgresql" and self.db_sslmode not in ("disable", "allow", "prefer", "require", "verify-ca", "verify-full"):
            raise SystemExit("SITE_DATABASE_SSLMODE invalide")
        if not self.allowed_origins:
            raise SystemExit("SITE_ALLOWED_ORIGINS est requis (ex. https://nhill.online)")
