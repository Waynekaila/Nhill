# Dépendances et licences (Nhill 3.0)

*Composants tiers et versions réellement utilisées par les constructions de la version
3.0.0 (intégration continue du 28 octobre 2026, manifestes `logs/deps-manifest-*.txt` des
branches `ci-results/*`). Les licences sont celles publiées par chaque projet ; à faire
confirmer par un juriste avant une distribution commerciale, en particulier pour Qt.*

## Serveur (paquets Linux)

| Composant | Ubuntu 24.04 | Debian 13 | Licence | Usage |
|---|---|---|---|---|
| PostgreSQL (serveur de la base) | 16 | 17 | PostgreSQL License | base de données (15 ou plus récent requis) |
| libpqxx | 7.8.1 | 7.10.0 | BSD-3-Clause | client PostgreSQL C++ |
| cpp-httplib | 0.14.3 (voir audit de sécurité, R1) | 0.18.7-1+deb13u1 | MIT | serveur HTTP |
| libsodium | 1.0.18 | 1.0.18 | ISC | Argon2id, chiffrement, HMAC, aléa |
| OpenSSL (libcrypto) | 3.0.13 | 3.5.7 | Apache-2.0 | HMAC-SHA1 pour TOTP (RFC 6238) |
| libharu | 2.3.0 | 2.3.0 | zlib/libpng | génération des PDF |
| libpng | 1.6.43 | 1.6.48 | libpng License | décodage PNG avant PDF |
| libcurl | 8.5.0 | 8.14.1 | curl License | envoi des e-mails (SMTP avec TLS) |
| nlohmann/json | 3.11.3 | 3.11.3 | MIT | JSON |
| GoogleTest | 1.14.0 | 1.16.0 | BSD-3-Clause | tests (non distribué) |

## Applications

| Composant | Version | Licence | Remarque |
|---|---|---|---|
| Qt 6 (Core, Gui, Network, Qml, Quick, QuickControls2, Svg) | Linux : celle de la distribution (6.4.2 sur Ubuntu 24.04, 6.8.2 sur Debian 13) ; Windows et Android : 6.5.3 | LGPL-3.0 (ou commerciale) | liaison **dynamique** ; notice LGPL fournie ; Qt 6.5.3 : voir l'audit de sécurité, R2 |
| QtKeychain | Linux : paquet de la distribution (0.14.2 sur Ubuntu 24.04, 0.15.0 sur Debian 13) ; Windows et Android : 0.17.0 compilée depuis la source vérifiée, liée statiquement | BSD-3-Clause | notice BSD dans `packaging/notices/THIRD-PARTY-NOTICES.txt`, installée à côté de chaque application Windows et incluse dans l'APK (`assets/THIRD-PARTY-NOTICES.txt`, présence vérifiée par `scripts/build-android.sh`) |

## Ressources

| Ressource | Licence | Remarque |
|---|---|---|
| Polices IBM Plex Sans et Mono | SIL Open Font License 1.1 | `assets/fonts/OFL.txt` ; intégrées dans les PDF et les applications ; nom réservé « Plex » |
| Logo, icônes Nhill | création du projet | `scripts/brand/build_brand.py` |
| Icônes d'interface (`Icon.qml`) | création du projet | tracés originaux |

## Outils de construction (non distribués)

CMake, Ninja, GCC (Linux), MSVC 2022 et NSIS (Windows), Android SDK, NDK r25c et JDK 17
(Android), Python 3, pandoc et Chromium (PDF), `age` pour le chiffrement des sauvegardes
(BSD-3-Clause).

## Obligations lors de la distribution

1. Joindre les textes de licence (Qt LGPL-3.0, OFL-1.1, BSD/MIT/ISC des bibliothèques).
2. Qt : ne pas lier statiquement les applications sans licence commerciale ; indiquer
   comment obtenir le code source de la version de Qt utilisée.
3. Ne pas modifier les polices IBM Plex sous le nom « Plex ».
