# Nhill — Référence de la base de données

*English: [../en/database-reference.md](../en/database-reference.md)*

Document **généré** par `scripts/docs/gen_db_reference.py` à partir d'une base PostgreSQL migrée avec les migrations du dépôt (ne pas modifier à la main ; l'intégration continue vérifie qu'il est à jour). Version 3.0.0 : 71 tables et 29 fonctions dans le schéma `nhill`, 15 migrations.

## Principes

* Deux rôles (`db/bootstrap/00-roles.sql`) : `nhill_owner` possède le schéma et applique les migrations, il n'est jamais utilisé par l'API ; `nhill_app` est le rôle du serveur en fonctionnement : il ne possède rien, n'a aucun droit de définition (DDL) et reste soumis à la sécurité au niveau des lignes.
* Isolement des établissements : les tables de gestion des établissements ont la sécurité au niveau des lignes **activée et forcée** ; le serveur fixe `app.establishment_id` dans chaque transaction et les politiques ne laissent voir que les lignes de cet établissement. Les tables de la plateforme sont lues par les requêtes du serveur avec les seuls droits nécessaires au rôle applicatif (voir chaque table).
* Montants : `bigint` en unités mineures avec une devise `char(3)` ; taux : entiers en points de base ; quantités : `bigint` en millièmes ; horodatages `timestamptz` (UTC).
* Migrations : fichiers `db/migrations/NNNN_nom.sql`, appliqués dans l'ordre par `nhill-server migrate` ; chaque migration appliquée est enregistrée avec son empreinte et une migration modifiée après coup est refusée ; un serveur refuse une base migrée par une version plus récente.
* Suppression d'un établissement : fonction `nh_erase_establishment` (propriétaire des tables, 90 jours après la fin de l'accès ; voir la console).

## Migrations

| Migration | Objet |
|---|---|
| `0001_platform` | Tables de la plateforme : établissements, identité, accès, abonnements |
| `0002_business` | Tables des établissements ; clés étrangères composées (établissement, id) pour qu'une ligne ne désigne jamais les données d'un autre établissement |
| `0003_security` | Sécurité au niveau des lignes et droits du rôle applicatif, sans contournement pour les administrateurs de la plateforme |
| `0004_reference_data` | Données de référence réelles : devises ISO 4217 et paramètres initiaux de la plateforme |
| `0005_helpers` | Fonctions utilitaires de l'API (dates ISO 8601 UTC, etc.) |
| `0006_activity_and_platform_stats` | Journal d'activité par employé ; statistiques de la plateforme limitées à des agrégats |
| `0007_local_time` | Heure locale de chaque établissement pour dater documents, sessions et rapports |
| `0008_console_billing` | Console et facturation : prix par durée et devise, changements de formule, codes promotionnels, prestataires de paiement, assistance, erreurs, notifications |
| `0009_pos` | Nhill POS : caisses, caissiers, moyens de paiement, sessions, mouvements d'espèces, autorisations, codes promotionnels de vente |
| `0010_fk_indexes` | Index sur les clés étrangères des requêtes fréquentes |
| `0011_password_recovery` | Mot de passe oublié et mot de passe temporaire (2.1.0) |
| `0012_plan_entitlements` | Formules configurables (3.0.0) : catalogue des fonctions et limites, valeurs par formule, conditions, règles d'attribution, accès calculé |
| `0013_documents` | Documents de vente (3.0.0) : mentions de l'établissement, factures émises en caisse avec la même numérotation |
| `0014_establishment_erasure` | Suppression définitive d'un établissement, archive de facturation, nettoyages de conservation |
| `0015_search_patterns` | Recherches : `%` et `_` pris littéralement |

## Plateforme et identité

### `establishments`

Établissements (entreprises clientes) : nom, pays, devise principale, statut.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Déclencheurs : `establishments_touch`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `kind` | `text` | oui | — |
| `trade_name` | `text` | oui | — |
| `legal_name` | `text` | — | — |
| `country_code` | `character(2)` | oui | — |
| `city` | `text` | — | — |
| `address` | `text` | — | — |
| `sector` | `text` | — | — |
| `preferred_language` | `text` | oui | `'fr'::text` |
| `main_currency` | `character(3)` | oui | — |
| `email` | `text` | — | — |
| `phone` | `text` | — | — |
| `website` | `text` | — | — |
| `vat_registered` | `boolean` | oui | `false` |
| `compliance_profile_code` | `text` | oui | — |
| `logo_object_id` | `uuid` | — | — |
| `status` | `text` | oui | `'active'::text` |
| `status_reason` | `text` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `updated_at` | `timestamp with time zone` | oui | `now()` |
| `timezone` | `text` | oui | `'UTC'::text` |

Contraintes : `FOREIGN KEY (main_currency) REFERENCES nhill.currencies(code)` ; `PRIMARY KEY (id)` ; 6 contrôle(s) `CHECK`

### `establishment_identifiers`

Identifiants légaux d'un établissement selon le profil pays.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : DELETE, INSERT, SELECT

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `establishment_id` | `uuid` | oui | — |
| `kind` | `text` | oui | — |
| `country_code` | `character(2)` | oui | — |
| `value` | `text` | oui | — |
| `normalized_value` | `text` | oui | — |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `PRIMARY KEY (establishment_id, kind)` ; `UNIQUE (country_code, kind, normalized_value)` ; 1 contrôle(s) `CHECK`

### `establishment_settings`

Paramètres des documents d'un établissement (langue, délai, arrondi, mentions).

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Politiques : `establishment_settings_tenant`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `establishment_id` | `uuid` | oui | — |
| `allow_negative_stock` | `boolean` | oui | `false` |
| `tax_rounding` | `text` | oui | `'per_line'::text` |
| `default_payment_terms_days` | `integer` | oui | `0` |
| `document_language` | `text` | oui | `'fr'::text` |
| `invoice_footer` | `text` | oui | `''::text` |
| `payment_instructions` | `text` | oui | `''::text` |
| `updated_at` | `timestamp with time zone` | oui | `now()` |
| `document_mentions` | `text` | oui | `''::text` |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `PRIMARY KEY (establishment_id)` ; 4 contrôle(s) `CHECK`

### `establishment_codes`

Codes d'établissement : empreinte HMAC pour la vérification, copie chiffrée pour l'affichage au propriétaire.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `code_hmac` | `bytea` | oui | — |
| `code_cipher` | `bytea` | oui | — |
| `code_nonce` | `bytea` | oui | — |
| `status` | `text` | oui | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `expires_at` | `timestamp with time zone` | oui | — |
| `ended_at` | `timestamp with time zone` | — | — |
| `ended_reason` | `text` | — | — |
| `created_by_type` | `text` | oui | — |
| `created_by` | `uuid` | — | — |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `PRIMARY KEY (id)` ; `UNIQUE (code_hmac)` ; 3 contrôle(s) `CHECK`

### `users`

Comptes utilisateurs (empreinte Argon2id du mot de passe).

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Déclencheurs : `users_touch`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `email` | `text` | oui | — |
| `password_hash` | `text` | oui | — |
| `full_name` | `text` | oui | — |
| `phone` | `text` | — | — |
| `preferred_language` | `text` | oui | `'fr'::text` |
| `status` | `text` | oui | `'active'::text` |
| `password_changed_at` | `timestamp with time zone` | oui | `now()` |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `updated_at` | `timestamp with time zone` | oui | `now()` |
| `must_change_password` | `boolean` | oui | `false` |
| `temporary_password_expires_at` | `timestamp with time zone` | — | — |

Contraintes : `PRIMARY KEY (id)` ; `UNIQUE (email)` ; 4 contrôle(s) `CHECK`

### `memberships`

Appartenance d'un utilisateur à un établissement, avec son rôle.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `user_id` | `uuid` | oui | — |
| `role` | `text` | oui | — |
| `status` | `text` | oui | `'active'::text` |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `FOREIGN KEY (user_id) REFERENCES nhill.users(id) ON DELETE CASCADE` ; `PRIMARY KEY (id)` ; `UNIQUE (establishment_id, user_id)` ; 2 contrôle(s) `CHECK`

### `invitations`

