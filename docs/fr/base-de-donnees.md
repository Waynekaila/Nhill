# Nhill 3.0 — Composant base de données

*English: [../en/database.md](../en/database.md)*

Toutes les données de Nhill sont dans une base PostgreSQL (version 15 ou plus récente),
dans un schéma dédié `nhill`. Seul le serveur Nhill s'y connecte ; les applications passent
toujours par l'API.

| Sujet | Document |
|---|---|
| Tables, colonnes, contraintes, politiques et fonctions (généré depuis une base migrée) | [reference-base-de-donnees.md](reference-base-de-donnees.md) |
| Installation et sauvegardes sur le VPS de production | [deploiement-vps-hostinger.md](deploiement-vps-hostinger.md) § 4, 7 et 8 |
| Installation manuelle (PostgreSQL ou Supabase) | [guide-installation.md](guide-installation.md) § 4.3 et 4.4 |
| Durées de conservation | [`legal/fr/15-politique-conservation.md`](../../legal/fr/15-politique-conservation.md) |

## 1. Hébergements possibles

| Hébergement | Conditions |
|---|---|
| PostgreSQL sur le même serveur (production retenue) | installé par `deploy/vps/provision.sh` ; écoute seulement en local (port 5432 fermé au réseau) |
| PostgreSQL auto-hébergé ailleurs | base vide `nhill`, compte capable de créer des rôles pour l'installation, authentification `scram-sha-256`, TLS si la base est distante |
| Supabase | « Session pooler » (port 5432), jamais le « transaction pooler » ; aucune clé d'API Supabase n'est utilisée ; le schéma `nhill` est fermé aux rôles `anon` et `authenticated` |

## 2. Rôles

| Rôle | Usage | Droits |
|---|---|---|
| `nhill_owner` | migrations (`NH_MIGRATION_DATABASE_URL`) | possède le schéma ; jamais utilisé par l'API |
| `nhill_app` | serveur en fonctionnement (`NH_DATABASE_URL`) | ne possède rien, aucun droit de définition, soumis à la sécurité au niveau des lignes, `NOBYPASSRLS` ; journal d'activité en lecture et ajout seulement |
| `nhill_backup` | sauvegardes (`db/bootstrap/01-backup-role.sql`) | lecture seule (`pg_read_all_data`, transactions en lecture seule par défaut) ; seul rôle autorisé à lire tous les établissements, pour l'export de sauvegarde |

L'installation (Nhill Console ou `nhill-server setup`) crée ces rôles avec des mots de passe
aléatoires ; le mot de passe de l'administrateur de la base n'est utilisé qu'une fois et
n'est pas conservé.

## 3. Isolement des établissements

