# Documentation de Nhill 3.0

*English: [../en/index.md](../en/index.md)*

Chaque document existe en français et en anglais ; sa version PDF est dans `docs/pdf/fr/`
ou `docs/pdf/en/` (même nom, extension `.pdf`), générée par `scripts/build-docs-pdf.sh`.

## Par composant

| Composant | Rôle | Documents |
|---|---|---|
| **Nhill Business** | application de l'établissement (Windows, Linux, Android) | [nhill-business.md](nhill-business.md) |
| **Nhill POS** | caisse (Windows, Linux) | [nhill-pos.md](nhill-pos.md) |
| **Nhill Console** | application de l'exploitant (Windows, Linux) | [nhill-console.md](nhill-console.md) |
| **Serveur Nhill** | API, droits, documents, tâches de fond (Linux) | [serveur.md](serveur.md) · [reference-api.md](reference-api.md) (généré depuis le code) |
| **Base de données** | PostgreSQL, isolement des établissements, sauvegardes | [base-de-donnees.md](base-de-donnees.md) · [reference-base-de-donnees.md](reference-base-de-donnees.md) (généré depuis une base migrée) |
| **Mise en production** | VPS Hostinger en France : installation, TLS, sauvegardes, surveillance, mises à jour | [deploiement-vps-hostinger.md](deploiement-vps-hostinger.md) |
| **Site vitrine** | site FR/EN, formulaires, contrôles | [`website/README.md`](../../website/README.md) |

## Guides transversaux

| Document | Contenu |
|---|---|
| [guide-utilisateur.md](guide-utilisateur.md) | nouveautés 3.0, règles communes, ce que Nhill fait et ne fait pas |
| [guide-installation.md](guide-installation.md) | installer les applications et le serveur, mettre à jour, dépanner |
| [guide-configuration.md](guide-configuration.md) | `/etc/nhill/server.env`, réglages des applications, de la plateforme et des établissements |
| [guide-compilation.md](guide-compilation.md) | construire les paquets Linux, Windows, Android et les PDF |
| [guide-developpement.md](guide-developpement.md) | architecture, tests, interface, traductions, migrations |
| [securite.md](securite.md) · [03-audit-securite.md](03-audit-securite.md) | mesures de sécurité, audit 3.0.0, risques et décisions en attente |
| [parametres-legaux.md](parametres-legaux.md) | paramètres légaux et fiscaux, textes publiés |
| [matrice-conformite-site.md](matrice-conformite-site.md) | correspondance entre le site et l'application |
| [rapport-validation.md](rapport-validation.md) | ce qui a été construit et testé, où et comment |
| [changelog.md](changelog.md) | journal des modifications |
| [01-audit-technique.md](01-audit-technique.md) · [02-audit-ui-ux.md](02-audit-ui-ux.md) | audits technique et d'interface |

Documents juridiques : `legal/fr/` (index : [`legal/fr/00-recherche-juridique.md`](../../legal/fr/00-recherche-juridique.md) § 4).
Documents commerciaux : `marketing/fr/`.

Les fichiers à la racine de `docs/` (`technique.md`, `api.md`, etc.) décrivent les versions
0.1 à 2.x et sont conservés dans `docs/archive/` pour l'historique ; ils ne sont plus tenus
à jour.