Invitations en attente (code d'activation stocké sous forme d'empreinte).

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `email` | `text` | oui | — |
| `role` | `text` | oui | — |
| `activation_hash` | `bytea` | oui | — |
| `expires_at` | `timestamp with time zone` | oui | — |
| `accepted_at` | `timestamp with time zone` | — | — |
| `revoked_at` | `timestamp with time zone` | — | — |
| `created_by` | `uuid` | oui | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (created_by) REFERENCES nhill.users(id)` ; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `PRIMARY KEY (id)` ; 2 contrôle(s) `CHECK`

### `join_tickets`

Tickets de courte durée remis après la vérification d'un code d'établissement.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : DELETE, INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `ticket_hash` | `bytea` | oui | — |
| `establishment_id` | `uuid` | oui | — |
| `expires_at` | `timestamp with time zone` | oui | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `PRIMARY KEY (ticket_hash)`

### `attachment_requests`

Demandes de rattachement à une entreprise existante.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `requester_name` | `text` | oui | — |
| `requester_email` | `text` | oui | — |
| `requester_phone` | `text` | — | — |
| `message` | `text` | — | — |
| `status` | `text` | oui | `'open'::text` |
| `resolution_note` | `text` | — | — |
| `resolved_by` | `uuid` | — | — |
| `resolved_at` | `timestamp with time zone` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id)` ; `PRIMARY KEY (id)` ; 1 contrôle(s) `CHECK`

### `sessions`

Sessions des applications (empreintes des jetons d'accès et de rafraîchissement).

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : DELETE, INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `user_id` | `uuid` | oui | — |
| `establishment_id` | `uuid` | oui | — |
| `access_hash` | `bytea` | oui | — |
| `access_expires_at` | `timestamp with time zone` | oui | — |
| `refresh_hash` | `bytea` | oui | — |
| `previous_refresh_hash` | `bytea` | — | — |
| `refresh_expires_at` | `timestamp with time zone` | oui | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `last_seen_at` | `timestamp with time zone` | oui | `now()` |
| `revoked_at` | `timestamp with time zone` | — | — |
| `revoke_reason` | `text` | — | — |
| `user_agent` | `text` | — | — |
| `ip` | `text` | — | — |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `FOREIGN KEY (user_id) REFERENCES nhill.users(id) ON DELETE CASCADE` ; `PRIMARY KEY (id)` ; `UNIQUE (access_hash)` ; `UNIQUE (refresh_hash)`

### `password_resets`

Codes de réinitialisation et tickets de changement de mot de passe (empreintes).

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : DELETE, INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `user_id` | `uuid` | oui | — |
| `purpose` | `text` | oui | — |
| `secret_hash` | `bytea` | oui | — |
| `expires_at` | `timestamp with time zone` | oui | — |
| `attempts` | `integer` | oui | `0` |
| `used_at` | `timestamp with time zone` | — | — |
| `revoked_at` | `timestamp with time zone` | — | — |
| `ip` | `text` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (user_id) REFERENCES nhill.users(id) ON DELETE CASCADE` ; `PRIMARY KEY (id)` ; 2 contrôle(s) `CHECK`

### `auth_throttle`

Compteurs de limitation de débit partagés par les instances du serveur.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : DELETE, INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `bucket` | `text` | oui | — |
| `window_start` | `timestamp with time zone` | oui | — |
| `hits` | `integer` | oui | `0` |

Contraintes : `PRIMARY KEY (bucket, window_start)`

### `trial_registry`

Empreintes HMAC des identifiants ayant déjà bénéficié d'un essai ; survit à la suppression de l'établissement.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `fingerprint` | `bytea` | oui | — |
| `establishment_id` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `PRIMARY KEY (fingerprint)`

### `compliance_profiles`

Profils pays chargés (identifiants, taxes, mentions, numérotation), versionnés.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `code` | `text` | oui | — |
| `version` | `integer` | oui | — |
| `country_code` | `character(2)` | — | — |
| `definition` | `jsonb` | oui | — |
| `content_sha256` | `text` | oui | — |
| `validation_note` | `text` | oui | `''::text` |
| `loaded_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `PRIMARY KEY (code, version)` ; 2 contrôle(s) `CHECK`

### `currencies`

Devises ISO 4217 et nombre de décimales.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `code` | `character(3)` | oui | — |
| `minor_units` | `smallint` | oui | — |
| `name_fr` | `text` | oui | — |
| `name_en` | `text` | oui | — |
| `active` | `boolean` | oui | `true` |

Contraintes : `PRIMARY KEY (code)` ; 2 contrôle(s) `CHECK`

### `notifications`

Notifications destinées aux utilisateurs.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `user_id` | `uuid` | — | — |
| `kind` | `text` | oui | — |
| `params` | `jsonb` | oui | `'{}'::jsonb` |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `read_at` | `timestamp with time zone` | — | — |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `FOREIGN KEY (user_id) REFERENCES nhill.users(id) ON DELETE CASCADE` ; `PRIMARY KEY (id)`

### `email_outbox`

File d'envoi des e-mails automatiques ; messages envoyés ou en échec supprimés après 30 jours.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : DELETE, INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `to_address` | `text` | oui | — |
| `template` | `text` | oui | — |
| `language` | `text` | oui | `'fr'::text` |
| `params` | `jsonb` | oui | `'{}'::jsonb` |
| `status` | `text` | oui | `'pending'::text` |
| `attempts` | `integer` | oui | `0` |
| `last_error` | `text` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `sent_at` | `timestamp with time zone` | — | — |

Contraintes : `PRIMARY KEY (id)` ; 1 contrôle(s) `CHECK`

### `schema_migrations`

Migrations appliquées et leur empreinte (une migration modifiée est refusée).

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : SELECT

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `version` | `text` | oui | — |
| `checksum` | `text` | oui | — |
| `applied_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `PRIMARY KEY (version)`

## Formules, droits et abonnements

### `plans`

Formules : public visé, niveau, conditions (essai, grâce, rappels, renouvellement, formule de repli).

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Déclencheurs : `plans_legacy_to_entitlements`, `plans_main_price`, `plans_touch`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `code` | `text` | oui | — |
| `name_fr` | `text` | oui | — |
| `name_en` | `text` | oui | — |
| `description_fr` | `text` | oui | `''::text` |
| `description_en` | `text` | oui | `''::text` |
| `billing_period` | `text` | oui | — |
| `price_minor` | `bigint` | oui | — |
| `currency` | `character(3)` | oui | — |
| `tax_rate_bp` | `integer` | oui | `0` |
| `max_users` | `integer` | — | — |
| `audience` | `text` | oui | `'any'::text` |
| `features` | `jsonb` | oui | `'{}'::jsonb` |
| `active` | `boolean` | oui | `true` |
| `sort_order` | `integer` | oui | `0` |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `updated_at` | `timestamp with time zone` | oui | `now()` |
| `tier` | `integer` | oui | `0` |
| `max_registers` | `integer` | — | — |
| `max_warehouses` | `integer` | — | — |
| `kind` | `text` | oui | `'paid'::text` |
| `visibility` | `text` | oui | `'public'::text` |
| `self_service` | `boolean` | oui | `true` |
| `auto_renew_allowed` | `boolean` | oui | `true` |
| `grace_days` | `integer` | — | — |
| `reminder_days` | `integer[]` | — | — |
| `trial_days` | `integer` | — | — |
| `expiry_fallback_plan_id` | `uuid` | — | — |
| `archived_at` | `timestamp with time zone` | — | — |

Contraintes : `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)` ; `FOREIGN KEY (expiry_fallback_plan_id) REFERENCES nhill.plans(id)` ; `PRIMARY KEY (id)` ; `UNIQUE (code)` ; 14 contrôle(s) `CHECK`

### `plan_prices`

Prix d'une formule par durée (1 à 36 mois) et par devise.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : DELETE, INSERT, SELECT, UPDATE
* Déclencheurs : `plan_prices_touch`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `plan_id` | `uuid` | oui | — |
| `duration_months` | `integer` | oui | — |
| `currency` | `character(3)` | oui | — |
| `price_minor` | `bigint` | oui | — |
| `active` | `boolean` | oui | `true` |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `updated_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)` ; `FOREIGN KEY (plan_id) REFERENCES nhill.plans(id) ON DELETE CASCADE` ; `PRIMARY KEY (id)` ; `UNIQUE (plan_id, duration_months, currency)` ; 2 contrôle(s) `CHECK`

### `plan_capabilities`

Catalogue des fonctions et des limites, valeurs par défaut et d'essai, libellés FR/EN.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : SELECT

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `key` | `text` | oui | — |
| `kind` | `text` | oui | — |
| `label_fr` | `text` | oui | — |
| `label_en` | `text` | oui | — |
| `description_fr` | `text` | oui | `''::text` |
| `description_en` | `text` | oui | `''::text` |
| `unit` | `text` | — | — |
| `default_enabled` | `boolean` | — | — |
| `default_limit` | `integer` | — | — |
| `trial_enabled` | `boolean` | — | — |
| `trial_limit` | `integer` | — | — |
| `sort_order` | `integer` | oui | `0` |

Contraintes : `PRIMARY KEY (key)` ; 5 contrôle(s) `CHECK`

### `plan_entitlements`

Valeur de chaque fonction ou limite pour une formule.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : DELETE, INSERT, SELECT, UPDATE
* Déclencheurs : `plan_entitlements_check`, `plan_entitlements_legacy`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `plan_id` | `uuid` | oui | — |
| `key` | `text` | oui | — |
| `enabled` | `boolean` | — | — |
| `limit_value` | `integer` | — | — |
| `updated_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (key) REFERENCES nhill.plan_capabilities(key)` ; `FOREIGN KEY (plan_id) REFERENCES nhill.plans(id) ON DELETE CASCADE` ; `PRIMARY KEY (plan_id, key)` ; 1 contrôle(s) `CHECK`

### `plan_assignment_rules`

Règles d'attribution d'une formule à la création d'un établissement (essai ou attribution).

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : DELETE, INSERT, SELECT, UPDATE
* Déclencheurs : `plan_assignment_rules_touch`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `priority` | `integer` | oui | `100` |
| `active` | `boolean` | oui | `true` |
| `audience` | `text` | oui | `'any'::text` |
| `country_code` | `character(2)` | — | — |
| `plan_id` | `uuid` | oui | — |
| `mode` | `text` | oui | — |
| `duration_days` | `integer` | — | — |
| `description` | `text` | oui | `''::text` |
| `created_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `updated_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (created_by) REFERENCES nhill.admin_users(id)` ; `FOREIGN KEY (plan_id) REFERENCES nhill.plans(id)` ; `PRIMARY KEY (id)` ; 6 contrôle(s) `CHECK`

