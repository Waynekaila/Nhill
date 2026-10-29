# Journal des modifications

*English: [../en/changelog.md](../en/changelog.md)*

Format inspiré de « Keep a Changelog » ; numérotation sémantique.

## [3.0.0] — non publiée (branche `release/3.0.0`)

Version majeure : les fonctions suivent la formule de chaque établissement, factures
communes à Nhill Business et Nhill POS, export complet, suppression des données en fin de
contrat, outils de mise en service sur un serveur privé virtuel, nouveau site et documents
juridiques 3.0. **Quatre migrations (0012 à 0015)**. Les applications 2.x sont invitées à
se mettre à jour.

### Formules et droits

- Catalogue des fonctions et des limites ; valeurs par formule modifiables dans Nhill
  Console ; conditions de formule (essai, période de grâce, rappels, renouvellement
  automatique avec accord exprès, formule de repli) ; règles d'attribution (migration 0012).
- Droits effectifs = permissions du rôle réduites aux fonctions de la formule, vérifiés par
  le serveur sur chaque route et sur les données indirectes (listes, tableau de bord,
  paiements, mouvements de stock, journal, notifications) ; limites protégées contre les
  accès concurrents.
- Applications : menus, onglets, boutons et réglages masqués quand la formule ne les
  comprend pas ; droits rechargés dès qu'ils changent.
- Accès calculé par la base à partir des dates : complet, lecture seule (consultation et
  export), aucun.

### Documents et caisse

- Un seul modèle pour tous les documents de vente, en A4 ou en ticket de 58 ou 80 mm,
  utilisé par Nhill Business et Nhill POS (même numérotation, mêmes mentions, mêmes
  arrondis) ; mentions configurables (migration 0013).
- Nhill POS : facture au nom d'un client identifié depuis la caisse ; champ de quantité à
  retourner sans libellé tronqué.
- Android : les PDF et exports s'ouvrent dans une autre application ou se partagent
  (fournisseur de fichiers, `content://`) ; bouton « Partager » sur un document.
- Tableau de bord : titres des indicateurs sur deux lignes sur les écrans étroits.

### Données

- Export complet par le titulaire (archive ZIP : JSON, CSV, pièces jointes, manifeste avec
  empreintes SHA-256), quel que soit l'état de l'abonnement.
- **Suppression d'un établissement** 90 jours après la fin de son accès, depuis Nhill
  Console (super-administrateur, nom commercial à saisir ; plus tôt seulement à la demande
  du client) ; commandes d'abonnement payées archivées 10 ans (migration 0014).
- Rétention : messages envoyés supprimés après 30 jours, sessions terminées après 90 jours.
- 36 devises (ISO 4217) et pays (ISO 3166-1) partagés par le serveur et les applications.
- Recherches : `%` et `_` pris littéralement (migration 0015).

### Serveur et exploitation

- Aucune adresse d'API compilée dans les applications : l'adresse est saisie et vérifiée à
  la première utilisation, modifiable dans les paramètres.
- Un serveur refuse une base migrée par une version plus récente ; le jeton du premier
  administrateur peut être réémis tant que le premier accès n'est pas terminé.
- `NH_SUPPORT_EMAIL` : adresse d'assistance dans la signature des e-mails automatiques.
- Kit `deploy/vps` pour un serveur privé virtuel Hostinger en France : installation,
  pare-feu, sauvegardes chiffrées, restauration d'essai, surveillance toutes les 5 minutes,
  mise à jour avec retour arrière ; recette de 12 contrôles réussie sur une machine neuve
  en intégration continue.
- Client : HTTP/1.1 seulement (avis HTTP/2 de Qt 6.5.3).

### Site, documents, coordonnées

- Coordonnées officielles : `nhill.online`, `info@nhill.online`, `support@nhill.online` ;
  éditeur Nhill Solution, Kinshasa.
- Nouveau site vitrine FR/EN, captures réelles de la version 3.0, image de partage,
  contrôle du domaine en ligne (`check_live.py`).
- Documents juridiques 3.0.0 (FR/EN) sans champ à compléter ; matrice de conformité entre
  le site et l'application ; audit de sécurité 3.0.0.
- Document juridique 20 : autorisation d'utiliser un nom, un témoignage ou une image ;
  politique de confidentialité, registre et conservation complétés (prospection sur le
  terrain, témoignages).
- Documentation par composant (Nhill Business, Nhill POS, Nhill Console, serveur, base de
  données) en français et en anglais ; références de l'API (203 points d'accès) et de la
  base (71 tables) générées depuis le code et vérifiées par la CI ; documents 0.1–2.x
  archivés dans `docs/archive/`.
