# Nhill — Database reference

*Français : [../fr/reference-base-de-donnees.md](../fr/reference-base-de-donnees.md)*

**Generated** by `scripts/docs/gen_db_reference.py` from a PostgreSQL database migrated with the repository's migrations (do not edit by hand; continuous integration checks that it is up to date). Version 3.0.0: 71 tables and 29 functions in the `nhill` schema, 15 migrations.

## Principles

* Two roles (`db/bootstrap/00-roles.sql`): `nhill_owner` owns the schema and applies the migrations, it is never used by the API; `nhill_app` is the running server's role: it owns nothing, has no definition (DDL) rights and remains subject to row-level security.
* Business isolation: the business-management tables have row-level security **enabled and forced**; the server sets `app.establishment_id` in each transaction and the policies only show that business's rows. Platform tables are read by the server's queries with only the rights the application role needs (see each table).
* Amounts: `bigint` in minor units with a `char(3)` currency; rates: integer basis points; quantities: `bigint` in thousandths; timestamps `timestamptz` (UTC).
* Migrations: files `db/migrations/NNNN_name.sql`, applied in order by `nhill-server migrate`; each applied migration is recorded with its checksum and a migration modified afterwards is refused; a server refuses a database migrated by a newer version.
* Erasing a business: function `nh_erase_establishment` (table owner, 90 days after access ends; see the console).

## Migrations

| Migration | Purpose |
|---|---|
| `0001_platform` | Platform tables: businesses, identity, access, subscriptions |
| `0002_business` | Business tables; composite foreign keys (business, id) so that a row never points to another business's data |
| `0003_security` | Row-level security and application-role privileges, with no bypass for platform administrators |
| `0004_reference_data` | Real reference data: ISO 4217 currencies and initial platform settings |
| `0005_helpers` | API helper functions (ISO 8601 UTC dates, etc.) |
| `0006_activity_and_platform_stats` | Activity log per employee; platform statistics limited to aggregates |
| `0007_local_time` | Each business's local time to date documents, sessions and reports |
| `0008_console_billing` | Console and billing: prices per duration and currency, plan changes, promo codes, payment providers, support, errors, notifications |
| `0009_pos` | Nhill POS: tills, cashiers, payment methods, sessions, cash movements, approvals, sales promo codes |
| `0010_fk_indexes` | Indexes on the foreign keys of frequent queries |
| `0011_password_recovery` | Forgotten password and temporary password (2.1.0) |
| `0012_plan_entitlements` | Configurable plans (3.0.0): catalogue of features and limits, values per plan, terms, assignment rules, computed access |
| `0013_documents` | Sales documents (3.0.0): the business's notices, invoices issued at the till with the same numbering |
| `0014_establishment_erasure` | Permanent erasure of a business, billing archive, retention clean-ups |
| `0015_search_patterns` | Searches: `%` and `_` taken literally |

## Platform and identity

### `establishments`

Businesses (customer companies): name, country, main currency, status.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Triggers: `establishments_touch`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `kind` | `text` | yes | — |
| `trade_name` | `text` | yes | — |
| `legal_name` | `text` | — | — |
| `country_code` | `character(2)` | yes | — |
| `city` | `text` | — | — |
| `address` | `text` | — | — |
| `sector` | `text` | — | — |
| `preferred_language` | `text` | yes | `'fr'::text` |
| `main_currency` | `character(3)` | yes | — |
| `email` | `text` | — | — |
| `phone` | `text` | — | — |
| `website` | `text` | — | — |
| `vat_registered` | `boolean` | yes | `false` |
| `compliance_profile_code` | `text` | yes | — |
| `logo_object_id` | `uuid` | — | — |
| `status` | `text` | yes | `'active'::text` |
| `status_reason` | `text` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `updated_at` | `timestamp with time zone` | yes | `now()` |
| `timezone` | `text` | yes | `'UTC'::text` |

Constraints: `FOREIGN KEY (main_currency) REFERENCES nhill.currencies(code)`; `PRIMARY KEY (id)`; 6 `CHECK` constraint(s)

### `establishment_identifiers`

A business's legal identifiers according to the country profile.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: DELETE, INSERT, SELECT

| Column | Type | Not null | Default |
|---|---|---|---|
| `establishment_id` | `uuid` | yes | — |
| `kind` | `text` | yes | — |
| `country_code` | `character(2)` | yes | — |
| `value` | `text` | yes | — |
| `normalized_value` | `text` | yes | — |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `PRIMARY KEY (establishment_id, kind)`; `UNIQUE (country_code, kind, normalized_value)`; 1 `CHECK` constraint(s)

### `establishment_settings`

A business's document settings (language, terms, rounding, notices).

* Row-level security: enabled and forced
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Policies: `establishment_settings_tenant`

| Column | Type | Not null | Default |
|---|---|---|---|
| `establishment_id` | `uuid` | yes | — |
| `allow_negative_stock` | `boolean` | yes | `false` |
| `tax_rounding` | `text` | yes | `'per_line'::text` |
| `default_payment_terms_days` | `integer` | yes | `0` |
| `document_language` | `text` | yes | `'fr'::text` |
| `invoice_footer` | `text` | yes | `''::text` |
| `payment_instructions` | `text` | yes | `''::text` |
| `updated_at` | `timestamp with time zone` | yes | `now()` |
| `document_mentions` | `text` | yes | `''::text` |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `PRIMARY KEY (establishment_id)`; 4 `CHECK` constraint(s)

### `establishment_codes`

Business codes: HMAC fingerprint for checking, encrypted copy for display to the owner.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `code_hmac` | `bytea` | yes | — |
| `code_cipher` | `bytea` | yes | — |
| `code_nonce` | `bytea` | yes | — |
| `status` | `text` | yes | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `expires_at` | `timestamp with time zone` | yes | — |
| `ended_at` | `timestamp with time zone` | — | — |
| `ended_reason` | `text` | — | — |
| `created_by_type` | `text` | yes | — |
| `created_by` | `uuid` | — | — |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `PRIMARY KEY (id)`; `UNIQUE (code_hmac)`; 3 `CHECK` constraint(s)

### `users`

User accounts (Argon2id password hash).

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Triggers: `users_touch`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `email` | `text` | yes | — |
| `password_hash` | `text` | yes | — |
| `full_name` | `text` | yes | — |
| `phone` | `text` | — | — |
| `preferred_language` | `text` | yes | `'fr'::text` |
| `status` | `text` | yes | `'active'::text` |
| `password_changed_at` | `timestamp with time zone` | yes | `now()` |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `updated_at` | `timestamp with time zone` | yes | `now()` |
| `must_change_password` | `boolean` | yes | `false` |
| `temporary_password_expires_at` | `timestamp with time zone` | — | — |

Constraints: `PRIMARY KEY (id)`; `UNIQUE (email)`; 4 `CHECK` constraint(s)

### `memberships`

A user's membership of a business, with their role.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `user_id` | `uuid` | yes | — |
| `role` | `text` | yes | — |
| `status` | `text` | yes | `'active'::text` |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `FOREIGN KEY (user_id) REFERENCES nhill.users(id) ON DELETE CASCADE`; `PRIMARY KEY (id)`; `UNIQUE (establishment_id, user_id)`; 2 `CHECK` constraint(s)

### `invitations`

Pending invitations (activation code stored as a hash).

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `email` | `text` | yes | — |
| `role` | `text` | yes | — |
| `activation_hash` | `bytea` | yes | — |
| `expires_at` | `timestamp with time zone` | yes | — |
| `accepted_at` | `timestamp with time zone` | — | — |
| `revoked_at` | `timestamp with time zone` | — | — |
| `created_by` | `uuid` | yes | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (created_by) REFERENCES nhill.users(id)`; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `PRIMARY KEY (id)`; 2 `CHECK` constraint(s)

### `join_tickets`

Short-lived tickets issued after a business code check.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: DELETE, INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `ticket_hash` | `bytea` | yes | — |
| `establishment_id` | `uuid` | yes | — |
| `expires_at` | `timestamp with time zone` | yes | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `PRIMARY KEY (ticket_hash)`

### `attachment_requests`

Requests to join an existing company.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `requester_name` | `text` | yes | — |
| `requester_email` | `text` | yes | — |
| `requester_phone` | `text` | — | — |
| `message` | `text` | — | — |
| `status` | `text` | yes | `'open'::text` |
| `resolution_note` | `text` | — | — |
| `resolved_by` | `uuid` | — | — |
| `resolved_at` | `timestamp with time zone` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id)`; `PRIMARY KEY (id)`; 1 `CHECK` constraint(s)

