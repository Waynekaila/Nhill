# Rapport de tests — Nhill 0.1.0

Date : 26 octobre 2026. Environnement : Ubuntu 24.04, GCC 13.2, Qt 6.4.2, PostgreSQL
16.15 (tests) et Supabase PostgreSQL 17.6 (base de production), rendu hors écran
(`QT_QPA_PLATFORM=offscreen`).

## 1. Résultat global

Reconstruction complète depuis l'archive livrée (`scripts/build-linux.sh`) puis `ctest` :

| Suite | Outil | Résultat observé |
|---|---|---|
| `common` — montants, codes, validations | GoogleTest | **18 / 18 réussis** |
| `server` — API + PostgreSQL jetable | GoogleTest | **82 tests : 81 réussis, 1 ignoré** (`Samples.GenerateDocumentSamples`, activé par `NH_SAMPLES_DIR`) |
| `setup` — installation de la base (grappes PostgreSQL dédiées, SCRAM sur TCP) | GoogleTest | **6 / 6 réussis** |
| `client` — formats, validations, règles réseau | Qt Test + faux serveur | **13 / 13 réussis** |

Résultat ctest : `100% tests passed, 0 tests failed out of 4`.

## 2. Scénarios couverts (serveur)

| Domaine | Tests |
|---|---|
| Migrations | appliquées une seule fois ; migration modifiée refusée ; rôle applicatif sans DDL ni contournement RLS ; tout dans le schéma `nhill`, fermé aux autres rôles |
| Établissement, essai, authentification, codes | création entreprise / entrepreneur ; essai non réinitialisable ; rotation des jetons et détection de réutilisation ; code seul sans accès ; erreurs génériques ; rotation atomique |
| **Installation** (`Setup.*`) | validation des paramètres (Supabase : TLS imposé, utilisateur `role.<projet>`) ; fichier de configuration écrit de façon atomique en 0600 ; installation sur PostgreSQL en superutilisateur ; installation **sans superutilisateur** sur une base de type hébergé (rôles `anon`, `authenticated`, `service_role` présents et sans droits) ; refus d'un compte sans `CREATEROLE` ; points d'accès du mode installation (jeton, vérification, application) |
| **Offres et abonnements** (`Billing.*`, `Subscription.*`) | prix par durée et devise ; passage à une offre supérieure immédiat avec crédit au prorata ; offre inférieure payée et appliquée à l'échéance ; codes promo validés, réservés, consommés ; commande gratuite appliquée sans paiement ; commandes impayées expirées et code libéré ; échecs de paiement comptés et notifiés ; essai, rappel, grâce, suspension ; détails de l'abonnement réservés aux administrateurs |
| **Prestataires** (`Providers.*`, `Webhook.*`) | secrets en écriture seule et chiffrés ; Stripe Checkout + webhook signé + relecture de la session (**réponses simulées**) ; PayPal commande + vérification de signature + capture (**réponses simulées**) ; doublon appliqué une fois ; signature, horodatage et montant vérifiés |
| **Console** | actions sur l'abonnement auditées, motif obligatoire, rôles appliqués ; TOTP RFC 6238 ; la console ne lit aucune donnée métier |
| **Assistance** (`Support.*`) | demandes limitées à l'établissement et à l'auteur, notes internes invisibles ; erreurs regroupées par empreinte ; **données personnelles masquées** dans les rapports d'erreur |
| **Point de vente** (`Pos.*`) | plafond de caisses selon l'offre (essai : 1) ; vente à plusieurs paiements avec monnaie rendue, stock décrémenté, ticket PDF ; caissier limité à sa caisse et à sa session ; remise au-delà du plafond → autorisation de superviseur à usage unique ; retours et annulations par avoirs, remboursement, remise en stock, retour sans remboursement explicite remboursé en espèces ; mouvements d'espèces, clôture avec écart, rapport Z ; codes promo de caisse ; comptable : lecture des sessions sans pouvoir vendre ni configurer |
| **Isolation** | données d'un autre établissement inaccessibles ; identifiants POS, tickets, caisses et demandes d'assistance d'un autre établissement inutilisables (404, rien d'écrit) ; fonctions d'agrégats sans fuite |
| Ventes, stock, documents, rapports, traçabilité | totaux exacts ; 24 émissions concurrentes → numéros consécutifs ; coupure réseau sans doublon ; PDF avec mentions ; journal « qui a fait quoi » ; statistiques des créateurs sans donnée nominative |

## 3. Applications (visites d'écran automatisées)

Les visites exécutent de **vraies opérations** contre un serveur de développement avec un
établissement de démonstration (données nommées « (démo) ») :

