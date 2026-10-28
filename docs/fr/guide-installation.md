# Nhill 2.1 — Guide d'installation

*English: [../en/installation-guide.md](../en/installation-guide.md)*

Ce guide couvre l'installation des **applications** (Linux, Windows, Android), du
**serveur** (Linux) et la mise à jour depuis la 2.0 ou la 0.1.0. Construction des paquets :
[guide-compilation.md](guide-compilation.md). Réglages : [guide-configuration.md](guide-configuration.md).
Site vitrine : [`website/README.md`](../../website/README.md).

> **État des livrables.** Chaque fichier livré dans `release/V<version>/` a été
> réellement construit ; une plateforme absente est listée dans
> `release/V<version>/checksums/MISSING.txt`. Le rapport de validation
> ([rapport-validation.md](rapport-validation.md)) dit ce qui a été testé et sur quelle
> machine. Les installateurs Windows et l'APK ne sont **pas signés** par un certificat
> d'éditeur tant que les créateurs n'en ont pas acquis un.

## 1. Vérifier un fichier téléchargé

Avant toute installation, comparez l'empreinte SHA-256 du fichier avec celle de
`checksums/SHA256SUMS` (publiée à part du fichier, par exemple sur le site) :

```bash
sha256sum -c SHA256SUMS --ignore-missing          # Linux
```
```powershell
Get-FileHash .\nhill-business-2.1.0-setup.exe -Algorithm SHA256   # Windows : comparer à la main
```

Si l'empreinte diffère, n'installez pas le fichier.

## 2. Configuration minimale

| Composant | Système | Mémoire | Remarques |
|---|---|---|---|
| Nhill Business, POS, Console (bureau) | Windows 10/11 64 bits ; Debian 13 ou Ubuntu 24.04 (amd64) | à mesurer sur le poste cible | écran 1280×720 ou plus conseillé ; la POS fonctionne dès 860×560 |
| Nhill Business (Android) | Android 7.0 (API 24) ou plus, processeur arm64 | — | connexion Internet requise |
| Serveur | Debian 13 ou Ubuntu 24.04, amd64 | point de départ : 2 vCPU, 4 Go, 40 Go SSD | PostgreSQL 15+ (Supabase ou auto-hébergé), heure synchronisée (NTP) |

Les besoins en mémoire du poste client n'ont pas été mesurés : testez sur le matériel
réellement utilisé (voir le rapport de validation).

## 3. Applications

### 3.1 Linux (Debian 13, Ubuntu 24.04)

Les paquets sont construits **pour une distribution précise** : prenez ceux du dossier de
votre système (`linux/debian-13/` ou `linux/ubuntu-24.04/`).

```bash
sudo apt install ./nhill-business_2.1.0-1_amd64.deb ./nhill-pos_2.1.0-1_amd64.deb
sudo apt install ./nhill-console_2.1.0-1_amd64.deb     # réservé aux créateurs de Nhill
```

`apt` installe les bibliothèques nécessaires depuis les dépôts officiels. Les applications
apparaissent dans le menu (Bureautique). Session mémorisée : il faut un trousseau Secret
Service (GNOME Keyring ou KWallet), présent sur les bureaux usuels.

Désinstallation : `sudo apt remove nhill-business nhill-pos nhill-console`.

### 3.2 Windows 10/11

1. Vérifiez l'empreinte (§ 1), puis lancez `nhill-business-2.1.0-setup.exe` (ou
   `nhill-pos-…`, `nhill-console-…`). L'installation demande les droits d'administrateur
   (installation dans `Program Files`).
2. Tant que l'installateur n'est pas signé, Windows SmartScreen peut afficher « Windows a
   protégé votre ordinateur ». Ne contournez cet avertissement **que pour ce fichier** et
   **seulement si l'empreinte correspond** : « Informations complémentaires » →
   « Exécuter quand même ». Ne désactivez jamais SmartScreen ni l'antivirus.
3. Raccourcis créés dans le menu Démarrer ; désinstallation par Paramètres → Applications.

Installation silencieuse (parc de postes) : `nhill-pos-2.1.0-setup.exe /S`.

### 3.3 Android

1. Vérifiez l'empreinte de `nhill-business-2.1.0-arm64-v8a.apk`.
2. Ouvrez le fichier sur le téléphone ; Android demande d'autoriser **cette application**
   (gestionnaire de fichiers ou navigateur) à installer des applications. Autorisez-la
   pour cette installation, puis retirez l'autorisation (Paramètres → Applications →
   Accès spéciaux → Installer des applis inconnues).