### `sessions`

Application sessions (hashes of access and refresh tokens).

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: DELETE, INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `user_id` | `uuid` | yes | — |
| `establishment_id` | `uuid` | yes | — |
| `access_hash` | `bytea` | yes | — |
| `access_expires_at` | `timestamp with time zone` | yes | — |
| `refresh_hash` | `bytea` | yes | — |
| `previous_refresh_hash` | `bytea` | — | — |
| `refresh_expires_at` | `timestamp with time zone` | yes | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `last_seen_at` | `timestamp with time zone` | yes | `now()` |
| `revoked_at` | `timestamp with time zone` | — | — |
| `revoke_reason` | `text` | — | — |
| `user_agent` | `text` | — | — |
| `ip` | `text` | — | — |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `FOREIGN KEY (user_id) REFERENCES nhill.users(id) ON DELETE CASCADE`; `PRIMARY KEY (id)`; `UNIQUE (access_hash)`; `UNIQUE (refresh_hash)`

### `password_resets`

Reset codes and password-change tickets (hashes).

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: DELETE, INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `user_id` | `uuid` | yes | — |
| `purpose` | `text` | yes | — |
| `secret_hash` | `bytea` | yes | — |
| `expires_at` | `timestamp with time zone` | yes | — |
| `attempts` | `integer` | yes | `0` |
| `used_at` | `timestamp with time zone` | — | — |
| `revoked_at` | `timestamp with time zone` | — | — |
| `ip` | `text` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (user_id) REFERENCES nhill.users(id) ON DELETE CASCADE`; `PRIMARY KEY (id)`; 2 `CHECK` constraint(s)

### `auth_throttle`

Rate-limiting counters shared by the server instances.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: DELETE, INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `bucket` | `text` | yes | — |
| `window_start` | `timestamp with time zone` | yes | — |
| `hits` | `integer` | yes | `0` |

Constraints: `PRIMARY KEY (bucket, window_start)`

### `trial_registry`

HMAC fingerprints of identifiers that already had a trial; survives the business's erasure.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `fingerprint` | `bytea` | yes | — |
| `establishment_id` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `PRIMARY KEY (fingerprint)`

### `compliance_profiles`

Loaded country profiles (identifiers, taxes, notices, numbering), versioned.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `code` | `text` | yes | — |
| `version` | `integer` | yes | — |
| `country_code` | `character(2)` | — | — |
| `definition` | `jsonb` | yes | — |
| `content_sha256` | `text` | yes | — |
| `validation_note` | `text` | yes | `''::text` |
| `loaded_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `PRIMARY KEY (code, version)`; 2 `CHECK` constraint(s)

### `currencies`

ISO 4217 currencies and number of decimals.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `code` | `character(3)` | yes | — |
| `minor_units` | `smallint` | yes | — |
| `name_fr` | `text` | yes | — |
| `name_en` | `text` | yes | — |
| `active` | `boolean` | yes | `true` |

Constraints: `PRIMARY KEY (code)`; 2 `CHECK` constraint(s)

### `notifications`

Notifications for users.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `user_id` | `uuid` | — | — |
| `kind` | `text` | yes | — |
| `params` | `jsonb` | yes | `'{}'::jsonb` |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `read_at` | `timestamp with time zone` | — | — |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `FOREIGN KEY (user_id) REFERENCES nhill.users(id) ON DELETE CASCADE`; `PRIMARY KEY (id)`

### `email_outbox`

Queue of automatic e-mails; sent or failed messages deleted after 30 days.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: DELETE, INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `to_address` | `text` | yes | — |
| `template` | `text` | yes | — |
| `language` | `text` | yes | `'fr'::text` |
| `params` | `jsonb` | yes | `'{}'::jsonb` |
| `status` | `text` | yes | `'pending'::text` |
| `attempts` | `integer` | yes | `0` |
| `last_error` | `text` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `sent_at` | `timestamp with time zone` | — | — |

Constraints: `PRIMARY KEY (id)`; 1 `CHECK` constraint(s)

### `schema_migrations`

Applied migrations and their checksum (a modified migration is refused).

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: SELECT

| Column | Type | Not null | Default |
|---|---|---|---|
| `version` | `text` | yes | — |
| `checksum` | `text` | yes | — |
| `applied_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `PRIMARY KEY (version)`

## Plans, rights and subscriptions

### `plans`

Plans: audience, tier, terms (trial, grace, reminders, renewal, fallback plan).

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Triggers: `plans_legacy_to_entitlements`, `plans_main_price`, `plans_touch`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `code` | `text` | yes | — |
| `name_fr` | `text` | yes | — |
| `name_en` | `text` | yes | — |
| `description_fr` | `text` | yes | `''::text` |
| `description_en` | `text` | yes | `''::text` |
| `billing_period` | `text` | yes | — |
| `price_minor` | `bigint` | yes | — |
| `currency` | `character(3)` | yes | — |
| `tax_rate_bp` | `integer` | yes | `0` |
| `max_users` | `integer` | — | — |
| `audience` | `text` | yes | `'any'::text` |
| `features` | `jsonb` | yes | `'{}'::jsonb` |
| `active` | `boolean` | yes | `true` |
| `sort_order` | `integer` | yes | `0` |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `updated_at` | `timestamp with time zone` | yes | `now()` |
| `tier` | `integer` | yes | `0` |
| `max_registers` | `integer` | — | — |
| `max_warehouses` | `integer` | — | — |
| `kind` | `text` | yes | `'paid'::text` |
| `visibility` | `text` | yes | `'public'::text` |
| `self_service` | `boolean` | yes | `true` |
| `auto_renew_allowed` | `boolean` | yes | `true` |
| `grace_days` | `integer` | — | — |
| `reminder_days` | `integer[]` | — | — |
| `trial_days` | `integer` | — | — |
| `expiry_fallback_plan_id` | `uuid` | — | — |
| `archived_at` | `timestamp with time zone` | — | — |

Constraints: `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)`; `FOREIGN KEY (expiry_fallback_plan_id) REFERENCES nhill.plans(id)`; `PRIMARY KEY (id)`; `UNIQUE (code)`; 14 `CHECK` constraint(s)

### `plan_prices`

A plan's prices per duration (1 to 36 months) and currency.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: DELETE, INSERT, SELECT, UPDATE
* Triggers: `plan_prices_touch`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `plan_id` | `uuid` | yes | — |
| `duration_months` | `integer` | yes | — |
| `currency` | `character(3)` | yes | — |
| `price_minor` | `bigint` | yes | — |
| `active` | `boolean` | yes | `true` |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `updated_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)`; `FOREIGN KEY (plan_id) REFERENCES nhill.plans(id) ON DELETE CASCADE`; `PRIMARY KEY (id)`; `UNIQUE (plan_id, duration_months, currency)`; 2 `CHECK` constraint(s)

### `plan_capabilities`

Catalogue of features and limits, default and trial values, FR/EN labels.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: SELECT

| Column | Type | Not null | Default |
|---|---|---|---|
| `key` | `text` | yes | — |
| `kind` | `text` | yes | — |
| `label_fr` | `text` | yes | — |
| `label_en` | `text` | yes | — |
| `description_fr` | `text` | yes | `''::text` |
| `description_en` | `text` | yes | `''::text` |
| `unit` | `text` | — | — |
| `default_enabled` | `boolean` | — | — |
| `default_limit` | `integer` | — | — |
| `trial_enabled` | `boolean` | — | — |
| `trial_limit` | `integer` | — | — |
| `sort_order` | `integer` | yes | `0` |

Constraints: `PRIMARY KEY (key)`; 5 `CHECK` constraint(s)

### `plan_entitlements`

Each feature's or limit's value for a plan.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: DELETE, INSERT, SELECT, UPDATE
* Triggers: `plan_entitlements_check`, `plan_entitlements_legacy`

| Column | Type | Not null | Default |
|---|---|---|---|
| `plan_id` | `uuid` | yes | — |
| `key` | `text` | yes | — |
| `enabled` | `boolean` | — | — |
| `limit_value` | `integer` | — | — |
| `updated_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (key) REFERENCES nhill.plan_capabilities(key)`; `FOREIGN KEY (plan_id) REFERENCES nhill.plans(id) ON DELETE CASCADE`; `PRIMARY KEY (plan_id, key)`; 1 `CHECK` constraint(s)

### `plan_assignment_rules`

Rules assigning a plan when a business is created (trial or grant).

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: DELETE, INSERT, SELECT, UPDATE
* Triggers: `plan_assignment_rules_touch`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `priority` | `integer` | yes | `100` |
| `active` | `boolean` | yes | `true` |
| `audience` | `text` | yes | `'any'::text` |
| `country_code` | `character(2)` | — | — |
| `plan_id` | `uuid` | yes | — |
| `mode` | `text` | yes | — |
| `duration_days` | `integer` | — | — |
| `description` | `text` | yes | `''::text` |
| `created_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `updated_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (created_by) REFERENCES nhill.admin_users(id)`; `FOREIGN KEY (plan_id) REFERENCES nhill.plans(id)`; `PRIMARY KEY (id)`; 6 `CHECK` constraint(s)

