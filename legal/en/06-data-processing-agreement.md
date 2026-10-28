# 06 — Data processing agreement

*Français (version de référence) : [../fr/06-accord-traitement-donnees.md](../fr/06-accord-traitement-donnees.md)*

| Header | |
|---|---|
| Publisher | Nhill Solution, Kinshasa (Democratic Republic of the Congo) |
| Version | 3.0.0 — 28 October 2026 |
| Status | written without review by a lawyer; recommended review points: V-05, V-06, V-07 ([00](00-legal-research.md) § 5). The French version is the reference text |
| Assumptions | H-03, H-04, H-05, H-14, H-15, H-19 ([00](00-legal-research.md) § 2) |
| Place | contract document of the terms of service (document 02); accepted with them |

## 1. Parties and roles

The customer business (the "Customer") is the controller of the data it records in Nhill.
**Nhill Solution** (the "Publisher") acts as processor: it processes this data only to
provide the Service and on the Customer's documented instructions, which are the
contract, this agreement and the settings made in the application.

## 2. Description of the processing

| Item | Content |
|---|---|
| Subject | business management and point of sale: sales, purchases, stock, expenses, payments, reports, team |
| Data subjects | the Customer's customers and suppliers, its employees and users |
| Data | identity and contact details, tax identifiers of third parties, purchase and payment history, attachments, connection data and users' activity log |
| Sensitive data | **none is expected**; the Customer undertakes not to record any in free-text fields |
| Operations | recording, storage, consultation, calculation, export, deletion |
| Duration | term of the contract, then 90 days of export before deletion (document 02, article 9) |
| Location | server located in France (European Union), operated remotely from Kinshasa |

## 3. Publisher's obligations

1. Process the data only on the Customer's instructions; inform it if an instruction seems
   unlawful.
2. Ensure confidentiality: authorised people, bound by secrecy, access limited to the
   needs of operations and support. The team does not consult the Customer's customers,
   products or documents; the overall usage figures it consults are recorded in the
   Customer's activity log.
3. Implement the security measures of article 6.
4. Not use another processor without informing the Customer first (article 5).
5. Help the Customer answer data subject requests (exports, corrections made by the
   Customer itself in the application).
6. Notify the Customer of any data breach affecting it **as soon as possible** after
   becoming aware of it, with the information available (nature, categories and
   approximate volume, likely consequences, measures taken), so that the Customer can
   notify the authority within the legal deadline. Procedure: document 14.
7. Delete the data at the end of the contract (document 02, article 9) and confirm the
   deletion in writing on request.
8. Make available to the Customer the information needed to demonstrate compliance with
   this agreement.

## 4. Customer's obligations

The Customer has a legal basis for each processing, informs the data subjects (including
its employees: document 17), completes the formalities that fall to it with the data
protection authority, assigns access rights in the application and removes those of
people who leave.

## 5. Sub-processors

| Sub-processor | Service | Location |
|---|---|---|
| Hostinger International Ltd, 61 Lordou Vironos str., 6023 Larnaca, Cyprus | virtual private server (application server, database, server backups) | France (European Union) |

At the date of this agreement, no other sub-processor has access to the Customer's data.
The Publisher informs the Customer before adding or replacing a sub-processor (for
example a service sending the automatic e-mails); the Customer may object on legitimate
grounds and, failing agreement, end the contract without penalty. The subscription
payment providers do not receive the Customer's data recorded in Nhill.

## 6. Security measures

- encryption of connections (HTTPS) between the applications and the server;
- separation of each business's data in the database (row-level security enforced by the
  database, application role without bypass privilege);
- database not reachable from the Internet; firewall opening only the web and
  administration ports;
- passwords stored as fingerprints (Argon2id), short sessions with rotation, limited
  attempts;
- role-based access control, activity log that cannot be changed from the application;
- mandatory two-factor authentication for platform administrators;
- encrypted daily backup, trial restore on a separate database;
- infrastructure secrets kept out of the code and the applications.

These measures are reviewed at least once a year. No measure guarantees the complete
absence of vulnerabilities.

## 7. Transfer outside the DRC

The server is located in France. For a Customer established in the DRC, this hosting is a
transfer of data outside the country. The Publisher imposes security and confidentiality
obligations on its host, and cooperates with the Customer on the formalities that apply
to this transfer.

## 8. Duration

This agreement applies as long as the Publisher processes data for the Customer.