### `subscriptions`

Abonnement de chaque établissement : formule, période, statut, dates d'accès.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Déclencheurs : `subscriptions_touch`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `plan_id` | `uuid` | — | — |
| `pending_plan_id` | `uuid` | — | — |
| `status` | `text` | oui | — |
| `is_trial` | `boolean` | oui | `false` |
| `period_start` | `timestamp with time zone` | oui | — |
| `period_end` | `timestamp with time zone` | oui | — |
| `grace_until` | `timestamp with time zone` | — | — |
| `reminder_sent_for` | `timestamp with time zone` | — | — |
| `auto_renew_consent_at` | `timestamp with time zone` | — | — |
| `canceled_at` | `timestamp with time zone` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `updated_at` | `timestamp with time zone` | oui | `now()` |
| `price_id` | `uuid` | — | — |
| `pending_price_id` | `uuid` | — | — |
| `pending_from` | `timestamp with time zone` | — | — |
| `failed_payments` | `integer` | oui | `0` |
| `last_failure_at` | `timestamp with time zone` | — | — |
| `open_ended` | `boolean` | oui | `false` |
| `suspension_reason` | `text` | — | — |
| `assignment_rule_id` | `uuid` | — | — |

Contraintes : `FOREIGN KEY (assignment_rule_id) REFERENCES nhill.plan_assignment_rules(id) ON DELETE SET NULL` ; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `FOREIGN KEY (pending_plan_id) REFERENCES nhill.plans(id)` ; `FOREIGN KEY (pending_price_id) REFERENCES nhill.plan_prices(id)` ; `FOREIGN KEY (plan_id) REFERENCES nhill.plans(id)` ; `FOREIGN KEY (price_id) REFERENCES nhill.plan_prices(id)` ; `PRIMARY KEY (id)` ; `UNIQUE (establishment_id)` ; 4 contrôle(s) `CHECK`

### `subscription_events`

Historique des changements d'abonnement.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `bigint` | oui | identité |
| `subscription_id` | `uuid` | oui | — |
| `establishment_id` | `uuid` | oui | — |
| `kind` | `text` | oui | — |
| `from_status` | `text` | — | — |
| `to_status` | `text` | — | — |
| `period_start` | `timestamp with time zone` | — | — |
| `period_end` | `timestamp with time zone` | — | — |
| `details` | `jsonb` | oui | `'{}'::jsonb` |
| `actor_type` | `text` | oui | — |
| `actor_id` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (subscription_id) REFERENCES nhill.subscriptions(id) ON DELETE CASCADE` ; `PRIMARY KEY (id)` ; 1 contrôle(s) `CHECK`

### `subscription_reminders`

Rappels d'échéance déjà envoyés (pas de doublon).

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `subscription_id` | `uuid` | oui | — |
| `period_end` | `timestamp with time zone` | oui | — |
| `days_before` | `integer` | oui | — |
| `sent_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (subscription_id) REFERENCES nhill.subscriptions(id) ON DELETE CASCADE` ; `PRIMARY KEY (subscription_id, period_end, days_before)` ; 1 contrôle(s) `CHECK`

### `subscription_orders`

Commandes d'abonnement et leur paiement.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `plan_id` | `uuid` | oui | — |
| `amount_minor` | `bigint` | oui | — |
| `currency` | `character(3)` | oui | — |
| `status` | `text` | oui | `'pending'::text` |
| `provider` | `text` | — | — |
| `provider_reference` | `text` | — | — |
| `created_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `paid_at` | `timestamp with time zone` | — | — |
| `kind` | `text` | oui | `'renewal'::text` |
| `price_id` | `uuid` | — | — |
| `duration_months` | `integer` | — | — |
| `subtotal_minor` | `bigint` | — | — |
| `discount_minor` | `bigint` | oui | `0` |
| `credit_minor` | `bigint` | oui | `0` |
| `tax_minor` | `bigint` | oui | `0` |
| `promo_code_id` | `uuid` | — | — |
| `provider_id` | `uuid` | — | — |
| `provider_session` | `text` | — | — |
| `checkout_url` | `text` | — | — |
| `expires_at` | `timestamp with time zone` | — | — |
| `failure_reason` | `text` | — | — |

Contraintes : `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)` ; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `FOREIGN KEY (plan_id) REFERENCES nhill.plans(id)` ; `FOREIGN KEY (price_id) REFERENCES nhill.plan_prices(id)` ; `FOREIGN KEY (promo_code_id) REFERENCES nhill.promo_codes(id)` ; `FOREIGN KEY (provider_id) REFERENCES nhill.payment_providers(id)` ; `PRIMARY KEY (id)` ; `UNIQUE (provider, provider_reference)` ; 6 contrôle(s) `CHECK`

### `promo_codes`

