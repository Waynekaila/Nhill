# Procédure de déploiement en production

> **Production sur VPS Hostinger (France), version 3.0 : [fr/deploiement-vps-hostinger.md](fr/deploiement-vps-hostinger.md)**
> (script `deploy/vps/provision.sh`, sauvegardes, restauration, mises à jour avec retour arrière).
>
> **Document de la version 0.1.0, repris** dans [docs/fr/guide-installation.md](fr/guide-installation.md) § 4 (EN : [installation-guide](en/installation-guide.md)). En 2.0, le mot de passe de la console compte 8 caractères minimum (TOTP toujours obligatoire).

Cible : un serveur Linux (Ubuntu 24.04 LTS recommandé) pour l'API, une base PostgreSQL
15+ — **Supabase** (hébergée) ou PostgreSQL auto-hébergé —, un nom de domaine et un
reverse proxy TLS (Caddy). Chaque étape indique comment vérifier qu'elle a réussi.

> Le paquet, le mode installation et l'installation de la base ont été exécutés dans un
> Ubuntu 24.04 vierge (sans systemd) ; la base Supabase de production a été initialisée
> par le connecteur Supabase. **La procédure complète n'a pas été exécutée sur un
> serveur de production réel avec TLS public.**

## 1. Prérequis

* 2 vCPU, 4 Go de RAM, 40 Go SSD pour démarrer ; heure synchronisée (NTP —
  indispensable pour TOTP et les webhooks horodatés).
* Pare-feu : seuls 22 (administration, restreint) et 443/80 (Caddy) ouverts.
* DNS : `api.<votre-domaine>` → IP du serveur.

## 2. Paquet

```bash
scripts/build-linux.sh build --deb          # sur une machine de construction Ubuntu 24.04
sudo apt install ./build/nhill-server_0.1.0_amd64.deb   # sur le serveur
```

Le paquet installe `/usr/bin/nhill-server`, `/usr/share/nhill` (migrations, profils,
traductions, polices, modèles de configuration), les unités systemd et les scripts de
sauvegarde ; il crée les utilisateurs `nhill` et `nhill-backup`, puis
`/etc/nhill/server.env` (0600) **avec une `NH_SECRET_KEY` générée sur le serveur et
sans base de données**. Sauvegarder immédiatement `NH_SECRET_KEY` dans un coffre.

Vérification : `curl -s http://127.0.0.1:8080/health` → `{"database":false,"status":"setup"}`.

(Sans paquet : `sudo deploy/scripts/install-server.sh build`, même résultat, puis
`systemctl enable --now nhill-server`.)

## 3. Base de données

### 3.1 Supabase

1. Créer le projet, noter le mot de passe de la base.
2. Tableau de bord → **Connect → Session pooler** : hôte
   `aws-0-<région>.pooler.supabase.com`, port **5432**, utilisateur
   `postgres.<référence-du-projet>`. Ne pas utiliser le *transaction pooler* (6543).
3. **Aucune clé d'API** (ni `anon`, ni `service_role`) n'est nécessaire ni acceptée.

Nhill crée son schéma `nhill`, fermé aux rôles d'API de Supabase ; les tables ne sont pas
exposées par l'API REST de Supabase.

### 3.2 PostgreSQL auto-hébergé

Créer une base vide `nhill` et un compte administrateur capable de créer des rôles
(superutilisateur ou `CREATEROLE`). `pg_hba.conf` : `scram-sha-256`.

## 4. Installation depuis Nhill Console

1. Lire le jeton d'installation (60 minutes, un seul usage) :
   `journalctl -u nhill-server | grep -i -A2 "setup mode"`.
2. Sur un poste de confiance : Nhill Console → **Installer un nouveau serveur** →
   adresse du serveur et jeton → type de base (Supabase / PostgreSQL), hôte, port,
   utilisateur, mot de passe → **Tester la connexion** → premier administrateur
   (e-mail, nom) → **Initialiser la base de données**.
3. Le serveur crée `nhill_owner`, `nhill_app` et `nhill_backup` (mots de passe
   aléatoires), applique les migrations, vérifie l'isolation (10 contrôles), écrit
   `/etc/nhill/server.env` et redémarre. Le mot de passe d'administration de la base
   n'est pas conservé. Le mot de passe du compte de sauvegarde est affiché **une seule
   fois** : le ranger dans le coffre.

Vérification : `/health` → `{"database":true,"status":"ok"}` ; l'assistant affiche les
contrôles réussis. Si un contrôle échoue, rien n'est enregistré et le serveur reste en
mode installation.

Alternative en ligne de commande (même procédure) :
`NH_SETUP_DB_PASSWORD=… sudo -u nhill nhill-server --config /etc/nhill/server.env setup --host … --user … --admin-email vous@domaine --admin-name "Votre nom"`.

## 5. Configuration complémentaire

Dans `/etc/nhill/server.env` : `NH_PUBLIC_BASE_URL=https://api.<domaine>` (adresses de
webhook et pages de retour de paiement), `NH_TRUST_PROXY=1`, `NH_THREADS`. Puis
`sudo systemctl restart nhill-server` et
`sudo -u nhill nhill-server --config /etc/nhill/server.env check`.

## 6. TLS

```bash
sudo apt install caddy
sudo cp /usr/share/nhill/examples/Caddyfile.example /etc/caddy/Caddyfile   # remplacer api.example.com
sudo systemctl reload caddy
curl -sI https://api.<domaine>/health        # 200, en-tête Strict-Transport-Security
```

## 7. Sauvegardes

Auto-hébergé : créer une paire de clés age **hors du serveur** (`age-keygen -o cle.txt`),
copier la clé publique dans `/etc/nhill/backup.env`, configurer le mot de passe de
`nhill_backup` (`~nhill-backup/.pgpass`), puis
`sudo systemctl enable --now nhill-backup.timer` et un test de restauration
(`sauvegarde-restauration.md` § 5). Supabase : sauvegardes selon l'offre du projet ; le
même script peut viser Supabase avec le compte `nhill_backup`.

## 8. Premier accès, offres et prestataires

* Nhill Console → **Premier accès** : e-mail, jeton reçu à l'étape 4 (30 min), mot de
  passe de 14 caractères minimum, TOTP.
* **Offres et tarifs** : aucun prix n'est préconfiguré ; saisir les prix par durée et
  devise, les plafonds (utilisateurs, caisses POS, dépôts) et les fonctionnalités.
* **Prestataires de paiement** : saisir identifiants et secrets (chiffrés, jamais
  réaffichés), rester en mode **test**, effectuer un paiement de test réel avec le compte
  du prestataire, déclarer l'adresse de webhook affichée chez le prestataire, puis
  seulement passer en production.
* Inviter les autres administrateurs avec le rôle minimal nécessaire.

## 9. Applications

Aucune adresse n'est compilée dans les applications (3.0.0) : l'utilisateur saisit
`https://api.<domaine>` à la première utilisation (vérifiée, puis enregistrée sur l'appareil) ; signer les exécutables (Authenticode) et l'APK
(apksigner). Linux : distribuer `nhill-business`, `nhill-pos` (établissements) et
`nhill-console` (créateurs seulement).

## 10. Exploitation

* Journaux : `journalctl -u nhill-server` (JSON par ligne) ; erreurs regroupées dans
  Nhill Console → Erreurs techniques.
* Surveillance : sonde sur `/health`, alerte si `503`.
* Mises à jour : `apt install ./nhill-server_<version>.deb` (les migrations s'appliquent
  au démarrage du service) ; voir `mise-a-jour.md`.
* E-mails : transport `log` seulement (aucun envoi réel).
