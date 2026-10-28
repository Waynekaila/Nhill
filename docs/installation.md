# Guide d'installation et de construction

> **Document de la version 0.1.0, remplacé** par [docs/fr/guide-compilation.md](fr/guide-compilation.md) et [docs/fr/guide-installation.md](fr/guide-installation.md) (EN : [build-guide](en/build-guide.md), [installation-guide](en/installation-guide.md)).

| Plateforme | Composant | État vérifié |
|---|---|---|
| Linux (Ubuntu 24.04, GCC 13, Qt 6.4.2) | Serveur, Nhill Business, Nhill Console, Nhill POS, tests | **Compilé et testé ; paquets .deb construits et installés dans un Ubuntu 24.04 vierge** |
| Windows 10/11 | Nhill Business, Nhill Console, Nhill POS | `scripts/build-windows.ps1` + installateurs Inno Setup (`packaging/windows/nhill-app.iss`) — **non exécutés** |
| Android 7.0+ (API 24, cible 34) | Nhill Business | Manifeste, icônes et `scripts/build-android.sh` fournis — **non compilé** |
| Serveur sous Windows | — | Non prévu (le serveur se déploie sous Linux) |

## 1. Linux

### 1.1 Paquets (Ubuntu 24.04)

```bash
sudo apt install build-essential cmake ninja-build pkg-config \
  libpqxx-dev libsodium-dev libpng-dev libcpp-httplib-dev nlohmann-json3-dev libhpdf-dev libssl-dev \
  libgtest-dev postgresql-16 \
  qt6-base-dev qt6-declarative-dev qt6-svg-dev qt6-tools-dev qt6-l10n-tools qtkeychain-qt6-dev \
  qml6-module-qtquick qml6-module-qtquick-controls qml6-module-qtquick-layouts \
  qml6-module-qtquick-dialogs qml6-module-qtquick-templates qml6-module-qtqml-workerscript
```

### 1.2 Construction

```bash
scripts/build-linux.sh build                 # tout + tests (ctest)
scripts/build-linux.sh build --server-only   # serveur seul
scripts/build-linux.sh build --client-only --no-tests
scripts/build-linux.sh build --deb          # + paquets nhill-server, nhill-business, nhill-console, nhill-pos
sudo cmake --install build                   # ou installation directe : /usr/bin, /usr/share/nhill, .desktop, icônes
```

### 1.3 Paquets Debian

`--deb` (ou `cd build && cpack -G DEB`) produit quatre paquets, un par application.
Les dépendances (bibliothèques Qt, modules QML, libpqxx…) sont déclarées et résolues
par `apt` :

```bash
sudo apt install ./nhill-server_0.1.0_amd64.deb      # serveur (Linux uniquement)
sudo apt install ./nhill-business_0.1.0_amd64.deb ./nhill-pos_0.1.0_amd64.deb
sudo apt install ./nhill-console_0.1.0_amd64.deb     # réservé aux créateurs de Nhill
```

Le paquet serveur crée les utilisateurs système, `/etc/nhill/server.env` (0600, clé
`NH_SECRET_KEY` générée sur la machine, **aucune base configurée**) et active le
service : le serveur démarre en **mode installation**. Lire le jeton avec
`journalctl -u nhill-server | grep -i -A2 "setup mode"`, puis ouvrir Nhill Console →
« Installer un nouveau serveur » (voir `deploiement-production.md`). Les paquets sont
construits pour Ubuntu 24.04 ; reconstruire sur la distribution cible pour une autre
version.

Options CMake : `NHILL_BUILD_SERVER`, `NHILL_BUILD_CLIENT`, `NHILL_BUILD_TESTS`. Aucune adresse
de serveur n'est compilée dans le client (3.0.0) : elle est saisie au premier lancement,
vérifiée et enregistrée sur l'appareil.

Les tests serveur démarrent un PostgreSQL 16 jetable (`initdb`/`pg_ctl` de
`/usr/lib/postgresql/16/bin`) ; exécutés en root, ils utilisent l'utilisateur `postgres`.

### 1.4 Client sans trousseau

Si QtKeychain est absent, l'application fonctionne mais ne mémorise pas la session
(aucun stockage du jeton en clair, par conception). Sous Linux, le trousseau passe par
Secret Service (GNOME Keyring, KWallet).

## 2. Environnement de développement