Codes promotionnels d'abonnement.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Déclencheurs : `promo_codes_touch`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `code` | `text` | oui | — |
| `description` | `text` | oui | `''::text` |
| `discount_kind` | `text` | oui | — |
| `percent_bp` | `integer` | — | — |
| `amount_minor` | `bigint` | — | — |
| `currency` | `character(3)` | — | — |
| `valid_from` | `timestamp with time zone` | oui | `now()` |
| `valid_until` | `timestamp with time zone` | — | — |
| `max_redemptions` | `integer` | — | — |
| `max_per_establishment` | `integer` | oui | `1` |
| `plan_ids` | `uuid[]` | — | — |
| `min_duration_months` | `integer` | — | — |
| `new_customers_only` | `boolean` | oui | `false` |
| `audience` | `text` | oui | `'any'::text` |
| `active` | `boolean` | oui | `true` |
| `created_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `updated_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (created_by) REFERENCES nhill.admin_users(id)` ; `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)` ; `PRIMARY KEY (id)` ; `UNIQUE (code)` ; 10 contrôle(s) `CHECK`

### `promo_redemptions`

Utilisations des codes promotionnels d'abonnement.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `promo_code_id` | `uuid` | oui | — |
| `establishment_id` | `uuid` | oui | — |
| `order_id` | `uuid` | oui | — |
| `status` | `text` | oui | `'reserved'::text` |
| `discount_minor` | `bigint` | oui | — |
| `currency` | `character(3)` | oui | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `settled_at` | `timestamp with time zone` | — | — |

Contraintes : `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)` ; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `FOREIGN KEY (order_id) REFERENCES nhill.subscription_orders(id) ON DELETE CASCADE` ; `FOREIGN KEY (promo_code_id) REFERENCES nhill.promo_codes(id)` ; `PRIMARY KEY (id)` ; `UNIQUE (order_id)` ; 2 contrôle(s) `CHECK`

### `payment_providers`

Prestataires de paiement configurés (secrets chiffrés).

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Déclencheurs : `payment_providers_touch`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `code` | `text` | oui | — |
| `kind` | `text` | oui | — |
| `display_name` | `text` | oui | — |
| `environment` | `text` | oui | `'test'::text` |
| `enabled` | `boolean` | oui | `false` |
| `currencies` | `character(3)[]` | oui | `'{}'::bpchar[]` |
| `settings` | `jsonb` | oui | `'{}'::jsonb` |
| `secret_cipher` | `bytea` | — | — |
| `secret_nonce` | `bytea` | — | — |
| `secret_fields` | `text[]` | oui | `'{}'::text[]` |
| `sort_order` | `integer` | oui | `0` |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `updated_at` | `timestamp with time zone` | oui | `now()` |
| `updated_by` | `uuid` | — | — |

Contraintes : `FOREIGN KEY (updated_by) REFERENCES nhill.admin_users(id)` ; `PRIMARY KEY (id)` ; `UNIQUE (code)` ; 5 contrôle(s) `CHECK`

### `payment_webhook_events`

Notifications reçues des prestataires (dédoublonnage).

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `bigint` | oui | identité |
| `provider` | `text` | oui | — |
| `event_id` | `text` | oui | — |
| `received_at` | `timestamp with time zone` | oui | `now()` |
| `payload` | `jsonb` | oui | — |
| `processed_at` | `timestamp with time zone` | — | — |
| `outcome` | `text` | — | — |

Contraintes : `PRIMARY KEY (id)` ; `UNIQUE (provider, event_id)`

### `billing_archive`

Commandes payées ou remboursées d'un établissement supprimé, conservées 10 ans.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : SELECT

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `bigint` | oui | identité |
| `establishment_id` | `uuid` | oui | — |
| `trade_name` | `text` | oui | — |
| `legal_name` | `text` | — | — |
| `country_code` | `character(2)` | — | — |
| `order_id` | `uuid` | oui | — |
| `plan_code` | `text` | — | — |
| `kind` | `text` | — | — |
| `duration_months` | `integer` | — | — |
| `subtotal_minor` | `bigint` | — | — |
| `discount_minor` | `bigint` | — | — |
| `tax_minor` | `bigint` | — | — |
| `amount_minor` | `bigint` | oui | — |
| `currency` | `character(3)` | oui | — |
| `status` | `text` | oui | — |
| `provider` | `text` | — | — |
| `provider_reference` | `text` | — | — |
| `created_at` | `timestamp with time zone` | oui | — |
| `paid_at` | `timestamp with time zone` | — | — |
| `archived_at` | `timestamp with time zone` | oui | `now()` |
| `keep_until` | `date` | oui | — |

Contraintes : `PRIMARY KEY (id)` ; `UNIQUE (order_id)`

## Gestion commerciale (données des établissements)

### `tax_rates`

Taux de taxe d'un établissement (le taux ne change jamais).

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Politiques : `tax_rates_tenant`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `code` | `text` | oui | — |
| `label` | `text` | oui | — |
| `rate_bp` | `integer` | oui | — |
| `kind` | `text` | oui | `'standard'::text` |
| `is_default` | `boolean` | oui | `false` |
| `active` | `boolean` | oui | `true` |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `PRIMARY KEY (id)` ; `UNIQUE (establishment_id, code)` ; `UNIQUE (establishment_id, id)` ; 3 contrôle(s) `CHECK`

### `product_categories`

Catégories de produits.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : DELETE, INSERT, SELECT, UPDATE
* Politiques : `product_categories_tenant`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `name` | `text` | oui | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `PRIMARY KEY (id)` ; `UNIQUE (establishment_id, id)` ; 1 contrôle(s) `CHECK`

### `products`

Produits et services : prix, taxe, unité, code-barres, seuil d'alerte.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Politiques : `products_tenant`
* Déclencheurs : `products_touch`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `kind` | `text` | oui | `'good'::text` |
| `sku` | `text` | oui | — |
| `barcode` | `text` | — | — |
| `name` | `text` | oui | — |
| `description` | `text` | oui | `''::text` |
| `category_id` | `uuid` | — | — |
| `unit` | `text` | oui | `'unit'::text` |
| `currency` | `character(3)` | oui | — |
| `purchase_price_minor` | `bigint` | oui | `0` |
| `sale_price_minor` | `bigint` | oui | `0` |
| `tax_rate_id` | `uuid` | — | — |
| `track_stock` | `boolean` | oui | `true` |
| `reorder_level_milli` | `bigint` | oui | `0` |
| `active` | `boolean` | oui | `true` |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `updated_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)` ; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `FOREIGN KEY (establishment_id, category_id) REFERENCES nhill.product_categories(establishment_id, id)` ; `FOREIGN KEY (establishment_id, tax_rate_id) REFERENCES nhill.tax_rates(establishment_id, id)` ; `PRIMARY KEY (id)` ; `UNIQUE (establishment_id, id)` ; `UNIQUE (establishment_id, sku)` ; 9 contrôle(s) `CHECK`

### `parties`

Clients et fournisseurs.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Politiques : `parties_tenant`
* Déclencheurs : `parties_touch`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `is_customer` | `boolean` | oui | `false` |
| `is_supplier` | `boolean` | oui | `false` |
| `kind` | `text` | oui | `'company'::text` |
| `name` | `text` | oui | — |
| `email` | `text` | — | — |
| `phone` | `text` | — | — |
| `address` | `text` | oui | `''::text` |
| `city` | `text` | oui | `''::text` |
| `country_code` | `character(2)` | — | — |
| `tax_ids` | `jsonb` | oui | `'{}'::jsonb` |
| `payment_terms_days` | `integer` | oui | `0` |
| `notes` | `text` | oui | `''::text` |
| `active` | `boolean` | oui | `true` |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `updated_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `PRIMARY KEY (id)` ; `UNIQUE (establishment_id, id)` ; 4 contrôle(s) `CHECK`

### `warehouses`

Dépôts ; le dépôt par défaut est le dépôt principal.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Politiques : `warehouses_tenant`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `name` | `text` | oui | — |
| `is_default` | `boolean` | oui | `false` |
| `active` | `boolean` | oui | `true` |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `PRIMARY KEY (id)` ; `UNIQUE (establishment_id, id)`

### `stock_levels`

Quantité en stock par produit et par dépôt.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : SELECT
* Politiques : `stock_levels_tenant`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `establishment_id` | `uuid` | oui | — |
| `product_id` | `uuid` | oui | — |
| `warehouse_id` | `uuid` | oui | — |
| `quantity_milli` | `bigint` | oui | `0` |
| `updated_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (establishment_id, product_id) REFERENCES nhill.products(establishment_id, id)` ; `FOREIGN KEY (establishment_id, warehouse_id) REFERENCES nhill.warehouses(establishment_id, id)` ; `PRIMARY KEY (establishment_id, product_id, warehouse_id)`

### `stock_movements`

