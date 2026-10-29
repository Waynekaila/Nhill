# Nhill 3.0

*English: [README.en.md](README.en.md)*

*Nommé en hommage à Napoleon Hill.* Suite de gestion commerciale multi-établissements pour
les commerçants et PME, trois applications sur un même serveur sécurisé :

| Application | Pour qui | Plateformes |
|---|---|---|
| **Nhill Business** | commerçants, entrepreneurs, PME : ventes (devis, commandes, factures, avoirs), clients et fournisseurs, produits et stock, achats, dépenses, paiements, rapports, équipe et journal « qui a fait quoi », abonnement | Windows, Linux, Android |
| **Nhill POS** | caissiers : sessions de caisse, ventes à plusieurs paiements, remises contrôlées, retours, tickets 58/80 mm ou facture A4 au nom du client, clôture avec écart, rapports X/Z | Windows, Linux |
| **Nhill Console** | exploitant de la plateforme (privée) : installation du serveur, établissements, formules, droits et prix, règles d'attribution, prestataires de paiement, assistance, suppression des données en fin de contrat, audit | Windows, Linux |

- **Serveur** : C++17, API `/api/v1`, PostgreSQL 15+ (Supabase ou auto-hébergé), isolation
  des établissements par sécurité au niveau des lignes forcée, aucun identifiant de base
  dans les applications.
- **Applications** : C++17 / Qt 6 Quick (QML), une seule base de code ; français et
  anglais ; CDF, USD, EUR… montants exacts, jamais additionnés entre devises.
- **Nhill n'est pas un logiciel de facturation homologué par la DGI** : ses documents sont
  des documents de gestion (voir [docs/fr/parametres-legaux.md](docs/fr/parametres-legaux.md)).

## État de la version 3.0.0 (29 octobre 2026, branche `release/3.0.0`)

| Élément | État réel | Preuve |
|---|---|---|
| Compilation + tests (common 22 ; serveur 122 : 121 réussis, 1 ignoré ; installation 6 ; client 15 ; trousseau réel Linux 3) | réussis sur Ubuntu 24.04 | GitHub Actions, branche `ci-results/ci` |
| Formules et droits (fonctions masquées et refusées par le serveur, limites) | 10 tests serveur (`Plans.*`, chaque formule et chaque rôle) ; masquage dans les applications par `Session.has()` (pas de test automatique de l'interface pour chaque formule) | `ci-results/ci` |
| « Se souvenir de moi » (QtKeychain) | réussi : Gestionnaire d'identification Windows (Windows Server 2022), Keystore d'un émulateur Android 11, Secret Service sous Linux ; pas encore sur un téléphone ou un poste d'utilisateur réel | `ci-results/windows`, `ci-results/android-keychain`, `ci-results/ci` |
| Paquets `.deb` (serveur, Business, POS, Console) | construits pour **Ubuntu 24.04** et **Debian 13** ; installés puis désinstallés dans des conteneurs propres | `ci-results/deb-*`, `ci-results/install-*` |
| Kit de mise en service VPS (`deploy/vps`) | recette de 12 contrôles réussie sur une machine Ubuntu 24.04 neuve de la CI ; **aucun déploiement réel sur le VPS Hostinger** (pas d'accès) | `ci-results/vps-test` |
| Installateurs Windows (NSIS, 3 applications) | construits (MSVC 2022, Qt 6.5.3) ; installation silencieuse testée sur Windows Server 2022 | `ci-results/windows` |
| APK Android (Nhill Business, arm64-v8a) | construit, notices tierces incluses ; **signé avec une clé de débogage** (installable, non publiable) ; non testé sur appareil réel | `ci-results/android` |
| Références de l'API (203 points d'accès) et de la base (71 tables) | générées depuis le code et une base migrée ; vérifiées à jour par la CI | `docs/fr/reference-api.md`, `docs/fr/reference-base-de-donnees.md` |
| Site vitrine + backend des formulaires | 24 pages contrôlées ; tests du backend sur SQLite et PostgreSQL ; **pas encore en ligne** : `nhill.online` affiche la page de parking d'Hostinger, aucune messagerie configurée (MX, SPF, DMARC) | `ci-results/site-backend-postgresql`, `ci-results/domain-check` |
| Signature de code (Authenticode, clé Android de publication) | **non faite** — certificats à acquérir | — |
| Audit externe, test d'intrusion | **non faits** ; risques ouverts R1 (cpp-httplib d'Ubuntu 24.04) et R2 (Qt 6.5.3) en attente de décision | [audit de sécurité](docs/fr/03-audit-securite.md) |

Détails, limites et actions humaines : [rapport de validation](docs/fr/rapport-validation.md).

## Démarrage rapide (développement, Linux)

```bash
scripts/install-deps-linux.sh --with-packaging --with-docs --yes
scripts/build-linux.sh                                          # compile + tests
NH_DEV_STATE=/tmp/nhdev scripts/dev/dev-stack.sh start build    # PostgreSQL jetable + API 127.0.0.1:8080
python3 scripts/dev/seed-demo.py --email demo@example.com        # établissement de DÉMONSTRATION
NHILL_SERVER=http://127.0.0.1:8080 build/client/nhill-business
```