### `subscriptions`

Each business's subscription: plan, period, status, access dates.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Triggers: `subscriptions_touch`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `plan_id` | `uuid` | — | — |
| `pending_plan_id` | `uuid` | — | — |
| `status` | `text` | yes | — |
| `is_trial` | `boolean` | yes | `false` |
| `period_start` | `timestamp with time zone` | yes | — |
| `period_end` | `timestamp with time zone` | yes | — |
| `grace_until` | `timestamp with time zone` | — | — |
| `reminder_sent_for` | `timestamp with time zone` | — | — |
| `auto_renew_consent_at` | `timestamp with time zone` | — | — |
| `canceled_at` | `timestamp with time zone` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `updated_at` | `timestamp with time zone` | yes | `now()` |
| `price_id` | `uuid` | — | — |
| `pending_price_id` | `uuid` | — | — |
| `pending_from` | `timestamp with time zone` | — | — |
| `failed_payments` | `integer` | yes | `0` |
| `last_failure_at` | `timestamp with time zone` | — | — |
| `open_ended` | `boolean` | yes | `false` |
| `suspension_reason` | `text` | — | — |
| `assignment_rule_id` | `uuid` | — | — |

Constraints: `FOREIGN KEY (assignment_rule_id) REFERENCES nhill.plan_assignment_rules(id) ON DELETE SET NULL`; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `FOREIGN KEY (pending_plan_id) REFERENCES nhill.plans(id)`; `FOREIGN KEY (pending_price_id) REFERENCES nhill.plan_prices(id)`; `FOREIGN KEY (plan_id) REFERENCES nhill.plans(id)`; `FOREIGN KEY (price_id) REFERENCES nhill.plan_prices(id)`; `PRIMARY KEY (id)`; `UNIQUE (establishment_id)`; 4 `CHECK` constraint(s)

### `subscription_events`

History of subscription changes.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `bigint` | yes | identity |
| `subscription_id` | `uuid` | yes | — |
| `establishment_id` | `uuid` | yes | — |
| `kind` | `text` | yes | — |
| `from_status` | `text` | — | — |
| `to_status` | `text` | — | — |
| `period_start` | `timestamp with time zone` | — | — |
| `period_end` | `timestamp with time zone` | — | — |
| `details` | `jsonb` | yes | `'{}'::jsonb` |
| `actor_type` | `text` | yes | — |
| `actor_id` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (subscription_id) REFERENCES nhill.subscriptions(id) ON DELETE CASCADE`; `PRIMARY KEY (id)`; 1 `CHECK` constraint(s)

### `subscription_reminders`

Due-date reminders already sent (no duplicates).

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT

| Column | Type | Not null | Default |
|---|---|---|---|
| `subscription_id` | `uuid` | yes | — |
| `period_end` | `timestamp with time zone` | yes | — |
| `days_before` | `integer` | yes | — |
| `sent_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (subscription_id) REFERENCES nhill.subscriptions(id) ON DELETE CASCADE`; `PRIMARY KEY (subscription_id, period_end, days_before)`; 1 `CHECK` constraint(s)

### `subscription_orders`

Subscription orders and their payment.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `plan_id` | `uuid` | yes | — |
| `amount_minor` | `bigint` | yes | — |
| `currency` | `character(3)` | yes | — |
| `status` | `text` | yes | `'pending'::text` |
| `provider` | `text` | — | — |
| `provider_reference` | `text` | — | — |
| `created_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `paid_at` | `timestamp with time zone` | — | — |
| `kind` | `text` | yes | `'renewal'::text` |
| `price_id` | `uuid` | — | — |
| `duration_months` | `integer` | — | — |
| `subtotal_minor` | `bigint` | — | — |
| `discount_minor` | `bigint` | yes | `0` |
| `credit_minor` | `bigint` | yes | `0` |
| `tax_minor` | `bigint` | yes | `0` |
| `promo_code_id` | `uuid` | — | — |
| `provider_id` | `uuid` | — | — |
| `provider_session` | `text` | — | — |
| `checkout_url` | `text` | — | — |
| `expires_at` | `timestamp with time zone` | — | — |
| `failure_reason` | `text` | — | — |

Constraints: `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)`; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `FOREIGN KEY (plan_id) REFERENCES nhill.plans(id)`; `FOREIGN KEY (price_id) REFERENCES nhill.plan_prices(id)`; `FOREIGN KEY (promo_code_id) REFERENCES nhill.promo_codes(id)`; `FOREIGN KEY (provider_id) REFERENCES nhill.payment_providers(id)`; `PRIMARY KEY (id)`; `UNIQUE (provider, provider_reference)`; 6 `CHECK` constraint(s)

### `promo_codes`