Mouvements de stock, avec leur auteur et leur origine.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : INSERT, SELECT
* Politiques : `stock_movements_tenant`
* Déclencheurs : `stock_movements_apply`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `bigint` | oui | identité |
| `establishment_id` | `uuid` | oui | — |
| `product_id` | `uuid` | oui | — |
| `warehouse_id` | `uuid` | oui | — |
| `quantity_milli` | `bigint` | oui | — |
| `kind` | `text` | oui | — |
| `source_type` | `text` | — | — |
| `source_id` | `uuid` | — | — |
| `note` | `text` | oui | `''::text` |
| `created_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (establishment_id, product_id) REFERENCES nhill.products(establishment_id, id)` ; `FOREIGN KEY (establishment_id, warehouse_id) REFERENCES nhill.warehouses(establishment_id, id)` ; `PRIMARY KEY (id)` ; 2 contrôle(s) `CHECK`

### `sales_documents`

Documents de vente : devis, commandes, factures, avoirs, tickets de caisse.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : DELETE, INSERT, SELECT, UPDATE
* Politiques : `sales_documents_tenant`
* Déclencheurs : `sales_documents_guard`, `sales_documents_usage`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `doc_type` | `text` | oui | — |
| `status` | `text` | oui | `'draft'::text` |
| `number` | `text` | — | — |
| `sequence_value` | `bigint` | — | — |
| `party_id` | `uuid` | oui | — |
| `currency` | `character(3)` | oui | — |
| `language` | `text` | oui | `'fr'::text` |
| `issue_date` | `date` | oui | `nhill.nh_local_today()` |
| `due_date` | `date` | — | — |
| `valid_until` | `date` | — | — |
| `main_currency` | `character(3)` | oui | — |
| `fx_rate` | `numeric(24,10)` | — | — |
| `fx_rate_date` | `date` | — | — |
| `fx_rate_source` | `text` | — | — |
| `tax_rounding` | `text` | oui | `'per_line'::text` |
| `gross_minor` | `bigint` | oui | `0` |
| `discount_minor` | `bigint` | oui | `0` |
| `net_minor` | `bigint` | oui | `0` |
| `tax_minor` | `bigint` | oui | `0` |
| `total_minor` | `bigint` | oui | `0` |
| `paid_minor` | `bigint` | oui | `0` |
| `credited_minor` | `bigint` | oui | `0` |
| `tax_breakdown` | `jsonb` | oui | `'[]'::jsonb` |
| `notes` | `text` | oui | `''::text` |
| `terms` | `text` | oui | `''::text` |
| `related_document_id` | `uuid` | — | — |
| `restock` | `boolean` | oui | `false` |
| `seller_snapshot` | `jsonb` | — | — |
| `buyer_snapshot` | `jsonb` | — | — |
| `profile_code` | `text` | — | — |
| `profile_version` | `integer` | — | — |
| `fiscal_reference` | `jsonb` | — | — |
| `pdf_sha256` | `text` | — | — |
| `created_by` | `uuid` | — | — |
| `issued_by` | `uuid` | — | — |
| `issued_at` | `timestamp with time zone` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `updated_at` | `timestamp with time zone` | oui | `now()` |
| `pos_session_id` | `uuid` | — | — |

Contraintes : `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)` ; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `FOREIGN KEY (establishment_id, party_id) REFERENCES nhill.parties(establishment_id, id)` ; `FOREIGN KEY (establishment_id, pos_session_id) REFERENCES nhill.pos_sessions(establishment_id, id)` ; `FOREIGN KEY (establishment_id, related_document_id) REFERENCES nhill.sales_documents(establishment_id, id)` ; `FOREIGN KEY (main_currency) REFERENCES nhill.currencies(code)` ; `PRIMARY KEY (id)` ; `UNIQUE (establishment_id, doc_type, number)` ; `UNIQUE (establishment_id, id)` ; 10 contrôle(s) `CHECK`

### `sales_document_lines`

Lignes des documents de vente.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : DELETE, INSERT, SELECT, UPDATE
* Politiques : `sales_document_lines_tenant`
* Déclencheurs : `sales_document_lines_guard`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `document_id` | `uuid` | oui | — |
| `line_no` | `integer` | oui | — |
| `product_id` | `uuid` | — | — |
| `description` | `text` | oui | — |
| `quantity_milli` | `bigint` | oui | — |
| `unit` | `text` | oui | `'unit'::text` |
| `unit_price_minor` | `bigint` | oui | — |
| `discount_bp` | `integer` | oui | `0` |
| `tax_rate_bp` | `integer` | oui | `0` |
| `tax_code` | `text` | oui | `''::text` |
| `gross_minor` | `bigint` | oui | — |
| `discount_minor` | `bigint` | oui | — |
| `net_minor` | `bigint` | oui | — |
| `tax_minor` | `bigint` | oui | — |
| `total_minor` | `bigint` | oui | — |

Contraintes : `FOREIGN KEY (establishment_id, document_id) REFERENCES nhill.sales_documents(establishment_id, id) ON DELETE CASCADE` ; `FOREIGN KEY (establishment_id, product_id) REFERENCES nhill.products(establishment_id, id)` ; `PRIMARY KEY (id)` ; `UNIQUE (document_id, line_no)` ; 6 contrôle(s) `CHECK`

### `document_status_history`

Historique des statuts des documents.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : INSERT, SELECT
* Politiques : `document_status_history_tenant`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `bigint` | oui | identité |
| `establishment_id` | `uuid` | oui | — |
| `document_id` | `uuid` | oui | — |
| `from_status` | `text` | — | — |
| `to_status` | `text` | oui | — |
| `actor_id` | `uuid` | — | — |
| `note` | `text` | oui | `''::text` |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (establishment_id, document_id) REFERENCES nhill.sales_documents(establishment_id, id) ON DELETE CASCADE` ; `PRIMARY KEY (id)`

### `document_sequences`

Compteurs de numérotation par type de document et par période.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Politiques : `document_sequences_tenant`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `establishment_id` | `uuid` | oui | — |
| `doc_type` | `text` | oui | — |
| `period_key` | `text` | oui | — |
| `next_value` | `bigint` | oui | — |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `PRIMARY KEY (establishment_id, doc_type, period_key)` ; 1 contrôle(s) `CHECK`

### `purchases`

Achats fournisseurs.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : DELETE, INSERT, SELECT, UPDATE
* Politiques : `purchases_tenant`
* Déclencheurs : `purchases_guard`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `supplier_id` | `uuid` | oui | — |
| `status` | `text` | oui | `'draft'::text` |
| `supplier_reference` | `text` | oui | `''::text` |
| `purchase_date` | `date` | oui | `nhill.nh_local_today()` |
| `due_date` | `date` | — | — |
| `currency` | `character(3)` | oui | — |
| `warehouse_id` | `uuid` | oui | — |
| `net_minor` | `bigint` | oui | `0` |
| `tax_minor` | `bigint` | oui | `0` |
| `total_minor` | `bigint` | oui | `0` |
| `paid_minor` | `bigint` | oui | `0` |
| `notes` | `text` | oui | `''::text` |
| `created_by` | `uuid` | — | — |
| `received_by` | `uuid` | — | — |
| `received_at` | `timestamp with time zone` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `updated_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)` ; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `FOREIGN KEY (establishment_id, supplier_id) REFERENCES nhill.parties(establishment_id, id)` ; `FOREIGN KEY (establishment_id, warehouse_id) REFERENCES nhill.warehouses(establishment_id, id)` ; `PRIMARY KEY (id)` ; `UNIQUE (establishment_id, id)` ; 4 contrôle(s) `CHECK`

### `purchase_lines`

Lignes d'achat.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : DELETE, INSERT, SELECT, UPDATE
* Politiques : `purchase_lines_tenant`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `purchase_id` | `uuid` | oui | — |
| `line_no` | `integer` | oui | — |
| `product_id` | `uuid` | — | — |
| `description` | `text` | oui | — |
| `quantity_milli` | `bigint` | oui | — |
| `unit_cost_minor` | `bigint` | oui | — |
| `tax_rate_bp` | `integer` | oui | `0` |
| `net_minor` | `bigint` | oui | — |
| `tax_minor` | `bigint` | oui | — |
| `total_minor` | `bigint` | oui | — |

Contraintes : `FOREIGN KEY (establishment_id, product_id) REFERENCES nhill.products(establishment_id, id)` ; `FOREIGN KEY (establishment_id, purchase_id) REFERENCES nhill.purchases(establishment_id, id) ON DELETE CASCADE` ; `PRIMARY KEY (id)` ; `UNIQUE (purchase_id, line_no)` ; 4 contrôle(s) `CHECK`

### `expense_categories`

Catégories de dépenses.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : DELETE, INSERT, SELECT, UPDATE
* Politiques : `expense_categories_tenant`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `name` | `text` | oui | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `PRIMARY KEY (id)` ; `UNIQUE (establishment_id, id)` ; 1 contrôle(s) `CHECK`

### `expenses`

Dépenses.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Politiques : `expenses_tenant`
* Déclencheurs : `expenses_touch`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `category_id` | `uuid` | — | — |
| `supplier_id` | `uuid` | — | — |
| `description` | `text` | oui | — |
| `amount_minor` | `bigint` | oui | — |
| `tax_minor` | `bigint` | oui | `0` |
| `currency` | `character(3)` | oui | — |
| `expense_date` | `date` | oui | `nhill.nh_local_today()` |
| `due_date` | `date` | — | — |
| `paid_minor` | `bigint` | oui | `0` |
| `receipt_object_id` | `uuid` | — | — |
| `created_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `updated_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)` ; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `FOREIGN KEY (establishment_id, category_id) REFERENCES nhill.expense_categories(establishment_id, id)` ; `FOREIGN KEY (establishment_id, receipt_object_id) REFERENCES nhill.objects(establishment_id, id)` ; `FOREIGN KEY (establishment_id, supplier_id) REFERENCES nhill.parties(establishment_id, id)` ; `PRIMARY KEY (id)` ; `UNIQUE (establishment_id, id)` ; 6 contrôle(s) `CHECK`

