# Nhill — Phase 1 : étude de faisabilité et conception détaillée

Version du document : 1.0 — 25 octobre 2026
Statut : document de conception de la version 0.1 (socle opérationnel + première version commerciale)

Ce document fixe les choix d'architecture, le modèle de données, le périmètre
réglementaire de la première version et les risques connus. Il est maintenu avec le
code : toute divergence entre ce document et le code est un défaut.

---

## 1. Faisabilité — ce qui est réellement vérifiable

| Élément | Vérifiable dans l'environnement de développement | Conséquence |
|---|---|---|
| Compilation C++17 (GCC 13) | Oui | Client, serveur et bibliothèque commune compilés et testés |
| Qt 6 (Quick/QML, Network, Test, Linguist) | Oui — Qt 6.4.2 (paquets Ubuntu 24.04) | Client compilé, testé avec Qt Test, rendu QML vérifié en mode `offscreen` |
| PostgreSQL 16 | Oui — instance éphémère lancée par les tests | Migrations, RLS, concurrence et scénarios testés sur un vrai serveur |
| Windows | **Non** (pas de machine Windows ni de Qt pour Windows) | Script de compilation fourni, **non exécuté** |
| Android | **Non** (SDK/NDK Android et Qt pour Android non téléchargeables) | Instructions et manifeste fournis, **non compilés** |
| Prestataires de paiement réels | **Non** (aucune API ni identifiant de test fournis) | Couche de paiement extensible + adaptateur générique signé HMAC, testé ; aucun prestataire réel déclaré opérationnel |
| Dispositif électronique fiscal (DEF) de la DGI | **Non** (spécifications réservées aux comptes du portail développeur DGI) | Interface prévue, non implémentée ; voir §6 |

Qt 6.4 est la version testée ici. Pour les binaires de production (Windows, Android),
la cible recommandée est Qt 6.8 LTS ; le code n'utilise aucune API postérieure à 6.4.

## 2. Architecture générale

```
 ┌──────────────────────────┐      HTTPS (TLS 1.2+)      ┌──────────────────────────────┐
 │ Client Nhill (C++/Qt)│ ─────────────────────────▶ │ Reverse proxy (Caddy)        │
 │ Windows · Linux · Android│                            │ TLS, HSTS, limite de taille   │
 └──────────────────────────┘                            └──────────────┬───────────────┘
 ┌──────────────────────────┐                                           │ HTTP loopback
 │ Console admin (C++/Qt)   │ ─────────────────────────▶                ▼
 └──────────────────────────┘                            ┌──────────────────────────────┐
                                                         │ nhill-server (C++17)     │
 ┌──────────────────────────┐   webhook signé HMAC       │ API REST /api/v1             │
 │ Prestataire de paiement  │ ─────────────────────────▶ │ Tâches planifiées (codes,    │
 └──────────────────────────┘                            │ abonnements) · PDF · CSV     │
                                                         └──────────────┬───────────────┘
                                                                        │ libpq (rôle nhill_app)
                                                                        ▼
                                                         ┌──────────────────────────────┐
                                                         │ PostgreSQL 16                │
                                                         │ RLS par établissement        │
                                                         │ Sauvegardes chiffrées        │
                                                         └──────────────────────────────┘
```

### 2.1 Application cliente — Qt 6 Quick/QML + C++

* **Choix retenu : Qt 6 avec Qt Quick/QML**, CMake, Qt Network, Qt Test, Qt Linguist.
  C'est la seule pile C++ mature qui couvre Windows, Linux et Android avec une seule base
  d'interface tactile et souris. Les alternatives étudiées (Dear ImGui, wxWidgets, Slint)
  ont soit une couverture Android insuffisante, soit une ergonomie de formulaires métier
  inférieure.
* La logique (appels API, sessions, formatage monétaire exact, validation) est en C++ ;
  QML ne fait que de la présentation. Les règles de validation sont partagées avec le
  serveur via la bibliothèque `nhill_common`.
* **Stockage local** : aucune donnée commerciale n'est stockée localement. Seul le jeton
  de rafraîchissement est conservé, dans le coffre du système via QtKeychain
  (Secret Service sous Linux, Credential Manager sous Windows, Keystore sous Android).
  Les préférences (langue, thème, URL du serveur) sont dans `QSettings`.
* **Qt SQL n'est pas utilisé** : sans mode hors ligne (voir §8), une base locale
  n'apporterait que des risques de divergence.

### 2.2 Backend — C++ (C++17), pas d'exception technologique

Le backend est entièrement en C++ :

