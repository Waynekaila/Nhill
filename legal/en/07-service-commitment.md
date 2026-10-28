# 07 — Service commitment

*Français (version de référence) : [../fr/07-engagement-de-service.md](../fr/07-engagement-de-service.md)*

| Header | |
|---|---|
| Publisher | Nhill Solution, Kinshasa (Democratic Republic of the Congo) |
| Version | 3.0.0 — 28 October 2026 (replaces the "service level agreement" of version 2.0.0) |
| Status | written without review by a lawyer; recommended review point: V-01 ([00](00-legal-research.md) § 5). The French version is the reference text |
| Assumptions | H-02, H-20 ([00](00-legal-research.md) § 2) |
| Place | contract document of the terms of service (document 02) |

## 1. Principle

The Publisher commits to the **means** described below, not to figures: no availability
rate or response time is guaranteed. This commitment creates no penalty or service
credit; the Publisher's liability is governed by article 10 of document 02.

## 2. Operating the service

| Area | Means |
|---|---|
| Hosting | virtual private server located in France (Hostinger International Ltd); database not exposed to the Internet |
| Monitoring | automatic check every 5 minutes (service, database, HTTPS access, certificate, disk space, day's backup); alert to the team on failure |
| Backups | encrypted backup every day, kept 14 days on the server; copies of the server by the host; trial restore on a separate database |
| Updates | backup and copy of the database before every server update; automatic return to the previous version if the service does not restart |
| Maintenance | announced in advance whenever possible, preferably outside shop opening hours |

## 3. Support

| Item | Commitment |
|---|---|
| Channels | "Help and support" menu of the applications; e-mail to support@nhill.online |
| Order of handling | first what prevents selling or taking payments, then what hinders work, then questions |
| Follow-up | every request gets a written answer; the business is kept informed until resolution |
| Times | no response time is guaranteed |

## 4. Connection outages

When the connection is lost, the applications record no operation without an answer from
the server; an interrupted sale can be validated again without being counted twice. The
business plans a fallback (sales book) for long network or power outages.

## 5. Information

On request, the Publisher gives the business the list of the previous month's incidents
that affected it and the measures taken.