### `payments`

Paiements reçus et versés, rattachés à un document, un achat ou une dépense.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Politiques : `payments_tenant`
* Déclencheurs : `payments_usage`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `direction` | `text` | oui | — |
| `party_id` | `uuid` | — | — |
| `document_id` | `uuid` | — | — |
| `purchase_id` | `uuid` | — | — |
| `expense_id` | `uuid` | — | — |
| `method` | `text` | oui | — |
| `account_id` | `uuid` | — | — |
| `amount_minor` | `bigint` | oui | — |
| `currency` | `character(3)` | oui | — |
| `paid_at` | `timestamp with time zone` | oui | `now()` |
| `reference` | `text` | oui | `''::text` |
| `note` | `text` | oui | `''::text` |
| `created_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)` ; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `FOREIGN KEY (establishment_id, account_id) REFERENCES nhill.cash_accounts(establishment_id, id)` ; `FOREIGN KEY (establishment_id, document_id) REFERENCES nhill.sales_documents(establishment_id, id)` ; `FOREIGN KEY (establishment_id, expense_id) REFERENCES nhill.expenses(establishment_id, id)` ; `FOREIGN KEY (establishment_id, party_id) REFERENCES nhill.parties(establishment_id, id)` ; `FOREIGN KEY (establishment_id, purchase_id) REFERENCES nhill.purchases(establishment_id, id)` ; `PRIMARY KEY (id)` ; `UNIQUE (establishment_id, id)` ; 4 contrôle(s) `CHECK`

### `cash_accounts`

Caisses et comptes bancaires.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Politiques : `cash_accounts_tenant`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `name` | `text` | oui | — |
| `kind` | `text` | oui | — |
| `currency` | `character(3)` | oui | — |
| `active` | `boolean` | oui | `true` |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)` ; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `PRIMARY KEY (id)` ; `UNIQUE (establishment_id, id)` ; 1 contrôle(s) `CHECK`

### `exchange_rates`

Taux de change enregistrés (date et source).

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Politiques : `exchange_rates_tenant`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `from_currency` | `character(3)` | oui | — |
| `to_currency` | `character(3)` | oui | — |
| `rate` | `numeric(24,10)` | oui | — |
| `rate_date` | `date` | oui | — |
| `source` | `text` | oui | `''::text` |
| `created_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `FOREIGN KEY (from_currency) REFERENCES nhill.currencies(code)` ; `FOREIGN KEY (to_currency) REFERENCES nhill.currencies(code)` ; `PRIMARY KEY (id)` ; `UNIQUE (establishment_id, from_currency, to_currency, rate_date)` ; 2 contrôle(s) `CHECK`

### `objects`

Fichiers stockés : logo, justificatifs (PDF, PNG, JPEG, 5 Mo au plus).

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : DELETE, INSERT, SELECT, UPDATE
* Politiques : `objects_tenant`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `purpose` | `text` | oui | — |
| `mime` | `text` | oui | — |
| `byte_size` | `integer` | oui | — |
| `sha256` | `text` | oui | — |
| `width` | `integer` | — | — |
| `height` | `integer` | — | — |
| `data` | `bytea` | oui | — |
| `created_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `PRIMARY KEY (id)` ; `UNIQUE (establishment_id, id)` ; 3 contrôle(s) `CHECK`

### `idempotency_keys`

Clés d'idempotence et réponses enregistrées, dans la même transaction que l'opération.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : DELETE, INSERT, SELECT, UPDATE
* Politiques : `idempotency_keys_tenant`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `establishment_id` | `uuid` | oui | — |
| `idem_key` | `text` | oui | — |
| `user_id` | `uuid` | — | — |
| `request_hash` | `bytea` | oui | — |
| `status_code` | `integer` | — | — |
| `response_body` | `text` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `PRIMARY KEY (establishment_id, idem_key)` ; 1 contrôle(s) `CHECK`

### `audit_log`

Journal d'activité, en ajout seul.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `bigint` | oui | identité |
| `establishment_id` | `uuid` | — | — |
| `actor_type` | `text` | oui | — |
| `actor_id` | `uuid` | — | — |
| `action` | `text` | oui | — |
| `entity` | `text` | — | — |
| `entity_id` | `text` | — | — |
| `details` | `jsonb` | oui | `'{}'::jsonb` |
| `ip` | `text` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `PRIMARY KEY (id)` ; 1 contrôle(s) `CHECK`

## Caisse (Nhill POS)

### `pos_settings`

Règles de caisse : client de passage, remise maximale, prix modifiable, pied et largeur du ticket.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Politiques : `pos_settings_tenant`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `establishment_id` | `uuid` | oui | — |
| `walk_in_party_id` | `uuid` | — | — |
| `max_discount_bp` | `integer` | oui | `1000` |
| `allow_price_override` | `boolean` | oui | `false` |
| `receipt_footer` | `text` | oui | `''::text` |
| `receipt_width_mm` | `integer` | oui | `80` |
| `updated_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `FOREIGN KEY (establishment_id, walk_in_party_id) REFERENCES nhill.parties(establishment_id, id)` ; `PRIMARY KEY (establishment_id)` ; 2 contrôle(s) `CHECK`

### `pos_registers`

Caisses et leur dépôt.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Politiques : `pos_registers_tenant`
* Déclencheurs : `pos_registers_touch`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `code` | `text` | oui | — |
| `name` | `text` | oui | — |
| `location` | `text` | oui | `''::text` |
| `warehouse_id` | `uuid` | oui | — |
| `active` | `boolean` | oui | `true` |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `updated_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `FOREIGN KEY (establishment_id, warehouse_id) REFERENCES nhill.warehouses(establishment_id, id)` ; `PRIMARY KEY (id)` ; `UNIQUE (establishment_id, code)` ; `UNIQUE (establishment_id, id)` ; 2 contrôle(s) `CHECK`

### `pos_register_users`

Caissiers autorisés par caisse.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : DELETE, INSERT, SELECT
* Politiques : `pos_register_users_tenant`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `establishment_id` | `uuid` | oui | — |
| `register_id` | `uuid` | oui | — |
| `user_id` | `uuid` | oui | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (establishment_id, register_id) REFERENCES nhill.pos_registers(establishment_id, id) ON DELETE CASCADE` ; `FOREIGN KEY (user_id) REFERENCES nhill.users(id) ON DELETE CASCADE` ; `PRIMARY KEY (register_id, user_id)`

### `pos_payment_methods`

