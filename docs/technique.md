# Nhill — Documentation technique

Version 0.1.0 — API `v1` — 26 octobre 2026.

## 1. Vue d'ensemble

```
 Nhill Business          Nhill POS               Nhill Console
 (Windows/Linux/Android) (Windows/Linux)         (Windows/Linux, créateurs)
   │ HTTPS, jetons courts  │ HTTPS, jetons courts   │ HTTPS, jeton admin (mot de passe + TOTP)
   ▼                       ▼                        ▼
 ┌──────────────── reverse proxy TLS (Caddy) ─────────────────────────────┐
 │                  nhill-server (C++17, cpp-httplib)                     │
 │  http/app (routage, droits, idempotence, erreurs) ─ services/*         │
 │  setup (mode installation) ─ providers (Stripe, PayPal, générique)     │
 │  jobs (codes, cycle d'abonnement, changements d'offre, expirations)    │
 │  pdf (factures, tickets 58/80 mm, rapports X/Z, rapports)              │
 └───────────────┬────────────────────────────────────────────────────────┘
                 │ rôle nhill_app (aucune propriété, RLS forcée)
                 ▼
   PostgreSQL 15+ (Supabase ou auto-hébergé) — schéma « nhill », db/migrations
```

Un seul serveur et une seule base pour les trois applications : Nhill POS utilise les
produits, prix, taxes, dépôts, tiers, numérotation et paiements de Nhill Business ; un
ticket est un document de vente de type `receipt`.

* Le client ne contient **aucun identifiant de base de données** : il ne parle qu'à l'API.
* Chaque requête = une transaction PostgreSQL ; le serveur positionne
  `app.establishment_id` pour la transaction (`Tx::setTenant`) et la base filtre
  elle-même toutes les tables métier (`FORCE ROW LEVEL SECURITY`, aucune dérogation).
* **C++17** : les bibliothèques de la distribution (libpqxx 7.8, cpp-httplib 0.14) sont
  compilées en C++17 ; les compiler en C++20 rompt l'ABI (`std::source_location`).

### 1.1 Modules serveur (`server/src`)

| Fichier | Rôle |
|---|---|
| `config.*` | Lecture `KEY=VALUE` + variables `NH_*` ; refuse un fichier lisible par d'autres ; dérive 3 sous-clés (libsodium KDF) de `NH_SECRET_KEY` |
| `db.*` | Pool de connexions, `statement_timeout` 20 s, `lock_timeout` 10 s, réessai automatique sur sérialisation / interblocage |
| `migrate.*` | Migrations sous verrou consultatif, empreinte SHA-256, refus d'une migration modifiée après application |
| `http/app.*` | Contexte de requête, contrôle d'accès (rôles, permissions, abonnement, suspension), mapping des erreurs SQL → codes API, file d'attente TCP agrandie |
| `services/common.*` | Analyse stricte des entrées, profils pays, i18n, audit, notifications, idempotence, limitation de débit |
| `services/report.*` | Moteur de rapports tabulaires JSON/CSV/PDF commun |
| `services/*.cpp` | Domaines : auth, establishments, codes, members, subscriptions (offres, commandes, prorata, webhooks), catalog, parties, stock, sales, purchases, payments, expenses, reports, activity, admin, console (abonnements, transactions, prestataires, codes promo, demandes, erreurs), support, pos |
| `services/providers.*` | Adaptateurs de paiement : Stripe Checkout, PayPal Orders v2, générique HMAC ; secrets scellés ; transport remplaçable pour les tests |
| `setup.*` | Mode installation : sondage de la base, création des rôles, migrations, 10 contrôles de vérification, écriture atomique de la configuration, premier administrateur |
| `jobs/jobs.cpp` | Tâches planifiées (`FOR UPDATE SKIP LOCKED`, sûres en multi-instance) |
| `pdf/pdf.*` | Rendu des documents de vente et des rapports (polices IBM Plex intégrées, logo sans déformation, décodage PNG protégé) |

### 1.2 Client (`client/`)