- Dossier commercial : modèles de messages à champs déclarés, fichiers de suivi livrés vides
  (sans ligne fictive), visuels refaits avec les captures 3.0 pour l'essai de 30 jours.
- APK : notices des composants tiers incluses (`assets/THIRD-PARTY-NOTICES.txt`).
- CI : attente du socket Xvfb pour les captures ; test du Keystore Android robuste au
  redémarrage du gestionnaire d'activités de l'émulateur.

## [2.1.0] — non publiée (branche `release/2.1.0`)

Deux corrections demandées : la session mémorisée sous Windows et Android, et la
récupération d'un mot de passe oublié. **Une migration (0011)**, quatre points d'accès
ajoutés à l'API `v1` (aucun modifié), clients 2.0 acceptés.

### Session mémorisée (« Se souvenir de moi »)

- Case « Se souvenir de moi » à la connexion (Nhill Business, Nhill POS) et réglage
  « Cet appareil » dans Paramètres › Mon compte ; décochée par défaut.
- Seul le jeton de renouvellement est gardé, jamais le mot de passe, par QtKeychain :
  Gestionnaire d'identification Windows, Keystore Android, Secret Service/KWallet sous Linux.
  Sans stockage sécurisé, la session n'est pas mémorisée et un message le dit : aucun
  repli en clair. « Se déconnecter » et le décochage effacent l'entrée.
- Windows : QtKeychain 0.17.0 compilé depuis sa source officielle (commit vérifié) par
  `packaging/windows/build-qtkeychain.ps1`, **lié statiquement** ; contrôle des imports
  `CredReadW`/`CredWriteW`/`CredDeleteW` ; notice de licence installée avec chaque
  application (`THIRD-PARTY-NOTICES.txt`).
- Android : QtKeychain 0.17.0 statique (`packaging/android/build-qtkeychain-android.sh`,
  `KEYCHAIN_PREFIX`), présence du code Keystore vérifiée dans l'APK.
- Test réel `tests/keychain` (écriture puis relecture dans un second processus,
  déconnexion, absence de stockage sans la case) : Gestionnaire d'identification en CI
  Windows, émulateur Android 11, gnome-keyring sous Linux.

### Mot de passe oublié

- **Employé** : l'administrateur de l'établissement donne un mot de passe temporaire
  (Paramètres › Équipe › « Réinitialiser le mot de passe ») ; 12 caractères, affiché une fois,
  valable 72 h ; changement obligatoire à la connexion suivante ; sessions fermées ;
  e-mail d'avis au membre. Limité aux membres de son établissement, ni soi-même, ni le
  propriétaire, ni un compte actif dans un autre établissement.
- **Propriétaire, administrateur** (et compte de plusieurs établissements) : « Mot de
  passe oublié ? » envoie un code à 8 chiffres par e-mail (20 min, 5 essais, usage
  unique) ; réponse identique que l'adresse existe ou non ; un employé reçoit la consigne
  de contacter son administrateur ; toutes les sessions sont fermées après la
  réinitialisation.
- Limites de fréquence par adresse, par adresse IP et par administrateur ; journal
  d'activité sans mot de passe ni code ; codes hachés (HMAC-SHA-256) et effacés de la file
  d'envoi après l'envoi.
- **Envoi réel des e-mails** : transport `smtp` (libcurl, TLS obligatoire, mot de passe du
  relais dans un fichier) ; `log` et `disabled` restent possibles.
- Paramètres de la plateforme : `password_reset_minutes`, `password_reset_max_attempts`,
  `temporary_password_hours`.
- Tests : 16 tests serveur (dont un faux relais SMTP local).

### Divers

- Gabarits d'e-mail complétés (signature, corps des rappels d'abonnement).
- Version 2.1.0 (code de version Android 20100).

## [2.0.0] — 2026-10-26

Version majeure : refonte de l'interface, nouvelle règle des mots de passe,
industrialisation de la construction et des paquets, site vitrine, documentation et
dossiers juridique et commercial bilingues. **L'API `v1` et le schéma de base
(migrations 0001–0010) sont inchangés** : aucune migration, clients 0.1.x acceptés.

### Sécurité et comptes

