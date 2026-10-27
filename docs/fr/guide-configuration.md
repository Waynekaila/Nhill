# Nhill 2.0 — Guide de configuration

*English: [../en/configuration-guide.md](../en/configuration-guide.md)*

Trois niveaux de réglage, de l'infrastructure à l'usage :

1. **Serveur** — fichier `/etc/nhill/server.env` (§ 1) ;
2. **Applications** — options de compilation et variables d'environnement (§ 2) ;
3. **Plateforme et établissements** — écrans de Nhill Console et de Nhill Business (§ 3).

Le site vitrine a sa propre configuration, séparée : [`website/README.md`](../../website/README.md) § 3.

## 1. Serveur : `/etc/nhill/server.env`

Modèle sans secret : `config/server.env.example` (installé dans
`/usr/share/nhill/examples/`). Le fichier appartient à `nhill`, mode **600** : le serveur
refuse de démarrer s'il est lisible par d'autres (`NH_ALLOW_INSECURE_CONFIG` lève ce
contrôle, à réserver aux tests). Chaque clé peut aussi être donnée en variable
d'environnement. Après modification : `sudo systemctl restart nhill-server` puis
`sudo -u nhill nhill-server --config /etc/nhill/server.env check`.

| Clé | Défaut | Rôle |
|---|---|---|
| `NH_DATABASE_URL` | — | connexion du rôle applicatif `nhill_app` (soumis à la RLS). Écrite par l'assistant d'installation |
| `NH_MIGRATION_DATABASE_URL` | — | connexion du rôle propriétaire `nhill_owner` (migrations au démarrage) |
| `NH_DB_PROVIDER` | — | `supabase` ou `postgresql`, écrit par l'installation |
| `NH_DB_POOL_SIZE` | = `NH_THREADS` | connexions ouvertes vers la base |
| `NH_LISTEN_ADDRESS` | `127.0.0.1` | adresse d'écoute (rester en local derrière Caddy) |
| `NH_LISTEN_PORT` | `8080` | port d'écoute |
| `NH_THREADS` | `16` | fils de traitement HTTP |
| `NH_TRUST_PROXY` | `0` | `1` derrière un proxy de confiance : l'adresse client est l'entrée **la plus à droite** de `X-Forwarded-For` (2.0) |
| `NH_PUBLIC_BASE_URL` | — | adresse publique `https://api.<domaine>` (webhooks, pages de retour de paiement) |
| `NH_SECRET_KEY` | — | 64 caractères hexadécimaux (`openssl rand -hex 32`), générée par le paquet. **À sauvegarder à part** |
| `NH_WEBHOOK_SECRET_<PRESTATAIRE>` | — | secret de webhook (≥ 32 caractères) d'un prestataire générique |
| `NH_ACCESS_TOKEN_MINUTES` | `15` | durée des jetons d'accès |
| `NH_REFRESH_TOKEN_DAYS` | `30` | durée des jetons de renouvellement (rotation, détection de réutilisation) |
| `NH_ARGON2_OPS` | `3` | coût Argon2id (itérations) |
| `NH_ARGON2_MEM_KIB` | `65536` | mémoire Argon2id (64 Mio) |
| `NH_JOBS_INTERVAL_SECONDS` | `300` | tâches de fond (rotation des codes, abonnements) ; `0` les désactive |
| `NH_EMAIL_TRANSPORT` | `log` | seul transport disponible : les e-mails sont écrits dans le journal, **aucun envoi réel** |
| `NH_PROFILE_DIR`, `NH_I18N_DIR`, `NH_FONT_DIR`, `NH_MIGRATIONS_DIR` | `/usr/share/nhill/…` | fichiers de données installés par le paquet |
| `NH_SETUP_DB_PASSWORD` | — | uniquement pour `nhill-server setup` en ligne de commande ; jamais conservé |

**Recommandations** : ne baissez pas `NH_ARGON2_*` en dessous des valeurs par défaut ;
mesurez le temps de connexion après une hausse. Gardez `NH_LISTEN_ADDRESS=127.0.0.1` et
exposez uniquement Caddy. `NH_TRUST_PROXY=1` sans proxy devant le serveur permettrait à
un client de choisir l'adresse vue par les limites de débit.

### Supabase

