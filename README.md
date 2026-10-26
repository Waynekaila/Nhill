# Nhill

*Nommé en hommage à Napoleon Hill.* Suite de gestion commerciale multi-établissements,
trois applications sur un même serveur sécurisé :

| Application | Pour qui | Plateformes |
|---|---|---|
| **Nhill Business** | Commerçants, entrepreneurs, PME : ventes (devis, commandes, factures, avoirs), clients et fournisseurs, produits et stock, achats, dépenses, paiements, rapports, équipe et journal « qui a fait quoi », abonnement | Windows, Linux, Android |
| **Nhill POS** | Caissiers d'un établissement Nhill Business : sessions de caisse, fond de caisse, ventes à plusieurs paiements, remises, retours et annulations autorisés, tickets, entrées/sorties d'espèces, clôture avec écart, rapports X/Z | Windows, Linux |
| **Nhill Console** | Créateurs de Nhill (privée) : installation du serveur, établissements, abonnements, offres et prix, codes promo, prestataires de paiement, transactions, demandes d'assistance, erreurs, audit, statistiques | Windows, Linux |

* **Serveur** : C++17, API `/api/v1`, PostgreSQL 15+ (**Supabase** ou auto-hébergé) ;
  isolation des établissements par row-level security forcée, schéma dédié `nhill`,
  aucune clé `service_role` ni identifiant de base dans les applications.
* **Applications** : C++17 / Qt 6 Quick (QML), une seule base de code.
* **Langues** : français et anglais (interfaces, factures, tickets, rapports PDF).
  **Devises** : CDF, USD, EUR et autres, montants exacts en unités mineures.

> **État réel (26 octobre 2026)** — détails dans `docs/rapport-tests.md` et
> `docs/limites.md`.
> * Linux : tout est compilé et testé (18 + 82 + 6 + 13 tests ; 1 génération d'exemples
>   ignorée par défaut). **Paquets .deb** des quatre composants construits et **installés
>   dans un Ubuntu 24.04 vierge** ; installation de la base par l'API et parcours POS
>   complet vérifiés avec les paquets installés.
> * **Windows et Android : scripts, installateurs Inno Setup et configuration fournis,
>   non compilés** (ni Qt pour Windows/Android ni SDK Android disponibles ici).
> * Base Supabase de production initialisée (migrations 0001–0010), conseiller de
>   sécurité Supabase : 0 alerte.
> * **Paiements** : adaptateurs Stripe (cartes Visa/Mastercard) et PayPal testés
>   **uniquement contre des réponses simulées** ; Payoneer en configuration seulement.
>   Aucun paiement réel effectué.
> * **Nhill n'est pas un système de facturation ou de caisse homologué** : factures et
>   tickets sont des documents de gestion (`docs/conformite/conformite.md`). La
>   disponibilité juridique du nom « Nhill » n'a pas été vérifiée.

## Arborescence

```
CMakeLists.txt            construction globale (NHILL_BUILD_SERVER/CLIENT/TESTS) + paquets (cmake/)
common/                   bibliothèque C++ partagée : montants exacts, codes, validations
server/                   API : src/ (http, services, setup, jobs, pdf, util), profiles/, i18n/
client/                   applications Qt : src/{core,business,console,pos}, qml/Nhill/{Ui,Business,Console,Pos} (+ module C++ Nhill.Core),
                          i18n/ (fr, en), android/, resources/ (icônes Windows, .desktop)
db/bootstrap/             création manuelle des rôles (alternative à l'installation par la console)
db/migrations/            schéma versionné 0001…0010 (empreintes SHA-256 vérifiées)
tests/                    common/, server/ (API + PostgreSQL jetable, installation), client/
assets/                   marque (logos SVG/PNG), icônes des trois applications, polices IBM Plex (OFL)
config/                   server.env.example (aucun secret)
deploy/                   systemd, Caddy (TLS), sauvegarde/restauration chiffrées, installation
packaging/                scripts Debian du serveur, installateur Windows (Inno Setup)
scripts/                  build-linux.sh (--deb), build-windows.ps1, build-android.sh, brand/, dev/
docs/                     documentation, captures d'écran
```

## Démarrage rapide (développement, Linux)

```bash
scripts/build-linux.sh build --deb                              # compile, teste, construit les .deb
NH_DEV_STATE=/tmp/nhdev scripts/dev/dev-stack.sh start build    # PostgreSQL jetable + API sur 127.0.0.1:8080
python3 scripts/dev/seed-demo.py --email demo@example.cd        # établissement de DÉMONSTRATION (données fictives)
NHILL_SERVER=http://127.0.0.1:8080 build/client/nhill-business
NHILL_SERVER=http://127.0.0.1:8080 build/client/nhill-pos
build/client/nhill-console
```

Mise en production : `docs/deploiement-production.md` (paquet serveur → mode
installation → assistant de Nhill Console avec Supabase ou PostgreSQL).

## Documentation

| Document | Contenu |
|---|---|
| `docs/technique.md` | Architecture, données, calculs, numérotation, abonnements, prestataires, point de vente, installation |
| `docs/installation.md` | Construction Linux / Windows / Android, paquets, développement |
| `docs/deploiement-production.md` | Mise en production pas à pas (Supabase ou PostgreSQL) |
| `docs/guide-utilisateur.md` | Nhill Business, Nhill Console, Nhill POS |
| `docs/api.md` | Référence de l'API |
| `docs/securite.md` | Modèle de sécurité et **audit interne** |
| `docs/sauvegarde-restauration.md` | Sauvegarde et restauration |
| `docs/mise-a-jour.md` | Mise à jour |
| `docs/conformite/conformite.md` | RDC, UE, international : méthode, points à valider |
| `docs/modeles-et-profils.md` | Modèles PDF et profils fiscaux |
| `docs/brand/identite.md` | Nom, logo, couleurs, typographie |
| `docs/licences-dependances.md` | Dépendances et licences |
| `docs/limites.md` | Limites connues et éléments non vérifiés |
| `docs/rapport-tests.md` | Tests exécutés et résultats observés |
| `docs/captures/` | Captures (installation `s*`, caisse `pos-*`, Business `business-*`, Console `console-*`) |

## Licence

Code du projet : propriétaire (à définir par les créateurs). Dépendances : voir
`docs/licences-dependances.md` (Qt en LGPL v3 : lien dynamique et notice obligatoires).