* **Nhill POS** (10 écrans, `docs/captures/pos-*`) : connexion, catalogue, ticket, paiement
  avec montant remis, **vente enregistrée (monnaie rendue 2 140,00 CDF)**, session,
  ticket, **retour partiel → avoir remboursé**, clôture, **session clôturée avec écart
  de −500,00 CDF**.
* **Nhill Business** (22 écrans) dont « Paramètres → Point de vente » ; **Nhill Console**
  (13 écrans) ; assistant d'installation (5 écrans, `docs/captures/s*`).
* 0 erreur ou avertissement QML dans les journaux après corrections.

## 4. Paquets Debian

Construits par CPack (`scripts/build-linux.sh build --deb`) : `nhill-server`,
`nhill-business`, `nhill-console`, `nhill-pos` (0.1.0, amd64). Vérifiés dans un
**Ubuntu 24.04 minimal vierge** (debootstrap, chroot) :

1. `apt install ./nhill-server_0.1.0_amd64.deb` : dépendances résolues depuis les dépôts
   Ubuntu ; utilisateurs `nhill` et `nhill-backup` créés ; `/etc/nhill/server.env`
   en 0600 avec une `NH_SECRET_KEY` générée sur place et **sans base** ; `migrate` ne
   fait rien ; `serve` démarre en **mode installation** et affiche le jeton.
2. Installation par l'API (`/api/v1/setup/check` puis `/apply`) sur le PostgreSQL 16 du
   chroot : rôles créés, 10 migrations, **10 contrôles de vérification réussis**,
   configuration écrite, redémarrage automatique, `/health` → `ok`.
3. `apt install` des trois applications : dépendances Qt/QML résolues ; Nhill POS installé
   a réalisé ventes, retour et clôture contre ce serveur ; Nhill Business installé a
   parcouru ses 22 écrans ; Nhill Console démarre sans erreur.
4. Purge du serveur : `/etc/nhill` et les sauvegardes sont conservés (message affiché).

## 5. Base Supabase (production)

Projet `Flowbills` (eu-west-1) : migrations 0001 à 0010 appliquées par le connecteur
Supabase (0008 et 0009 en deux parties, schéma résultant **identique** à la migration
d'origine, vérifié par comparaison de `pg_dump` sur une base locale), empreintes
SHA-256 enregistrées. Contrôles : tables POS toutes en RLS forcée, 0 droit pour `anon`,
`authenticated`, `service_role`, 0 fonction sans `search_path`. Conseiller sécurité :
**0 alerte**. Conseiller performance : uniquement des informations (index de clés
étrangères peu utilisées, index inutilisés sur une base vide).

## 6. Défauts trouvés et corrigés pendant cette recette

| Défaut | Correction |
|---|---|
| Propriété QML nommée `onPrimary` interprétée comme gestionnaire de signal (texte foncé sur les boutons principaux) | Renommée `textOnPrimary`, vérifiée au pixel |
| PDF POS et page de retour de paiement renvoyés vides (204) | Réponse binaire via `Ctx::file` |
| Boucle de liaison dans le dialogue de paiement POS (lignes recréées pendant la saisie) | Lignes modifiées en place |
| Détails de l'abonnement visibles de tous les membres | Réservés aux administrateurs (API et interface) |
| Comptable incapable de consulter les sessions de caisse | Lecture ouverte à `pos.report` sans droit de vente |
| Remboursement d'un retour recalculé par l'application | Calculé par le serveur |
| Données personnelles possibles dans les rapports d'erreur | Masquage côté serveur |
| `migrate` en échec avant installation (service systemd bloqué) | Sans base : rien à migrer, démarrage en mode installation |
| Unité systemd en lecture seule sur `/etc` (installation impossible) | `ReadWritePaths=/etc/nhill` |
| Paquet Nhill Business vide (composant CMake mal attribué) | Composant précisé pour chaque type de cible |
| Titres de section non étirables (boutons collés aux titres) | `SectionTitle` corrigé pour toutes les pages |

## 7. Non testé

* **Windows** et **Android** : non compilés, non exécutés (listes de recette dans
  `installation.md`).
* **Paiements réels** (Stripe, PayPal) ; Payoneer non intégré ; e-mails.
* Service systemd réel (démarrage par `systemctl`) : la commande de l'unité a été
  exécutée à la main.
* Imprimante de tickets physique, lecteur de codes-barres physique, tiroir-caisse.
* Trousseau du système en conditions réelles ; accessibilité ; charge prolongée ;
  déploiement avec TLS public.
