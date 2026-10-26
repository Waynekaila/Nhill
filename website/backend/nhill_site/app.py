"""WSGI application of the Nhill website form backend (Python standard library only).

Routes:
  POST    /api/forms/<kind>   kind in: demo, pilot, partner, contact   -> 201 {"ok": true}
  OPTIONS /api/forms/<kind>   CORS preflight (allowed origins only)
  GET     /api/health         -> 200 {"status": "ok"}

Protections: JSON only, 16 KB maximum, CORS limited to SITE_ALLOWED_ORIGINS, honeypot
field, minimum filling time, rate limits per IP fingerprint (HMAC, never the raw IP) and
global, strict server-side validation, data minimisation, retention date on every row.
"""
import datetime as dt
import hashlib
import hmac
import json
import logging
import re

from . import notify
from .storage import Storage, utcnow

log = logging.getLogger("nhill_site")

KINDS = {
    #          required fields                                           optional fields
    "demo":    ({"name", "organisation", "email", "phone", "city"},      {"meeting_mode", "message"}),
    "pilot":   ({"name", "organisation", "email", "city", "sector"},     {"phone", "team_size", "message"}),
    "partner": ({"name", "organisation", "email", "city", "partner_type", "message"}, {"phone"}),
    "contact": ({"name", "email", "message"},                            set()),
}
LIMITS = {"name": 120, "organisation": 160, "email": 200, "phone": 30, "city": 80, "message": 2000}
ENUMS = {
    "sector": {"retail", "hardware", "wholesale", "food", "services", "health", "other"},
    "team_size": {"1", "2-5", "6-20", "21+"},
    "partner_type": {"referral", "integrator", "training", "support_org", "other"},
    "meeting_mode": {"onsite", "remote"},
}
EMAIL_RE = re.compile(r"^[^\s@<>\"']{1,64}@[^\s@<>\"']+\.[A-Za-z]{2,}$")
PHONE_RE = re.compile(r"^\+?[0-9 ().-]{8,20}$")
CONTROL_RE = re.compile(r"[\x00-\x08\x0b\x0c\x0e-\x1f\x7f]")
MAX_BODY = 16 * 1024


class Invalid(Exception):
    def __init__(self, field, reason):
        super().__init__(f"{field}: {reason}")
        self.field, self.reason = field, reason


def clean_text(v, limit):
    if v is None:
        return ""
    if not isinstance(v, str):
        raise ValueError("not a string")
    v = CONTROL_RE.sub("", v).strip()
    if len(v) > limit:
        raise ValueError("too long")
    return v


def validate(kind, data, cfg):
    """Returns the cleaned row or raises Invalid. Only known fields are kept (minimisation)."""
    required, optional = KINDS[kind]
    out = {}
    for f in required | optional:
        try:
            v = clean_text(data.get(f), LIMITS.get(f, 40))
        except ValueError as e:
            raise Invalid(f, str(e))
        if f in required and not v:
            raise Invalid(f, "required")
        if v and f in ENUMS and v not in ENUMS[f]:
            raise Invalid(f, "invalid_choice")
        out[f] = v or None
    if not EMAIL_RE.match(out["email"] or ""):
        raise Invalid("email", "invalid_email")
    out["email"] = out["email"].lower()
    if out.get("phone") and not PHONE_RE.match(out["phone"]):
        raise Invalid("phone", "invalid_phone")
    if data.get("consent_privacy") is not True:
        raise Invalid("consent_privacy", "required")
    elapsed = data.get("elapsed_ms")
    if not isinstance(elapsed, (int, float)) or elapsed < cfg.min_fill_ms:
        raise Invalid("elapsed_ms", "too_fast")
    lang = data.get("lang") if data.get("lang") in ("fr", "en") else "fr"
    now = utcnow()
    out.update(kind=kind, lang=lang, consent_marketing=1 if data.get("consent_marketing") is True else 0,
               consent_privacy_at=now.isoformat(), created_at=now.isoformat(),
               delete_after=(now + dt.timedelta(days=cfg.retention_days)).isoformat())
    return out


