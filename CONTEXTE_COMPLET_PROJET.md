# CONTEXTE COMPLET DU PROJET NHILL
**Document de passation pour une nouvelle session Claude**
Généré le 2026-11-05 · Dépôt : `Waynekaila/Nhill` · Branche courante : `release/3.0.0` · Dernier commit : `c2e9613`

> Ce document a été rédigé en vérifiant directement les fichiers du dépôt (`git log`, arborescence, `CMakeLists.txt`, listes de documents, `release/V3.0.0/`). Aucune information ci-dessous n'est inventée : quand un point n'a pas pu être vérifié, il est marqué **« à vérifier »**.

---

## 1. Présentation générale du projet

**Nhill** est une solution logicielle multi-établissements (SaaS) de gestion commerciale et de caisse, écrite en C++17, destinée aux commerçants et petites/moyennes entreprises de Kinshasa (République Démocratique du Congo), avec une architecture pensée pour s'étendre à d'autres marchés.

Elle est exploitée par **Nhill Solution**, nom commercial. **L'entreprise n'est pas légalement enregistrée à ce jour** — ce fait est une contrainte permanente du projet (voir section 13) : ne jamais mentionner ou laisser entendre un numéro d'enregistrement, un statut juridique ou une certification quelconque.

Le projet comprend :
- Un **serveur** C++17 multi-tenant (API HTTP, base PostgreSQL).
- Trois **applications clientes** Qt 6 Quick/QML : **Nhill Business** (gestion), **Nhill POS** (caisse), **Nhill Console** (administration de la plateforme, réservée à Nhill Solution).
- Un **site vitrine** statique bilingue (FR/EN).
- Des **corpus documentaires** complets : documentation technique/utilisateur, corpus juridique (21 documents), corpus marketing (16 documents).
- Un **kit de déploiement** pour VPS Hostinger (France).
- Une **chaîne d'intégration continue** (GitHub Actions) et un **pipeline de publication** (`release/V3.0.0`).

---

## 2. Vision, objectifs et public cible

**Public cible** : commerçants et PME de Kinshasa/RDC — boutiques, dépôts, points de vente avec caisse — qui ont besoin de facturer, suivre leur stock, gérer plusieurs dépôts/devises et avoir une caisse fiable, sans connexion internet permanente garantie.

**Positionnement** : international, avec les normes européennes citées **comme référence de qualité uniquement**, sans aucune revendication de conformité réglementaire (ni certification française/européenne, ni certification fiscale congolaise). Nhill n'est **certifié dans aucun pays** : ce n'est ni une caisse certifiée, ni un dispositif de facturation électronique réglementaire, ni un dispositif fiscal.

**Principe fondateur, non négociable** : chaque établissement ne voit et n'utilise que les fonctionnalités incluses dans sa **formule (plan)** d'abonnement. Une fonctionnalité hors formule doit être **invisible** dans les applications **et refusée côté serveur**. Ce principe structure toute l'architecture (voir section 5).

**Offre commerciale actuelle** : **essai gratuit accompagné de 30 jours** (« 30-day assisted free trial »), qui a remplacé définitivement l'ancienne notion de « pilote de 8 semaines » / « programme pilote » dans tous les documents (marketing, légal, produit). Toute occurrence résiduelle de « pilote »/« pilot » dans un futur document doit être corrigée vers « essai accompagné »/« assisted trial ».

---

## 3. Historique synthétique de l'évolution

Le projet est arrivé à cette session avec une base déjà construite (versions antérieures 0.x à 2.x), documentée dans `docs/fr/changelog.md` / `docs/en/changelog.md` et archivée dans `docs/archive/`. Cette session (et la session précédente dont elle est la continuation) a porté sur la **refonte complète menant à la version 3.0.0** :

1. **Système de formules configurables** construit de bout en bout (base de données, serveur, applications) — remplace un modèle antérieur moins strict.
2. **Nettoyage total des données fictives et des placeholders** dans le corpus marketing et juridique (règle absolue du projet).
3. **Reconstruction de la documentation** : abandon d'un ensemble `docs/*.md` plat et partiellement obsolète au profit d'une documentation par composant (FR/EN), avec **génération automatique** des références API et base de données directement depuis le code source et un schéma de base vivant, vérifiée en intégration continue.
4. **Corrections de fiabilité CI** : une course critique dans les captures d'écran (Xvfb) et une instabilité de l'émulateur Android (service `activity`).
5. **Correction d'un manquement de conformité réel** : notice BSD manquante dans l'APK Android.
6. **Packaging et livraison complète de la version 3.0.0** : `release/V3.0.0/` assemblé, vérifié par somme de contrôle SHA-256 (331 fichiers, `checksums/MISSING.txt` vide), livré à l'utilisateur sous forme d'archive ZIP, avec un rapport final écrit dans le projet claude.ai (`claude/nhill-3.0.0-rapport-final.md`).

Le dernier message utilisateur avant la présente demande était **« bien »** (accusé de réception), suivi d'une demande explicite de **document de continuité complet** — objet du présent fichier.

---

## 4. Cahier des charges actuel et spécifications définitives

### 4.1 Règles absolues (non négociables, valables pour toute session future)