Subscription promo codes.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Triggers: `promo_codes_touch`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `code` | `text` | yes | — |
| `description` | `text` | yes | `''::text` |
| `discount_kind` | `text` | yes | — |
| `percent_bp` | `integer` | — | — |
| `amount_minor` | `bigint` | — | — |
| `currency` | `character(3)` | — | — |
| `valid_from` | `timestamp with time zone` | yes | `now()` |
| `valid_until` | `timestamp with time zone` | — | — |
| `max_redemptions` | `integer` | — | — |
| `max_per_establishment` | `integer` | yes | `1` |
| `plan_ids` | `uuid[]` | — | — |
| `min_duration_months` | `integer` | — | — |
| `new_customers_only` | `boolean` | yes | `false` |
| `audience` | `text` | yes | `'any'::text` |
| `active` | `boolean` | yes | `true` |
| `created_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `updated_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (created_by) REFERENCES nhill.admin_users(id)`; `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)`; `PRIMARY KEY (id)`; `UNIQUE (code)`; 10 `CHECK` constraint(s)

### `promo_redemptions`

Uses of subscription promo codes.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `promo_code_id` | `uuid` | yes | — |
| `establishment_id` | `uuid` | yes | — |
| `order_id` | `uuid` | yes | — |
| `status` | `text` | yes | `'reserved'::text` |
| `discount_minor` | `bigint` | yes | — |
| `currency` | `character(3)` | yes | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `settled_at` | `timestamp with time zone` | — | — |

Constraints: `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)`; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `FOREIGN KEY (order_id) REFERENCES nhill.subscription_orders(id) ON DELETE CASCADE`; `FOREIGN KEY (promo_code_id) REFERENCES nhill.promo_codes(id)`; `PRIMARY KEY (id)`; `UNIQUE (order_id)`; 2 `CHECK` constraint(s)

### `payment_providers`

Configured payment providers (encrypted secrets).

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Triggers: `payment_providers_touch`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `code` | `text` | yes | — |
| `kind` | `text` | yes | — |
| `display_name` | `text` | yes | — |
| `environment` | `text` | yes | `'test'::text` |
| `enabled` | `boolean` | yes | `false` |
| `currencies` | `character(3)[]` | yes | `'{}'::bpchar[]` |
| `settings` | `jsonb` | yes | `'{}'::jsonb` |
| `secret_cipher` | `bytea` | — | — |
| `secret_nonce` | `bytea` | — | — |
| `secret_fields` | `text[]` | yes | `'{}'::text[]` |
| `sort_order` | `integer` | yes | `0` |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `updated_at` | `timestamp with time zone` | yes | `now()` |
| `updated_by` | `uuid` | — | — |

Constraints: `FOREIGN KEY (updated_by) REFERENCES nhill.admin_users(id)`; `PRIMARY KEY (id)`; `UNIQUE (code)`; 5 `CHECK` constraint(s)

### `payment_webhook_events`

Notifications received from providers (deduplication).

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `bigint` | yes | identity |
| `provider` | `text` | yes | — |
| `event_id` | `text` | yes | — |
| `received_at` | `timestamp with time zone` | yes | `now()` |
| `payload` | `jsonb` | yes | — |
| `processed_at` | `timestamp with time zone` | — | — |
| `outcome` | `text` | — | — |

Constraints: `PRIMARY KEY (id)`; `UNIQUE (provider, event_id)`

### `billing_archive`

Paid or refunded orders of an erased business, kept 10 years.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: SELECT

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `bigint` | yes | identity |
| `establishment_id` | `uuid` | yes | — |
| `trade_name` | `text` | yes | — |
| `legal_name` | `text` | — | — |
| `country_code` | `character(2)` | — | — |
| `order_id` | `uuid` | yes | — |
| `plan_code` | `text` | — | — |
| `kind` | `text` | — | — |
| `duration_months` | `integer` | — | — |
| `subtotal_minor` | `bigint` | — | — |
| `discount_minor` | `bigint` | — | — |
| `tax_minor` | `bigint` | — | — |
| `amount_minor` | `bigint` | yes | — |
| `currency` | `character(3)` | yes | — |
| `status` | `text` | yes | — |
| `provider` | `text` | — | — |
| `provider_reference` | `text` | — | — |
| `created_at` | `timestamp with time zone` | yes | — |
| `paid_at` | `timestamp with time zone` | — | — |
| `archived_at` | `timestamp with time zone` | yes | `now()` |
| `keep_until` | `date` | yes | — |

Constraints: `PRIMARY KEY (id)`; `UNIQUE (order_id)`

## Business management (businesses' data)

### `tax_rates`

A business's tax rates (the rate never changes).

* Row-level security: enabled and forced
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Policies: `tax_rates_tenant`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `code` | `text` | yes | — |
| `label` | `text` | yes | — |
| `rate_bp` | `integer` | yes | — |
| `kind` | `text` | yes | `'standard'::text` |
| `is_default` | `boolean` | yes | `false` |
| `active` | `boolean` | yes | `true` |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `PRIMARY KEY (id)`; `UNIQUE (establishment_id, code)`; `UNIQUE (establishment_id, id)`; 3 `CHECK` constraint(s)

### `product_categories`

Product categories.

* Row-level security: enabled and forced
* `nhill_app` rights: DELETE, INSERT, SELECT, UPDATE
* Policies: `product_categories_tenant`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `name` | `text` | yes | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `PRIMARY KEY (id)`; `UNIQUE (establishment_id, id)`; 1 `CHECK` constraint(s)

### `products`

Products and services: price, tax, unit, barcode, alert threshold.

* Row-level security: enabled and forced
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Policies: `products_tenant`
* Triggers: `products_touch`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `kind` | `text` | yes | `'good'::text` |
| `sku` | `text` | yes | — |
| `barcode` | `text` | — | — |
| `name` | `text` | yes | — |
| `description` | `text` | yes | `''::text` |
| `category_id` | `uuid` | — | — |
| `unit` | `text` | yes | `'unit'::text` |
| `currency` | `character(3)` | yes | — |
| `purchase_price_minor` | `bigint` | yes | `0` |
| `sale_price_minor` | `bigint` | yes | `0` |
| `tax_rate_id` | `uuid` | — | — |
| `track_stock` | `boolean` | yes | `true` |
| `reorder_level_milli` | `bigint` | yes | `0` |
| `active` | `boolean` | yes | `true` |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `updated_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)`; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `FOREIGN KEY (establishment_id, category_id) REFERENCES nhill.product_categories(establishment_id, id)`; `FOREIGN KEY (establishment_id, tax_rate_id) REFERENCES nhill.tax_rates(establishment_id, id)`; `PRIMARY KEY (id)`; `UNIQUE (establishment_id, id)`; `UNIQUE (establishment_id, sku)`; 9 `CHECK` constraint(s)

### `parties`

Customers and suppliers.

* Row-level security: enabled and forced
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Policies: `parties_tenant`
* Triggers: `parties_touch`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `is_customer` | `boolean` | yes | `false` |
| `is_supplier` | `boolean` | yes | `false` |
| `kind` | `text` | yes | `'company'::text` |
| `name` | `text` | yes | — |
| `email` | `text` | — | — |
| `phone` | `text` | — | — |
| `address` | `text` | yes | `''::text` |
| `city` | `text` | yes | `''::text` |
| `country_code` | `character(2)` | — | — |
| `tax_ids` | `jsonb` | yes | `'{}'::jsonb` |
| `payment_terms_days` | `integer` | yes | `0` |
| `notes` | `text` | yes | `''::text` |
| `active` | `boolean` | yes | `true` |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `updated_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `PRIMARY KEY (id)`; `UNIQUE (establishment_id, id)`; 4 `CHECK` constraint(s)

### `warehouses`

Warehouses; the default warehouse is the main one.

* Row-level security: enabled and forced
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Policies: `warehouses_tenant`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `name` | `text` | yes | — |
| `is_default` | `boolean` | yes | `false` |
| `active` | `boolean` | yes | `true` |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `PRIMARY KEY (id)`; `UNIQUE (establishment_id, id)`

### `stock_levels`

Quantity in stock per product and warehouse.

* Row-level security: enabled and forced
* `nhill_app` rights: SELECT
* Policies: `stock_levels_tenant`

| Column | Type | Not null | Default |
|---|---|---|---|
| `establishment_id` | `uuid` | yes | — |
| `product_id` | `uuid` | yes | — |
| `warehouse_id` | `uuid` | yes | — |
| `quantity_milli` | `bigint` | yes | `0` |
| `updated_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (establishment_id, product_id) REFERENCES nhill.products(establishment_id, id)`; `FOREIGN KEY (establishment_id, warehouse_id) REFERENCES nhill.warehouses(establishment_id, id)`; `PRIMARY KEY (establishment_id, product_id, warehouse_id)`

### `stock_movements`

Stock movements, with their author and origin.

* Row-level security: enabled and forced
* `nhill_app` rights: INSERT, SELECT
* Policies: `stock_movements_tenant`
* Triggers: `stock_movements_apply`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `bigint` | yes | identity |
| `establishment_id` | `uuid` | yes | — |
| `product_id` | `uuid` | yes | — |
| `warehouse_id` | `uuid` | yes | — |
| `quantity_milli` | `bigint` | yes | — |
| `kind` | `text` | yes | — |
| `source_type` | `text` | — | — |
| `source_id` | `uuid` | — | — |
| `note` | `text` | yes | `''::text` |
| `created_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (establishment_id, product_id) REFERENCES nhill.products(establishment_id, id)`; `FOREIGN KEY (establishment_id, warehouse_id) REFERENCES nhill.warehouses(establishment_id, id)`; `PRIMARY KEY (id)`; 2 `CHECK` constraint(s)

### `sales_documents`

Sales documents: quotes, orders, invoices, credit notes, till receipts.