```bash
NH_DEV_STATE=/tmp/nhdev scripts/dev/dev-stack.sh start build   # PostgreSQL :55433 + API 127.0.0.1:8080
python3 scripts/dev/seed-demo.py --email demo@example.com        # établissement de DÉMONSTRATION
NHILL_SERVER=http://127.0.0.1:8080 build/client/nhill-business
NHILL_SERVER=http://127.0.0.1:8080 build/client/nhill-pos
NH_DEV_STATE=/tmp/nhdev scripts/dev/dev-stack.sh stop
```

* Données de démonstration : toutes nommées « (démo) », identifiants et taux fictifs
  (« Valeur de démonstration — non officielle »). Le script refuse un serveur non local.
* Console d'administration en développement :
  `build/server/nhill-server --config /tmp/nhdev/server.env admin-bootstrap admin@exemple.dev "Admin"`,
  puis « Premier accès » dans `build/client/nhill-console`
  (`scripts/dev/admin-session.py` automatise ces étapes pour les tests seulement).
* Variables utiles du client : `NHILL_SERVER`, `NHILL_LANG=fr|en`,
  `NHILL_THEME=light|dark|system`. Captures automatiques : `NHILL_TOUR_DIR`,
  `NHILL_TOUR_EMAIL`, `NHILL_TOUR_PASSWORD`, `NHILL_TOUR_SIZE=390x844`
  (avec `QT_QPA_PLATFORM=offscreen`).

## 3. Windows (non vérifié)

Prérequis : Visual Studio 2022 (C++), CMake ≥ 3.21, Ninja, Qt 6.5+ MSVC 64 bits,
QtKeychain (`vcpkg install qtkeychain-qt6:x64-windows`).

```powershell
.\scripts\build-windows.ps1 -QtDir C:\Qt\6.5.3\msvc2019_64 -VcpkgToolchain C:\vcpkg\scripts\buildsystems\vcpkg.cmake
```

Le script compile `nhill-business.exe`, `nhill-console.exe` et `nhill-pos.exe`,
rassemble le runtime Qt de chacun avec `windeployqt` (`build-windows\dist\<application>`),
puis, si Inno Setup 6 (`ISCC.exe`) est dans le `PATH`, produit un installateur par
application (`nhill-pos-0.1.0-setup.exe`…). Icônes : `client/resources/nhill-*.rc`.
Signer exécutables et installateurs (`signtool`) avant distribution.

**Recette à effectuer sur Windows** (rien de cela n'a pu être fait ici) : installation et
désinstallation par l'installateur ; compilation
MSVC sans avertissement bloquant ; démarrage ; connexion HTTPS à un serveur avec
certificat valide et refus d'un certificat invalide ; mémorisation de la session dans le
Gestionnaire d'identification ; téléchargement et ouverture d'un PDF ; import CSV par la
boîte de dialogue native ; affichage HiDPI 150 % ; mode sombre ; raccourcis clavier.

## 4. Android (non compilé)

Seule Nhill Business est construite pour Android (Nhill Console et Nhill POS sont des
applications de bureau).

Prérequis : Qt 6.5+ pour Android arm64-v8a et Qt hôte de même version, SDK Android
(plateforme 34), NDK r25+, JDK 17, QtKeychain compilé pour Android (Android Keystore).

```bash
QT_ANDROID=~/Qt/6.5.3/android_arm64_v8a QT_HOST=~/Qt/6.5.3/gcc_64 \
ANDROID_SDK_ROOT=~/Android/Sdk ANDROID_NDK_ROOT=~/Android/Sdk/ndk/25.2.9519653 scripts/build-android.sh
```

`client/android/` : `AndroidManifest.xml` (INTERNET, ACCESS_NETWORK_STATE,
`usesCleartextTraffic=false`, `allowBackup=false`), icônes adaptatives. Signer l'APK/AAB
avec la clé de publication (`apksigner`) ; ne jamais versionner le keystore.

En émulateur, le serveur de développement est joignable sur `http://10.0.2.2:8080`
(seule exception HTTP admise par le client) — le manifeste interdit le trafic en clair :
utiliser un certificat de développement ou autoriser temporairement le trafic en clair
dans une configuration de débogage, jamais en version publiée.

**Recette à effectuer sur Android** : installation sur Android 7 et 14 ; clavier
virtuel (champs non masqués, `adjustResize`) ; tiroir de navigation ; tableaux en cartes ;
partage/ouverture des PDF téléchargés ; rotation ; perte de réseau pendant une émission
(bouton « Réessayer », pas de double facture) ; stockage de session dans le Keystore.

## 5. Serveur

Voir `docs/deploiement-production.md`.