| Besoin | Bibliothèque | Justification |
|---|---|---|
| Serveur HTTP | cpp-httplib 0.14 | Modèle synchrone à pool de threads : chaque requête = une transaction PostgreSQL, ce qui rend les verrous de ligne (numérotation, stocks) simples et sûrs |
| PostgreSQL | libpqxx 7.8 | Requêtes paramétrées uniquement, transactions RAII |
| Cryptographie | libsodium + OpenSSL | Argon2id, CSPRNG, HMAC-SHA256, chiffrement authentifié XSalsa20-Poly1305 ; OpenSSL pour HMAC-SHA1 (TOTP RFC 6238) |
| PDF | libharu 2.3 | Génération native, polices TrueType embarquées (UTF-8), PNG/JPEG |
| JSON | nlohmann/json | — |

Drogon (asynchrone, ORM intégré) a été évalué. Il a été écarté pour la V1 : son modèle
asynchrone complique les transactions longues à verrous et n'apporte rien à la charge
attendue (quelques milliers d'établissements). Le passage ultérieur reste possible car la
couche « services » ne dépend pas du serveur HTTP.

Le TLS est terminé par un reverse proxy (Caddy) devant le serveur, qui n'écoute qu'en
boucle locale. C'est de l'infrastructure, pas une exception au C++.

### 2.3 Console d'administration

Application Qt distincte (`nhill-console`), qui parle aux routes `/api/v1/admin/*`.
Authentification par mot de passe **et** TOTP obligatoire, rôles administrateurs
distincts (`superadmin`, `billing`, `support`, `auditor`), sessions courtes, journal
d'audit. Le premier administrateur est créé par une commande serveur
(`nhill-server admin-bootstrap`) qui n'agit que si aucun administrateur n'existe et
qui émet un jeton d'initialisation à usage unique, valable 30 minutes.

## 3. Modèle de données (PostgreSQL 16)

Les migrations sont dans `db/migrations/` et sont appliquées par
`nhill-server migrate` (table `schema_migrations` avec somme de contrôle SHA-256 :
une migration déjà appliquée et modifiée bloque le démarrage).

### 3.1 Principes

* **Montants** : `bigint` en unités mineures (centimes) + code devise ISO 4217 par
  ligne. Jamais de `float`/`double`. Les quantités sont des `bigint` en millièmes.
* **Taux de taxe** : entiers en points de base (`1600` = 16,00 %).
* **Taux de change** : `numeric(24,10)` avec date et source ; enregistrés sur chaque
  document au moment de l'émission ; jamais recalculés.
* **Isolation** : chaque table métier porte `establishment_id`. La sécurité au niveau
  des lignes (RLS, `FORCE ROW LEVEL SECURITY`) filtre sur
  `current_setting('app.establishment_id')`, positionné par le serveur au début de
  chaque transaction. Le rôle applicatif `nhill_app` n'est propriétaire d'aucune
  table : même une requête mal écrite ne peut pas lire un autre établissement.
  Les administrateurs de la plateforme n'ont **aucun** contournement de la RLS sur les
  tables métier ; les statistiques de la console passent par une fonction agrégée.
* **Traçabilité** : `audit_log` est en ajout seul (droits `INSERT, SELECT` uniquement).
  Les stocks ne sont modifiables que par insertion dans `stock_movements` (le rôle
  applicatif n'a aucun droit d'écriture sur `stock_levels`, mis à jour par déclencheur).
  Un document émis ne peut plus être modifié ni supprimé (déclencheur de garde).

### 3.2 Tables plateforme

| Table | Rôle |
|---|---|
| `establishments` | Identité de l'entreprise, type (`individual`/`company`), pays, devise, statut, profil de conformité |
| `establishment_identifiers` | Identifiants légaux normalisés (RCCM, Id. Nat., NIF…) ; unicité `(pays, type, valeur)` → anti-doublon au niveau de la base |
| `establishment_codes` | Codes d'établissement : HMAC pour la recherche, chiffré pour l'affichage au propriétaire, statut, expiration ; un seul code actif par établissement (index partiel unique) |
| `trial_registry` | Empreintes HMAC des identifiants (légaux, e-mail, téléphone du propriétaire) ayant déjà consommé un essai |
| `users`, `memberships`, `invitations` | Comptes personnels, rattachements avec rôle, invitations à code d'activation |
| `sessions` | Jetons d'accès (15 min) et de rafraîchissement (30 j) hachés, rotation avec détection de réutilisation |
| `plans`, `subscriptions`, `subscription_events`, `subscription_orders` | Offres configurables, abonnement courant, historique, commandes de paiement |
| `payment_webhook_events` | Événements reçus des prestataires, unicité `(prestataire, id_événement)` → idempotence |
| `admin_users`, `admin_sessions` | Console des créateurs |
| `platform_settings` | Durées configurables (essai 30 j, grâce 7 j, rappel J-7, rotation 30 j…) |
| `compliance_profiles` | Profils de facturation par pays, versionnés (JSON) |
| `attachment_requests` | Demandes de rattachement/récupération quand un établissement existe déjà |
| `announcements`, `notifications`, `email_outbox`, `audit_log`, `auth_throttle` | Communication, journalisation, limitation des tentatives |

### 3.3 Tables métier (toutes sous RLS)

`objects` (logos, justificatifs), `tax_rates`, `product_categories`, `products`,
`warehouses`, `stock_levels`, `stock_movements`, `parties` (clients et/ou fournisseurs),
`sales_documents`, `sales_document_lines`, `document_sequences`,
`document_status_history`, `cash_accounts`, `payments`, `purchases`, `purchase_lines`,
`expense_categories`, `expenses`, `exchange_rates`, `idempotency_keys`,
`establishment_settings`.

## 4. Flux critiques

### 4.1 Création d'un établissement (option B)

1. Le client envoie le formulaire. Les identifiants légaux exigés dépendent du profil
   pays ; aucun n'est obligatoire pour une entreprise individuelle.
2. Le serveur normalise les identifiants et cherche une correspondance exacte. En cas de
   correspondance : réponse `409 establishment_exists` **sans** révéler le nom, et
   possibilité de déposer une demande de rattachement (traitée manuellement par le
   support après vérification d'identité). Une simple ressemblance de nom ne déclenche
   jamais de rattachement.
3. Dans une seule transaction : établissement, identifiants, compte propriétaire
   (Argon2id), rattachement `owner`, code d'établissement, taux de taxe du profil,
   entrepôt par défaut, compte de caisse, abonnement d'essai (si aucune empreinte
   n'existe déjà dans `trial_registry`), journal d'audit. L'unicité en base protège
   contre deux créations simultanées.

### 4.2 Code d'établissement

* 12 caractères Crockford Base32 (60 bits issus de `randombytes_buf`), affichés
  `NH-XXXX-XXXX-XXXX`. Saisie tolérante (minuscules, tirets, O→0, I/L→1).
* Stocké sous deux formes : HMAC-SHA256 avec une clé serveur (recherche) et chiffré
  (affichage au propriétaire). Une fuite de la base ne livre pas les codes en clair.
* La validation renvoie la même erreur générique pour un code inconnu, expiré, révoqué
  ou lié à un établissement suspendu. Limitation : 10 essais / 15 min par adresse IP.
* **Le code n'ouvre aucun accès** : il produit un ticket de rattachement de 10 minutes,
  qui doit être complété par un mot de passe (membre existant) ou par le code
  d'activation de l'invitation émise par le propriétaire.
* Rotation automatique tous les 30 jours par la tâche `rotate-codes` :
  `SELECT … FOR UPDATE SKIP LOCKED` (aucune double rotation entre deux serveurs),
  insertion du nouveau code puis passage de l'ancien à `superseded`, dans la même
  transaction ; notification au propriétaire ; échec isolé par établissement et
  réessayé au passage suivant.
* Les sessions sont indépendantes du code : une rotation ne déconnecte personne.

### 4.3 Abonnement — source de vérité des droits

États : `trialing` → (`active` | `past_due`) → `suspended` ; `canceled`.

* Essai de 30 jours à la création, une seule fois par empreinte.
* Rappel J-7 avant `period_end` (notification + e-mail en file d'attente).
* À l'échéance sans paiement confirmé : `past_due` avec `grace_until = period_end + 7 j`
  (service maintenu, alertes). À la fin de la grâce : `suspended` → **lecture et
  export seuls** ; toute écriture répond `402 subscription_suspended`.
* Paiement confirmé **uniquement** par webhook signé du prestataire : la nouvelle période
  commence à l'ancienne `period_end` (la grâce ne décale jamais le contrat) et dure
  1 mois ou 12 mois selon l'offre. La rotation mensuelle des codes n'a aucun lien avec la
  facturation.
* Aucun prélèvement automatique : `auto_renew_consent_at` doit être renseigné par une
  action explicite, et aucun prestataire n'est branché en V1.

### 4.4 Factures

* Brouillon modifiable → émission : numéro attribué par
  `INSERT … ON CONFLICT DO UPDATE … RETURNING` sur `document_sequences` dans la
  transaction d'émission (verrou de ligne, pas de doublon, pas de trou dans les
  transactions validées) ; instantanés vendeur/client/profil figés ; mouvements de stock
  de vente ; empreinte SHA-256 du PDF.
* Après émission : aucune modification, aucune suppression ; correction par avoir.
* Format par défaut : `FA-2026-000001` (facture), `AV-` (avoir), `DV-` (devis),
  `BC-` (commande) ; préfixes anglais pour les documents en anglais.

### 4.5 Coupures réseau et idempotence

Toute écriture sensible porte un en-tête `Idempotency-Key` (UUID généré par le client
avant le premier envoi). Le serveur enregistre la clé et la réponse **dans la même
transaction** que l'opération : un renvoi après une coupure renvoie la réponse initiale
sans rejouer l'effet ; une clé réutilisée avec un autre contenu renvoie `409`.
Le client affiche « résultat incertain — vérification en cours » et renvoie avec la même
clé, jamais avec une nouvelle.

## 5. Sécurité (résumé — détail dans `docs/securite.md`)

Argon2id (libsodium, 64 Mio, 3 passes) ; jetons opaques 256 bits stockés hachés ;
contrôle d'accès et de rôle exclusivement côté serveur ; RLS ; requêtes paramétrées ;
limitation des tentatives en base (valable sur plusieurs instances) ; réponses
temporellement homogènes pour les e-mails inconnus ; secrets uniquement par variables
d'environnement ou fichier `0600` ; journaux d'audit en ajout seul ; sauvegardes
`pg_dump` chiffrées (age/GPG) avec restauration testée par script ; en-têtes HSTS et
CSP au niveau du proxy ; validation stricte des certificats côté client (jamais
d'`ignoreSslErrors`).

## 6. Périmètre réglementaire de la V1

### 6.1 RDC — constat vérifié (sources au §10)

* Taux normal de TVA : 16 % (Ordonnance-loi n° 10/001 du 20 août 2010, art. 35), 0 %
  à l'exportation. Un taux de 8 % dit « TVA sociale » est évoqué par l'Agence congolaise
  de presse (2023) sans que nous ayons pu identifier le texte et la liste des produits :
  il est **configurable mais non activé** par défaut.
* Art. 58 : les mentions de la facture sont fixées par voie réglementaire.
* **Facture normalisée** : Décret n° 23/010 du 3 mars 2023 et arrêtés de 2023-2025.
  Obligatoire pour les assujettis à la TVA depuis le 1er décembre 2025 ; fin du
  moratoire et sanctions annoncées à partir du 15 mai 2026. La facture normalisée doit
  être produite par un **dispositif électronique fiscal (DEF)** et porter notamment
  l'identifiant du DEF, le code d'authentification et un QR code. Les systèmes de
  facturation d'entreprise (SFE) doivent être **homologués** par la DGI ; le dossier et
  les spécifications techniques passent par le portail `developper.dgirdc.cd`.
* Conservation : 10 ans pour les livres et pièces comptables (Acte uniforme OHADA
  relatif au droit comptable, 2017).
* Données personnelles : Ordonnance-loi n° 23/010 du 13 mars 2023 portant Code du
  numérique (base légale, sécurité, notification des violations sous 72 h, encadrement
  des transferts hors du pays) — résumé secondaire, à vérifier sur le texte officiel.

### 6.2 Conséquence directe pour Nhill

**Nhill V1 n'est pas un SFE homologué et n'est pas connecté à un DEF.** Il ne peut
donc pas produire de factures normalisées. La V1 adopte le comportement suivant :

| Profil | Établissement | Comportement |
|---|---|---|
| `CD` | Non assujetti à la TVA | Factures commerciales complètes (mentions vendeur : RCCM, Id. Nat., NIF si disponibles) |
| `CD` | Assujetti à la TVA | Les documents Nhill portent la mention « Document de gestion — ne remplace pas la facture normalisée délivrée par le DEF ». L'utilisateur peut enregistrer la référence de la facture normalisée émise par son DEF (identifiant DEF, code d'authentification) pour le rapprochement. Nhill n'imprime jamais de code DEF qu'il n'a pas obtenu lui-même. |
| `INTL` | Hors RDC | Facture commerciale générique. **Aucune conformité aux obligations de facturation électronique européennes n'est revendiquée** (France : émission obligatoire depuis septembre 2026 pour les grandes et moyennes entreprises ; Belgique : B2B obligatoire depuis le 1er janvier 2026 ; directive ViDA 2025/516 : intra-UE en 2030). |

Feuille de route : (1) ouvrir un compte sur le portail développeur de la DGI, obtenir
les spécifications ; (2) implémenter l'adaptateur `FiscalDeviceAdapter` ;
(3) déposer le dossier d'homologation SFE ; (4) pour l'UE, export EN 16931 (UBL/CII)
via une plateforme agréée.

Toutes ces règles sont des données (`server/profiles/*.json`), versionnées, et chaque
document émis garde la version du profil utilisée. **Points à faire valider par un
fiscaliste congolais** : liste exacte des mentions hors TVA, régime des entreprises
individuelles, seuil d'assujettissement (80 millions de FC selon la presse), texte du
taux de 8 %.

## 7. Langues et devises

Français et anglais, chaînes source en anglais dans le code, traductions Qt Linguist
(`client/i18n/*.ts`) et dictionnaires serveur pour les PDF (`server/i18n/*.json`).
Devises : CDF, USD, EUR à l'installation ; table `currencies` extensible. Les rapports
sont présentés **par devise**, sans conversion implicite.

## 8. Fonctionnement en ligne

La V1 est **en ligne uniquement** pour les écritures. Justification : la numérotation
des factures, les stocks et les droits d'abonnement sont arbitrés par le serveur ; une
file hors ligne exigerait une résolution de conflits qui n'est pas spécifiée. Le client
affiche l'état de connexion, applique des délais d'attente (15 s), ne réessaie
automatiquement que les lectures, et protège les écritures par clé d'idempotence.

## 9. Risques identifiés

| Risque | Impact | Mesure |
|---|---|---|
| Obligation DEF/SFE pour les assujettis TVA en RDC | Bloquant pour ce segment | Mention explicite, rapprochement manuel, homologation planifiée |
| Disponibilité du nom « Nhill » non vérifiée (marques, domaines, magasins d'applications ; homonyme géographique en Australie) | Risque de confusion ou d'opposition | Recherche d'antériorité formelle avant dépôt ; nom centralisé dans la configuration pour pouvoir changer |
| Fiabilité réseau en RDC | Échecs d'opérations | Idempotence, délais d'attente, messages explicites |
| Android/Windows non compilés ici | Défauts spécifiques plateforme | Scripts fournis, liste de contrôle de recette par plateforme |
| Prestataires Mobile Money | Pas d'encaissement automatisé en V1 | Adaptateur générique ; intégration réelle dès réception des accès bac à sable |

## 10. Sources consultées (octobre 2026)

* DGI — Réforme de la facture normalisée : https://dgi.gouv.cd/reforme-de-la-facture-normalisee/
* Ordonnance-loi n° 10/001 du 20 août 2010 (TVA), texte consolidé : https://www.leganet.be/Legislation/Dfiscal/TVA/OL.10.001.20.2010.htm
* Bankable, homologation des systèmes de facturation (19/06/2025) : https://bankable.africa/en/climat-des-affaires/1906-1335-facturation-seuls-les-logiciels-homologues-autorises-des-le-1er-juillet-en-rdc
* Bankable, moratoire et seuil d'assujettissement (17/10/2025) : https://bankable.africa/fr/climat-des-affaires/1710-1854-facture-normalisee-nouveau-moratoire-sur-une-reforme-cle-de-la-tva-en-rdc
* FEC, procédure d'homologation via developper.dgirdc.cd (08/2025) : https://fec-rdc.com/mise-en-oeuvre-de-la-reforme-relative-a-la-facture-normalisee-le-cabinet-du-ministre-des-finances-et-la-dgi-rencontrent-les-preoccupations-des-entreprises-et-clarifient-la-procedure-dhomologation/
* La Prospérité, fin du moratoire, sanctions au 15 mai 2026 : https://www.laprosperite.cd/rdc-le-ministere-des-finances-met-fin-au-moratoire-sur-la-facture-normalisee-les-sanctions-debutent-le-15-mai-2026/
* ACP, TVA sociale de 8 % (28/07/2023) : https://acp.cd/economie/rdc-linstitution-dune-tva-sociale-a-8-rend-accessible-les-produits-de-premiere-necessite/
* Acte uniforme OHADA relatif au droit comptable (2017) : https://www.vie-publique.sn/documents/11026/acte-uniforme-droit-comptable-information-financiere-ohada-2017
* Code du numérique RDC (résumé) : https://pacmap.dev/regulation/cd-digital-code-2023
* Calendrier ViDA et pays européens : https://econnect.eu/en/docs/knowledge/regulations/europe/vida/timeline
* Nom « Nhill » : aucune recherche d'antériorité ni vérification de domaine effectuée à ce jour (les références relevées pour l'ancien nom de travail ne s'appliquent pas).