* Row-level security: enabled and forced
* `nhill_app` rights: DELETE, INSERT, SELECT, UPDATE
* Policies: `sales_documents_tenant`
* Triggers: `sales_documents_guard`, `sales_documents_usage`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `doc_type` | `text` | yes | — |
| `status` | `text` | yes | `'draft'::text` |
| `number` | `text` | — | — |
| `sequence_value` | `bigint` | — | — |
| `party_id` | `uuid` | yes | — |
| `currency` | `character(3)` | yes | — |
| `language` | `text` | yes | `'fr'::text` |
| `issue_date` | `date` | yes | `nhill.nh_local_today()` |
| `due_date` | `date` | — | — |
| `valid_until` | `date` | — | — |
| `main_currency` | `character(3)` | yes | — |
| `fx_rate` | `numeric(24,10)` | — | — |
| `fx_rate_date` | `date` | — | — |
| `fx_rate_source` | `text` | — | — |
| `tax_rounding` | `text` | yes | `'per_line'::text` |
| `gross_minor` | `bigint` | yes | `0` |
| `discount_minor` | `bigint` | yes | `0` |
| `net_minor` | `bigint` | yes | `0` |
| `tax_minor` | `bigint` | yes | `0` |
| `total_minor` | `bigint` | yes | `0` |
| `paid_minor` | `bigint` | yes | `0` |
| `credited_minor` | `bigint` | yes | `0` |
| `tax_breakdown` | `jsonb` | yes | `'[]'::jsonb` |
| `notes` | `text` | yes | `''::text` |
| `terms` | `text` | yes | `''::text` |
| `related_document_id` | `uuid` | — | — |
| `restock` | `boolean` | yes | `false` |
| `seller_snapshot` | `jsonb` | — | — |
| `buyer_snapshot` | `jsonb` | — | — |
| `profile_code` | `text` | — | — |
| `profile_version` | `integer` | — | — |
| `fiscal_reference` | `jsonb` | — | — |
| `pdf_sha256` | `text` | — | — |
| `created_by` | `uuid` | — | — |
| `issued_by` | `uuid` | — | — |
| `issued_at` | `timestamp with time zone` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `updated_at` | `timestamp with time zone` | yes | `now()` |
| `pos_session_id` | `uuid` | — | — |

Constraints: `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)`; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `FOREIGN KEY (establishment_id, party_id) REFERENCES nhill.parties(establishment_id, id)`; `FOREIGN KEY (establishment_id, pos_session_id) REFERENCES nhill.pos_sessions(establishment_id, id)`; `FOREIGN KEY (establishment_id, related_document_id) REFERENCES nhill.sales_documents(establishment_id, id)`; `FOREIGN KEY (main_currency) REFERENCES nhill.currencies(code)`; `PRIMARY KEY (id)`; `UNIQUE (establishment_id, doc_type, number)`; `UNIQUE (establishment_id, id)`; 10 `CHECK` constraint(s)

### `sales_document_lines`

Lines of sales documents.

* Row-level security: enabled and forced
* `nhill_app` rights: DELETE, INSERT, SELECT, UPDATE
* Policies: `sales_document_lines_tenant`
* Triggers: `sales_document_lines_guard`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `document_id` | `uuid` | yes | — |
| `line_no` | `integer` | yes | — |
| `product_id` | `uuid` | — | — |
| `description` | `text` | yes | — |
| `quantity_milli` | `bigint` | yes | — |
| `unit` | `text` | yes | `'unit'::text` |
| `unit_price_minor` | `bigint` | yes | — |
| `discount_bp` | `integer` | yes | `0` |
| `tax_rate_bp` | `integer` | yes | `0` |
| `tax_code` | `text` | yes | `''::text` |
| `gross_minor` | `bigint` | yes | — |
| `discount_minor` | `bigint` | yes | — |
| `net_minor` | `bigint` | yes | — |
| `tax_minor` | `bigint` | yes | — |
| `total_minor` | `bigint` | yes | — |

Constraints: `FOREIGN KEY (establishment_id, document_id) REFERENCES nhill.sales_documents(establishment_id, id) ON DELETE CASCADE`; `FOREIGN KEY (establishment_id, product_id) REFERENCES nhill.products(establishment_id, id)`; `PRIMARY KEY (id)`; `UNIQUE (document_id, line_no)`; 6 `CHECK` constraint(s)

### `document_status_history`

History of document statuses.

* Row-level security: enabled and forced
* `nhill_app` rights: INSERT, SELECT
* Policies: `document_status_history_tenant`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `bigint` | yes | identity |
| `establishment_id` | `uuid` | yes | — |
| `document_id` | `uuid` | yes | — |
| `from_status` | `text` | — | — |
| `to_status` | `text` | yes | — |
| `actor_id` | `uuid` | — | — |
| `note` | `text` | yes | `''::text` |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (establishment_id, document_id) REFERENCES nhill.sales_documents(establishment_id, id) ON DELETE CASCADE`; `PRIMARY KEY (id)`

### `document_sequences`

Numbering counters per document type and period.

* Row-level security: enabled and forced
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Policies: `document_sequences_tenant`

| Column | Type | Not null | Default |
|---|---|---|---|
| `establishment_id` | `uuid` | yes | — |
| `doc_type` | `text` | yes | — |
| `period_key` | `text` | yes | — |
| `next_value` | `bigint` | yes | — |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `PRIMARY KEY (establishment_id, doc_type, period_key)`; 1 `CHECK` constraint(s)

### `purchases`

Supplier purchases.

* Row-level security: enabled and forced
* `nhill_app` rights: DELETE, INSERT, SELECT, UPDATE
* Policies: `purchases_tenant`
* Triggers: `purchases_guard`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `supplier_id` | `uuid` | yes | — |
| `status` | `text` | yes | `'draft'::text` |
| `supplier_reference` | `text` | yes | `''::text` |
| `purchase_date` | `date` | yes | `nhill.nh_local_today()` |
| `due_date` | `date` | — | — |
| `currency` | `character(3)` | yes | — |
| `warehouse_id` | `uuid` | yes | — |
| `net_minor` | `bigint` | yes | `0` |
| `tax_minor` | `bigint` | yes | `0` |
| `total_minor` | `bigint` | yes | `0` |
| `paid_minor` | `bigint` | yes | `0` |
| `notes` | `text` | yes | `''::text` |
| `created_by` | `uuid` | — | — |
| `received_by` | `uuid` | — | — |
| `received_at` | `timestamp with time zone` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `updated_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)`; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `FOREIGN KEY (establishment_id, supplier_id) REFERENCES nhill.parties(establishment_id, id)`; `FOREIGN KEY (establishment_id, warehouse_id) REFERENCES nhill.warehouses(establishment_id, id)`; `PRIMARY KEY (id)`; `UNIQUE (establishment_id, id)`; 4 `CHECK` constraint(s)

### `purchase_lines`

Purchase lines.

* Row-level security: enabled and forced
* `nhill_app` rights: DELETE, INSERT, SELECT, UPDATE
* Policies: `purchase_lines_tenant`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `purchase_id` | `uuid` | yes | — |
| `line_no` | `integer` | yes | — |
| `product_id` | `uuid` | — | — |
| `description` | `text` | yes | — |
| `quantity_milli` | `bigint` | yes | — |
| `unit_cost_minor` | `bigint` | yes | — |
| `tax_rate_bp` | `integer` | yes | `0` |
| `net_minor` | `bigint` | yes | — |
| `tax_minor` | `bigint` | yes | — |
| `total_minor` | `bigint` | yes | — |

Constraints: `FOREIGN KEY (establishment_id, product_id) REFERENCES nhill.products(establishment_id, id)`; `FOREIGN KEY (establishment_id, purchase_id) REFERENCES nhill.purchases(establishment_id, id) ON DELETE CASCADE`; `PRIMARY KEY (id)`; `UNIQUE (purchase_id, line_no)`; 4 `CHECK` constraint(s)

### `expense_categories`

Expense categories.

* Row-level security: enabled and forced
* `nhill_app` rights: DELETE, INSERT, SELECT, UPDATE
* Policies: `expense_categories_tenant`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `name` | `text` | yes | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `PRIMARY KEY (id)`; `UNIQUE (establishment_id, id)`; 1 `CHECK` constraint(s)

### `expenses`

Expenses.

* Row-level security: enabled and forced
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Policies: `expenses_tenant`
* Triggers: `expenses_touch`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `category_id` | `uuid` | — | — |
| `supplier_id` | `uuid` | — | — |
| `description` | `text` | yes | — |
| `amount_minor` | `bigint` | yes | — |
| `tax_minor` | `bigint` | yes | `0` |
| `currency` | `character(3)` | yes | — |
| `expense_date` | `date` | yes | `nhill.nh_local_today()` |
| `due_date` | `date` | — | — |
| `paid_minor` | `bigint` | yes | `0` |
| `receipt_object_id` | `uuid` | — | — |
| `created_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `updated_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)`; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `FOREIGN KEY (establishment_id, category_id) REFERENCES nhill.expense_categories(establishment_id, id)`; `FOREIGN KEY (establishment_id, receipt_object_id) REFERENCES nhill.objects(establishment_id, id)`; `FOREIGN KEY (establishment_id, supplier_id) REFERENCES nhill.parties(establishment_id, id)`; `PRIMARY KEY (id)`; `UNIQUE (establishment_id, id)`; 6 `CHECK` constraint(s)

