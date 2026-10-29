# Nhill 3.0.0 — notes de version / release notes

API `v1` (points d'accès ajoutés, contrôles de formule sur les existants) · schéma de base :
migrations 0001-0015 (quatre migrations ajoutées : 0012 à 0015) · applications 2.x invitées
à se mettre à jour

## Français

**Nouveautés** :
- **Formules configurables** : l'exploitant règle dans Nhill Console les fonctions
  (factures, devis, caisse, achats, dépenses, plusieurs dépôts, plusieurs devises, import du
  catalogue, rapports avancés) et les limites (utilisateurs, caisses, dépôts, produits,
  documents par mois, stockage) de chaque formule, ses conditions (essai, grâce, rappels,
  renouvellement avec accord exprès, formule de repli) et des règles d'attribution. Une
  fonction hors formule est **invisible** dans les applications et **refusée** par le
  serveur.
- **Documents** : un seul modèle (A4 ou ticket 58/80 mm) pour Nhill Business et Nhill POS ;
  facture au nom d'un client depuis la caisse ; mentions rédigées par l'établissement.
- **Données** : export complet par le propriétaire (archive ZIP) ; suppression définitive
  d'un établissement 90 jours après la fin de son accès, depuis la console ; nettoyages de
  conservation automatiques.
- **Mise en service** : kit `deploy/vps` pour un VPS Hostinger en France (TLS, pare-feu,
  sauvegardes chiffrées, restauration d'essai, surveillance, mise à jour avec retour
  arrière) ; aucune adresse de serveur intégrée aux applications.
- **Documentation par composant** (FR/EN + PDF) et références de l'API et de la base
  générées depuis le code.

Détail : `docs/fr/changelog.md`.

**Avant la mise à jour** : sauvegarder la base et `NH_SECRET_KEY` ; suivre
`docs/fr/guide-installation.md` § 5. Un serveur 2.x refuse une base migrée en 3.0 : le
retour arrière passe par la restauration de la copie prise avant la mise à jour.

**Limites connues** : APK signé avec une clé de débogage ; installateurs Windows non
signés ; risques R1 (cpp-httplib du paquet Ubuntu 24.04) et R2 (Qt 6.5.3 sous Windows et
Android) atténués, décision attendue (`docs/fr/03-audit-securite.md`) ; aucun déploiement
réel sur le VPS de production ; site `nhill.online` pas encore publié ; aucun audit externe.

## English

**What is new**:
- **Configurable plans**: in Nhill Console the operator sets each plan's features
  (invoices, quotes, till, purchases, expenses, several warehouses, several currencies,
  catalogue import, advanced reports) and limits (users, tills, warehouses, products,
  documents per month, storage), its terms (trial, grace, reminders, renewal with express
  consent, fallback plan) and assignment rules. A feature outside the plan is **invisible**
  in the applications and **refused** by the server.
- **Documents**: a single template (A4 or 58/80 mm receipt) for Nhill Business and Nhill
  POS; invoice in a customer's name from the till; notices written by the business.
- **Data**: full export by the owner (ZIP archive); permanent erasure of a business 90 days
  after its access ends, from the console; automatic retention clean-ups.
- **Deployment**: `deploy/vps` kit for a Hostinger VPS in France (TLS, firewall, encrypted
  backups, test restore, monitoring, update with rollback); no server address built into the
  applications.
- **Documentation per component** (FR/EN + PDF) and API and database references generated
  from the code.

Details: `docs/en/changelog.md`.

**Before upgrading**: back up the database and `NH_SECRET_KEY`; follow
`docs/en/installation-guide.md` § 5. A 2.x server refuses a database migrated to 3.0:
rollback means restoring the copy taken before the update.

**Known limits**: APK signed with a debug key; Windows installers not signed; risks R1
(cpp-httplib of the Ubuntu 24.04 package) and R2 (Qt 6.5.3 on Windows and Android)
mitigated, decision pending (`docs/en/03-security-audit.md`); no real deployment on the
production VPS; `nhill.online` website not published yet; no external audit.
