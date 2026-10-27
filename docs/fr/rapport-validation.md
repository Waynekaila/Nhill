# Rapport de validation — Nhill 2.0.0

*English: [../en/validation-report.md](../en/validation-report.md)*

| | |
|---|---|
| Date | 27 octobre 2026 |
| Version | 2.0.0 (`project(nhill VERSION 2.0.0)`) |
| Dépôt | `Waynekaila/Nhill` (GitHub, privé), branches `main` et `release/2.0.0` |
| Commit des livrables | `1389b46a5b5f97adc5b7e15f7906a470d43f81b2` |
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
| `common` (montants, taxes, codes, mots de passe, entier 128 bits) | CI Ubuntu 24.04 et Debian 13 ; espace de travail (outil autonome) | **22/22** réussis ; aussi 22/22 en mode `NHILL_PORTABLE_INT128` (chemin Windows) dans l'espace de travail | [exécution 40461610927](https://github.com/Waynekaila/Nhill/actions/runs/40461610927), `Testing/Temporary/LastTest.log` |
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
| `linux/ubuntu-24.04/nhill-business_2.0.0-1_amd64.deb` | CI, conteneur `ubuntu:24.04` | lintian 0 erreur ; installation `apt` dans un conteneur propre, lancement hors écran, désinstallation | `0cf37139440533d5961749118536e0163ab7f018d47db1653b254510988e1995` |
| `linux/ubuntu-24.04/nhill-console_2.0.0-1_amd64.deb` | idem | idem | `7545b6883e88cf19dcac418339428df956133fb393b8aa9c5b70a594a5414dc0` |
| `linux/ubuntu-24.04/nhill-pos_2.0.0-1_amd64.deb` | idem | idem | `453c3d554b1457a3315e7f5a7487921778dcfb3ab965532d1529708c93a99f77` |
| `linux/ubuntu-24.04/nhill-server_2.0.0-1_amd64.deb` | idem | lintian 0 erreur, 7 avertissements ; installé dans le conteneur propre | `a0c75e986e6614b887e9b61cc9356971ca8c652896e572a422d93d5839832c5c` |
| `linux/debian-13/nhill-business_2.0.0-1_amd64.deb` | CI, conteneur `debian:trixie` (Qt 6.8 de Debian) | installation, lancement, désinstallation dans un conteneur propre | `5f45dbcf7a730ce20313cb4b1f58f0dfed16b11cf3941847230b794985020fd4` |
| `linux/debian-13/nhill-console_2.0.0-1_amd64.deb` | idem | idem | `f7f87e521ac45c284b18db5cf3a330e8640f004d6ed0d410c9b0c2aa96f936f5` |
| `linux/debian-13/nhill-pos_2.0.0-1_amd64.deb` | idem | idem | `9f934b74718736a81334c1fd7f6979109afd37977fa3445106b681262c04678f` |
| `linux/debian-13/nhill-server_2.0.0-1_amd64.deb` | idem | installé | `13b582cfcd6e89e9b094cc7125d815194acd6e422834d9baa6319617bed162bb` |
| `windows/nhill-business-2.0.0-setup.exe` | CI `windows-2022`, MSVC 2022, Qt 6.5.3, NSIS | format PE vérifié | `1d65aaa1da100550c09d020af2e94a06a99e78a4913e6ef5a862e792892e53e1` |
| `windows/nhill-console-2.0.0-setup.exe` | idem | format PE vérifié | `48c2434afe25fb48678a6af7c247596d7d669af26595d05c180b4ece0dbcfe26` |
| `windows/nhill-pos-2.0.0-setup.exe` | idem | format PE ; **installation et désinstallation silencieuses réussies** sur le runner | `bc812f9fcb4bcc47d94d69e98544941b91bd4f3c3c3a505ee56c350965d76872` |
| `android/nhill-business-2.0.0-arm64-v8a.apk` | CI, Qt 6.5.3 pour Android, NDK r25c, plate-forme 34 | contenu (manifeste, bibliothèques arm64-v8a), signature **de débogage** vérifiée par `apksigner` | `a8156401b2a0cc45fddfad7f6be34da419337bcb6fc8f756e85ac5c1c8b9fba8` |
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
| Captures de Nhill POS 2.0 | la visite automatique s'arrêtait faute de caisse de démonstration (corrigé dans `seed-demo.py`, à reconfirmer en CI) | relancer la CI ; captures Business 2.0 disponibles dans `docs/assets/v2/` |
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