class App:
    def __init__(self, cfg, storage=None):
        self.cfg = cfg
        self.storage = storage or Storage(cfg)

    # ------------------------------------------------------------------ helpers
    def client_ip(self, environ):
        ip = environ.get("REMOTE_ADDR", "")
        if self.cfg.trust_proxy:
            xff = environ.get("HTTP_X_FORWARDED_FOR", "")
            if xff:
                ip = xff.split(",")[-1].strip()   # added by our own reverse proxy
        return ip

    def ip_hash(self, ip):
        return hmac.new(self.cfg.ip_salt.encode(), ip.encode(), hashlib.sha256).hexdigest()

    def cors_headers(self, environ):
        origin = (environ.get("HTTP_ORIGIN") or "").rstrip("/")
        if origin and origin in self.cfg.allowed_origins:
            return [("Access-Control-Allow-Origin", origin), ("Vary", "Origin"),
                    ("Access-Control-Allow-Methods", "POST, OPTIONS"),
                    ("Access-Control-Allow-Headers", "Content-Type, Accept"),
                    ("Access-Control-Max-Age", "600")]
        return [("Vary", "Origin")]

    def respond(self, start, status, body, environ, extra=()):
        payload = json.dumps(body).encode()
        headers = [("Content-Type", "application/json; charset=utf-8"), ("Content-Length", str(len(payload))),
                   ("Cache-Control", "no-store"), ("X-Content-Type-Options", "nosniff"),
                   ("Referrer-Policy", "no-referrer")] + self.cors_headers(environ) + list(extra)
        start(status, headers)
        return [payload]

    # ------------------------------------------------------------------ WSGI
    def __call__(self, environ, start):
        method = environ.get("REQUEST_METHOD", "GET")
        path = environ.get("PATH_INFO", "")
        try:
            if path == "/api/health" and method == "GET":
                return self.respond(start, "200 OK", {"status": "ok"}, environ)
            m = re.fullmatch(r"/api/forms/([a-z]+)", path)
            if not m:
                return self.respond(start, "404 Not Found", {"error": "not_found"}, environ)
            kind = m.group(1)
            if kind not in KINDS:
                return self.respond(start, "404 Not Found", {"error": "unknown_form"}, environ)
            if method == "OPTIONS":
                return self.respond(start, "204 No Content", {}, environ)
            if method != "POST":
                return self.respond(start, "405 Method Not Allowed", {"error": "method_not_allowed"}, environ, [("Allow", "POST, OPTIONS")])
            origin = (environ.get("HTTP_ORIGIN") or "").rstrip("/")
            if origin and origin not in self.cfg.allowed_origins:
                return self.respond(start, "403 Forbidden", {"error": "origin_not_allowed"}, environ)
            if not (environ.get("CONTENT_TYPE") or "").startswith("application/json"):
                return self.respond(start, "415 Unsupported Media Type", {"error": "json_required"}, environ)
            try:
                length = int(environ.get("CONTENT_LENGTH") or 0)
            except ValueError:
                length = 0
            if length <= 0:
                return self.respond(start, "411 Length Required", {"error": "length_required"}, environ)
            if length > MAX_BODY:
                return self.respond(start, "413 Payload Too Large", {"error": "too_large"}, environ)
            try:
                data = json.loads(environ["wsgi.input"].read(length).decode("utf-8"))
            except (ValueError, UnicodeDecodeError):
                return self.respond(start, "400 Bad Request", {"error": "malformed_json"}, environ)
            if not isinstance(data, dict):
                return self.respond(start, "400 Bad Request", {"error": "malformed_json"}, environ)

            # Rate limits before any other work: per IP fingerprint and per form, then global.
            ip_h = self.ip_hash(self.client_ip(environ))
            now = utcnow()
            hour = now.replace(minute=0, second=0).isoformat()
            day = now.replace(hour=0, minute=0, second=0).isoformat()
            if self.storage.hit(f"ip:{ip_h}:{kind}", hour) > self.cfg.rate_per_hour or \
               self.storage.hit("global", day) > self.cfg.rate_per_day_global:
                return self.respond(start, "429 Too Many Requests", {"error": "rate_limited"}, environ, [("Retry-After", "3600")])

            # Honeypot: answer like a success, store nothing (bots learn nothing).
            if data.get("website"):
                log.info("honeypot submission discarded (%s)", kind)
                return self.respond(start, "201 Created", {"ok": True}, environ)
            try:
                row = validate(kind, data, self.cfg)
            except Invalid as e:
                return self.respond(start, "422 Unprocessable Entity", {"error": e.reason, "field": e.field}, environ)
            row["ip_hash"] = ip_h
            row["user_agent"] = (environ.get("HTTP_USER_AGENT") or "")[:200]
            sid = self.storage.insert(row)
            log.info("submission %s stored (%s)", sid, kind)          # never the content
            notify.new_submission(self.cfg, kind, sid)
            return self.respond(start, "201 Created", {"ok": True}, environ)
        except Exception:  # never leak details
            log.exception("unhandled error")
            return self.respond(start, "500 Internal Server Error", {"error": "internal_error"}, environ)
