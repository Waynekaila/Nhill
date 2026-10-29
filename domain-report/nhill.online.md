# Live check of nhill.online

Checked at 2026-10-29 06:16 UTC by `website/tools/check_live.py` (read-only).

| Result | Group | Check | Evidence |
|---|---|---|---|
| OK | dns | addresses nhill.online | 2.57.91.91 |
| OK | dns | network holder of 2.57.91.91 | HOSTINGER-CDN / LT / Hostinger NOC, MNT-HOSTINGER |
| OK | dns | addresses www.nhill.online | 2.57.91.91 |
| OK | https | certificate nhill.online | Let's Encrypt, YE1 · valid for nhill.online, www.nhill.online · until 2027-01-25T12:09:12+00:00 (88 days) · TLSv1.3 |
| OK | https | certificate valid for more than 14 days | 88 days left |
| OK | https | certificate www.nhill.online | valid for nhill.online, www.nhill.online |
| OK | https | https://nhill.online/ answers | HTTP 200 at https://nhill.online/ |
| not met | https | HTTP Strict-Transport-Security | header absent |
| not met | https | http:// redirected to https:// | HTTP 200 → (no redirect) |
| OK | https | www.nhill.online leads to the site | HTTP 200 at https://www.nhill.online/ |
| **FAILED** | site | home page is the Nhill site (canonical link) | canonical link of the Nhill home page absent; page served: "Parked Domain name on Hostinger DNS system" (32079 bytes) |
| **FAILED** | site | /robots.txt of the Nhill site | HTTP 200 |
| **FAILED** | site | /sitemap.xml of the Nhill site | HTTP 200 |
| **FAILED** | mail | MX records (mail reception) | none (DNS status 0, no answer) |
| **FAILED** | mail | SPF record (authorised senders) | none |
| **FAILED** | mail | DMARC record | none |

MX, SPF and DMARC records do not prove that a mailbox exists or is read.