### `payments`

Payments received and made, linked to a document, purchase or expense.

* Row-level security: enabled and forced
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Policies: `payments_tenant`
* Triggers: `payments_usage`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `direction` | `text` | yes | — |
| `party_id` | `uuid` | — | — |
| `document_id` | `uuid` | — | — |
| `purchase_id` | `uuid` | — | — |
| `expense_id` | `uuid` | — | — |
| `method` | `text` | yes | — |
| `account_id` | `uuid` | — | — |
| `amount_minor` | `bigint` | yes | — |
| `currency` | `character(3)` | yes | — |
| `paid_at` | `timestamp with time zone` | yes | `now()` |
| `reference` | `text` | yes | `''::text` |
| `note` | `text` | yes | `''::text` |
| `created_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)`; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `FOREIGN KEY (establishment_id, account_id) REFERENCES nhill.cash_accounts(establishment_id, id)`; `FOREIGN KEY (establishment_id, document_id) REFERENCES nhill.sales_documents(establishment_id, id)`; `FOREIGN KEY (establishment_id, expense_id) REFERENCES nhill.expenses(establishment_id, id)`; `FOREIGN KEY (establishment_id, party_id) REFERENCES nhill.parties(establishment_id, id)`; `FOREIGN KEY (establishment_id, purchase_id) REFERENCES nhill.purchases(establishment_id, id)`; `PRIMARY KEY (id)`; `UNIQUE (establishment_id, id)`; 4 `CHECK` constraint(s)

### `cash_accounts`

Cash and bank accounts.

* Row-level security: enabled and forced
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Policies: `cash_accounts_tenant`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `name` | `text` | yes | — |
| `kind` | `text` | yes | — |
| `currency` | `character(3)` | yes | — |
| `active` | `boolean` | yes | `true` |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)`; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `PRIMARY KEY (id)`; `UNIQUE (establishment_id, id)`; 1 `CHECK` constraint(s)

### `exchange_rates`

Recorded exchange rates (date and source).

* Row-level security: enabled and forced
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Policies: `exchange_rates_tenant`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `from_currency` | `character(3)` | yes | — |
| `to_currency` | `character(3)` | yes | — |
| `rate` | `numeric(24,10)` | yes | — |
| `rate_date` | `date` | yes | — |
| `source` | `text` | yes | `''::text` |
| `created_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `FOREIGN KEY (from_currency) REFERENCES nhill.currencies(code)`; `FOREIGN KEY (to_currency) REFERENCES nhill.currencies(code)`; `PRIMARY KEY (id)`; `UNIQUE (establishment_id, from_currency, to_currency, rate_date)`; 2 `CHECK` constraint(s)

### `objects`

Stored files: logo, receipts (PDF, PNG, JPEG, 5 MB at most).

* Row-level security: enabled and forced
* `nhill_app` rights: DELETE, INSERT, SELECT, UPDATE
* Policies: `objects_tenant`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `purpose` | `text` | yes | — |
| `mime` | `text` | yes | — |
| `byte_size` | `integer` | yes | — |
| `sha256` | `text` | yes | — |
| `width` | `integer` | — | — |
| `height` | `integer` | — | — |
| `data` | `bytea` | yes | — |
| `created_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `PRIMARY KEY (id)`; `UNIQUE (establishment_id, id)`; 3 `CHECK` constraint(s)

### `idempotency_keys`

Idempotency keys and recorded answers, in the same transaction as the operation.

* Row-level security: enabled and forced
* `nhill_app` rights: DELETE, INSERT, SELECT, UPDATE
* Policies: `idempotency_keys_tenant`

| Column | Type | Not null | Default |
|---|---|---|---|
| `establishment_id` | `uuid` | yes | — |
| `idem_key` | `text` | yes | — |
| `user_id` | `uuid` | — | — |
| `request_hash` | `bytea` | yes | — |
| `status_code` | `integer` | — | — |
| `response_body` | `text` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `PRIMARY KEY (establishment_id, idem_key)`; 1 `CHECK` constraint(s)

### `audit_log`

Activity log, append-only.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `bigint` | yes | identity |
| `establishment_id` | `uuid` | — | — |
| `actor_type` | `text` | yes | — |
| `actor_id` | `uuid` | — | — |
| `action` | `text` | yes | — |
| `entity` | `text` | — | — |
| `entity_id` | `text` | — | — |
| `details` | `jsonb` | yes | `'{}'::jsonb` |
| `ip` | `text` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `PRIMARY KEY (id)`; 1 `CHECK` constraint(s)

## Till (Nhill POS)

### `pos_settings`

Till rules: walk-in customer, maximum discount, price override, receipt footer and width.

* Row-level security: enabled and forced
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Policies: `pos_settings_tenant`

| Column | Type | Not null | Default |
|---|---|---|---|
| `establishment_id` | `uuid` | yes | — |
| `walk_in_party_id` | `uuid` | — | — |
| `max_discount_bp` | `integer` | yes | `1000` |
| `allow_price_override` | `boolean` | yes | `false` |
| `receipt_footer` | `text` | yes | `''::text` |
| `receipt_width_mm` | `integer` | yes | `80` |
| `updated_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `FOREIGN KEY (establishment_id, walk_in_party_id) REFERENCES nhill.parties(establishment_id, id)`; `PRIMARY KEY (establishment_id)`; 2 `CHECK` constraint(s)

### `pos_registers`

Tills and their warehouse.

* Row-level security: enabled and forced
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Policies: `pos_registers_tenant`
* Triggers: `pos_registers_touch`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `code` | `text` | yes | — |
| `name` | `text` | yes | — |
| `location` | `text` | yes | `''::text` |
| `warehouse_id` | `uuid` | yes | — |
| `active` | `boolean` | yes | `true` |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `updated_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `FOREIGN KEY (establishment_id, warehouse_id) REFERENCES nhill.warehouses(establishment_id, id)`; `PRIMARY KEY (id)`; `UNIQUE (establishment_id, code)`; `UNIQUE (establishment_id, id)`; 2 `CHECK` constraint(s)

### `pos_register_users`

Authorised cashiers per till.

* Row-level security: enabled and forced
* `nhill_app` rights: DELETE, INSERT, SELECT
* Policies: `pos_register_users_tenant`

| Column | Type | Not null | Default |
|---|---|---|---|
| `establishment_id` | `uuid` | yes | — |
| `register_id` | `uuid` | yes | — |
| `user_id` | `uuid` | yes | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (establishment_id, register_id) REFERENCES nhill.pos_registers(establishment_id, id) ON DELETE CASCADE`; `FOREIGN KEY (user_id) REFERENCES nhill.users(id) ON DELETE CASCADE`; `PRIMARY KEY (register_id, user_id)`

### `pos_payment_methods`

Till payment methods.

* Row-level security: enabled and forced
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Policies: `pos_payment_methods_tenant`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `code` | `text` | yes | — |
| `label` | `text` | yes | — |
| `kind` | `text` | yes | — |
| `account_id` | `uuid` | — | — |
| `active` | `boolean` | yes | `true` |
| `sort_order` | `integer` | yes | `0` |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `FOREIGN KEY (establishment_id, account_id) REFERENCES nhill.cash_accounts(establishment_id, id)`; `PRIMARY KEY (id)`; `UNIQUE (establishment_id, code)`; `UNIQUE (establishment_id, id)`; 3 `CHECK` constraint(s)