Construction de tous les livrables : [guide de compilation](docs/fr/guide-compilation.md)
(`scripts/build-all.sh`, ou GitHub Actions sur une branche `release/**`).

## Documentation

| Document | FR | EN |
|---|---|---|
| **Index par composant** | [docs/fr/index.md](docs/fr/index.md) | [docs/en/index.md](docs/en/index.md) |
| Nhill Business · Nhill POS · Nhill Console | [business](docs/fr/nhill-business.md) · [pos](docs/fr/nhill-pos.md) · [console](docs/fr/nhill-console.md) | [business](docs/en/nhill-business.md) · [pos](docs/en/nhill-pos.md) · [console](docs/en/nhill-console.md) |
| Serveur · base de données | [serveur](docs/fr/serveur.md) · [base](docs/fr/base-de-donnees.md) | [server](docs/en/server.md) · [database](docs/en/database.md) |
| Références générées (API, base) | [API](docs/fr/reference-api.md) · [base](docs/fr/reference-base-de-donnees.md) | [API](docs/en/api-reference.md) · [database](docs/en/database-reference.md) |
| Mise en production (VPS Hostinger) | [docs/fr/deploiement-vps-hostinger.md](docs/fr/deploiement-vps-hostinger.md) | [docs/en/vps-deployment-hostinger.md](docs/en/vps-deployment-hostinger.md) |
| Guide utilisateur | [docs/fr/guide-utilisateur.md](docs/fr/guide-utilisateur.md) | [docs/en/user-guide.md](docs/en/user-guide.md) |
| Installation | [docs/fr/guide-installation.md](docs/fr/guide-installation.md) | [docs/en/installation-guide.md](docs/en/installation-guide.md) |
| Configuration | [docs/fr/guide-configuration.md](docs/fr/guide-configuration.md) | [docs/en/configuration-guide.md](docs/en/configuration-guide.md) |
| Développement | [docs/fr/guide-developpement.md](docs/fr/guide-developpement.md) | [docs/en/developer-guide.md](docs/en/developer-guide.md) |
| Compilation et livraison | [docs/fr/guide-compilation.md](docs/fr/guide-compilation.md) | [docs/en/build-guide.md](docs/en/build-guide.md) |
| Sécurité | [docs/fr/securite.md](docs/fr/securite.md) | [docs/en/security.md](docs/en/security.md) |
| Paramètres légaux | [docs/fr/parametres-legaux.md](docs/fr/parametres-legaux.md) | [docs/en/legal-parameters.md](docs/en/legal-parameters.md) |
| Journal des modifications | [docs/fr/changelog.md](docs/fr/changelog.md) | [docs/en/changelog.md](docs/en/changelog.md) |
| Audits technique, UI/UX, sécurité | [docs/fr/01…03](docs/fr/) | [docs/en/01…03](docs/en/) |
| Rapport de validation | [docs/fr/rapport-validation.md](docs/fr/rapport-validation.md) | [docs/en/validation-report.md](docs/en/validation-report.md) |
| Site vitrine | [website/README.md](website/README.md) | [website/README.en.md](website/README.en.md) |
| Dossier juridique (RDC, 20 modèles) | [legal/fr/](legal/fr/00-recherche-juridique.md) | [legal/en/](legal/en/00-legal-research.md) |
| Dossier commercial (Kinshasa) | [marketing/fr/](marketing/README.md) | [marketing/en/](marketing/README.md) |

Chaque document Markdown a son PDF généré (`docs/pdf/`, `legal/pdf/`, `marketing/pdf/`,
`README.pdf`, `README.en.pdf`) par `scripts/build-docs-pdf.sh`. Les documents des versions
0.1 à 2.x sont archivés dans `docs/archive/` (non tenus à jour).

## Arborescence

```
common/      bibliothèque C++ partagée (montants, codes, validations, entier 128 bits portable)
server/      API C++17 (http, services, setup, jobs, pdf), profils pays, traductions
client/      applications Qt 6 : src/, qml/Nhill/{Ui,Business,Console,Pos}, i18n/, android/
db/          migrations 0001-0015 (empreintes vérifiées)
tests/       common, server (PostgreSQL jetable), client, outil de test autonome
packaging/   Debian, Windows (NSIS), Android, notices des composants tiers
deploy/      serveur : systemd, Caddy, sauvegardes, kit VPS Hostinger
scripts/     construction, vérification, livraison, QA, i18n, développement
website/     site vitrine FR/EN et backend des formulaires
docs/ legal/ marketing/   documentation, dossiers juridique et commercial (FR/EN, MD + PDF)
.github/     workflows CI (tests, paquets Linux, Windows, Android, documentation et site)
```

## Contact

Nhill Solution — siège à Kinshasa (République démocratique du Congo).
Contact général : info@nhill.online · Assistance aux clients : support@nhill.online.

## Licence

Code du projet : propriétaire, Nhill Solution, tous droits réservés. Dépendances :
`docs/licences-dependances.md` (Qt en LGPL v3 : liaison dynamique et notice obligatoires).
