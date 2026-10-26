#!/usr/bin/env python3
"""Nhill — writes the PUBLIC site configuration (config/site.json) for one deployment.

  SITE_PUBLIC_BASE_URL=https://www.exemple.cd SITE_API_BASE_URL=https://www.exemple.cd \
  SITE_CONTACT_EMAIL=contact@exemple.cd python3 website/tools/configure_site.py [--site DIR]

config/site.json is downloaded by every visitor: it only ever holds the three public
values below. Database settings and passwords stay in the backend environment
(/etc/nhill-site/site.env) and are refused here.
"""
import argparse
import json
import os
import re
import sys

PUBLIC = {"publicBaseUrl": "SITE_PUBLIC_BASE_URL", "apiBaseUrl": "SITE_API_BASE_URL", "contactEmail": "SITE_CONTACT_EMAIL"}
COMMENT = ("Public configuration read by the browser (assets/js/site.js) and by tools/build_site.py. NEVER put a "
           "secret here: this file is downloaded by every visitor. apiBaseUrl: address of the form backend (empty = "
           "forms say they are not connected). contactEmail: shown when forms are not connected.")


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--site", default=os.path.abspath(os.path.join(os.path.dirname(__file__), "..")))
    a = ap.parse_args(argv)
    path = os.path.join(a.site, "config", "site.json")
    with open(path, encoding="utf-8") as f:
        cfg = json.load(f)
    for key, var in PUBLIC.items():
        if var in os.environ:
            cfg[key] = os.environ[var].strip().rstrip("/") if key != "contactEmail" else os.environ[var].strip()
    for key in ("publicBaseUrl", "apiBaseUrl"):
        v = cfg.get(key, "")
        if v and not re.fullmatch(r"https://[A-Za-z0-9.-]+(:\d+)?(/[A-Za-z0-9._~/-]*)?", v):
            if not re.fullmatch(r"http://(localhost|127\.0\.0\.1)(:\d+)?", v):
                sys.exit(f"{key} : adresse https:// attendue (reçu {v!r})")
        if v and "@" in v:
            sys.exit(f"{key} : identifiants interdits dans une adresse publique")
    email = cfg.get("contactEmail", "")
    if email and not re.fullmatch(r"[^@\s]+@[^@\s]+\.[A-Za-z]{2,}", email):
        sys.exit(f"contactEmail invalide : {email!r}")
    unknown = set(cfg) - set(PUBLIC) - {"_comment"}
    if unknown:
        sys.exit(f"clés non publiques refusées dans site.json : {', '.join(sorted(unknown))}")
    cfg["_comment"] = COMMENT
    out = {"_comment": COMMENT, **{k: cfg.get(k, "") for k in PUBLIC}}
    with open(path, "w", encoding="utf-8") as f:
        json.dump(out, f, ensure_ascii=False, indent=2)
        f.write("\n")
    print(f"{path} : " + ", ".join(f"{k}={out[k] or '(vide)'}" for k in PUBLIC))
    return 0


if __name__ == "__main__":
    sys.exit(main())