- **Mots de passe : 8 caractères minimum** sur tous les parcours (création
  d'établissement, invitation, changement, première connexion à la console). Règle unique
  dans `common/` (`kMinPasswordLength`), appliquée par les applications avant l'envoi et
  par le serveur.
- Console : suppression de l'exigence de 14 caractères ; **TOTP toujours obligatoire**.
- Liste des mots de passe refusés portée à 40 entrées (dont « 12345678 », « password »,
  « azerty123 », « kinshasa »).
- Aucune expiration périodique, aucune règle de composition, pas de double
  authentification imposée aux établissements.
- Validation côté application ajoutée à la première connexion de la console.
- Serveur derrière proxy : l'adresse client est l'entrée **la plus à droite** de
  `X-Forwarded-For`.

### Interface (Nhill Business, POS, Console)

- Fenêtres dimensionnées selon l'écran disponible (`Theme.fitWidth/fitHeight`), centrées,
  agrandies sur petit écran ; minimums bornés par l'écran ; POS utilisable dès 860×560.
- Tableaux : largeur minimale par colonne, défilement horizontal, info-bulles sur texte
  coupé, navigation au clavier.
- Jetons de couleur revus pour le contraste WCAG AA (texte secondaire, bordures des champs
  ≥ 3:1, états inactifs, vert de succès) ; tailles de texte minimales 12 et 13 px.
- Boutons : anneau de focus visible, état inactif lisible, info-bulles (obligatoires sur
  les boutons à icône).
- Champs de mot de passe : bouton afficher/masquer.
- Messages : erreurs affichées 8 s, 4 au maximum, annoncées aux lecteurs d'écran.
- Menu latéral défilant ; recherche globale de largeur adaptative ; listes déroulantes,
  calendrier et notifications maintenus dans la fenêtre.
- POS : mode étroit (ticket sous la grille), raccourcis `F2` (recherche) et `F9`
  (encaisser), boutons − / +, noms de produits sur deux lignes, colonne « Date / heure »,
  montant compté obligatoire avant la clôture.
- Journal d'activité : libellés des actions de caisse, d'assistance et de la console ;
  champs techniques masqués ; filtres « Point de vente » et « Assistance ».

### Construction et livraison

- Scripts `scripts/` : `install-deps-linux.sh`, `build-linux.sh`, `build-deb.sh`,
  `build-windows.sh` (+ `packaging/windows/build-windows.ps1`), `build-android.sh`,
  `build-docs-pdf.sh`, `build-website.sh`, `verify-build.sh`, `package-release.sh`,
  `build-all.sh` ; mode `--dry-run`, journaux, codes de sortie, sommes SHA-256.
- Paquets Debian 13 **natifs** (Qt de Debian) en plus d'Ubuntu 24.04.
- Installateurs Windows NSIS (un par application, Windows 10/11 64 bits, installation
  silencieuse `/S`).
- Workflows GitHub Actions : CI, paquets Linux (conteneurs propres + installation de
  contrôle), Windows (MSVC, installation silencieuse de contrôle), Android, documentation
  et site.
- Tests : 2 tests serveur et 2 tests `common` ajoutés (règle des 8 caractères, console) ;
  outil de test autonome `tests/support/minigtest` ; contrôle statique QML.

### Site vitrine et documentation

- Site statique FR/EN (24 pages, HTML5/CSS3/JavaScript sans dépendance) et backend de
  formulaires (SQLite ou PostgreSQL dédié, rôle web en insertion seule, protections
  anti-abus, purge automatique).
- Documentation FR/EN (guides utilisateur, installation, configuration, développement,
  compilation, sécurité), audits technique, UI/UX et sécurité, PDF générés depuis le
  Markdown.
- Dossier juridique (RDC) : recherche sourcée, registre d'hypothèses, modèles à faire
  valider par un juriste.
- Dossier commercial (Kinshasa) : étude de marché sourcée, programme pilote, plan de
  90 jours, scripts, modèles, suivi des indicateurs.

### Limites connues

- Aucune réinitialisation de mot de passe (pas d'envoi d'e-mails).
- cpp-httplib 0.14 : CVE-2025-46728 et CVE-2025-53629, atténuées par Caddy ; mise à
  niveau ≥ 0.23.0 à faire.
- Nhill n'est pas un logiciel de facturation homologué par la DGI.
- Voir le [rapport de validation](rapport-validation.md) pour ce qui a été construit et
  testé, et sur quelle machine.

## [0.1.0] — 2026-10-26

Première version : serveur C++17 / PostgreSQL (RLS forcée), Nhill Business, Nhill POS,
Nhill Console (Qt 6), paquets `.deb` Ubuntu 24.04, base Supabase initialisée. Détails :
`docs/archive/rapport-tests.md`, `docs/archive/limites.md`.
