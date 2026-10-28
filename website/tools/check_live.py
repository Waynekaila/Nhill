#!/usr/bin/env python3
"""Nhill — checks that the public domain really works before it is announced.

  python3 website/tools/check_live.py [--domain nhill.online] [--json report.json] [--markdown report.md]
                                      [--require-site] [--require-mail]

Read-only checks, run from any machine with Internet access (a CI runner, an administrator's
computer). Nothing is changed on the domain, the site or the mailboxes.

  * DNS: addresses of the domain and of www (A / AAAA);
  * HTTPS: certificate trusted by the system, valid for the name, expiry date; answer of
    https://DOMAIN/; HTTP Strict-Transport-Security; http:// redirected to https://;
    www.DOMAIN redirected to DOMAIN (or serving the same site);
  * site: is it the Nhill site (canonical link of the home page on https://DOMAIN/), with
    robots.txt and sitemap.xml;
  * e-mail: MX, SPF and DMARC records (DNS over HTTPS). They show that the domain is set up
    to receive and send mail; they do NOT prove that a given mailbox (info@, support@)
    exists or is read: only a real message answered by a person proves that.

Exit code: 1 when a required check fails (HTTPS always; the Nhill site with --require-site;
the e-mail records with --require-mail), else 0. Every result is printed, with its evidence.
"""
import argparse
import datetime as dt
import http.client
import json
import socket
import ssl
import sys
import urllib.error
import urllib.parse
import urllib.request

DOH = ("https://dns.google/resolve", "https://cloudflare-dns.com/dns-query")
UA = "nhill-check-live/1 (+read-only verification of the Nhill domain)"
TIMEOUT = 15


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *a, **k):
        return None


def doh(name, rtype):
    """Answers of a DNS query through DNS over HTTPS (first resolver that answers)."""
    last = None
    for base in DOH:
        url = f"{base}?name={urllib.parse.quote(name)}&type={rtype}"
        req = urllib.request.Request(url, headers={"accept": "application/dns-json", "user-agent": UA})
        try:
            with urllib.request.urlopen(req, timeout=TIMEOUT) as r:
                data = json.load(r)
            return {"status": data.get("Status"), "resolver": base,
                    "answers": [a.get("data", "") for a in data.get("Answer", []) if a.get("type") in (1, 5, 15, 16, 28)]}
        except Exception as e:  # noqa: BLE001 — next resolver
            last = f"{type(e).__name__}: {e}"
    return {"status": None, "error": last, "answers": []}


def rdap_owner(ip):
    """Holder of the network of an address (public RDAP registry), for the evidence."""
    req = urllib.request.Request(f"https://rdap.org/ip/{ip}", headers={"accept": "application/rdap+json", "user-agent": UA})
    try:
        with urllib.request.urlopen(req, timeout=TIMEOUT) as r:
            data = json.load(r)
    except Exception:  # noqa: BLE001
        return ""
    names = []
    for ent in data.get("entities", []):
        for item in (ent.get("vcardArray") or [None, []])[1]:
            if item and item[0] == "fn" and item[3] not in names:
                names.append(item[3])
    return " / ".join(filter(None, [data.get("name"), data.get("country"), ", ".join(names[:2])]))


def page_title(body):
    import re
    m = re.search(r"<title[^>]*>(.*?)</title>", body or "", re.I | re.S)
    return " ".join(m.group(1).split())[:100] if m else "(no title)"


def addresses(host):
    try:
        return sorted({i[4][0] for i in socket.getaddrinfo(host, 443, type=socket.SOCK_STREAM)}), None
    except socket.gaierror as e:
        return [], str(e)


def tls_info(host):
    ctx = ssl.create_default_context()
    try:
        with socket.create_connection((host, 443), timeout=TIMEOUT) as raw:
            with ctx.wrap_socket(raw, server_hostname=host) as s:
                cert = s.getpeercert()
                not_after = dt.datetime.strptime(cert["notAfter"], "%b %d %H:%M:%S %Y %Z").replace(tzinfo=dt.timezone.utc)
                issuer = ", ".join(v for part in cert.get("issuer", ()) for k, v in part if k in ("organizationName", "commonName"))
                names = [v for k, v in cert.get("subjectAltName", ()) if k == "DNS"]
                return {"ok": True, "protocol": s.version(), "issuer": issuer, "names": names,
                        "not_after": not_after.isoformat(),
                        "days_left": (not_after - dt.datetime.now(dt.timezone.utc)).days}
    except ssl.SSLCertVerificationError as e:
        return {"ok": False, "error": f"certificate refused: {e.verify_message or e}"}
    except Exception as e:  # noqa: BLE001
        return {"ok": False, "error": f"{type(e).__name__}: {e}"}