1. **Aucun placeholder, nulle part.** Pas de `[À COMPLÉTER]`, pas de crochets de substitution, pas de donnée fictive ou inventée dans le site, les interfaces, les documents ou tout livrable final. Si une information essentielle manque, **poser la question directement à l'utilisateur et attendre sa réponse**, tout en continuant les tâches indépendantes de cette information.
2. **Isolation stricte des fonctionnalités par formule.** Toute fonctionnalité non incluse dans la formule active d'un établissement doit être totalement invisible, et ceci est **appliqué côté serveur**, jamais seulement côté interface.
3. **Contrôle côté serveur de tout.** Aucune confiance faite au client.
4. **Administration configurable.** Les réglages (formules, limites, politiques) se pilotent depuis Nhill Console, pas en dur dans le code.
5. **Tests de non-régression obligatoires**, par formule et par rôle.

### 4.2 Contraintes de méthode (standing constraints)

- Ne **jamais simuler** une compilation, des tests, un audit ou une validation juridique : tout doit être réellement exécuté et son résultat réellement observé.
- Ne jamais prétendre qu'un fichier existe s'il n'existe pas.
- Ne jamais affirmer l'absence de vulnérabilités.
- Aucun secret dans le code ni sur le site public ; ne jamais publier de `.env` contenant des secrets ; aucun mot de passe de base de données dans le JavaScript du navigateur.
- Ne pas modifier la logique métier sans justification et sans tests.
- **« Ne prétends pas qu'une correction est terminée si elle n'a pas été vérifiée »** — règle citée textuellement par l'utilisateur, à respecter à la lettre à chaque correction.

### 4.3 Faits d'entreprise fixés (binding)

| Point | Valeur fixée |
|---|---|
| Nom commercial | Nhill Solution |
| Siège | Kinshasa |
| Statut légal | **Non enregistrée légalement** — ne jamais l'impliquer ni le mentionner |
| Téléphone | Aucun |
| Hébergement | VPS Hostinger, France |
| Adresse API | Configurée par l'utilisateur à l'installation ; **jamais codée en dur** dans les builds de production |
| Référentiel de conformité | Normes européennes citées **comme référence uniquement**, aucune revendication de conformité |
| Droit applicable | Droit de la RDC, tribunaux de Kinshasa, **plus** règles impératives de protection du consommateur et des données du propre pays de l'utilisateur |
| Plafond de responsabilité | 12 mois |
| SLA chiffré | Aucun |
| Domaine officiel | `nhill.online` |
| Contacts officiels | `info@nhill.online`, `support@nhill.online` |
| Usage du domaine | `https://nhill.online` ne doit être utilisé/affirmé comme opérationnel **qu'une fois le HTTPS et le domaine vérifiés en direct** — **à ce jour, le site n'est pas publié** (voir section 8) |

---

## 5. Architecture technique et technologies utilisées

### 5.1 Serveur