### `pos_sessions`

Till sessions: opening, float, closing, count, difference.

* Row-level security: enabled and forced
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Policies: `pos_sessions_tenant`
* Triggers: `pos_sessions_guard`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `register_id` | `uuid` | yes | — |
| `number` | `integer` | yes | — |
| `status` | `text` | yes | `'open'::text` |
| `currency` | `character(3)` | yes | — |
| `opened_by` | `uuid` | yes | — |
| `opened_at` | `timestamp with time zone` | yes | `now()` |
| `opening_float_minor` | `bigint` | yes | `0` |
| `closed_by` | `uuid` | — | — |
| `closed_at` | `timestamp with time zone` | — | — |
| `expected` | `jsonb` | — | — |
| `counted` | `jsonb` | — | — |
| `cash_expected_minor` | `bigint` | — | — |
| `cash_counted_minor` | `bigint` | — | — |
| `cash_variance_minor` | `bigint` | — | — |
| `closing_note` | `text` | yes | `''::text` |

Constraints: `FOREIGN KEY (closed_by) REFERENCES nhill.users(id)`; `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)`; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `FOREIGN KEY (establishment_id, register_id) REFERENCES nhill.pos_registers(establishment_id, id)`; `FOREIGN KEY (opened_by) REFERENCES nhill.users(id)`; `PRIMARY KEY (id)`; `UNIQUE (establishment_id, id)`; `UNIQUE (register_id, number)`; 4 `CHECK` constraint(s)

### `pos_cash_movements`

Cash in and out during a session.

* Row-level security: enabled and forced
* `nhill_app` rights: INSERT, SELECT
* Policies: `pos_cash_movements_tenant`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `session_id` | `uuid` | yes | — |
| `kind` | `text` | yes | — |
| `amount_minor` | `bigint` | yes | — |
| `reason` | `text` | yes | — |
| `created_by` | `uuid` | yes | — |
| `approved_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (approved_by) REFERENCES nhill.users(id)`; `FOREIGN KEY (created_by) REFERENCES nhill.users(id)`; `FOREIGN KEY (establishment_id, session_id) REFERENCES nhill.pos_sessions(establishment_id, id)`; `PRIMARY KEY (id)`; 3 `CHECK` constraint(s)

### `pos_approvals`

Single-use supervisor approvals.

* Row-level security: enabled and forced
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Policies: `pos_approvals_tenant`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `token_hash` | `bytea` | yes | — |
| `action` | `text` | yes | — |
| `approver_id` | `uuid` | yes | — |
| `requested_by` | `uuid` | yes | — |
| `session_id` | `uuid` | — | — |
| `expires_at` | `timestamp with time zone` | yes | — |
| `used_at` | `timestamp with time zone` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (approver_id) REFERENCES nhill.users(id)`; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `FOREIGN KEY (establishment_id, session_id) REFERENCES nhill.pos_sessions(establishment_id, id)`; `FOREIGN KEY (requested_by) REFERENCES nhill.users(id)`; `PRIMARY KEY (id)`; `UNIQUE (token_hash)`; 1 `CHECK` constraint(s)

### `sales_promo_codes`

A business's sales promo codes.

* Row-level security: enabled and forced
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Policies: `sales_promo_codes_tenant`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `code` | `text` | yes | — |
| `description` | `text` | yes | `''::text` |
| `percent_bp` | `integer` | yes | — |
| `min_amount_minor` | `bigint` | yes | `0` |
| `currency` | `character(3)` | — | — |
| `valid_from` | `timestamp with time zone` | yes | `now()` |
| `valid_until` | `timestamp with time zone` | — | — |
| `max_redemptions` | `integer` | — | — |
| `active` | `boolean` | yes | `true` |
| `created_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (created_by) REFERENCES nhill.users(id)`; `FOREIGN KEY (currency) REFERENCES nhill.currencies(code)`; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `PRIMARY KEY (id)`; `UNIQUE (establishment_id, code)`; `UNIQUE (establishment_id, id)`; 5 `CHECK` constraint(s)

### `sales_promo_redemptions`

Uses of sales promo codes.

* Row-level security: enabled and forced
* `nhill_app` rights: INSERT, SELECT
* Policies: `sales_promo_redemptions_tenant`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `establishment_id` | `uuid` | yes | — |
| `promo_id` | `uuid` | yes | — |
| `document_id` | `uuid` | yes | — |
| `discount_minor` | `bigint` | yes | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (establishment_id, document_id) REFERENCES nhill.sales_documents(establishment_id, id)`; `FOREIGN KEY (establishment_id, promo_id) REFERENCES nhill.sales_promo_codes(establishment_id, id)`; `PRIMARY KEY (id)`; `UNIQUE (document_id)`; 1 `CHECK` constraint(s)

## Console and operations

### `admin_users`

Platform administrators (password and TOTP).

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `email` | `text` | yes | — |
| `full_name` | `text` | yes | — |
| `password_hash` | `text` | — | — |
| `totp_cipher` | `bytea` | — | — |
| `totp_nonce` | `bytea` | — | — |
| `totp_enabled` | `boolean` | yes | `false` |
| `role` | `text` | yes | — |
| `status` | `text` | yes | `'pending'::text` |
| `setup_token_hash` | `bytea` | — | — |
| `setup_expires_at` | `timestamp with time zone` | — | — |
| `last_totp_step` | `bigint` | — | — |
| `last_login_at` | `timestamp with time zone` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `PRIMARY KEY (id)`; `UNIQUE (email)`; 3 `CHECK` constraint(s)

### `admin_sessions`

Console sessions.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `admin_id` | `uuid` | yes | — |
| `token_hash` | `bytea` | yes | — |
| `expires_at` | `timestamp with time zone` | yes | — |
| `revoked_at` | `timestamp with time zone` | — | — |
| `ip` | `text` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (admin_id) REFERENCES nhill.admin_users(id) ON DELETE CASCADE`; `PRIMARY KEY (id)`; `UNIQUE (token_hash)`

### `admin_notifications`

Console notifications.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `bigint` | yes | identity |
| `kind` | `text` | yes | — |
| `params` | `jsonb` | yes | `'{}'::jsonb` |
| `establishment_id` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `read_at` | `timestamp with time zone` | — | — |
| `read_by` | `uuid` | — | — |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `FOREIGN KEY (read_by) REFERENCES nhill.admin_users(id)`; `PRIMARY KEY (id)`

### `announcements`

Announcements published to businesses.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `kind` | `text` | yes | — |
| `title_fr` | `text` | yes | — |
| `title_en` | `text` | yes | — |
| `body_fr` | `text` | yes | `''::text` |
| `body_en` | `text` | yes | `''::text` |
| `starts_at` | `timestamp with time zone` | yes | `now()` |
| `ends_at` | `timestamp with time zone` | — | — |
| `min_client_version` | `text` | — | — |
| `created_by` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (created_by) REFERENCES nhill.admin_users(id)`; `PRIMARY KEY (id)`; 1 `CHECK` constraint(s)

### `platform_settings`

Platform settings (allowed list).

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `key` | `text` | yes | — |
| `value` | `jsonb` | yes | — |
| `description` | `text` | yes | `''::text` |
| `updated_at` | `timestamp with time zone` | yes | `now()` |
| `updated_by` | `uuid` | — | — |

Constraints: `PRIMARY KEY (key)`

### `support_tickets`

Support requests.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE
* Triggers: `support_tickets_touch`

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `uuid` | yes | `gen_random_uuid()` |
| `number` | `bigint` | yes | identity |
| `establishment_id` | `uuid` | yes | — |
| `created_by` | `uuid` | — | — |
| `source` | `text` | yes | — |
| `category` | `text` | yes | — |
| `severity` | `text` | yes | `'normal'::text` |
| `subject` | `text` | yes | — |
| `status` | `text` | yes | `'open'::text` |
| `app_version` | `text` | — | — |
| `platform` | `text` | — | — |
| `assigned_to` | `uuid` | — | — |
| `created_at` | `timestamp with time zone` | yes | `now()` |
| `updated_at` | `timestamp with time zone` | yes | `now()` |
| `last_message_at` | `timestamp with time zone` | yes | `now()` |
| `resolved_at` | `timestamp with time zone` | — | — |