Utilisez le **Session pooler** (5432). Aucune clé `anon` ni `service_role` n'est utilisée.
Le conseiller de sécurité Supabase peut être consulté après installation ; Nhill crée un
schéma `nhill` non exposé par l'API REST.

## 2. Applications

### 2.1 Options de compilation (CMake)

| Option | Défaut | Effet |
|---|---|---|
| `NHILL_SERVER_URL` | `http://127.0.0.1:8080` | adresse du serveur compilée dans les applications — **HTTPS en production** |
| `NHILL_BUILD_SERVER` | `ON` sous Linux | serveur (Linux uniquement) |
| `NHILL_BUILD_CLIENT` / `NHILL_BUILD_TESTS` | `ON` | applications / tests |
| `NHILL_WARNINGS_AS_ERRORS` | `OFF` | avertissements bloquants (CI) |

Les scripts de construction acceptent `--server-url https://api.<domaine>`.

### 2.2 Variables d'environnement (bureau)

| Variable | Valeurs | Effet |
|---|---|---|
| `NHILL_SERVER` | URL | serveur à utiliser à la place de l'adresse compilée (essais, recette) |
| `NHILL_LANG` | `fr`, `en` | langue de l'interface au démarrage |
| `NHILL_THEME` | `light`, `dark`, `system` | thème au démarrage |
| `QT_SCALE_FACTOR` | ex. `1.25` | agrandissement global (variable Qt standard) |

Les variables `NHILL_TOUR_*` et `NHILL_SETUP_TOUR` servent uniquement aux captures
d'écran automatiques (développement).

Le client refuse les adresses `http://` sauf `localhost`, `127.0.0.1`, `::1` et `10.0.2.2`
(émulateur Android).

### 2.3 Préférences de l'utilisateur

Paramètres › Mon compte : langue, thème. La taille et la position des fenêtres sont
calculées à chaque ouverture en fonction de l'écran disponible.

## 3. Plateforme et établissements

### 3.1 Nhill Console (créateurs)

| Écran | Réglages |
|---|---|
| Paramètres de la plateforme | durée d'essai (30 j par défaut), période de grâce, rappel, rotation des codes d'établissement (30 j), limites, instructions de paiement |
| Offres et tarifs | offres, plafonds (utilisateurs, caisses POS, dépôts), fonctionnalités, **prix par durée et par devise** |
| Codes promo | remises de lancement ou de partenariat |
| Prestataires de paiement | identifiants (chiffrés, jamais réaffichés), mode test/production, adresse de webhook |

### 3.2 Nhill Business (établissement)

| Onglet | Réglages principaux |
|---|---|
| Établissement | identité, identifiants légaux, logo, langue des documents, délai de paiement, arrondi des taxes, pied de page, stock négatif |
| Taxes et comptes | taux de taxe (TVA 16 % en RDC selon le profil ; un taux ne se modifie pas), caisses et comptes bancaires, taux de change datés |
| Point de vente | caisses, caissiers autorisés, moyens de paiement, remise maximale sans validation, ticket 58/80 mm |
| Équipe | rôles et invitations |

### 3.3 Profils pays et modèles

`server/profiles/*.json` : profils fiscaux (taux, mentions, numérotation) synchronisés en
base au démarrage. Un document émis garde la version du profil appliqué. Détails :
`docs/modeles-et-profils.md`. Les valeurs fiscales des profils doivent être vérifiées par
un professionnel avant usage (voir [parametres-legaux.md](parametres-legaux.md)).

## 4. Règle des mots de passe (2.0)

| Élément | Valeur | Où |
|---|---|---|
| Longueur minimale | **8 caractères** (Unicode : un « é » compte pour un) | `common/include/nhill/validation.hpp` → `kMinPasswordLength` |
| Longueur maximale | 128 octets | `kMaxPasswordBytes` |
| Refusés | mots de passe courants (liste intégrée), identique à l'e-mail, répétitifs | `common/src/validation.cpp` |
| Expiration périodique | **aucune** | — |
| Stockage | Argon2id (libsodium), jamais en clair | serveur |
| Double authentification | obligatoire pour Nhill Console ; non imposée aux établissements | — |

La même fonction est appelée par les applications (avant l'envoi) et par le serveur (qui
décide). Modifier la règle impose de reconstruire serveur **et** applications.