- **Langage** : C++17.
- **Bibliothèques** : cpp-httplib (HTTP), libpqxx (PostgreSQL), libsodium (cryptographie), libharu (PDF), libcurl, ZLIB.
- **Base de données** : PostgreSQL 15 minimum (16/17 en usage), avec :
  - **Sécurité au niveau ligne (row-level security) forcée** sur **31 tables de gestion métier** (ce chiffre a été vérifié par requête SQL directe contre le schéma réel — une affirmation antérieure trop large « toutes les tables » a été corrigée).
  - Les tables de plateforme (comptes, abonnements, journal d'audit, etc.) sont protégées non par RLS mais par des **privilèges minimaux accordés au rôle `nhill_app`**.
  - **16 migrations** présentes dans `db/migrations/` : `0001_platform.sql` à `0015_search_patterns.sql` (vérifié dans le dépôt).
  - Schéma dédié `nhill`, fermé aux rôles publics, fonctions à `search_path` fixe.
  - **31 fonctions SQL** (vérifiées lors de la génération de `reference-base-de-donnees.md`), **71 tables** au total.
- **Services serveur** : 31 fichiers dans `server/src/services/` (vérifié).
- **Routes API** : **203 points d'accès** (vérifié par le générateur `scripts/docs/api_routes.py` / `scripts/docs/gen_api_reference.py`), chacun déclaré avec `RouteOpts{Access, permission, write, adminRoles, feature}` dans `server/src/http/app.hpp`.
- **Entitlements** : constantes `entitlements::key::kXxx`, tables `plan_capabilities` / `plan_entitlements`.
- **Codes d'erreur de limite** (vérifiés dans `entitlements.cpp`) : `409 plan_limit_reached`, `409 user_limit_reached`, `409 plan_register_limit`.
- **Mot de passe Nhill Console (premier accès)** : 8 caractères minimum, pas un mot de passe trop commun (vérifié dans `v::checkPassword`, corrigeant une mention antérieure erronée de « 14 caractères »).

### 5.2 Applications clientes (Qt 6 Quick/QML)

- **Nhill Business** : Windows, Linux, Android. Gestion complète (ventes, clients, stock, achats, dépenses, rapports, équipe, réglages).
- **Nhill POS** : Windows, Linux uniquement (pas d'Android/iOS). Caisse, utilise mêmes produits/prix/taxes/stock/numérotation que Nhill Business.
- **Nhill Console** : Windows, Linux. Application privée, réservée à l'exploitant Nhill Solution, non distribuée aux établissements.
- Composants partagés : design system `Nhill.Ui` (`client/qml/Nhill`, `client/qml/components`).
- Internationalisation : `client/i18n/nhill_fr.ts`, `nhill_en.ts`.
- Visibilité des fonctions pilotée par `Session.has()` / `Session.can()` en QML, miroir côté client du contrôle serveur (mais le serveur reste la source de vérité).
- Dossiers client vérifiés : `client/qml/Nhill`, `client/qml/components`, `client/qml/admin`, `client/qml/pages`, `client/android`.

### 5.3 CI/CD

- Dépôt GitHub : `Waynekaila/Nhill`.
- Workflows (`.github/workflows/`) : `ci.yml`, `windows.yml`, `android.yml`, `linux-packages.yml`, `docs-website.yml` — **5 fichiers vérifiés** (le fichier `android-keychain` n'est pas un workflow séparé mais un job/étape au sein des workflows Android).
- `concurrency: cancel-in-progress: true` : un nouveau push annule les exécutions en cours, donc chaque push doit attendre la fin de la CI précédente pour être significatif.
- Résultats publiés sur des branches orphelines `ci-results/*`, lisibles via `git fetch` + `git show origin/ci-results/<branche>:<chemin>`.
- Preuves CI archivées localement dans `dist/ci-evidence/` (fichiers `*-RUN-INFO.txt` par job : `ci`, `windows`, `android`, `android-keychain`, `deb-ubuntu-24.04`, `deb-debian-13`, `install-ubuntu-24.04`, `install-debian-13`, `vps-test`, `docs-website`, `site-backend-postgresql`).

### 5.4 Documentation générée depuis le code

- `scripts/docs/api_routes.py` + `scripts/docs/api_descriptions.py` + `scripts/docs/gen_api_reference.py` → `docs/fr/reference-api.md` / `docs/en/api-reference.md`, avec mode `--check` intégré en CI.
- `scripts/docs/db_descriptions.py` + `scripts/docs/gen_db_reference.py` → `docs/fr/reference-base-de-donnees.md` / `docs/en/database-reference.md` (utilise une base PostgreSQL temporaire réelle, migrée depuis zéro, pour introspection).
- `scripts/docs/md2pdf.py` + `scripts/build-docs-pdf.sh`, pilotés par `docs/pdf-manifest.txt`, avec vérification par somme SHA-256 (`docs/pdf/manifest.json`).

### 5.5 Déploiement

- Kit `deploy/vps/` pour VPS Hostinger (France) : sous-dossiers `caddy`, `systemd`, `scripts`, `backup`, `tests`.
- Script `deploy/vps/provision.sh` pour l'installation en ligne de commande, en miroir de la procédure manuelle décrite dans Nhill Console.
- Documentation associée : `docs/fr/deploiement-vps-hostinger.md` / `docs/en/vps-deployment-hostinger.md`.

### 5.6 Packaging de version

- `scripts/package-release.sh` assemble `release/V<version>/` **strictement depuis `dist/`**, sans jamais fabriquer de fichier manquant — toute plateforme absente est consignée dans `checksums/MISSING.txt` plutôt que simulée.

---

## 6. Fonctionnalités réalisées et leur état

> Convention : **✅ Implémenté et vérifié** (code lu + testé en CI) · **⚠️ Implémenté, test limité** (code présent, preuve de test partielle) · **❌ Non fait**.

| Fonctionnalité | État | Preuve |
|---|---|---|
| Formules configurables (fonctions + limites + conditions + règles d'attribution) | ✅ | Code `entitlements.cpp`, tables migration 0012, UI Nhill Console §5-6, 203 routes contrôlées |
| Invisibilité stricte des fonctions hors formule (client + serveur) | ✅ | `Session.has()/can()` côté QML, `RouteOpts.feature` côté serveur |
| Documents unifiés (A4 / ticket 58-80mm, un seul modèle) | ✅ | Migration 0013 « documents », doc Nhill POS §3 |
| Facture au nom d'un client depuis la caisse | ✅ | Nhill POS §3, même numérotation que Nhill Business |
| Export complet par le propriétaire (ZIP) | ✅ | Doc Nhill Business/Console |
| Suppression définitive 90 jours après fin d'accès | ✅ | Migration 0014, Nhill Console §4 |
| Nettoyages de rétention automatiques | ✅ | Doc légal 15 / `docs/fr/rapport-validation.md` |
| Recherche littérale de `%` et `_` | ✅ | Migration 0015, commit `5e7e30e`, test dédié |
| Kit de mise en service VPS Hostinger | ✅ | 12/12 contrôles d'acceptation passés (voir section 10) |
| Documentation par composant générée/vérifiée (API 203 routes, BDD 71 tables) | ✅ | `--check` en CI (`ci.yml`, `docs-website.yml`) |
| Notice tierce BSD dans l'APK Android | ✅ | `client/android/assets/THIRD-PARTY-NOTICES.txt` + vérification build + QML-check |
| Authentification à deux facteurs obligatoire (Console) | ✅ | Doc Nhill Console §2 |
| Rôles Console (`superadmin`, `billing`, `support`, `auditor`) | ✅ | Doc Nhill Console §2, référence API colonne « Console roles » |
| Déploiement réel sur le VPS de production | ❌ | Aucun accès VPS production ; seul le kit est testé en CI sur machine neuve |
| Site `nhill.online` publié en HTTPS | ❌ | Domaine renvoie la page de parking Hostinger (voir section 8) |
| Signature de l'APK (clé de production) | ❌ | APK signé avec une **clé de débogage** uniquement |
| Signature des installateurs Windows | ❌ | Non signés |
| Mise à jour testée avec de vraies données 2.1→3.0 | ❌ | Non testé (pas de base 2.1 réelle disponible) |
| Envoi réel d'e-mails (SMTP de production) | ❌ | Non testé |
| Audit de sécurité externe | ❌ | Non réalisé |
| Relecture juridique par un avocat RDC agréé | ❌ | Non réalisée (V-01 à V-14 du registre de validation juridique restent ouverts) |

---

## 7. Décisions importantes et raisons de ces décisions

| Décision | Raison |
|---|---|
| Essai gratuit accompagné de 30 jours, remplaçant le « pilote de 8 semaines » | Alignement commercial définitif ; tout document résiduel mentionnant le pilote a été corrigé |
| Trackers marketing livrés **vides** (sans ligne d'exemple fictive) | Respect strict de la règle « aucune donnée fictive » ; les lignes « EXEMPLE — fictif » ont été supprimées |
| Champs de fusion `{curly_brace}` documentés avec leur source exacte | Éviter tout placeholder dans les modèles de messages/scripts, tout en gardant les modèles réutilisables |
| RLS qualifiée précisément à « 31 tables de gestion métier » (pas « toutes ») | Une affirmation plus large était fausse ; corrigée après vérification SQL directe sur le schéma réel |
| Risque R1 (cpp-httplib Ubuntu 24.04) documenté avec 3 options (bundler une version ≥0.23, Ubuntu Pro ESM, migrer vers Debian 13) | Debian 13 a été vérifié comme déjà corrigé (Debian Security Tracker) ; décision laissée à l'utilisateur, non tranchée unilatéralement |
| Échec du contrôle de domaine (`nhill.online`) documenté explicitement comme « attendu » dans le README et le rapport de validation, plutôt que masqué | Règle « ne jamais affirmer un statut opérationnel non vérifié » |
| Ancienne documentation (versions 0.1 à 2.x) déplacée vers `docs/archive/` plutôt que supprimée | Traçabilité ; rien n'est perdu, mais rien d'obsolète ne reste au premier niveau |
| Commission des partenaires non chiffrée dans le corpus marketing | Le taux dépend de chaque lettre d'accord individuelle (voir `legal/fr/10-contrat-partenariat.md` §5) ; aucun taux standard n'a été fixé par l'utilisateur |

---

## 8. Problèmes connus, bugs et limitations

### 8.1 Limitations déclarées dans `RELEASE-NOTES.md` (vérifiées dans le fichier)

- **APK Android** : signé avec une clé de débogage (installable, non publiable sur un store).
- **Installateurs Windows** : non signés.
- **Risque R1** : `cpp-httplib` du paquet Ubuntu 24.04 — faille connue, atténuée mais non corrigée dans ce paquet. Décision attendue (`docs/fr/03-audit-securite.md`).
- **Risque R2** : Qt 6.5.3 utilisé sous Windows et Android — décision de mise à niveau en attente, impact potentiel sur la version Android minimale.
- **Aucun déploiement réel** sur le VPS de production.
- **Site `nhill.online` non publié** : le domaine renvoie actuellement la page de parking Hostinger, sans MX/SPF/DMARC — **6 contrôles obligatoires échouent**, documenté explicitement comme échec attendu en CI (`docs-website` / `site-backend-postgresql`, voir le détail dans `docs/fr/rapport-validation.md`).
- **Aucun audit de sécurité externe.**

### 8.2 Bugs rencontrés et corrigés durant cette session (tous vérifiés, pas simulés)

| Bug | Cause | Correction | Vérification |
|---|---|---|---|
| Captures d'écran POS manquantes (1366×768) | Course Xvfb : numéro d'affichage aléatoire + `sleep 1s` insuffisant | `scripts/qa/ui-screenshots.sh` : scan d'un numéro d'affichage libre + attente active (jusqu'à 10s) du socket `/tmp/.X11-unix/X$n`, repli explicite avec avertissement | Testé avec un harnais autonome avant intégration |
| `cmd: Can't find service: activity` sur émulateur Android (CI) | Service `activity` pas encore prêt juste après le démarrage de l'émulateur | `.github/scripts/android-keychain-emulator.sh` : boucle d'attente sur `service check activity` + wrapper `adb_am()` avec 20 tentatives/3s | Log CI confirmé : 4 phases (écriture/lecture/déconnexion/oubli) → OK |
| Table Markdown cassée dans `reference-api.md` | Une description de route contenait littéralement des `|` (issu d'un enum C++ `bug\|question\|billing\|feature`) | Fonctions `cell()`/`code()` dans `gen_api_reference.py` échappant les `|` | Table régénérée et vérifiée visuellement |
| `gen_db_reference.py` cherchait un fichier `VERSION` inexistant | Mauvaise hypothèse initiale | Lecture de `project(nhill VERSION ...)` dans `CMakeLists.txt`, cohérent avec `md2pdf.py` | Généré avec succès |
| Règle de visibilité des caisses POS mal décrite | Description initiale omettait le rôle superviseur | Lecture de `assigned()` dans `pos.cpp` : les superviseurs (till supervisor/manager/owner/administrator) voient toutes les caisses via le contournement `pos.supervise` | Corrigé dans `docs/fr/nhill-pos.md` §2 |

### 8.3 Décisions explicitement en attente de l'utilisateur (non résolues à ce jour)

1. **R1** — remédiation `cpp-httplib` : bundler une version ≥0.23, souscrire Ubuntu Pro ESM, ou migrer la production vers Debian 13 (déjà corrigé en paquet).
2. **R2** — mise à niveau Qt 6.5.3, avec impact potentiel sur la version Android minimale.
3. **Hébergeur du site vitrine** à préciser pour les mentions légales.
4. **Adresse d'expéditeur** des e-mails automatiques du serveur, et configuration DNS du domaine (MX, SPF, DMARC).
5. **Taux de commission standard des partenaires**, s'il doit y en avoir un (actuellement laissé à la lettre d'accord individuelle).

---

## 9. Fonctionnalités et améliorations restantes

- Signature des livrables (certificats de signature de code — Windows et Android) : **dépend d'une action de l'utilisateur** (achat/obtention des certificats).
- Déploiement réel sur le VPS de production avec le kit `deploy/vps/` : **dépend d'un accès VPS fourni par l'utilisateur**.
- Publication effective du site `nhill.online` (DNS, HTTPS, e-mail du domaine) : **dépend des décisions de la section 8.3**.
- Test de mise à jour avec une vraie base 2.1 en production : **dépend d'un jeu de données réel**.
- Test avec un vrai serveur d'envoi SMTP : **dépend d'une configuration e-mail fournie**.
- Audit de sécurité externe / test d'intrusion : **dépend d'un prestataire externe mandaté par l'utilisateur**.
- Relecture juridique par un avocat agréé en RDC (registre V-01 à V-14 dans `legal/fr/00-recherche-juridique.md`) : **dépend d'un avocat mandaté par l'utilisateur**.
- Test sur appareils réels (téléphone Android physique, poste Windows physique), hors émulateur/CI.

Aucune de ces tâches ne peut être avancée de façon autonome par une future session Claude sans une information ou un accès fourni par l'utilisateur ; elles doivent rester **posées comme questions ouvertes**, jamais contournées par une simulation.

---

## 10. État actuel du projet et niveau d'avancement

**Version courante : 3.0.0, livrée.**

- Branche : `release/3.0.0`.
- Dernier commit : `c2e9613` (« Validation report 3.0.0 with the CI results of 18d7054 »).
- Commit de code (hors rapport) : `18d7054`, avec CI entièrement verte sur tous les jobs : `ci`, `windows`, `android`, `android-keychain`, `deb-ubuntu-24.04`, `deb-debian-13`, `install-ubuntu-24.04`, `install-debian-13`, `vps-test`, `docs-website`, `site-backend-postgresql`.
- Arbre de travail propre (`git status` sans modification en attente au moment de la rédaction de ce document).

**Résultats de tests (CI du commit `18d7054`, vérifiés dans `docs/fr/rapport-validation.md`)** :

| Élément | Résultat |
|---|---|
| Tests de base (`common`) | 22/22 |
| Tests serveur | 121 réussis, 1 ignoré, sur 122 |
| Installation (paquets) | 6/6 |
| Applications (Linux/Windows/Android) | 15/15 |
| « Se souvenir de moi » (remember-me) | Réussi sur Linux, Windows, émulateur Android |
| Kit de mise en service VPS (machine neuve) | 12/12 contrôles d'acceptation |

**Livraison** :
- `release/V3.0.0/` assemblé par `scripts/package-release.sh --force`, contenant : `android/`, `linux/` (ubuntu-24.04 + debian-13), `windows/`, `website/`, `documentation/`, `legal/`, `marketing/`, `source/`, `checksums/`.
- `checksums/SHA256SUMS` : **331 fichiers**, vérifiés par `sha256sum -c` → tous OK.
- `checksums/MISSING.txt` : vide (aucune plateforme manquante).
- Livré à l'utilisateur sous forme d'archive ZIP (`Nhill-V3.0.0.zip`, ~196 Mo) via `SendUserFile`.
- Rapport final écrit dans le projet claude.ai : `claude/nhill-3.0.0-rapport-final.md` (doc_uuid `9f4bd867-242d-4f12-a469-969479015e34`).

**Ce qui n'est PAS fait** (voir aussi sections 8 et 9) : déploiement production, site publié, signatures, audit externe, relecture juridique, tests sur matériel réel, test SMTP réel, test de migration avec données réelles.

---

## 11. Roadmap détaillée des prochaines étapes

Cette roadmap dépend entièrement des réponses de l'utilisateur sur les points de la section 8.3. Une nouvelle session doit **d'abord vérifier si ces réponses sont arrivées** (nouveau message utilisateur, nouvelle pièce jointe) avant de choisir une étape.

### Étape A — Si l'utilisateur a répondu sur R1/R2 (sécurité)
1. Appliquer la décision choisie (bundler cpp-httplib ≥0.23, Ubuntu Pro, ou migration Debian 13 ; mise à niveau Qt ou non).
2. Mettre à jour `docs/fr/03-audit-securite.md` / `docs/en/03-security-audit.md` pour refléter la décision et son statut (« appliqué » et non plus « décision requise »).
3. Relancer les tests concernés en CI réelle, jamais simulée.
4. Mettre à jour le changelog et, si la version change, re-packager.

### Étape B — Si l'utilisateur fournit un accès VPS de production
1. Exécuter `deploy/vps/provision.sh` sur la machine réelle, suivre `docs/fr/deploiement-vps-hostinger.md`.
2. Vérifier chaque contrôle d'acceptation listé en section 10 **sur la machine réelle**, pas seulement en CI.
3. Documenter le résultat réel (ne jamais réutiliser les résultats CI comme preuve de production).

### Étape C — Si l'utilisateur fournit l'hébergeur du site et l'adresse e-mail d'expéditeur
1. Compléter les mentions légales (`website/src/pages/fr/mentions-legales.html` / `en/legal-notice.html`) avec l'hébergeur réel.
2. Configurer l'envoi d'e-mails serveur avec l'adresse fournie.
3. Vérifier DNS (MX/SPF/DMARC) une fois configuré, puis seulement alors mettre à jour toute mention de `https://nhill.online` comme opérationnel.

### Étape D — Si aucune réponse n'est encore arrivée
- **Ne pas inventer** de réponse à ces questions.
- Travailler uniquement sur les tâches indépendantes qui restent légitimement ouvertes (ex. : vérifications supplémentaires de documentation, tests additionnels n'exigeant pas d'accès externe).
- Reposer les questions de façon concise si l'utilisateur relance la conversation sans y répondre.

---

## 12. Structure des fichiers et des livrables existants

### 12.1 Racine du dépôt (`/home/claude/work/nhill`, vérifié)

```
CMakeLists.txt          # version 3.0.0
README.md / README.en.md / README.pdf / README.en.pdf
RELEASE-NOTES.md
assets/
build/
client/                 # Qt6 QML : android, i18n, qml, resources, src
cmake/
common/
config/
db/                      # bootstrap/, migrations/ (0001 à 0015)
deploy/                  # vps/, caddy/, systemd/, scripts/, backup/
dist/                    # binaires construits + preuves CI (ci-evidence/)
docs/                    # fr/, en/, archive/, assets/v3/, pdf/
legal/                   # fr/ (21 fichiers), en/ (21 fichiers)
marketing/               # fr/ (16), en/ (16), visuals/, trackers/, tools/
packaging/               # notices/ (THIRD-PARTY-NOTICES.txt)
release/                 # V3.0.0/ (livraison assemblée)
scripts/                 # docs/, qa/, dev/, tools/, i18n/, brand/
server/                  # src/ (http/, services/ [31 fichiers])
tests/                   # server/, keychain/, common/, client/, support/
website/                 # src/pages/fr+en, tools/ (build_site.py, check_site.py…)
```

### 12.2 Documentation (`docs/fr/` et `docs/en/`, vérifié par `ls`)

| FR | EN |
|---|---|
| `index.md` | `index.md` |
| `serveur.md` | `server.md` |
| `base-de-donnees.md` | `database.md` |
| `nhill-business.md` | `nhill-business.md` |
| `nhill-pos.md` | `nhill-pos.md` |
| `nhill-console.md` | `nhill-console.md` |
| `reference-api.md` (généré) | `api-reference.md` (généré) |
| `reference-base-de-donnees.md` (généré) | `database-reference.md` (généré) |
| `guide-utilisateur.md` | `user-guide.md` |
| `guide-installation.md` | `installation-guide.md` |
| `guide-configuration.md` | `configuration-guide.md` |
| `guide-developpement.md` | `developer-guide.md` |
| `guide-compilation.md` | `build-guide.md` |
| `01-audit-technique.md` | `01-technical-audit.md` |
| `02-audit-ui-ux.md` | `02-ui-ux-audit.md` |
| `03-audit-securite.md` | `03-security-audit.md` |
| `securite.md` | `security.md` |
| `parametres-legaux.md` | `legal-parameters.md` |
| `matrice-conformite-site.md` | `website-compliance-matrix.md` |
| `deploiement-vps-hostinger.md` | `vps-deployment-hostinger.md` |
| `rapport-validation.md` | `validation-report.md` |
| `changelog.md` | `changelog.md` |
| `docs/licences-dependances.md` (racine `docs/`, pas de section FR/EN séparée — à vérifier si une version EN est attendue) | |
| `docs/archive/` : 12 anciens documents (0.x-2.x) + `README.md` bilingue | |

### 12.3 Corpus juridique (`legal/fr/`, 21 fichiers vérifiés — `legal/en/` en compte également 21)

`00-recherche-juridique.md` (registre central H-xx/V-xx) · `01-conditions-generales-utilisation.md` · `02-conditions-service-saas.md` · `03-politique-confidentialite.md` · `04-politique-cookies.md` · `05-mentions-legales.md` · `06-accord-traitement-donnees.md` · `07-engagement-de-service.md` · `08-politique-utilisation-acceptable.md` · `09-conditions-essai-accompagne.md` · `10-contrat-partenariat.md` · `11-accord-confidentialite.md` · `12-licence-applications.md` · `13-politique-securite.md` · `14-procedure-violation-donnees.md` · `15-politique-conservation.md` · `16-consentement-prospection.md` · `17-information-personnel.md` · `18-registre-traitements.md` · `19-demande-exercice-droits.md` · `20-autorisation-temoignage-image.md` (nouveau cette session).

### 12.4 Corpus marketing (`marketing/fr/`, 16 fichiers vérifiés — `marketing/en/` en miroir)

`01-etude-marche.md` · `02-programme-pilote.md` *(nom de fichier historique conservé, contenu mis à jour vers l'essai accompagné — à vérifier si un renommage est souhaité)* · `03-plan-90-jours.md` · `04-icp-qualification.md` · `05-scripts-prospection.md` · `06-modeles-messages.md` · `07-script-demonstration.md` · `08-suivi-et-processus.md` · `09-modele-partenaires.md` · `10-indicateurs.md` · `11-scripts-videos.md` · `12-voix-de-marque.md` · `13-pitchs.md` · `14-presentation-commerciale.md` · `15-faq-commerciale.md` · `16-etude-financiere-abonnements.md`.

Plus : `marketing/visuals/` (générateur `build_visuals.py`), `marketing/trackers/` (CSV livrés **vides**/en-tête seul + `kpi-tableau-de-bord.xlsx`), `marketing/tools/` (`build_trackers.py`, `build_visuals.py`, `subscription_model.py`).

> **Doc projet associé** (claude.ai) : `claude/nhill-etude-financiere-abonnements.md` — à relire/synchroniser avec `marketing/fr/16-etude-financiere-abonnements.md` v1.2 si des écarts apparaissent.

### 12.5 Release livrée (`release/V3.0.0/`, vérifié)

```
release/V3.0.0/
├── RELEASE-NOTES.md / .pdf
├── android/            (APK, clé de débogage)
├── linux/               (debian-13/, ubuntu-24.04/)
├── windows/              (3 installateurs .exe, non signés)
├── website/              (archive tar.gz du site statique)
├── documentation/
├── legal/
├── marketing/
├── source/
└── checksums/            (SHA256SUMS : 331 fichiers, MISSING.txt vide)
```

### 12.6 Documents du projet claude.ai (base de connaissance « Nhill »)

- `claude/nhill-3.0.0-rapport-final.md` — rapport de livraison 3.0.0.
- `claude/nhill-etude-financiere-abonnements.md` — étude financière (à recouper avec `marketing/fr/16-...`).
- `claude/nhill-2.0.0-rapport-validation.md` — rapport de validation d'une version antérieure (2.0.0), conservé pour historique.
- **Le présent document** doit être ajouté à cette base (`claude/CONTEXTE_COMPLET_PROJET.md` ou nom équivalent) en plus d'exister dans le dépôt, pour qu'une future session le retrouve même sans accès au dépôt.

---

## 13. Mes préférences et exigences permanentes (utilisateur)

- **Aucune invention, jamais.** Si une donnée manque, poser la question et attendre — ne jamais combler par une supposition présentée comme un fait.
- **Rien n'est « terminé » sans vérification réelle.** Compilation, tests, audits : toujours exécutés réellement, jamais simulés ni décrits comme faits sans preuve.
- **Isolation stricte par formule**, contrôlée côté serveur, comme principe architectural permanent — ne jamais l'affaiblir pour aller plus vite.
- **Aucune affirmation de statut légal/opérationnel non vérifiée** : pas de certification implicite, pas de "nhill.online est en ligne" avant vérification HTTPS réelle, pas de mention d'enregistrement d'entreprise.
- **Précision des registres de risques et de limites** : toute limitation connue (R1, R2, absence de signature, absence d'audit externe) doit rester visible dans le README/RELEASE-NOTES, jamais masquée.
- **Travail autonome mais questions ciblées** : l'utilisateur travaille par messages courts (« continue », « finalise », « bien ») et attend que l'IA poursuive la méthodologie standard (inspecter → planifier → implémenter → tester → documenter → reconstruire → rapporter) sans sur-solliciter, tout en posant directement les questions **réellement bloquantes**.
- **Corrections jamais déclarées faites sans vérification** — règle citée textuellement par l'utilisateur, à rappeler explicitement dans toute nouvelle session.
- **Bilinguisme FR/EN systématique** pour tout document destiné à l'utilisateur, au site ou aux établissements.

---

## 14. Instructions destinées à la prochaine session Claude

1. Lire ce document en entier avant toute action.
2. Lire `RELEASE-NOTES.md` et `docs/fr/rapport-validation.md` (section « Version 3.0.0 ») pour confirmer l'état réel au moment de la reprise — **ne pas supposer que rien n'a changé depuis la rédaction de ce document**.
3. Vérifier `git log --oneline -5` et `git status` dans `/home/claude/work/nhill` (ou l'emplacement équivalent du dépôt cloné) pour confirmer la branche et le dernier commit avant toute modification.
4. Vérifier si l'utilisateur a répondu aux questions ouvertes de la section 8.3 ; si oui, suivre la roadmap de la section 11 ; si non, ne pas les relancer de façon insistante mais rester prêt à les reposer si le sujet revient.
5. Ne reconstruire aucune fonctionnalité déjà marquée **✅** en section 6 sans raison explicite — vérifier d'abord l'état réel du code avant toute intervention.
6. Respecter toutes les règles de la section 4 (aucun placeholder, pas de simulation, isolation par formule, etc.) sans exception.

---

## 15. Liste des informations manquantes ou nécessitant une vérification

### 15.1 En attente d'une réponse de l'utilisateur (bloquant pour certaines tâches, section 8.3)

1. Décision R1 — remédiation `cpp-httplib` (bundler ≥0.23 / Ubuntu Pro / migration Debian 13).
2. Décision R2 — mise à niveau Qt 6.5.3 et impact sur la version Android minimale.
3. Hébergeur du site vitrine (nécessaire aux mentions légales).
4. Adresse d'expéditeur des e-mails automatiques et configuration DNS du domaine (MX/SPF/DMARC).
5. Éventuel taux de commission standard pour les partenaires (actuellement laissé à la négociation individuelle).

### 15.2 Signalées « à vérifier » dans ce document même

- `docs/licences-dependances.md` existe à la racine de `docs/` sans suffixe `fr`/`en` apparent dans le nom — à confirmer s'il couvre les deux langues en un seul fichier ou s'il manque un pendant anglais séparé.
- Le fichier `marketing/fr/02-programme-pilote.md` conserve un nom de fichier historique malgré la bascule de contenu vers l'« essai accompagné » — à confirmer avec l'utilisateur si un renommage de fichier (et de ses références croisées) est souhaité, ou si seul le contenu devait changer.
- Cohérence entre `claude/nhill-etude-financiere-abonnements.md` (base de connaissance claude.ai) et `marketing/fr/16-etude-financiere-abonnements.md` v1.2 (dépôt) — non recomparés ligne à ligne dans cette session ; à vérifier si des divergences existent avant toute diffusion externe de l'étude.
- Accès VPS de production : jamais fourni à aucune session ; toute affirmation sur un déploiement réel doit être vérifiée nulle tant qu'un accès n'a pas été donné.

---

## INSTRUCTIONS POUR LA REPRISE DU PROJET DANS UNE NOUVELLE SESSION

Une nouvelle session Claude qui reprend ce projet doit procéder ainsi, dans l'ordre :

1. **Lire ce document en entier** avant de répondre ou d'agir, y compris avant de poser une question de clarification.
2. **Vérifier l'état réel du dépôt** (`git log`, `git status`, arborescence des fichiers cités en section 12) avant toute supposition — ce document décrit un état constaté à une date donnée, qui peut avoir évolué depuis.
3. **Respecter sans exception** toutes les décisions validées en sections 4, 7 et 13 : ne pas les remettre en question sans une raison nouvelle et explicite de l'utilisateur.
4. **Ne pas refaire ce qui est marqué ✅ en section 6** — vérifier d'abord que le code/fichier correspondant existe toujours et fonctionne, puis continuer à partir de là, sans reconstruire depuis zéro.
5. **Ne jamais supprimer ni modifier une fonctionnalité existante sans justification explicite** liée à une demande de l'utilisateur ou à un bug réellement constaté.
6. **Identifier la tâche prioritaire** en consultant d'abord la section 8.3 (décisions en attente) puis la section 11 (roadmap conditionnelle) : si une réponse de l'utilisateur est arrivée entre-temps, elle détermine l'étape à suivre ; sinon, continuer les tâches indépendantes légitimement ouvertes (section 9), jamais celles qui dépendent d'un accès ou d'une information non fournie.
7. **Poursuivre le développement de façon cohérente et autonome** sur tout ce qui ne dépend pas d'une information manquante, en suivant la méthodologie déjà en place : inspecter → planifier → implémenter → tester réellement → documenter → reconstruire → rapporter.
8. **Ne demander à l'utilisateur que les informations réellement indispensables** — celles listées en section 15.1 — et continuer à travailler sur le reste pendant l'attente d'une réponse, conformément à la règle « pose la question et attends, mais continue les tâches indépendantes ».
9. Si un doute existe sur la validité d'une information de ce document face à l'état réel observé dans le dépôt ou dans les documents du projet claude.ai, **le fichier et le code source font foi**, pas ce document de synthèse — mettre à jour ce document en conséquence après vérification.