- Les 31 tables de gestion des établissements (catalogue, clients et fournisseurs,
  documents, stock, achats, dépenses, paiements, caisse…) ont la sécurité au niveau des
  lignes **activée et forcée** (elle s'applique aussi au propriétaire des tables). Les
  tables de la plateforme (comptes, appartenances, abonnements, journal d'activité…) ne
  sont lues qu'à travers les requêtes du serveur, avec les seuls droits nécessaires au rôle
  applicatif (le journal d'activité : lecture et ajout seulement).
- Le serveur fixe `app.establishment_id` au début de chaque transaction ; sans cette valeur,
  aucune ligne n'est visible ni modifiable.
- Les références entre tables utilisent des clés composées (établissement, identifiant) :
  une ligne ne peut pas désigner les données d'un autre établissement, même par erreur de
  programmation.
- Les statistiques de la console passent par des fonctions qui ne renvoient que des
  agrégats (nombres, montants par devise), jamais de contenu.

Ces règles sont vérifiées par les tests du serveur et par l'écran de vérification de
l'installation dans Nhill Console (rôle sans privilège, sécurité forcée, fonctions à
`search_path` fixé, aucune donnée visible ni écriture sans établissement).

## 4. Migrations

- Fichiers `db/migrations/NNNN_nom.sql`, appliqués dans l'ordre par `nhill-server migrate`
  (ou au premier démarrage d'une installation) ; version 3.0.0 : migrations 0001 à 0015.
- Chaque migration appliquée est enregistrée avec son empreinte SHA-256 dans
  `schema_migrations` : une migration modifiée après coup est refusée. Ne modifiez jamais
  une migration publiée ; ajoutez-en une nouvelle.
- Le serveur refuse de démarrer s'il reste une migration à appliquer, et refuse une base
  migrée par une version plus récente que lui (retour arrière : restaurer la copie prise
  avant la mise à jour).
- Les migrations 3.0.0 : 0012 (formules configurables et droits), 0013 (mentions des
  documents, factures émises en caisse), 0014 (suppression définitive d'un établissement,
  archive de facturation), 0015 (recherches avec `%` et `_` littéraux).

## 5. Sauvegardes et restauration

| Élément | Fonctionnement |
|---|---|
| Sauvegarde | `deploy/backup/nhill-backup.sh` (service `nhill-backup.timer`, chaque nuit vers 02:30) : `pg_dump` au format personnalisé avec le rôle `nhill_backup`, chiffré par `age` à la volée (aucune copie en clair sur le disque), empreinte SHA-256 à côté |
| Conservation | 14 jours sur le VPS (`NH_BACKUP_KEEP_DAYS`) ; copie hors site possible à chaque sauvegarde (`NH_BACKUP_OFFSITE_CMD`) |
| Clé | seule la clé **publique** `age` est sur le serveur ; la clé privée, nécessaire pour restaurer, est gardée hors ligne |
| Restauration | `deploy/backup/nhill-restore.sh SAUVEGARDE CLÉ NOUVELLE_BASE` : vérifie l'empreinte, restaure dans une **nouvelle** base, affiche migrations, établissements et sécurité ; la bascule se fait ensuite à la main |
| Test de restauration | `deploy/vps/nhill-restore-test.sh` sur un poste séparé : PostgreSQL jetable, restauration, comptages, contrôle de la sécurité forcée et de l'isolement du rôle applicatif, puis effacement ; à faire chaque mois |

Procédure complète, y compris la bascule sur le VPS : [deploiement-vps-hostinger.md](deploiement-vps-hostinger.md) § 7 et 8.

## 6. Conservation et suppression

| Données | Règle | Mise en œuvre |
|---|---|---|
| Établissement après la fin de son accès | lecture seule et export pendant 90 jours, puis suppression définitive | Nhill Console › Établissements › « À supprimer » ; fonction `nh_erase_establishment` (`SECURITY DEFINER`, propriétaire des tables) : documents, clients, produits, stock, paiements, journal d'activité et comptes utilisés uniquement dans cet établissement |
| Commandes d'abonnement payées d'un établissement supprimé | 10 ans | table `billing_archive` |
| E-mails envoyés ou abandonnés | 30 jours | tâche de fond du serveur |
| Sessions terminées | 90 jours | tâche de fond du serveur |
| Sauvegardes | 14 jours sur le VPS | `nhill-backup.sh` ; une donnée supprimée disparaît des sauvegardes à leur rotation |

## 7. Requêtes d'exploitation utiles

À lancer avec un compte d'administration de la base (jamais avec `nhill_app`) :

```sql
-- migrations appliquées
SELECT version, applied_at FROM nhill.schema_migrations ORDER BY version;
-- tables à sécurité au niveau des lignes forcée (31 en 3.0.0 ; liste dans la référence)
SELECT count(*) FROM pg_class c JOIN pg_namespace n ON n.oid = c.relnamespace
 WHERE n.nspname = 'nhill' AND c.relrowsecurity AND c.relforcerowsecurity;
-- e-mails en échec (erreur du relais, sans mot de passe)
SELECT template, attempts, last_error, created_at FROM nhill.email_outbox WHERE status = 'failed' ORDER BY created_at DESC LIMIT 20;
```
