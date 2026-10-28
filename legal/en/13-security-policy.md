# 13 — Information security policy (public summary)

*Français (version de référence) : [../fr/13-politique-securite.md](../fr/13-politique-securite.md)*

| Header | |
|---|---|
| Publisher | Nhill Solution, Kinshasa (Democratic Republic of the Congo) |
| Version | 3.0.0 — 28 October 2026 |
| Status | summary for customers and partners; each statement is checked against the code and operating tools of version 3.0.0 and must be reread with every version (technical reference: `docs/en/security.md`, `docs/en/03-security-audit.md`, `docs/en/vps-deployment-hostinger.md`). The French version is the reference text |
| Assumptions | H-03, H-04, H-15, H-19 ([00](00-legal-research.md) § 2) |

## Our commitments

1. **Your data stays separate**: the database isolates each business (row-level security,
   enforced by the database itself); our tests check that a business cannot read
   another's data.
2. **Your connections are encrypted**: the applications talk to the server over HTTPS; the
   database cannot be reached from the Internet.
3. **Your passwords are never stored in clear**: they are stored as fingerprints
   (Argon2id). Minimum 8 characters, common passwords refused, attempts limited. No
   periodic change is imposed.
4. **Every action is logged**: your business's activity log records who did what; no Nhill
   function lets anyone change or erase it, neither you nor our team (the database role
   used by the application does not have that right).
5. **Our team does not access your documents**: the administration console gives access
   neither to your invoices nor to your customers; direct access to the server is reserved
   for technical operations (maintenance, backups); we only consult overall usage figures,
   and each consultation appears in your log.
6. **Our administrators use mandatory two-factor authentication.**
7. **Encrypted backups** every day, kept 14 days on the server; the key that decrypts them
   is not kept on the server; a trial restore on a separate database checks that they can
   be used.
8. **No secret in the applications**: they contain neither database password nor access
   key.

## What we do not promise

No software is free of flaws. Nhill has not yet undergone an external audit or an
independent penetration test; we plan one before large-scale deployment and will publish
its date.

## What we expect from you

One account per person; remove access for employees who leave; a password used only for
Nhill (a short phrase is easy to remember); an up-to-date device, protected by antivirus
software on Windows; report any suspicious activity to us.

## Incidents

In case of a data breach affecting you, we inform you as soon as possible, with what we
know and the measures taken (procedure: document 14).

## Reporting a flaw

support@nhill.online, subject "Security". Please do not exploit or disclose the flaw before
it is fixed.
