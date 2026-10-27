# Dépendances et licences

Versions effectivement utilisées pour la construction et les tests (Ubuntu 24.04,
25 octobre 2026). Les licences sont celles publiées par chaque projet ; à faire
confirmer par un juriste avant distribution commerciale, en particulier pour Qt.

## Serveur

| Composant | Version testée | Licence | Usage |
|---|---|---|---|
| PostgreSQL | 16.15 | PostgreSQL License (permissive) | Base de données |
| libpqxx | 7.8.1 | BSD-3-Clause | Client PostgreSQL C++ |
| cpp-httplib | 0.14.3 | MIT | Serveur HTTP |
| libsodium | 1.0.18 | ISC | Argon2id, chiffrement, HMAC, KDF, aléa |
| OpenSSL (libcrypto) | 3.0.13 | Apache-2.0 | HMAC-SHA1 pour TOTP (RFC 6238) |
| libharu | 2.3.0 | zlib/libpng | Génération PDF |
| libpng | 1.6.43 | libpng License | Décodage PNG sécurisé avant PDF |
| nlohmann/json | 3.11.3 | MIT | JSON |
| libcurl | 8.5.0 (Ubuntu 24.04) | curl License (MIT/X) | 2.1 : envoi des e-mails par SMTP avec TLS |
| GoogleTest | 1.14.0 | BSD-3-Clause | Tests (non distribué) |

## Client

| Composant | Version testée | Licence | Remarque |
|---|---|---|---|
| Qt (Core, Gui, Network, Qml, Quick, QuickControls2, Svg, Linguist) | 6.4.2 | LGPL-3.0 (ou commerciale) | Lier **dynamiquement**, fournir la notice LGPL et permettre le remplacement des bibliothèques Qt ; sur Android, vérifier les obligations LGPL avec l'APK (ou licence commerciale Qt) |
| QtKeychain | 0.14.2 (paquets Linux de la distribution) ; 0.17.0 compilée depuis la source vérifiée pour Windows et Android (2.1) | BSD-3-Clause | Trousseau du système (Windows Credential Manager, Secret Service/KWallet, Android Keystore). **Liée statiquement** sous Windows et Android : la notice BSD doit accompagner le binaire — `packaging/notices/THIRD-PARTY-NOTICES.txt`, installé à côté de chaque application Windows ; **reste à faire pour l'APK** (écran « À propos » ou fichier dans l'application) |

## Ressources

| Ressource | Licence | Remarque |
|---|---|---|
| Polices IBM Plex Sans / Mono | SIL Open Font License 1.1 | `assets/fonts/OFL.txt` ; intégrées dans les PDF et l'application, nom réservé « Plex » |
| Logo, icônes Nhill | Création du projet | Générés par `scripts/brand/build_brand.py` |
| Jeu d'icônes d'interface (`Icon.qml`) | Création du projet | Tracés originaux sur grille 24×24 |

## Outils de construction (non distribués)

CMake 3.28, Ninja 1.11, GCC 13.2, Python 3 (fontTools, Pillow) et rsvg-convert pour la
génération des ressources de marque, age 1.1.1 pour le chiffrement des sauvegardes
(BSD-3-Clause).

## Obligations à respecter lors de la distribution

1. Joindre les textes de licence (Qt LGPL-3.0, OFL-1.1, BSD/MIT/ISC des bibliothèques).
2. Qt : ne pas lier statiquement l'application sans licence commerciale ; documenter
   comment obtenir le code source de Qt utilisé.
3. Ne pas modifier les polices IBM Plex sous le nom « Plex ».