Constraints: `FOREIGN KEY (assigned_to) REFERENCES nhill.admin_users(id)`; `FOREIGN KEY (created_by) REFERENCES nhill.users(id)`; `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE CASCADE`; `PRIMARY KEY (id)`; `UNIQUE (number)`; 5 `CHECK` constraint(s)

### `support_ticket_messages`

Request messages, internal notes included.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `bigint` | yes | identity |
| `ticket_id` | `uuid` | yes | — |
| `author_type` | `text` | yes | — |
| `author_id` | `uuid` | — | — |
| `body` | `text` | yes | — |
| `internal` | `boolean` | yes | `false` |
| `created_at` | `timestamp with time zone` | yes | `now()` |

Constraints: `FOREIGN KEY (ticket_id) REFERENCES nhill.support_tickets(id) ON DELETE CASCADE`; `PRIMARY KEY (id)`; 2 `CHECK` constraint(s)

### `error_events`

Technical errors grouped by fingerprint.

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: INSERT, SELECT, UPDATE

| Column | Type | Not null | Default |
|---|---|---|---|
| `id` | `bigint` | yes | identity |
| `source` | `text` | yes | — |
| `level` | `text` | yes | `'error'::text` |
| `code` | `text` | yes | — |
| `message` | `text` | yes | — |
| `fingerprint` | `text` | yes | — |
| `context` | `jsonb` | yes | `'{}'::jsonb` |
| `establishment_id` | `uuid` | — | — |
| `user_id` | `uuid` | — | — |
| `app_version` | `text` | — | — |
| `platform` | `text` | — | — |
| `occurrences` | `integer` | yes | `1` |
| `first_seen` | `timestamp with time zone` | yes | `now()` |
| `last_seen` | `timestamp with time zone` | yes | `now()` |
| `resolved_at` | `timestamp with time zone` | — | — |
| `resolved_by` | `uuid` | — | — |

Constraints: `FOREIGN KEY (establishment_id) REFERENCES nhill.establishments(id) ON DELETE SET NULL`; `FOREIGN KEY (resolved_by) REFERENCES nhill.admin_users(id)`; `PRIMARY KEY (id)`; 4 `CHECK` constraint(s)

### `usage_daily`

Daily usage counters per business (counts only, never content).

* Row-level security: no (platform table, access limited by the `nhill_app` role's rights)
* `nhill_app` rights: SELECT

| Column | Type | Not null | Default |
|---|---|---|---|
| `day` | `date` | yes | — |
| `establishment_id` | `uuid` | yes | — |
| `documents_issued` | `integer` | yes | `0` |
| `payments_recorded` | `integer` | yes | `0` |

Constraints: `PRIMARY KEY (day, establishment_id)`

## Functions

| Function | Result | Properties | Executable by `nhill_app` |
|---|---|---|---|
| `nh_apply_stock_movement()` | `trigger` | volatile, `SECURITY DEFINER`, `search_path=nhill, pg_catalog` | — |
| `nh_check_plan_entitlement()` | `trigger` | volatile, `search_path=nhill, pg_catalog` | — |
| `nh_count_document_issue()` | `trigger` | volatile, `SECURITY DEFINER`, `search_path=nhill, pg_catalog` | — |
| `nh_count_payment()` | `trigger` | volatile, `SECURITY DEFINER`, `search_path=nhill, pg_catalog` | — |
| `nh_current_establishment()` | `uuid` | stable, `search_path=nhill, pg_catalog` | yes |
| `nh_erase_establishment(p_est uuid, p_early boolean)` | `jsonb` | volatile, `SECURITY DEFINER`, `search_path=nhill, pg_catalog` | yes |
| `nh_erasing(p_est uuid)` | `boolean` | stable, `search_path=nhill, pg_catalog` | yes |
| `nh_establishment_erasure(p_est uuid)` | `TABLE(eligible boolean, reason text, access_ended_at timestamp with time zone, erasable_from timestamp with time zone)` | stable, `search_path=nhill, pg_catalog` | yes |
| `nh_establishment_usage(p_est uuid)` | `TABLE(users integer, registers integer, warehouses integer, products integer, documents_month integer, storage_bytes bigint)` | volatile, `SECURITY DEFINER`, `search_path=nhill, pg_catalog` | yes |
| `nh_guard_pos_session()` | `trigger` | volatile, `search_path=nhill, pg_catalog` | — |
| `nh_guard_purchase()` | `trigger` | volatile, `search_path=nhill, pg_catalog` | — |
| `nh_guard_sales_document()` | `trigger` | volatile, `search_path=nhill, pg_catalog` | — |
| `nh_guard_sales_line()` | `trigger` | volatile, `search_path=nhill, pg_catalog` | — |
| `nh_iso(ts timestamp with time zone)` | `text` | immutable, `search_path=nhill, pg_catalog` | yes |
| `nh_like_contains(p text)` | `text` | immutable, `search_path=pg_catalog` | yes |
| `nh_local_date(p_ts timestamp with time zone)` | `date` | stable, `search_path=nhill, pg_catalog` | yes |
| `nh_local_start(p_day date)` | `timestamp with time zone` | stable, `search_path=nhill, pg_catalog` | yes |
| `nh_local_today()` | `date` | stable, `search_path=nhill, pg_catalog` | yes |
| `nh_plan_main_price()` | `trigger` | volatile, `search_path=nhill, pg_catalog` | — |
| `nh_platform_establishment_counts(p_est uuid)` | `TABLE(products bigint, customers bigint, suppliers bigint, documents bigint, drafts bigint)` | volatile, `SECURITY DEFINER`, `search_path=nhill, pg_catalog` | yes |
| `nh_platform_establishment_monthly(p_est uuid, p_from date, p_to date)` | `TABLE(month text, currency text, invoices_issued bigint, invoiced_net_minor bigint, invoiced_total_minor bigint, credited_total_minor bigint, payments_in_minor bigint, expenses_minor bigint)` | volatile, `SECURITY DEFINER`, `search_path=nhill, pg_catalog` | yes |
| `nh_platform_establishment_volumes(p_est uuid, p_from date, p_to date)` | `TABLE(currency text, invoices_issued bigint, invoiced_net_minor bigint, invoiced_total_minor bigint, credit_notes_issued bigint, credited_total_minor bigint, payments_in_count bigint, payments_in_minor bigint, payments_out_minor bigint, expenses_count bigint, expenses_minor bigint, purchases_minor bigint)` | volatile, `SECURITY DEFINER`, `search_path=nhill, pg_catalog` | yes |
| `nh_setting_int(p_key text, p_default integer)` | `integer` | stable, `search_path=nhill, pg_catalog` | yes |
| `nh_subscription_access(p_est uuid)` | `TABLE(access text, reason text, access_until timestamp with time zone, effective_plan_id uuid, is_trial boolean, status text, grace_end timestamp with time zone)` | stable, `search_path=nhill, pg_catalog` | yes |
| `nh_sync_plan_entitlements_from_legacy()` | `trigger` | volatile, `search_path=nhill, pg_catalog` | — |
| `nh_sync_plan_legacy_limits()` | `trigger` | volatile, `search_path=nhill, pg_catalog` | — |
| `nh_touch_updated_at()` | `trigger` | volatile, `search_path=nhill, pg_catalog` | — |
| `nh_tz(p_est uuid)` | `text` | stable, `search_path=nhill, pg_catalog` | yes |
| `nh_window(minutes integer)` | `timestamp with time zone` | stable, `search_path=nhill, pg_catalog` | yes |

## Summary

| Domain | Tables |
|---|---|
| Platform and identity | 18 |
| Plans, rights and subscriptions | 14 |
| Business management (businesses' data) | 21 |
| Till (Nhill POS) | 9 |
| Console and operations | 9 |
| **Total** | **71** |
