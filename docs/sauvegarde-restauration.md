# Politique de sauvegarde et de restauration

## 1. Ce qui est sauvegardé

| Élément | Où | Méthode |
|---|---|---|
| Base PostgreSQL complète (données, logos, justificatifs, journal d'audit, schéma, droits, politiques RLS) | Serveur de base | `deploy/backup/nhill-backup.sh` : `pg_dump --format=custom` chiffré à la volée avec **age** |
| `NH_SECRET_KEY` | `/etc/nhill/server.env` | Copie manuelle dans un coffre de mots de passe, **séparée** des sauvegardes |
| Configuration (`server.env` sans secrets, Caddyfile, unités systemd) | Serveur applicatif | Gestion de configuration / copie chiffrée |

Les fichiers téléversés sont stockés en base : une seule sauvegarde suffit.

## 2. Rôle de sauvegarde

Les tables métier imposent la sécurité par ligne à tous les rôles, y compris le
propriétaire : `pg_dump` échoue avec eux (vérifié). Le rôle `nhill_backup`
(`db/bootstrap/01-backup-role.sql`) est le seul autorisé à contourner la RLS ; il est en
**lecture seule** (`pg_read_all_data`, `default_transaction_read_only = on` — une
suppression tentée échoue, vérifié). Restreindre ce rôle à une connexion locale dans
`pg_hba.conf`.

## 3. Fréquence et rétention (recommandation)

* Quotidienne à 02 h 30 (`nhill-backup.timer`, avec rattrapage si la machine était
  éteinte), rétention locale 30 jours (`NH_BACKUP_KEEP_DAYS`).
* Copie hors site après chaque sauvegarde (`NH_BACKUP_OFFSITE_CMD`, par ex. rclone).
* Conserver en plus une sauvegarde mensuelle 12 mois et une annuelle 10 ans : les
  pièces comptables doivent être conservées 10 ans (Acte uniforme OHADA, voir
  `docs/conformite/conformite.md`).
* Pour un objectif de perte de données inférieur à 24 h : activer l'archivage continu
  des WAL de PostgreSQL (`archive_mode`, outil type pgBackRest) — **non fourni** ici.

## 4. Chiffrement

`pg_dump | age -r <clé publique>` : le dump en clair n'est jamais écrit sur disque.
La **clé privée age** reste hors du serveur (coffre, support hors ligne) ; elle n'est
nécessaire que pour restaurer. Une empreinte SHA-256 accompagne chaque fichier.

## 5. Restauration

```bash
# sur une machine disposant des rôles nhill_owner et nhill_app (00-roles.sql)
sudo -u postgres PGHOST=/var/run/postgresql \
  /usr/sbin/nhill-restore.sh /var/backups/nhill/nhill-AAAAMMJJTHHMMSSZ.dump.age \
  /chemin/vers/cle-privee-age.txt nhill_restauree
```

Le script vérifie l'empreinte, crée une **nouvelle** base (jamais par-dessus la base de
production), restaure en superutilisateur (propriété `nhill_owner`, droits et
politiques RLS issus de la sauvegarde) puis affiche des contrôles : nombre de
migrations, d'établissements, d'entrées d'audit et état de la RLS. Ensuite :

1. `nhill-server --config … check` avec `NH_DATABASE_URL` pointant vers la base restaurée ;
2. contrôles fonctionnels (connexion, consultation d'une facture, rapport) ;
3. bascule : modifier `NH_DATABASE_URL`, redémarrer `nhill-server`.

## 6. Test de restauration

**Exécuté le 25 octobre 2026** sur l'environnement de développement : sauvegarde de
212 Kio chiffrée (`age-encryption.org/v1`), restauration dans une base neuve, contrôles
« migrations : 6, établissements : 2, entrées d'audit : 84, RLS forcée : true » ; avec le
rôle applicatif, 0 document visible sans contexte et 5 documents dans le contexte de
l'établissement de démonstration ; `nhill-server check` : « configuration OK,
database reachable, no pending migration ».

Procédure à répéter **chaque trimestre** et après toute mise à jour majeure, avec un
compte rendu daté.

## 7. Incident : perte de `NH_SECRET_KEY`

Les données métier restent lisibles. Générer une nouvelle clé, réémettre les codes
d'établissement (console › Établissements › Réémettre le code), demander aux
administrateurs de refaire leur premier accès (nouveau jeton via un superadmin ou
`admin-bootstrap` si aucun administrateur n'existe plus).