Moyens de paiement de la caisse.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Politiques : `pos_payment_methods_tenant`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `code` | `text` | oui | — |
| `label` | `text` | oui | — |
| `kind` | `text` | oui | — |
| `account_id` | `uuid` | — | — |
| `active` | `boolean` | oui | `true` |
| `sort_order` | `integer` | oui | `0` |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `FOREIGN KEY (establishment_id, account_id) REFERENCES nhill.cash_accounts(establishment_id, id)` ; `PRIMARY KEY (id)` ; `UNIQUE (establishment_id, code)` ; `UNIQUE (establishment_id, id)` ; 3 contrôle(s) `CHECK`

### `pos_sessions`

Sessions de caisse : ouverture, fonds de caisse, clôture, comptage, écart.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Politiques : `pos_sessions_tenant`
* Déclencheurs : `pos_sessions_guard`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `register_id` | `uuid` | oui | — |
| `number` | `integer` | oui | — |
| `status` | `text` | oui | `'open'::text` |
| `currency` | `character(3)` | oui | — |
| `opened_by` | `uuid` | oui | — |
| `opened_at` | `timestamp with time zone` | oui | `now()` |
| `opening_float_minor` | `bigint` | oui | `0` |
| `closed_by` | `uuid` | — | — |
| `closed_at` | `timestamp with time zone` | — | — |
| `expected` | `jsonb` | — | — |
| `counted` | `jsonb` | — | — |
| `cash_expected_minor` | `bigint` | — | — |
| `cash_counted_minor` | `bigint` | — | — |
| `cash_variance_minor` | `bigint` | — | — |
| `closing_note` | `text` | oui | `''::text` |

Contraintes : `FOREIGN KEY (closed_by) REFERENCES nhill.users(id)` ; `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)` ; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `FOREIGN KEY (establishment_id, register_id) REFERENCES nhill.pos_registers(establishment_id, id)` ; `FOREIGN KEY (opened_by) REFERENCES nhill.users(id)` ; `PRIMARY KEY (id)` ; `UNIQUE (establishment_id, id)` ; `UNIQUE (register_id, number)` ; 4 contrôle(s) `CHECK`

### `pos_cash_movements`

Entrées et sorties d'espèces d'une session.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : INSERT, SELECT
* Politiques : `pos_cash_movements_tenant`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `session_id` | `uuid` | oui | — |
| `kind` | `text` | oui | — |
| `amount_minor` | `bigint` | oui | — |
| `reason` | `text` | oui | — |
| `created_by` | `uuid` | oui | — |
| `approved_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (approved_by) REFERENCES nhill.users(id)` ; `FOREIGN KEY (created_by) REFERENCES nhill.users(id)` ; `FOREIGN KEY (establishment_id, session_id) REFERENCES nhill.pos_sessions(establishment_id, id)` ; `PRIMARY KEY (id)` ; 3 contrôle(s) `CHECK`

### `pos_approvals`

Autorisations de responsable à usage unique.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Politiques : `pos_approvals_tenant`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `token_hash` | `bytea` | oui | — |
| `action` | `text` | oui | — |
| `approver_id` | `uuid` | oui | — |
| `requested_by` | `uuid` | oui | — |
| `session_id` | `uuid` | — | — |
| `expires_at` | `timestamp with time zone` | oui | — |
| `used_at` | `timestamp with time zone` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (approver_id) REFERENCES nhill.users(id)` ; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `FOREIGN KEY (establishment_id, session_id) REFERENCES nhill.pos_sessions(establishment_id, id)` ; `FOREIGN KEY (requested_by) REFERENCES nhill.users(id)` ; `PRIMARY KEY (id)` ; `UNIQUE (token_hash)` ; 1 contrôle(s) `CHECK`

### `sales_promo_codes`

Codes promotionnels de vente d'un établissement.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Politiques : `sales_promo_codes_tenant`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `code` | `text` | oui | — |
| `description` | `text` | oui | `''::text` |
| `percent_bp` | `integer` | oui | — |
| `min_amount_minor` | `bigint` | oui | `0` |
| `currency` | `character(3)` | — | — |
| `valid_from` | `timestamp with time zone` | oui | `now()` |
| `valid_until` | `timestamp with time zone` | — | — |
| `max_redemptions` | `integer` | — | — |
| `active` | `boolean` | oui | `true` |
| `created_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (created_by) REFERENCES nhill.users(id)` ; `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)` ; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `PRIMARY KEY (id)` ; `UNIQUE (establishment_id, code)` ; `UNIQUE (establishment_id, id)` ; 5 contrôle(s) `CHECK`

### `sales_promo_redemptions`

Utilisations des codes promotionnels de vente.

* Sécurité au niveau des lignes : activée et forcée
* Droits de `nhill_app` : INSERT, SELECT
* Politiques : `sales_promo_redemptions_tenant`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `establishment_id` | `uuid` | oui | — |
| `promo_id` | `uuid` | oui | — |
| `document_id` | `uuid` | oui | — |
| `discount_minor` | `bigint` | oui | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (establishment_id, document_id) REFERENCES nhill.sales_documents(establishment_id, id)` ; `FOREIGN KEY (establishment_id, promo_id) REFERENCES nhill.sales_promo_codes(establishment_id, id)` ; `PRIMARY KEY (id)` ; `UNIQUE (document_id)` ; 1 contrôle(s) `CHECK`

## Console et exploitation

### `admin_users`

Administrateurs de la plateforme (mot de passe et TOTP).

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `email` | `text` | oui | — |
| `full_name` | `text` | oui | — |
| `password_hash` | `text` | — | — |
| `totp_cipher` | `bytea` | — | — |
| `totp_nonce` | `bytea` | — | — |
| `totp_enabled` | `boolean` | oui | `false` |
| `role` | `text` | oui | — |
| `status` | `text` | oui | `'pending'::text` |
| `setup_token_hash` | `bytea` | — | — |
| `setup_expires_at` | `timestamp with time zone` | — | — |
| `last_totp_step` | `bigint` | — | — |
| `last_login_at` | `timestamp with time zone` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `PRIMARY KEY (id)` ; `UNIQUE (email)` ; 3 contrôle(s) `CHECK`

### `admin_sessions`

Sessions de la console.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `admin_id` | `uuid` | oui | — |
| `token_hash` | `bytea` | oui | — |
| `expires_at` | `timestamp with time zone` | oui | — |
| `revoked_at` | `timestamp with time zone` | — | — |
| `ip` | `text` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (admin_id) REFERENCES nhill.admin_users(id) ON DELETE CASCADE` ; `PRIMARY KEY (id)` ; `UNIQUE (token_hash)`

### `admin_notifications`

Notifications de la console.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `bigint` | oui | identité |
| `kind` | `text` | oui | — |
| `params` | `jsonb` | oui | `'{}'::jsonb` |
| `establishment_id` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `read_at` | `timestamp with time zone` | — | — |
| `read_by` | `uuid` | — | — |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `FOREIGN KEY (read_by) REFERENCES nhill.admin_users(id)` ; `PRIMARY KEY (id)`

### `announcements`

Annonces publiées aux établissements.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `kind` | `text` | oui | — |
| `title_fr` | `text` | oui | — |
| `title_en` | `text` | oui | — |
| `body_fr` | `text` | oui | `''::text` |
| `body_en` | `text` | oui | `''::text` |
| `starts_at` | `timestamp with time zone` | oui | `now()` |
| `ends_at` | `timestamp with time zone` | — | — |
| `min_client_version` | `text` | — | — |
| `created_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (created_by) REFERENCES nhill.admin_users(id)` ; `PRIMARY KEY (id)` ; 1 contrôle(s) `CHECK`

### `platform_settings`

Paramètres de la plateforme (liste autorisée).

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `key` | `text` | oui | — |
| `value` | `jsonb` | oui | — |
| `description` | `text` | oui | `''::text` |
| `updated_at` | `timestamp with time zone` | oui | `now()` |
| `updated_by` | `uuid` | — | — |

Contraintes : `PRIMARY KEY (key)`

### `support_tickets`

Demandes d'assistance.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE
* Déclencheurs : `support_tickets_touch`

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `uuid` | oui | `gen_random_uuid()` |
| `number` | `bigint` | oui | identité |
| `establishment_id` | `uuid` | oui | — |
| `created_by` | `uuid` | — | — |
| `source` | `text` | oui | — |
| `category` | `text` | oui | — |
| `severity` | `text` | oui | `'normal'::text` |
| `subject` | `text` | oui | — |
| `status` | `text` | oui | `'open'::text` |
| `app_version` | `text` | — | — |
| `platform` | `text` | — | — |
| `assigned_to` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | oui | `now()` |
| `updated_at` | `timestamp with time zone` | oui | `now()` |
| `last_message_at` | `timestamp with time zone` | oui | `now()` |
| `resolved_at` | `timestamp with time zone` | — | — |

Contraintes : `FOREIGN KEY (assigned_to) REFERENCES nhill.admin_users(id)` ; `FOREIGN KEY (created_by) REFERENCES nhill.users(id)` ; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE` ; `PRIMARY KEY (id)` ; `UNIQUE (number)` ; 5 contrôle(s) `CHECK`

