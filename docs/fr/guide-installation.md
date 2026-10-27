# Nhill 2.0 — Guide d'installation

*English: [../en/installation-guide.md](../en/installation-guide.md)*

Ce guide couvre l'installation des **applications** (Linux, Windows, Android), du
**serveur** (Linux) et la mise à jour depuis la 0.1.0. Construction des paquets :
[guide-compilation.md](guide-compilation.md). Réglages : [guide-configuration.md](guide-configuration.md).
Site vitrine : [`website/README.md`](../../website/README.md).

> **État des livrables 2.0.0.** Chaque fichier livré dans `release/V2.0.0/` a été
> réellement construit ; une plateforme absente est listée dans
> `release/V2.0.0/checksums/MISSING.txt`. Le rapport de validation
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
Get-FileHash .\nhill-business-2.0.0-setup.exe -Algorithm SHA256   # Windows : comparer à la main
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
sudo apt install ./nhill-business_2.0.0-1_amd64.deb ./nhill-pos_2.0.0-1_amd64.deb
sudo apt install ./nhill-console_2.0.0-1_amd64.deb     # réservé aux créateurs de Nhill
```

`apt` installe les bibliothèques nécessaires depuis les dépôts officiels. Les applications
apparaissent dans le menu (Bureautique). Session mémorisée : il faut un trousseau Secret
Service (GNOME Keyring ou KWallet), présent sur les bureaux usuels.

Désinstallation : `sudo apt remove nhill-business nhill-pos nhill-console`.

### 3.2 Windows 10/11

1. Vérifiez l'empreinte (§ 1), puis lancez `nhill-business-2.0.0-setup.exe` (ou
   `nhill-pos-…`, `nhill-console-…`). L'installation demande les droits d'administrateur
   (installation dans `Program Files`).
2. Tant que l'installateur n'est pas signé, Windows SmartScreen peut afficher « Windows a
   protégé votre ordinateur ». Ne contournez cet avertissement **que pour ce fichier** et
   **seulement si l'empreinte correspond** : « Informations complémentaires » →
   « Exécuter quand même ». Ne désactivez jamais SmartScreen ni l'antivirus.
3. Raccourcis créés dans le menu Démarrer ; désinstallation par Paramètres → Applications.

Installation silencieuse (parc de postes) : `nhill-pos-2.0.0-setup.exe /S`.

### 3.3 Android

1. Vérifiez l'empreinte de `nhill-business-2.0.0-arm64-v8a.apk`.
2. Ouvrez le fichier sur le téléphone ; Android demande d'autoriser **cette application**
   (gestionnaire de fichiers ou navigateur) à installer des applications. Autorisez-la
   pour cette installation, puis retirez l'autorisation (Paramètres → Applications →
   Accès spéciaux → Installer des applis inconnues).
3. Un APK signé avec la clé de **débogage** (construction sans clé de publication) peut
   être installé pour des essais mais ne doit pas être distribué aux clients ; il ne peut
   pas être mis à jour par un APK signé avec la clé de publication (désinstaller d'abord).

Nhill POS et Nhill Console ne sont pas proposées sur Android (applications de bureau).

### 3.4 Premier démarrage

L'adresse du serveur est compilée dans l'application (`NHILL_SERVER_URL`). Pour un essai
sur un autre serveur, définissez la variable d'environnement `NHILL_SERVER` (bureau
seulement). Ensuite : guide utilisateur, Partie A § 1.

## 4. Serveur (production)

Cible : serveur Linux pour l'API, base PostgreSQL 15+ (**Supabase** ou auto-hébergée),
nom de domaine, proxy TLS (Caddy). La procédure complète n'a pas encore été exécutée sur
un serveur de production public avec TLS : faites-la d'abord sur un serveur de recette.

### 4.1 Prérequis

- Heure synchronisée (NTP) — indispensable pour TOTP et les webhooks.
- Pare-feu : seuls 22 (administration, restreint) et 80/443 (Caddy) ouverts.
- DNS : `api.<votre-domaine>` → adresse du serveur.

### 4.2 Paquet

```bash
sudo apt install ./nhill-server_2.0.0-1_amd64.deb
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
- **Prestataires de paiement** : rester en mode test jusqu'à un paiement de test réussi
  avec le compte du prestataire.

## 5. Mise à jour depuis la 0.1.0

1. **Sauvegarder** la base : `sudo systemctl start nhill-backup.service`.
2. Serveur : `sudo apt install ./nhill-server_2.0.0-1_amd64.deb` ; le service applique les
   migrations au démarrage (la 2.0.0 n'ajoute **aucune migration** : schéma 0001–0010).
3. Applications : installer les nouveaux paquets / installateurs / APK. Les clients 0.1.x
   restent acceptés par un serveur 2.0 (API `v1` inchangée, `min_client_version` 0.1.0) ;
   un client 2.0 fonctionne aussi avec un serveur 0.1.x, mais ce serveur impose encore
   10 caractères (14 pour la console) ; les clients 0.1.x, eux, refusent localement les
   mots de passe de moins de 10 caractères. Mettez à jour serveur et clients ensemble.
4. Mots de passe existants : **aucun changement imposé**. La règle de 8 caractères
   s'applique aux nouveaux mots de passe.

Retour arrière : réinstaller le paquet 0.1.0 (pas de migration à annuler en 2.0.0).

## 6. Dépannage

| Symptôme | Action |
|---|---|
| « Problème serveur » dans l'application | `curl https://api.<domaine>/health` ; `journalctl -u nhill-server` |
| Session non mémorisée sous Linux | installer/déverrouiller GNOME Keyring ou KWallet |
| « Mise à jour obligatoire » | installer la version demandée par le serveur |
| Android : « Application non installée » | APK d'une autre signature déjà présent : le désinstaller |
| Windows : application qui ne démarre pas | réinstaller ; vérifier que l'antivirus n'a pas mis un fichier en quarantaine |
| Serveur : refus de démarrer, `server.env` lisible | `chmod 600 /etc/nhill/server.env` |
| Serveur : migration modifiée refusée | ne jamais modifier une migration publiée ; restaurer la version d'origine |