def fetch(url, follow=True):
    opener = urllib.request.build_opener() if follow else urllib.request.build_opener(NoRedirect)
    req = urllib.request.Request(url, headers={"user-agent": UA})
    try:
        with opener.open(req, timeout=TIMEOUT) as r:
            body = r.read(400_000).decode("utf-8", "replace")
            return {"status": r.status, "url": r.geturl(), "headers": dict(r.headers), "body": body}
    except urllib.error.HTTPError as e:
        return {"status": e.code, "url": url, "headers": dict(e.headers or {}), "body": "",
                "location": (e.headers or {}).get("Location")}
    except Exception as e:  # noqa: BLE001
        return {"status": None, "url": url, "error": f"{type(e).__name__}: {e}", "headers": {}, "body": ""}


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--domain", default="nhill.online")
    ap.add_argument("--json")
    ap.add_argument("--markdown")
    ap.add_argument("--require-site", action="store_true")
    ap.add_argument("--require-mail", action="store_true")
    a = ap.parse_args(argv)
    d = a.domain.strip().lower().rstrip(".")
    www = "www." + d
    checks = []

    def add(group, cid, ok, evidence, required):
        checks.append({"group": group, "id": cid, "ok": bool(ok), "evidence": evidence, "required": required})

    for host in (d, www):
        ips, err = addresses(host)
        add("dns", f"addresses {host}", bool(ips), ", ".join(ips) if ips else f"no address ({err})", host == d)
        for ip in ips[:2] if host == d else []:
            owner = rdap_owner(ip)
            add("dns", f"network holder of {ip}", bool(owner), owner or "RDAP lookup failed", False)

    t = tls_info(d)
    add("https", f"certificate {d}", t.get("ok"),
        f"{t.get('issuer')} · valid for {', '.join(t.get('names', []))} · until {t.get('not_after')} "
        f"({t.get('days_left')} days) · {t.get('protocol')}" if t.get("ok") else t.get("error"), True)
    if t.get("ok"):
        add("https", "certificate valid for more than 14 days", t["days_left"] > 14, f"{t['days_left']} days left", True)
    tw = tls_info(www)
    add("https", f"certificate {www}", tw.get("ok"),
        f"valid for {', '.join(tw.get('names', []))}" if tw.get("ok") else tw.get("error"), False)

    home = fetch(f"https://{d}/")
    add("https", f"https://{d}/ answers", home["status"] == 200,
        f"HTTP {home['status']} at {home['url']}" if home["status"] else home.get("error"), True)
    hsts = {k.lower(): v for k, v in home["headers"].items()}.get("strict-transport-security")
    add("https", "HTTP Strict-Transport-Security", bool(hsts), hsts or "header absent", False)
    plain = fetch(f"http://{d}/", follow=False)
    loc = plain.get("location") or ""
    add("https", "http:// redirected to https://", plain["status"] in (301, 302, 307, 308) and loc.startswith("https://"),
        f"HTTP {plain['status']} → {loc or '(no redirect)'}" if plain["status"] else plain.get("error"), False)
    w = fetch(f"https://{www}/")
    add("https", f"{www} leads to the site", w["status"] == 200,
        f"HTTP {w['status']} at {w['url']}" if w["status"] else w.get("error"), False)

    canonical = f'<link rel="canonical" href="https://{d}/">'
    add("site", "home page is the Nhill site (canonical link)", canonical in home["body"],
        f"canonical link found; title: {page_title(home['body'])}" if canonical in home["body"] else
        f"canonical link of the Nhill home page absent; page served: \"{page_title(home['body'])}\" "
        f"({len(home['body'])} bytes)", a.require_site)
    for path in ("robots.txt", "sitemap.xml"):
        r = fetch(f"https://{d}/{path}")
        ok = r["status"] == 200 and (f"https://{d}/sitemap.xml" in r["body"] if path == "robots.txt" else f"<loc>https://{d}/" in r["body"])
        add("site", f"/{path} of the Nhill site", ok, f"HTTP {r['status']}" if r["status"] else r.get("error"), a.require_site)

    mx = doh(d, "MX")
    add("mail", "MX records (mail reception)", bool(mx["answers"]),
        "; ".join(mx["answers"]) or f"none (DNS status {mx.get('status')}, {mx.get('error') or 'no answer'})", a.require_mail)
    txt = doh(d, "TXT")
    spf = [x for x in txt["answers"] if x.strip('"').lower().startswith("v=spf1")]
    add("mail", "SPF record (authorised senders)", len(spf) == 1, "; ".join(spf) or "none", a.require_mail)
    dmarc = [x for x in doh("_dmarc." + d, "TXT")["answers"] if "v=dmarc1" in x.lower()]
    add("mail", "DMARC record", bool(dmarc), "; ".join(dmarc) or "none", a.require_mail)

    failed = [c for c in checks if c["required"] and not c["ok"]]
    now = dt.datetime.now(dt.timezone.utc).strftime("%Y-%m-%d %H:%M UTC")
    for c in checks:
        mark = "OK  " if c["ok"] else ("FAIL" if c["required"] else "warn")
        print(f"{mark} [{c['group']}] {c['id']}: {c['evidence']}")
    print(f"\n{len(checks)} checks on {d} at {now}: {len(failed)} required check(s) failed")
    if a.json:
        with open(a.json, "w", encoding="utf-8") as f:
            json.dump({"domain": d, "checked_at": now, "checks": checks, "required_failed": len(failed)}, f, indent=2)
    if a.markdown:
        with open(a.markdown, "w", encoding="utf-8") as f:
            f.write(f"# Live check of {d}\n\nChecked at {now} by `website/tools/check_live.py` (read-only).\n\n")
            f.write("| Result | Group | Check | Evidence |\n|---|---|---|---|\n")
            for c in checks:
                res = "OK" if c["ok"] else ("**FAILED**" if c["required"] else "not met")
                f.write(f"| {res} | {c['group']} | {c['id']} | {str(c['evidence']).replace('|', '/')} |\n")
            f.write("\nMX, SPF and DMARC records do not prove that a mailbox exists or is read.\n")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