### `support_ticket_messages`

Messages des demandes, notes internes comprises.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `bigint` | oui | identité |
| `ticket_id` | `uuid` | oui | — |
| `author_type` | `text` | oui | — |
| `author_id` | `uuid` | — | — |
| `body` | `text` | oui | — |
| `internal` | `boolean` | oui | `false` |
| `created_at` | `timestamp with time zone` | oui | `now()` |

Contraintes : `FOREIGN KEY (ticket_id) REFERENCES nhill.support_tickets(id) ON DELETE CASCADE` ; `PRIMARY KEY (id)` ; 2 contrôle(s) `CHECK`

### `error_events`

Erreurs techniques regroupées par empreinte.

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : INSERT, SELECT, UPDATE

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `id` | `bigint` | oui | identité |
| `source` | `text` | oui | — |
| `level` | `text` | oui | `'error'::text` |
| `code` | `text` | oui | — |
| `message` | `text` | oui | — |
| `fingerprint` | `text` | oui | — |
| `context` | `jsonb` | oui | `'{}'::jsonb` |
| `establishment_id` | `uuid` | — | — |
| `user_id` | `uuid` | — | — |
| `app_version` | `text` | — | — |
| `platform` | `text` | — | — |
| `occurrences` | `integer` | oui | `1` |
| `first_seen` | `timestamp with time zone` | oui | `now()` |
| `last_seen` | `timestamp with time zone` | oui | `now()` |
| `resolved_at` | `timestamp with time zone` | — | — |
| `resolved_by` | `uuid` | — | — |

Contraintes : `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE SET NULL` ; `FOREIGN KEY (resolved_by) REFERENCES nhill.admin_users(id)` ; `PRIMARY KEY (id)` ; 4 contrôle(s) `CHECK`

### `usage_daily`

Compteurs journaliers d'usage par établissement (nombres seulement, jamais de contenu).

* Sécurité au niveau des lignes : non (table de la plateforme, accès limité par les droits du rôle `nhill_app`)
* Droits de `nhill_app` : SELECT

| Colonne | Type | Non nul | Défaut |
|---|---|---|---|
| `day` | `date` | oui | — |
| `establishment_id` | `uuid` | oui | — |
| `documents_issued` | `integer` | oui | `0` |
| `payments_recorded` | `integer` | oui | `0` |

Contraintes : `PRIMARY KEY (day, establishment_id)`

## Fonctions

| Fonction | Résultat | Propriétés | Exécutable par `nhill_app` |
|---|---|---|---|
| `nh_apply_stock_movement()` | `trigger` | volatile, `SECURITY DEFINER`, `search_path=nhill, pg_catalog` | — |
| `nh_check_plan_entitlement()` | `trigger` | volatile, `search_path=nhill, pg_catalog` | — |
| `nh_count_document_issue()` | `trigger` | volatile, `SECURITY DEFINER`, `search_path=nhill, pg_catalog` | — |
| `nh_count_payment()` | `trigger` | volatile, `SECURITY DEFINER`, `search_path=nhill, pg_catalog` | — |
| `nh_current_establishment()` | `uuid` | stable, `search_path=nhill, pg_catalog` | oui |
| `nh_erase_establishment(p_est uuid, p_early boolean)` | `jsonb` | volatile, `SECURITY DEFINER`, `search_path=nhill, pg_catalog` | oui |
| `nh_erasing(p_est uuid)` | `boolean` | stable, `search_path=nhill, pg_catalog` | oui |
| `nh_establishment_erasure(p_est uuid)` | `TABLE(eligible boolean, reason text, access_ended_at timestamp with time zone, erasable_from timestamp with time zone)` | stable, `search_path=nhill, pg_catalog` | oui |
| `nh_establishment_usage(p_est uuid)` | `TABLE(users integer, registers integer, warehouses integer, products integer, documents_month integer, storage_bytes bigint)` | volatile, `SECURITY DEFINER`, `search_path=nhill, pg_catalog` | oui |
| `nh_guard_pos_session()` | `trigger` | volatile, `search_path=nhill, pg_catalog` | — |
| `nh_guard_purchase()` | `trigger` | volatile, `search_path=nhill, pg_catalog` | — |
| `nh_guard_sales_document()` | `trigger` | volatile, `search_path=nhill, pg_catalog` | — |
| `nh_guard_sales_line()` | `trigger` | volatile, `search_path=nhill, pg_catalog` | — |
| `nh_iso(ts timestamp with time zone)` | `text` | immutable, `search_path=nhill, pg_catalog` | oui |
| `nh_like_contains(p text)` | `text` | immutable, `search_path=pg_catalog` | oui |
| `nh_local_date(p_ts timestamp with time zone)` | `date` | stable, `search_path=nhill, pg_catalog` | oui |
| `nh_local_start(p_day date)` | `timestamp with time zone` | stable, `search_path=nhill, pg_catalog` | oui |
| `nh_local_today()` | `date` | stable, `search_path=nhill, pg_catalog` | oui |
| `nh_plan_main_price()` | `trigger` | volatile, `search_path=nhill, pg_catalog` | — |
| `nh_platform_establishment_counts(p_est uuid)` | `TABLE(products bigint, customers bigint, suppliers bigint, documents bigint, drafts bigint)` | volatile, `SECURITY DEFINER`, `search_path=nhill, pg_catalog` | oui |
| `nh_platform_establishment_monthly(p_est uuid, p_from date, p_to date)` | `TABLE(month text, currency text, invoices_issued bigint, invoiced_net_minor bigint, invoiced_total_minor bigint, credited_total_minor bigint, payments_in_minor bigint, expenses_minor bigint)` | volatile, `SECURITY DEFINER`, `search_path=nhill, pg_catalog` | oui |
| `nh_platform_establishment_volumes(p_est uuid, p_from date, p_to date)` | `TABLE(currency text, invoices_issued bigint, invoiced_net_minor bigint, invoiced_total_minor bigint, credit_notes_issued bigint, credited_total_minor bigint, payments_in_count bigint, payments_in_minor bigint, payments_out_minor bigint, expenses_count bigint, expenses_minor bigint, purchases_minor bigint)` | volatile, `SECURITY DEFINER`, `search_path=nhill, pg_catalog` | oui |
| `nh_setting_int(p_key text, p_default integer)` | `integer` | stable, `search_path=nhill, pg_catalog` | oui |
| `nh_subscription_access(p_est uuid)` | `TABLE(access text, reason text, access_until timestamp with time zone, effective_plan_id uuid, is_trial boolean, status text, grace_end timestamp with time zone)` | stable, `search_path=nhill, pg_catalog` | oui |
| `nh_sync_plan_entitlements_from_legacy()` | `trigger` | volatile, `search_path=nhill, pg_catalog` | — |
| `nh_sync_plan_legacy_limits()` | `trigger` | volatile, `search_path=nhill, pg_catalog` | — |
| `nh_touch_updated_at()` | `trigger` | volatile, `search_path=nhill, pg_catalog` | — |
| `nh_tz(p_est uuid)` | `text` | stable, `search_path=nhill, pg_catalog` | oui |
| `nh_window(minutes integer)` | `timestamp with time zone` | stable, `search_path=nhill, pg_catalog` | oui |

## Récapitulatif

| Domaine | Tables |
|---|---|
| Plateforme et identité | 18 |
| Formules, droits et abonnements | 14 |
| Gestion commerciale (données des établissements) | 21 |
| Caisse (Nhill POS) | 9 |
| Console et exploitation | 9 |
| **Total** | **71** |
