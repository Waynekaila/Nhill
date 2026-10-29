# Rapport de validation — Nhill 3.0.0

*English: [../en/validation-report.md](../en/validation-report.md)*

## Version 3.0.0 — formules, documents, données, mise en service

| | |
|---|---|
| Date | 29 octobre 2026 |
| Version | 3.0.0 (`project(nhill VERSION 3.0.0)`), branche `release/3.0.0`, non publiée |
| Commit du code validé | `18d7054` (les commits suivants ne modifient que la documentation) |

### Tests exécutés (intégration continue du commit `18d7054`)

| Suite | Où | Résultat | Preuve |
|---|---|---|---|
| `common` | CI Ubuntu 24.04 | **22/22** | [exécution 40826508382](https://github.com/Waynekaila/Nhill/actions/runs/40826508382), `ci-results/ci` |
| `server` (formules et droits pour chaque formule et chaque rôle, limites, accès calculé, documents, caisse, suppression d'un établissement, conservation, recherches, mot de passe oublié…) | CI Ubuntu 24.04 (PostgreSQL 16) ; espace de travail | **121 réussis, 1 ignoré** (`Samples.GenerateDocumentSamples`) sur 122 | idem |
| `setup`, `client` | CI | **6/6**, **15/15** | idem |
| `keychain` — Secret Service réel | CI Ubuntu 24.04 | **3/3** (+ init/nettoyage) | `ci-results/ci`, `logs/keychain-linux.txt` |
| `keychain` — Gestionnaire d'identification Windows réel | CI `windows-2022` | **3/3** | [exécution 40826508391](https://github.com/Waynekaila/Nhill/actions/runs/40826508391) |
| `keychain` — Keystore Android | émulateur Android 11 (CI) | phases `write`, `read`, `logout`, `forget` : **OK** | [exécution 40826508406](https://github.com/Waynekaila/Nhill/actions/runs/40826508406), `ci-results/android-keychain` |
| Paquets `.deb` Ubuntu 24.04 et Debian 13, installation par `apt` puis désinstallation | CI (conteneurs propres) | **réussis** | [exécution 40826508393](https://github.com/Waynekaila/Nhill/actions/runs/40826508393) |
| Kit VPS : `provision.sh` sur une machine Ubuntu 24.04 neuve, puis recette | CI | **12/12 contrôles** (ports locaux, pare-feu, secrets, TLS et HSTS par Caddy, surveillance, sauvegarde chiffrée restaurée hors du serveur, mise à jour, retour arrière, refus d'une base plus récente, premier administrateur) | `ci-results/vps-test` |
| Installateurs Windows, installation silencieuse | CI `windows-2022` | réussis | `ci-results/windows` |
| APK : notices tierces présentes | CI | `assets/THIRD-PARTY-NOTICES.txt` présent | `ci-results/android` |
| Références de l'API et de la base à jour | CI | réussi | `ci-results/ci`, `ci-results/docs-website` |
| PDF à jour, site vitrine, backend des formulaires (SQLite, PostgreSQL) | CI | réussis ; 24 pages, 0 problème | [exécution 40826508436](https://github.com/Waynekaila/Nhill/actions/runs/40826508436) |
| Contrôle QML | espace de travail, CI | 100 fichiers, 0 problème, 0 texte non traduit | `scripts/qa/qml-check.py` |
| Domaine `nhill.online` | CI | **échec attendu** : page de parking d'Hostinger, aucun MX, SPF ni DMARC | `ci-results/domain-check` |

Empreintes SHA-256 des livrables construits par la CI (`18d7054`) :

```
fe68846cf61ff330f5ca8f332916d38685417bba272f4213c14f6d82cc19da9b  windows/nhill-business-3.0.0-setup.exe
9d5d55a841850ee9dc3a8d8911fd84398f0a01b10c472384e1dbb65b899be763  windows/nhill-pos-3.0.0-setup.exe
eb654f3bfa0ef7e39785f03c8bacc8f466e281556744686f8ca6d3da8aa498e6  windows/nhill-console-3.0.0-setup.exe
500d06e310fe7a68d5b29f5a2a5ddab5c6055b54274300eb10f671fde573c0e8  android/nhill-business-3.0.0-arm64-v8a.apk
456ca24392c57fd860b3096b05d564c0e828b1afad97df6c61be2bd2d542757f  linux/ubuntu-24.04/nhill-server_3.0.0-1_amd64.deb
f9cddb2f63854b65423757d521dd2eea97d4fc3a287df904b8ae23c9d4366c0f  linux/debian-13/nhill-server_3.0.0-1_amd64.deb
cb60e13fa18e09f1430f9d983bb86bdd6fee141c4fe5e48adeae67fa30d34c87  website/nhill-website-3.0.0.tar.gz
```

Liste complète : `release/V3.0.0/checksums/SHA256SUMS`.

### Anomalies trouvées et corrigées pendant la validation 3.0

| Anomalie | Correction |
|---|---|
| Suppression d'un établissement promise par les conditions mais impossible (documents et journal immuables pour l'application) | migration 0014, fonction de suppression contrôlée, écran de la console, tests |
| `%` et `_` interprétés dans les recherches | migration 0015, `nh_like_contains` |
| Recette VPS : service PostgreSQL « parapluie » inactif, certificat interne à 0 jour, test de restauration illisible pour `nobody`, empreinte de sauvegarde avec chemin absolu | contrôle par `pg_isready`, seuil adapté, copie du script, empreinte sur le nom du fichier |
| Captures POS 1366×768 absentes (affichage Xvfb pas prêt) | attente du socket X avant le lancement |
| Test du Keystore Android : « Can't find service: activity » juste après le démarrage | attente du service et nouvelles tentatives des commandes `am` |
| APK sans la notice BSD de QtKeychain (liée statiquement) | notices incluses et vérifiées à la construction |
| Dossier commercial : champs entre crochets, lignes de suivi fictives, offre « pilote 8 semaines » périmée | champs de fusion déclarés, fichiers vides, essai de 30 jours partout |

### Non testé

- Déploiement réel sur le VPS Hostinger (aucun accès) ; site `nhill.online` non publié.
- Téléphone Android réel, poste Windows 10/11 d'utilisateur, envoi d'e-mails par un vrai relais SMTP.
- Mise à jour d'une base 2.1 contenant de vraies données vers 0012–0015 (les tests appliquent 0001–0015 sur une base vide).
- Masquage des fonctions dans l'interface pour chaque formule (vérifié côté serveur par les tests `Plans.*` ; côté applications par lecture du code et contrôle statique).

### Décisions et actions humaines en attente

- R1 (cpp-httplib du paquet Ubuntu 24.04) et R2 (Qt 6.5.3 sous Windows et Android) : choix de la correction ([03-audit-securite.md](03-audit-securite.md) § 4).
- Hébergeur du site vitrine (mentions légales), adresse d'expéditeur des e-mails automatiques, enregistrements DNS de messagerie (MX, SPF, DMARC).
- Certificats de signature (Authenticode, clé Android de publication), relecture juridique (points V-01 à V-14), audit externe.

---

## Version 2.1.0 — session mémorisée et mot de passe oublié

| | |
|---|---|
| Date | 27 octobre 2026 |
| Version | 2.1.0 (`project(nhill VERSION 2.1.0)`), branche `release/2.1.0`, non publiée |
| Commit du code validé | `f8524fdd048c7f8462165925aae30d9b4d4357f1` (les commits suivants ne modifient que la documentation, le site vitrine et les documents commerciaux) |

### Tests exécutés

| Suite | Où | Résultat | Preuve |
|---|---|---|---|
| `common` | CI Ubuntu 24.04, espace de travail | **22/22** | [exécution 40541503872](https://github.com/Waynekaila/Nhill/actions/runs/40541503872) |
| `server` (dont 16 tests du mot de passe oublié : réinitialisation par l'administrateur, code par e-mail, envoi SMTP vers un faux relais local, configuration SMTP) | CI Ubuntu 24.04 (PostgreSQL 16) ; espace de travail | **99 réussis, 1 ignoré** (`Samples.GenerateDocumentSamples`) sur 100 | idem |
| `setup`, `client` | CI | **6/6**, **13/13** | idem |
| `keychain` — Secret Service réel (gnome-keyring, deux processus) | CI Ubuntu 24.04 | **5/5** (écriture puis relecture dans un second processus, déconnexion, rien de stocké sans « Se souvenir de moi ») | `ci-results/ci`, `logs/keychain-linux.txt` |
| `keychain` — Gestionnaire d'identification Windows réel, `NHILL_KEYCHAIN_REQUIRED=1` | CI `windows-2022` | **5/5** | [exécution 40541504066](https://github.com/Waynekaila/Nhill/actions/runs/40541504066), `keychain-test-log.txt` |
| QtKeychain lié à chaque application Windows | CI `windows-2022` | `CredReadW`, `CredWriteW`, `CredDeleteW` importés d'ADVAPI32 ; aucune DLL QtKeychain | idem |
| Installation silencieuse et désinstallation (Nhill POS, Nhill Business) | CI `windows-2022` | réussies ; exécutable installé identique à celui vérifié (SHA-256) | idem |
| `keychain` — Keystore Android réel | émulateur Android 11 x86_64 (CI) | phases `write`, `read` (nouveau lancement), `logout`, `forget` : **OK** | [exécution 40541503866](https://github.com/Waynekaila/Nhill/actions/runs/40541503866), `ci-results/android-keychain` |
| APK Nhill Business | CI | construit ; code Keystore de QtKeychain présent dans la bibliothèque | idem |
| Paquets `.deb` Ubuntu 24.04 et Debian 13, installation propre par `apt` puis désinstallation | CI (conteneurs `ubuntu:24.04`, `debian:trixie`) | **réussis** | [exécution 40541504028](https://github.com/Waynekaila/Nhill/actions/runs/40541504028), `ci-results/deb-*`, `ci-results/install-*` |
| Contrôle statique QML | espace de travail | 98 fichiers, 0 problème, 0 texte non traduit | `scripts/qa/qml-check.py` |
| Site vitrine | espace de travail | 24 pages, 0 problème ; contraste 0 échec (16 pages × 2 largeurs) | `check_site.py`, sondes d'accessibilité |
| Modèle financier | espace de travail | documents à jour | `subscription_model.py --check` |

Empreintes des installateurs Windows construits par la CI (`f8524fd`) :

```
e7b12542145330b7f9f5afc1131bd1ba6ed66660773969462552a88a37108b28  nhill-business-2.1.0-setup.exe
fd7048f0b4d6ec1031019b21242ea21b16cd6c3a8bded5a53e8a01b3ff2837b0  nhill-console-2.1.0-setup.exe
64e0493dc921090209b6a459c021da9c76f11ec4835159711a442d5c843278d3  nhill-pos-2.1.0-setup.exe
```

### Anomalies trouvées et corrigées pendant la validation 2.1

| Anomalie | Correction |
|---|---|
| `windeployqt` prenait `qt6keychain.dll` pour un module Qt et la cherchait dans le dossier de Qt | QtKeychain liée statiquement sous Windows ; contrôle des imports |
| Programme de test Windows sans console (sous-système graphique par défaut de `qt_add_executable`) : résultats perdus | `WIN32_EXECUTABLE FALSE` ; journal QtTest écrit dans un fichier |
| Cibles importées (OpenGL, DBus, Qt6Keychain) absentes dans `tests/keychain` | mêmes `find_package` que `client/` |
| `QSKIP` appelé depuis une fonction auxiliaire ; phase « déconnexion » sans session stockée | macro dans la fonction de test ; phase ignorée sauf si le stockage est exigé |
| Émulateur Android : AVD introuvable | dossier des AVD explicite |

### Non testé

- Téléphone Android réel (arm64) et poste Windows 10/11 d'utilisateur (seulement l'émulateur et le serveur Windows 2022 de la CI).
- Envoi d'e-mails par un vrai fournisseur SMTP (relais local de test seulement).
- Mise à jour d'une base 2.0 contenant de vraies données vers la migration 0011 (les tests appliquent 0001–0011 sur une base vide) ; retour arrière vers la 2.0.
- Client 2.0 face au serveur 2.1 ; parcours complets à l'écran (compilation et contrôle statique seulement pour les nouveaux dialogues).

### Limites connues de la 2.1

- Le Gestionnaire d'identification Windows conserve la session après une désinstallation tant que l'utilisateur ne s'est pas déconnecté.
- La compilation croisée MinGW ne contient pas QtKeychain : session jamais mémorisée.
- Notice de licence QtKeychain installée avec les applications Windows, pas encore affichée dans l'APK.
- Transport `log` : aucun code n'est remis ; `NH_EMAIL_TRANSPORT=smtp` est nécessaire en production.
- Réinitialisation du mot de passe d'un compte de la console : hors périmètre (procédure existante des créateurs).

# Validation 2.0.0

| | |
|---|---|
| Date | 27 octobre 2026 |
| Version | 2.0.0 (`project(nhill VERSION 2.0.0)`) |
| Dépôt | `Waynekaila/Nhill` (GitHub, privé), branches `main` et `release/2.0.0` |
| Commit des livrables | `1c0f30c2fd33d6ac37b852f1f68adb9b154b2e97` |
| Principe | ce rapport ne mentionne que ce qui a été exécuté ; tout le reste figure dans « Non testé » (§ 5) |

## 1. Où les vérifications ont eu lieu

| Environnement | Rôle | Limites |
|---|---|---|
| Espace de travail de la session (Linux, sans Qt ni libpqxx de développement) | modifications du code, tests autonomes `common`, contrôle statique QML, site vitrine, backend des formulaires (SQLite), schéma PostgreSQL 16 local, documentation, PDF | impossible d'y compiler le serveur et les applications Qt |
| GitHub Actions (`ubuntu-24.04`, conteneurs `ubuntu:24.04` et `debian:trixie`, `windows-2022`) | compilation complète, tests, paquets, installation de contrôle | machines virtuelles de CI, pas de poste d'utilisateur réel |

Les résultats de chaque job sont publiés dans des branches `ci-results/*` du dépôt (fichier
`RUN-INFO.txt` : workflow, état, commit, lien de l'exécution).

## 2. Tests exécutés et résultats

| Suite | Où | Résultat | Preuve |
|---|---|---|---|
| `common` (montants, taxes, codes, mots de passe, entier 128 bits) | CI Ubuntu 24.04 et Debian 13 ; espace de travail (outil autonome) | **22/22** réussis ; aussi 22/22 en mode `NHILL_PORTABLE_INT128` (chemin Windows) dans l'espace de travail | [exécution 40464011312](https://github.com/Waynekaila/Nhill/actions/runs/40464011312), `Testing/Temporary/LastTest.log` |
| `server` (API + PostgreSQL jetable) | CI Ubuntu 24.04 (PostgreSQL 16), Debian 13 (PostgreSQL 17) | **83 réussis, 1 ignoré** (`Samples.GenerateDocumentSamples`, génération d'exemples désactivée par défaut) sur 84 | idem |
| `setup` (mode installation) | CI | **6/6** | idem |
| `client` (Qt Test) | CI | **13/13** | idem |
| Contrôle statique QML | espace de travail + CI | 95 fichiers, 0 problème, 0 texte non traduit | `scripts/qa/qml-check.py` |
| Backend du site (SQLite) | espace de travail + CI | **13/13** | `website/backend/tests` |
| Backend du site (PostgreSQL 16, rôles réels) | CI | **réussi** (13 tests + refus de lecture par le rôle applicatif) | [exécution 40461610891](https://github.com/Waynekaila/Nhill/actions/runs/40461610891) |
| Pages du site | espace de travail | 24 pages, 0 problème (liens, `alt`, titres, paires FR/EN, secrets) | `website/tools/check_site.py` |
| Classeur des indicateurs | espace de travail | 91 formules, 0 erreur ; valeurs contrôlées avec des données d'essai | recalcul LibreOffice |
| Serveur du site (fumée) | espace de travail | `/api/health` 200, envoi d'un formulaire 201 | journal de la session |

## 3. Livrables construits

| Livrable | Construction | Vérifications | SHA-256 |
|---|---|---|---|
| `linux/ubuntu-24.04/nhill-business_2.0.0-1_amd64.deb` | CI, conteneur `ubuntu:24.04` | lintian 0 erreur ; installation `apt` dans un conteneur propre, lancement hors écran, désinstallation | `d46cadeb0701b6ff823ca65e8533116c6b1a372cd6a8bbbe078536cfa06d1c89` |
| `linux/ubuntu-24.04/nhill-console_2.0.0-1_amd64.deb` | idem | idem | `f26aa9f414343b8a80156b44f9891cce3e58614a060783caa4f9ccb438103e63` |
| `linux/ubuntu-24.04/nhill-pos_2.0.0-1_amd64.deb` | idem | idem | `c67f88b6af4c99bcf1ba3a8a3e6921e21c17e8ced11e41f0d6dbe2d2e1c34d3d` |
| `linux/ubuntu-24.04/nhill-server_2.0.0-1_amd64.deb` | idem | lintian 0 erreur, 7 avertissements ; installé dans le conteneur propre | `997c29c8c44cfe444ddc02bb21f2e2a0bcb6224339e76ab0222ecd2f8cba77ae` |
| `linux/debian-13/nhill-business_2.0.0-1_amd64.deb` | CI, conteneur `debian:trixie` (Qt 6.8 de Debian) | installation, lancement, désinstallation dans un conteneur propre | `63ce71aad7a5b67208384aaf1e740d4915561d978659183732e4ab949fe1d1d4` |
| `linux/debian-13/nhill-console_2.0.0-1_amd64.deb` | idem | idem | `579f363770d21e3c5660e920551d8f3864e03b084e3aea5f2ce8433a37f2bf87` |
| `linux/debian-13/nhill-pos_2.0.0-1_amd64.deb` | idem | idem | `ce804cd61b037b515a50d83ff1f513ca204798dea876b9ff58fc35d2a69f55b9` |
| `linux/debian-13/nhill-server_2.0.0-1_amd64.deb` | idem | installé | `dc05552506a6b7cfcfe787d6f7abc9ffd8fb078e0efa4500b58939f154cc13d8` |
| `windows/nhill-business-2.0.0-setup.exe` | CI `windows-2022`, MSVC 2022, Qt 6.5.3, NSIS | format PE vérifié | `5a224a1b21f439cf6a63aba074830de8a3df56d6845e1ac80604ada7aeac2a1b` |
| `windows/nhill-console-2.0.0-setup.exe` | idem | format PE vérifié | `e1f1c768b593a4097598e22e1c1a0e523b82c09de9c30b536638318b6b7e81b9` |
| `windows/nhill-pos-2.0.0-setup.exe` | idem | format PE ; **installation et désinstallation silencieuses réussies** sur le runner | `b95c1dd8fae02f48157606637b1b185486abf8d04e9a5153bb4039027c5cdd07` |
| `android/nhill-business-2.0.0-arm64-v8a.apk` | CI, Qt 6.5.3 pour Android, NDK r25c, plate-forme 34 | contenu (manifeste, bibliothèques arm64-v8a), signature **de débogage** vérifiée par `apksigner` | `70c9664ad39baeadb61885d8d2ead5a6de19bb33d13d856f50d475e75f2fdf01` |
| Site vitrine (sources + archive) | espace de travail | contrôles du § 2 | `release/V2.0.0/checksums/SHA256SUMS` |
| PDF de la documentation, du juridique et du marketing | espace de travail (pandoc + Chromium) | `scripts/build-docs-pdf.sh --check` | `docs/pdf/manifest.json` |

## 4. Anomalies trouvées et corrigées pendant la validation

| Anomalie | Correction |
|---|---|
| Compilation Windows impossible : `__int128` inconnu de MSVC dans les calculs monétaires | entier 128 bits portable (`common/include/nhill/int128.hpp`), comparé à `__int128` sur 20 000 opérations ; `ExchangeRate::scaled` en `int64_t` |
| Paquets `.deb` : dossier de compilation dans le code source refusé ; journaux comptés dans l'empreinte du source | compilation hors du source, `build/`, `dist/`, `release/` exclus de l'empreinte |
| Debian 13 : greffon SVG dans un paquet séparé, recherche de paquet bloquante | installation et dépendance `qt6-svg-plugins`, recherche tolérante |
| Windows : `Get-FileHash` indisponible dans PowerShell 5.1 lancé depuis pwsh ; accents altérés | empreintes par .NET ; script enregistré en UTF-8 avec BOM |
| Android : règle d'installation CMake invalide pour la cible ; plate-forme 36 du runner illisible par Gradle 7 | règle limitée au bureau ; plates-formes > 34 retirées en CI |
| Console : pas de contrôle du mot de passe avant envoi à la première connexion | contrôle par la règle partagée (8 caractères) |

## 5. Non testé (à faire par des personnes, sur du matériel réel)

| Élément | Pourquoi | Action |
|---|---|---|
| Installateurs Windows sur un poste Windows 10/11 d'utilisateur (SmartScreen, antivirus, HiDPI 150 %, impression de tickets) | seulement le runner CI | recette manuelle (`docs/fr/guide-installation.md` § 3.2) |
| Nhill Business et Nhill Console installés par leurs installateurs Windows | seul Nhill POS est installé en CI | recette manuelle |
| APK sur des téléphones/tablettes Android 7 et 14 | aucun appareil | recette manuelle ; puis signature de publication |
| Mémorisation de session sous Windows et Android | QtKeychain non intégré aux constructions Windows/Android de la CI : la session n'y est pas mémorisée | intégrer QtKeychain (vcpkg / compilation Android) puis tester |
| Déploiement de production avec TLS public, Supabase de production, sauvegardes, restauration | pas d'infrastructure de production dans la session | `docs/fr/guide-installation.md` § 4 |
| Paiements réels (Stripe, PayPal) | adaptateurs testés contre des réponses simulées seulement | paiement de test avec un compte du prestataire |
| Captures de Nhill POS 2.0 | la visite automatique atteint désormais le choix de la caisse de démonstration (créée par `seed-demo.py`) mais n'ouvre pas la session : les écrans de vente 2.0 ne sont pas capturés automatiquement | adapter la visite (`PosMain.qml`) ou capturer à la main ; captures Business 2.0 dans `docs/assets/v2/` |
| Accessibilité avec un lecteur d'écran (NVDA, Orca, TalkBack) | non disponible | test manuel |
| Juridique | aucune validation par un avocat | `legal/fr/00-recherche-juridique.md` § 6 |
| Audit externe, test d'intrusion | non commandés | avant ouverture large |

## 6. Limites connues de la version

- Pas de réinitialisation du mot de passe oublié (pas d'envoi d'e-mails).
- cpp-httplib 0.14 (lié dynamiquement au paquet de la distribution) : CVE-2025-46728 et
  CVE-2025-53629 ; atténuation par Caddy ; mise à niveau ≥ 0.23.0 recommandée.
- Journal d'activité : certaines valeurs de détail s'affichent sous forme brute (ex.
  « Montant : 95000.00 », « type : invoice ») — constaté sur les captures 2.0.
- Nhill n'est pas homologué pour la facture normalisée DGI.
- Exigence de 8 caractères inférieure à la recommandation NIST (15) pour un mot de passe
  sans second facteur : risque accepté, à consigner (`docs/fr/securite.md` § 3).

## 7. Actions humaines indispensables avant le lancement commercial

1. Créer la société exploitante ; compléter tous les champs `[…]` (`docs/fr/parametres-legaux.md`).
2. Faire valider les 19 modèles juridiques par un avocat ; vérifier l'autorité de
   protection des données compétente (ARPTC / ARPTIC) ; formalités et transferts.
3. Acquérir un certificat de signature de code Windows et créer la clé Android de
   publication (secrets `ANDROID_KEYSTORE_*`) ; signer, puis republier les empreintes.
4. Recette manuelle sur postes Windows, Linux et appareils Android réels.
5. Choisir l'hébergement de production ; déployer serveur, Caddy, sauvegardes ; tester une
   restauration.
6. Fixer les prix après le programme pilote ; les saisir dans Nhill Console.
7. Faire réaliser un test d'intrusion externe avant un déploiement large.
8. Vérifier la disponibilité de la marque « Nhill » et la déposer.