3. Un APK signé avec la clé de **débogage** (construction sans clé de publication) peut
   être installé pour des essais mais ne doit pas être distribué aux clients ; il ne peut
   pas être mis à jour par un APK signé avec la clé de publication (désinstaller d'abord).

Nhill POS et Nhill Console ne sont pas proposées sur Android (applications de bureau).

### 3.4 Premier démarrage

Aucune adresse de serveur n'est compilée dans l'application. Nhill Business la demande à la
création d'un établissement ; Nhill Console et Nhill POS la demandent à leur première connexion.
Saisissez l'adresse fournie par votre prestataire (HTTPS) : elle est vérifiée, puis enregistrée
sur l'appareil, et reste modifiable dans les paramètres. Pour un essai ponctuel sur un autre
serveur, la variable d'environnement `NHILL_SERVER` la remplace (bureau seulement). Ensuite :
guide utilisateur, Partie A § 1.

## 4. Serveur (production)

> Procédure retenue pour la production : **VPS Hostinger en France**, script
> `deploy/vps/provision.sh` — voir [deploiement-vps-hostinger.md](deploiement-vps-hostinger.md).
> Cette section décrit l'installation manuelle, utile pour un autre hébergement.

Cible : serveur Linux pour l'API, base PostgreSQL 15+ (**Supabase** ou auto-hébergée),
nom de domaine, proxy TLS (Caddy). La procédure complète n'a pas encore été exécutée sur
un serveur de production public avec TLS : faites-la d'abord sur un serveur de recette.

### 4.1 Prérequis

- Heure synchronisée (NTP) — indispensable pour TOTP et les webhooks.
- Pare-feu : seuls 22 (administration, restreint) et 80/443 (Caddy) ouverts.
- DNS : `api.<votre-domaine>` → adresse du serveur.

### 4.2 Paquet

```bash
sudo apt install ./nhill-server_2.1.0-1_amd64.deb
curl -s http://127.0.0.1:8080/health     # {"database":false,"status":"setup"}
```

Le paquet crée les utilisateurs système `nhill` et `nhill-backup`, le fichier
`/etc/nhill/server.env` (mode 600) avec une clé `NH_SECRET_KEY` **générée sur le
serveur** et sans base de données, puis démarre le service en **mode installation**.
Copiez immédiatement `NH_SECRET_KEY` dans un coffre (sans elle, les codes
d'établissement et les secrets TOTP des administrateurs deviennent illisibles).

### 4.3 Base de données

- **Supabase** : Connect → **Session pooler** (port 5432, utilisateur
  `postgres.<référence>`) ; pas le *transaction pooler* (6543). Aucune clé d'API
  (`anon`, `service_role`) n'est nécessaire ni acceptée ; Nhill crée un schéma `nhill`
  fermé aux rôles d'API de Supabase.
- **PostgreSQL auto-hébergé** : base vide `nhill`, compte capable de créer des rôles,
  `scram-sha-256` dans `pg_hba.conf`, TLS si la base est distante.

### 4.4 Installation depuis Nhill Console

1. Jeton d'installation (60 minutes, usage unique) :
   `journalctl -u nhill-server | grep -i -A2 "setup mode"`.
2. Nhill Console → **Installer un nouveau serveur** → adresse et jeton → type de base,
   hôte, port, utilisateur, mot de passe → **Tester la connexion** → premier
   administrateur → **Initialiser la base de données**.
3. Le serveur crée `nhill_owner`, `nhill_app`, `nhill_backup` (mots de passe aléatoires),
   applique les migrations, vérifie l'isolation, écrit `/etc/nhill/server.env` et
   redémarre. Le mot de passe d'administration de la base n'est pas conservé ; celui du
   compte de sauvegarde est affiché **une seule fois**.

Vérification : `/health` → `{"database":true,"status":"ok"}`. En ligne de commande :
`NH_SETUP_DB_PASSWORD=… sudo -u nhill nhill-server --config /etc/nhill/server.env setup --host … --user … --admin-email … --admin-name "…"`.

### 4.5 TLS et réglages

```bash
sudo apt install caddy
sudo cp /usr/share/nhill/examples/Caddyfile.example /etc/caddy/Caddyfile   # remplacer api.example.com
sudo systemctl reload caddy
curl -sI https://api.<domaine>/health       # 200 + Strict-Transport-Security
```

Dans `/etc/nhill/server.env` : `NH_PUBLIC_BASE_URL=https://api.<domaine>`,
`NH_TRUST_PROXY=1` ; puis `sudo systemctl restart nhill-server` et
`sudo -u nhill nhill-server --config /etc/nhill/server.env check`.

### 4.6 Sauvegardes, premier accès, offres

- Sauvegardes chiffrées (age) : `docs/sauvegarde-restauration.md` ;
  `sudo systemctl enable --now nhill-backup.timer`, puis **un test de restauration**.
- Nhill Console → **Premier accès** (jeton 30 min, mot de passe de 8 caractères minimum,
  TOTP obligatoire).
- **Offres et tarifs** : aucun prix n'est préconfiguré.
- **Courrier** (2.1) : relais SMTP avec TLS (`NH_EMAIL_TRANSPORT=smtp`, `NH_SMTP_*`) ; sans
  lui, propriétaires et administrateurs ne peuvent pas récupérer un mot de passe oublié.
- **Prestataires de paiement** : rester en mode test jusqu'à un paiement de test réussi
  avec le compte du prestataire.

## 5. Mise à jour vers la 2.1.0

Depuis la 2.0.x :

1. **Sauvegarder** la base : `sudo systemctl start nhill-backup.service`.
2. Serveur : `sudo apt install ./nhill-server_2.1.0-1_amd64.deb` ; au démarrage, le service
   applique la migration **0011** (mot de passe oublié : deux colonnes dans `users`, table
   `password_resets`, trois paramètres de la plateforme). Schéma : 0001–0011.
3. Courrier : renseignez `NH_EMAIL_TRANSPORT=smtp` et les variables `NH_SMTP_*`
   ([guide-configuration.md](guide-configuration.md) § 1 et § 5), puis
   `sudo -u nhill nhill-server --config /etc/nhill/server.env check` et un essai de
   « Mot de passe oublié ? » avec un compte de test.
4. Applications : installez les nouvelles versions **avant** d'utiliser la réinitialisation
   par l'administrateur. Un client 2.0 reste accepté par le serveur 2.1 (API `v1`), mais
   il ne sait pas afficher le changement obligatoire d'un mot de passe temporaire : le
   membre concerné ne pourrait pas se connecter depuis ce poste.
5. Session mémorisée : un utilisateur dont la session était déjà gardée par la 2.0 (Linux)
   la conserve ; ailleurs, « Se souvenir de moi » s'active à la prochaine connexion.

Depuis la 0.1.0 : mêmes étapes ; la 2.0 n'ajoutait aucune migration (la règle de
8 caractères s'applique aux nouveaux mots de passe, sans changement imposé aux
existants). Mettez à jour serveur et clients ensemble (les clients 0.1.x refusent
localement les mots de passe de moins de 10 caractères).

Retour arrière vers la 2.0 : restaurer la sauvegarde de l'étape 1 est la méthode sûre.
Réinstaller seulement le paquet 2.0 laisse la migration 0011 en place (le serveur 2.0
ignore les colonnes et la table ajoutées) ; ce cas n'a pas été testé, et un mot de passe
temporaire déjà donné y fonctionnerait comme un mot de passe ordinaire.

## 6. Dépannage

| Symptôme | Action |
|---|---|
| « Problème serveur » dans l'application | `curl https://api.<domaine>/health` ; `journalctl -u nhill-server` |
| Session non mémorisée sous Linux | installer/déverrouiller GNOME Keyring ou KWallet |
| Session non mémorisée sous Windows ou Android | vérifier que « Se souvenir de moi » est coché (Paramètres › Mon compte › Cet appareil) ; Android : définir un verrouillage d'écran ; un message « stockage sécurisé indisponible » s'affiche si le système refuse l'enregistrement |
| « Mot de passe oublié ? » : aucun code reçu | serveur : `NH_EMAIL_TRANSPORT=smtp` ? erreur du relais dans `journalctl -u nhill-server` ; utilisateur : dossier des indésirables ; un employé d'un seul établissement reçoit la consigne de contacter son administrateur, pas un code |
| « Mise à jour obligatoire » | installer la version demandée par le serveur |
| Android : « Application non installée » | APK d'une autre signature déjà présent : le désinstaller |
| Windows : application qui ne démarre pas | réinstaller ; vérifier que l'antivirus n'a pas mis un fichier en quarantaine |
| Serveur : refus de démarrer, `server.env` lisible | `chmod 600 /etc/nhill/server.env` |
| Serveur : migration modifiée refusée | ne jamais modifier une migration publiée ; restaurer la version d'origine |
