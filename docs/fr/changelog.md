# Journal des modifications

*English: [../en/changelog.md](../en/changelog.md)*

Format inspiré de « Keep a Changelog » ; numérotation sémantique.

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
`docs/rapport-tests.md`, `docs/limites.md`.
