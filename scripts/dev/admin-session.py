#!/usr/bin/env python3
"""Development helper: completes the administrator first access (password + TOTP)
and prints a console session token. DEVELOPMENT ONLY — it stores the TOTP secret
in the dev state folder, which must never happen for a real administrator.

  admin-session.py --email admin@nhill.dev --setup-token TOKEN   # first time
  admin-session.py --email admin@nhill.dev                         # afterwards
"""
import argparse, base64, hashlib, hmac, json, os, struct, sys, time, urllib.error, urllib.request

def totp(secret: bytes, step: int) -> str:
    mac = hmac.new(secret, struct.pack(">Q", step), hashlib.sha1).digest()
    o = mac[-1] & 0x0F
    return f"{(struct.unpack('>I', mac[o:o + 4])[0] & 0x7FFFFFFF) % 1000000:06d}"

def post(base, path, body):
    req = urllib.request.Request(base + path, data=json.dumps(body).encode(), method="POST",
                                 headers={"Content-Type": "application/json", "Accept": "application/json"})
    try:
        with urllib.request.urlopen(req, timeout=20) as r:
            raw = r.read()
            return json.loads(raw) if raw else {}
    except urllib.error.HTTPError as e:
        sys.exit(f"{path} -> {e.code}: {e.read().decode(errors='replace')}")

ap = argparse.ArgumentParser()
ap.add_argument("--server", default="http://127.0.0.1:8080")
ap.add_argument("--email", required=True)
ap.add_argument("--setup-token")
ap.add_argument("--password", default="Dev-admin-password-2026")
a = ap.parse_args()
if not a.server.startswith(("http://127.0.0.1", "http://localhost")):
    sys.exit("development helper: local servers only")
state = os.environ.get("NH_DEV_STATE", "/tmp/nhill-dev")
secret_file = os.path.join(state, "admin-totp-" + a.email)
if a.setup_token:
    r = post(a.server, "/api/v1/admin/auth/setup", {"email": a.email, "setup_token": a.setup_token, "password": a.password})
    secret = base64.b32decode(r["totp_secret"] + "=" * (-len(r["totp_secret"]) % 8))
    post(a.server, "/api/v1/admin/auth/setup/confirm",
         {"email": a.email, "setup_token": a.setup_token, "totp_code": totp(secret, int(time.time()) // 30)})
    with open(secret_file, "wb") as f:
        f.write(secret)
    os.chmod(secret_file, 0o600)
    time.sleep(31 - time.time() % 30)  # a TOTP step is accepted once
secret = open(secret_file, "rb").read()
r = post(a.server, "/api/v1/admin/auth/login", {"email": a.email, "password": a.password,
                                               "totp_code": totp(secret, int(time.time()) // 30)})
print(r["token"])