* `src/core` : `ApiClient` (HTTPS, délais, réessai des lectures seulement, rafraîchissement
  de jeton unique, état de connexion), `Session` (jeton de rafraîchissement dans le
  trousseau du système via QtKeychain, jamais en clair), `Formatter`/`Validator` (mêmes
  règles C++ que le serveur via `common/`), `AppContext` (préférences, messages d'erreur).
* `qml/Nhill/Ui` : composants (thème clair/sombre, tables adaptatives bureau/téléphone,
  champs montant/date, dialogues, icônes SVG).
* `qml/Nhill/Business` : Nhill Business. `qml/Nhill/Console` : Nhill Console (dont
  l'assistant d'installation). `qml/Nhill/Pos` : Nhill POS. Trois exécutables
  (`nhill-business`, `nhill-console`, `nhill-pos`) partagent `core` et `Ui`.
* Les avertissements QML et erreurs critiques sont envoyés toutes les 30 s à
  `/api/v1/client-errors` (masqués côté serveur), sauf pour la console.
* Aucune donnée métier n'est stockée localement (seulement préférences : serveur, thème, langue).

## 2. Données

### 2.1 Principes

* Identifiants `uuid` ; toutes les tables métier portent `establishment_id`, avec clés
  étrangères composites `(establishment_id, id)` : une ligne ne peut pas référencer un
  objet d'un autre établissement, même par erreur de code.
* Montants : `bigint` en **unités mineures** de la devise du document (2 décimales pour
  CDF/USD/EUR) ; quantités en **millièmes** ; taux de taxe et remises en **points de base**
  (1600 = 16 %) ; taux de change `numeric(24,10)`. Dans l'API, les montants circulent en
  **chaînes décimales** (`"1234.50"`) — jamais en nombres flottants.
* Chaque document stocke sa devise ; chaque conversion stocke le taux, sa date et sa
  source ; aucun montant historique n'est recalculé au taux du jour.
* `audit_log` : ajout seul (le rôle applicatif n'a que `INSERT, SELECT`).
* Stock : uniquement par `stock_movements` ; la table `stock_levels` est tenue par un
  déclencheur `SECURITY DEFINER` (le rôle applicatif n'a que `SELECT` dessus).
* Documents émis : immuables (déclencheurs de garde, erreur `NH002`).

### 2.2 Migrations

| Version | Contenu |
|---|---|
| 0001_platform | Devises, paramètres, profils, établissements, identifiants légaux, essais, codes, utilisateurs, appartenances, invitations, sessions, offres, abonnements, commandes, webhooks, admins, annonces, notifications, e-mails, audit |
| 0002_business | Paramètres d'établissement, fichiers, taxes, catalogue, dépôts, stocks, tiers, documents de vente et lignes, séquences, historique de statut, achats, caisses, dépenses, paiements, taux de change, idempotence, compteurs d'usage |
| 0003_security | Politiques RLS `<table>_tenant` et privilèges du rôle applicatif |
| 0004_reference_data | Devises et paramètres de plateforme par défaut |
| 0005_helpers | Fonctions utilitaires (`nh_iso`, …) |
| 0006_activity_and_platform_stats | Index du journal par employé ; fonctions d'**agrégats** pour les statistiques des créateurs |
| 0007_local_time | Fuseau horaire de l'établissement pour les dates des documents et des rapports |
| 0008_console_billing | Prix par durée et devise, niveaux et plafonds des offres (caisses, dépôts), changements d'offre différés, échecs de paiement, prestataires (secrets chiffrés), codes promo et utilisations, demandes d'assistance, erreurs techniques, notifications de la console ; politiques RLS en forme « init-plan » |
| 0009_pos | Rôles `cash_manager` et `cashier`, caisses, affectations, moyens de paiement, paramètres POS, sessions (immuables après clôture), mouvements d'espèces, autorisations de superviseur, codes promo de vente ; tickets `receipt` |
| 0010_fk_indexes | Index des clés étrangères les plus utilisées |

## 3. Règles de calcul

* Ligne : `brut = quantité × prix unitaire` ; `remise = brut × taux de remise` ;
  `net = brut − remise` ; `taxe = net × taux`. Chaque produit est arrondi à l'unité
  mineure **au demi supérieur** (half-up), en arithmétique entière 128 bits.
* Document : deux modes par établissement — `per_line` (somme des taxes de ligne) ou
  `per_rate` (taxe recalculée sur la base totale de chaque taux). Les bases et taxes
  sont regroupées par taux.
* Statut d'une facture : `issued` → `partially_paid` → `paid` selon `payé + avoirs` ;
  `credited` si les avoirs couvrent le total. Un paiement ne peut jamais dépasser le
  reste dû et doit être dans la devise du document.
* Transitions : devis `draft→sent→accepted|declined`, `sent|accepted→converted` ;
  commande `draft→confirmed→converted|canceled` ; facture `draft→issued` puis statuts
  de paiement ; avoir `draft→issued`. Toute autre transition : `409 invalid_transition`.
* Marge des rapports produits : **estimée** au prix d'achat actuel (affiché comme tel).

## 4. Numérotation

`document_sequences(establishment_id, doc_type, year)` incrémentée par
`INSERT … ON CONFLICT DO UPDATE … RETURNING` dans la transaction d'émission : verrou de
ligne, pas de trou en cas de succès, pas de doublon (contrainte d'unicité). Le format
vient du profil pays (`{prefix}-{year}-{seq6}`, préfixes FA/AV/DV/BC en français,
INV/CN/QT/SO en anglais, tickets TK en français et RCP en anglais, remise à zéro
annuelle). Test de charge : 24 émissions
concurrentes → 24 numéros distincts consécutifs.

## 5. Sessions, codes, abonnements

* Jeton d'accès opaque 15 min, jeton de rafraîchissement 30 jours avec rotation ; la
  réutilisation d'un ancien jeton de rafraîchissement révoque toute la famille de sessions.
  Seules les empreintes SHA-256 des jetons sont stockées.
* Code d'établissement : 12 symboles base32 Crockford (60 bits) `NH-XXXX-XXXX-XXXX`,
  stocké en HMAC (recherche) + chiffrement libsodium (affichage au propriétaire).
  Le code **n'authentifie personne** : il délivre un ticket de 10 minutes qui doit être
  complété par un mot de passe ou un code d'invitation à 8 chiffres. Rotation automatique
  tous les 30 jours (tâche `rotateCodes`, transaction atomique, notification du
  propriétaire, aucune session coupée) ; révocation manuelle ; limitation à 10 essais / 15 min / IP.
* Abonnement : source de vérité des droits. `trialing` (30 jours, une fois par empreinte
  d'identifiants légaux/e-mail/téléphone) → `active` → `past_due` (grâce de 7 jours,
  `grace_until = fin de période + 7 j`, la période contractuelle ne bouge pas) →
  `suspended` (lecture et export seulement, écritures `402`). Rappel à J-7. Une offre
  annuelle prolonge d'un an. Paiement uniquement par webhook signé et horodaté
  (idempotent par `event_id`) ou confirmation manuelle auditée.

### 5.1 Offres, prix et commandes

* `plans` (niveau `tier`, `max_users`, `max_registers`, `max_warehouses`, `features`) et
  `plan_prices` (offre × durée 1/3/6/12/24/36 mois × devise), administrés dans la console.
* `subscription/quote` puis `subscription/orders` : le serveur calcule le type
  (`new`, `renewal`, `upgrade`, `downgrade`), le prix, la remise du code promo et le
  crédit au prorata (offre supérieure, même devise : valeur non consommée de la période
  en cours). Total nul → appliqué sans paiement.
* `upgrade` : nouvelle période immédiate. `downgrade` : payé, enregistré dans
  `pending_price_id`/`pending_from`, appliqué par la tâche `plan_change` à l'échéance.
  `renewal` : commence à la fin de la période en cours.
* Code promo réservé à la création de la commande, confirmé au paiement, libéré à
  l'expiration (48 h, tâche `order_expiry`), à l'annulation ou à l'échec.

### 5.2 Prestataires de paiement

`payment_providers` : type, mode test/production, devises, champs publics et secrets
scellés (`secretbox`, clé dérivée « NHPAYSEC »). `orders/:id/checkout` crée la session de
paiement chez le prestataire (Stripe : Checkout Session, `Idempotency-Key` ; PayPal :
commande v2 après jeton OAuth2) et renvoie l'URL ouverte par l'application.
`POST /webhooks/payments/:provider` vérifie la signature (Stripe `t=,v1=`, PayPal API de
vérification, générique HMAC), dédoublonne, relit l'objet chez le prestataire, contrôle
montant et devise puis applique la commande. Payoneer : configuration seulement.

### 5.3 Point de vente

* `pos_registers` (dépôt, actif, affectations `pos_register_users`), plafond
  `plans.max_registers` (`trial_max_registers` pendant l'essai).
* `pos_sessions` : une ouverte par caisse et par utilisateur ; numéro Z par caisse ;
  clôture : comptages déclarés (`counted`), attendus et écart calculés par le serveur ;
  déclencheur `nh_guard_pos_session` : une session clôturée est immuable.
* Vente (`POST /pos/sales`) : lignes tarifées par `sales::priceLines`, remise de ligne et
  code promo combinés, plafond `max_discount_bp`, prix modifié seulement si autorisé,
  ticket émis par `sales::issue` (numérotation, stock du dépôt de la caisse),
  paiements `payments` avec note `pos:<moyen>` ; somme des paiements = total ; monnaie
  rendue sur les espèces.
* Retour / annulation : avoir lié (`related_document_id`, `pos_session_id`),
  quantités ≤ restant, remise en stock, remboursement par moyen (par défaut espèces).
* Autorisations : `pos_approvals` (empreinte du jeton, action, demandeur, session,
  3 min, usage unique).
* Rapports X/Z : `pdf::renderTicket`, même format rouleau que les tickets.

## 6. Idempotence et réseau

Toute écriture sensible (création de document, émission, paiement, mouvement de stock,
achat, dépense) exige l'en-tête `Idempotency-Key`. La clé et la réponse sont enregistrées
dans **la même transaction** que l'opération : un rejeu renvoie la même réponse avec
`Idempotent-Replayed: true`, une clé réutilisée pour un autre contenu → `409`. Le client
ne rejoue jamais seul une écriture : si la réponse est perdue, il affiche « Réessayer »
avec **la même clé**.

## 7. Traçabilité par employé (journal d'activité)

* Chaque écriture de l'API inscrit dans `audit_log` : auteur (`user`, `admin`, `system`,
  `provider`, `anonymous`), action, objet, détails, IP, date.
* `GET /api/v1/activity` : filtres employé, préfixe d'action, période ; pagination ;
  export `format=csv|pdf`. Les références lisibles (numéro de document, produit, contact,
  achat) sont résolues **dans l'établissement** (RLS). Propriétaire, administrateur et
  comptable (`audit.read`) voient tout ; les autres rôles ne voient que **leurs propres
  actions** — appliqué côté serveur, quel que soit le filtre demandé.
* `GET /api/v1/reports/staff` : par employé et par devise, documents créés, factures et
  avoirs émis (nombre, TTC), encaissements, dépenses saisies, corrections de stock,
  dernière activité. Journal des ventes : colonne « Émis par » ; rapport des paiements :
  « Enregistré par ».
* Tableau de bord : un rôle sans `reports.read` (vendeur, lecture seule) ne reçoit que
  ses propres ventes et créances, sans dépenses, dettes ni valeur de stock.

## 8. Statistiques des créateurs (console d'administration)

Les administrateurs de la plateforme n'ont **aucun accès ligne à ligne** aux tables
métier (RLS sans dérogation). La migration 0006 crée trois fonctions
`SECURITY DEFINER` qui renvoient uniquement des **agrégats**, établissement par
établissement :

| Fonction | Renvoie |
|---|---|
| `nh_platform_establishment_volumes(est, du, au)` | Par devise : factures émises (nombre, HT, TTC), avoirs, encaissements (nombre, montant), décaissements, dépenses, achats réceptionnés |
| `nh_platform_establishment_monthly(est, du, au)` | Même chose par mois |
| `nh_platform_establishment_counts(est)` | Nombre de produits, clients, fournisseurs, documents, brouillons |

Elles positionnent temporairement le contexte de l'établissement puis **restaurent le
contexte précédent** (testé). Endpoints : `GET /api/v1/admin/establishments/:id/stats`,
`GET /api/v1/admin/reports/establishments` (une ligne par établissement et devise),
`GET /api/v1/admin/reports/establishments/:id` (mensuel) — JSON, CSV ou PDF.
Réservés aux rôles `billing` et `auditor` (et `superadmin`) ; **chaque consultation est
inscrite au journal d'audit de l'établissement** et visible par lui (« Support
Nhill — Statistiques agrégées consultées »). Aucun nom de client, produit ou numéro
de document n'est renvoyé (vérifié par test).

## 9. Rapports

Moteur commun (`services/report.cpp`) : colonnes typées (texte, montant, quantité,
date, entier) ; JSON pour l'écran, CSV (séparateur `;`, BOM UTF-8, neutralisation des
formules commençant par `= + - @`), PDF (paysage si plus de 7 colonnes, totaux par devise,
jamais additionnés entre devises). Rapports établissement : ventes, paiements, dépenses,
produits (marge estimée), mouvements de stock, créances, dettes, activité par employé,
journal d'activité.

## 10. Tâches planifiées

`nhill-server serve` exécute les tâches toutes les `NH_JOBS_INTERVAL_SECONDS` ;
`nhill-server run-jobs` les exécute une fois (cron/systemd possible). Rotation des
codes échus, changements d'offre différés, cycle d'abonnement (rappel J-7, grâce,
suspension), expiration des commandes impayées, envoi des e-mails
(transport `log` seulement : **aucun SMTP n'est implémenté**), purge des tickets et
compteurs de limitation.

## 11. Mode installation

Si ni `NH_DATABASE_URL` ni `NH_MIGRATION_DATABASE_URL` n'est défini, `serve` génère
`NH_SECRET_KEY` si besoin et démarre `setup::runSetupServer` : `/health` →
`{"status":"setup"}`, `/api/v1/setup/status`, `/check` (sondage : version ≥ 15,
superutilisateur ou `CREATEROLE`, schéma existant), `/apply` (rôles, migrations,
vérification, configuration, premier administrateur), tous protégés par le jeton
d'installation à usage unique. Après succès, le processus se relance (`execv`) en mode
normal. Sur Supabase, la connexion passe par le pooler de session avec l'utilisateur
`<rôle>.<référence>` ; quand le compte ne peut pas recevoir `CREATE` sur la base, le
schéma est créé par l'administrateur pour `nhill_owner`
(`GRANT nhill_owner … WITH INHERIT FALSE, SET TRUE`, PostgreSQL 16+).

## 12. Paquets

`cmake/NhillPackaging.cmake` (CPack DEB, un composant par application) ; scripts de
maintenance du serveur dans `packaging/debian/server/` ; installateurs Windows :
`packaging/windows/nhill-app.iss`.

## 13. Codes d'erreur

Format : `{"error": {"code", "message", "details", "request_id"}}`. Principaux codes :
`validation_failed` (422, `details.field/reason`), `invalid_token`/`session_expired` (401),
`permission_denied` (403), `subscription_suspended` (402), `not_found` (404),
`invalid_transition`, `document_immutable`, `insufficient_stock`, `idempotency_key_reused`
(409), `too_many_attempts` (429), `busy` (503). POS : `approval_required` (403,
`details.action`), `session_already_open`, `plan_register_limit`, `cash_insufficient`,
`already_refunded`, `void_other_session` (409). Abonnement : `promo_*`,
`pending_plan_change`, `order_not_payable`, `provider_not_implemented` (501). Les erreurs SQL métier (`NH001`
stock insuffisant, `NH002` document immuable) sont traduites par `http/app.cpp`.
