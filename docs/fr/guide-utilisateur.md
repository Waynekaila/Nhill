# Nhill 2.1 — Guide utilisateur

*English: [../en/user-guide.md](../en/user-guide.md)*

| Partie | Application | Pour qui | Plateformes |
|---|---|---|---|
| A | **Nhill Business** | l'établissement : ventes, clients, stock, achats, dépenses, rapports, équipe | Windows, Linux, Android |
| B | **Nhill Console** | les créateurs de Nhill (application privée) | Windows, Linux |
| C | **Nhill POS** | les caissiers d'un établissement | Windows, Linux |

Captures d'écran : `docs/captures/` (prises sur la version 0.1.0 ; les écrans 2.0 gardent
la même organisation, avec les améliorations décrites au § 0).

---

## 0. Nouveautés pour l'utilisateur

### Version 2.1

- **Se souvenir de moi** (Nhill Business et Nhill POS, sur Windows, Linux et Android) :
  cochée à la connexion, l'application se rouvre déjà connectée. Seul un jeton de session
  est gardé, dans le stockage sécurisé du système (Gestionnaire d'identification de
  Windows, Keystore d'Android, trousseau Linux), **jamais le mot de passe**. Décochée, rien
  n'est gardé ; la déconnexion efface aussi la session mémorisée (§ 10).
- **Mot de passe oublié** : l'administrateur de l'établissement donne un mot de passe
  temporaire à un employé ; propriétaires et administrateurs reçoivent un code par e-mail
  (§ 9).

### Version 2.0

- **Mots de passe** : 8 caractères minimum partout (création de compte, invitation,
  changement de mot de passe, réinitialisation, console d'administration). Les mots de passe trop courants
  (« 12345678 », « password », « kinshasa »…) sont refusés. Aucun changement périodique
  n'est imposé ; changez-le seulement si vous pensez qu'il a été vu ou deviné.
- **Afficher le mot de passe** : un bouton en forme d'œil, dans chaque champ de mot de
  passe, affiche ou masque la saisie (utile sur téléphone).
- **Fenêtres adaptées à l'écran** : à l'ouverture, chaque application prend une taille
  adaptée à l'écran (petits écrans d'ordinateur portable compris) et se centre ; sur un
  écran trop petit, elle s'ouvre agrandie. Les fenêtres se redimensionnent librement.
- **Tableaux** : défilement horizontal quand les colonnes ne tiennent pas, info-bulle sur
  un texte coupé, navigation au clavier (flèches, Entrée).
- **Accessibilité** : contrastes renforcés (textes secondaires, bordures des champs,
  boutons inactifs), cadre de focus visible au clavier, info-bulles sur les boutons à
  icône, messages d'erreur affichés plus longtemps (8 s) et annoncés aux lecteurs d'écran.
- **Nhill POS** : `F2` place le curseur dans la recherche ou le code-barres, `F9` ouvre
  l'encaissement ; boutons − / + pour les quantités ; mise en page adaptée aux écrans
  étroits ; la clôture exige le montant compté.

---

# Partie A — Nhill Business

## 1. Premier lancement

L'écran **Bienvenue** propose deux choix.

### 1.1 « Mon entreprise existe déjà » (rejoindre)

1. Saisissez le **code d'établissement** (`NH-XXXX-XXXX-XXXX`) remis par le propriétaire.
   Le code change automatiquement tous les 30 jours : s'il est refusé, demandez le
   nouveau. Le message d'erreur est volontairement le même dans tous les cas.
2. Le code ne suffit pas : soit **« J'ai un compte »** (votre e-mail et votre mot de
   passe), soit **« J'ai reçu une invitation »** (l'e-mail de l'invitation, le code
   d'activation à 8 chiffres donné par votre responsable et un mot de passe de
   8 caractères minimum à choisir).

### 1.2 « Créer mon entreprise »

1. **Type d'activité** : *Entrepreneur individuel* (un seul utilisateur, écrans
   simplifiés) ou *Entreprise ou organisation* (plusieurs utilisateurs, rôles,
   invitations).
2. **Identité** : nom commercial, raison sociale, secteur, pays, assujettissement à la
   TVA, identifiants légaux (RCCM, Id. Nat., NIF… uniquement ceux qui vous concernent).
3. **Coordonnées et préférences** : ville, adresse, téléphone, e-mail, devise principale,
   langue des documents.
4. **Votre compte** (propriétaire, mot de passe de 8 caractères minimum) puis
   **vérification et création**.

L'essai gratuit (30 jours par défaut) démarre sans moyen de paiement. Notez le **code
d'établissement** affiché à la fin. Si une entreprise avec le même identifiant légal
existe déjà, Nhill ne la rattache jamais automatiquement : vous pouvez envoyer une
**demande de rattachement**, vérifiée par le propriétaire ou le support.

## 2. L'écran principal

Menu latéral (tiroir sur téléphone, défilant si la fenêtre est basse) : Tableau de bord,
Ventes, Clients et fournisseurs, Produits et services, Stock, Achats, Dépenses,
Paiements, Rapports, Journal d'activité (ou Mon activité), Paramètres. En haut : recherche
globale (`Ctrl+K`), état de la connexion (En ligne / Hors ligne / Problème serveur), état
de l'abonnement, notifications.

| Raccourci | Action |
|---|---|
| `Ctrl+K` | recherche globale |
| `Ctrl+N` | nouvelle facture (si votre rôle le permet) |
| `Ctrl+1` … `Ctrl+4` | Tableau de bord, Ventes, Clients, Produits |
| `F5` | actualiser l'écran |
| `Alt+←` (touche Retour) | revenir à l'écran précédent |
| `Tab` / `Maj+Tab` | champ suivant / précédent (aussi dans les zones de texte) |

Thème clair, sombre ou automatique et langue (français/anglais) : Paramètres › Mon compte.

## 3. Ventes

- **Nouvelle facture** (ou devis, commande) : client, devise, date, échéance, lignes
  (produit ou ligne libre, quantité, prix HT, remise, taxe). Le total affiché est une
  estimation ; le serveur recalcule et fait foi.
- **Enregistrer le brouillon** : modifiable, sans numéro, filigrane « BROUILLON » sur le PDF.
- **Émettre** : la facture reçoit son **numéro définitif** (ex. `FA-2026-000012`), le stock
  diminue, elle **ne peut plus être modifiée ni supprimée**. Pour corriger : **Avoir**
  (total ou partiel, avec remise en stock possible).
- **Document en devise étrangère** : un taux de change est demandé à l'émission ; il est
  enregistré avec sa date et sa source et n'est jamais recalculé.
- **Perte de connexion pendant l'émission ou un paiement** : un bandeau propose
  **Réessayer**. L'opération ne sera jamais enregistrée deux fois.
- **Devis** : Envoyer → Accepté / Refusé → Convertir en facture. **Commande** :
  Confirmer → Convertir en facture.
- **Paiement** : depuis la facture, « Enregistrer un paiement » (espèces, Mobile Money,
  carte, virement, chèque…, référence). Le montant ne peut pas dépasser le reste dû.
- **PDF** : bouton PDF ; le fichier est enregistré dans `Téléchargements/Nhill` et ouvert.
- **RDC, assujetti à la TVA** : les documents Nhill sont des **documents de gestion**,
  pas des factures normalisées. Émettez la facture normalisée avec le dispositif
  électronique fiscal requis par la DGI, puis « Enregistrer la référence DEF » sur la
  facture Nhill pour le rapprochement (voir `docs/fr/parametres-legaux.md`).

## 4. Clients, produits, stock, achats, dépenses

- **Clients et fournisseurs** : coordonnées, NIF, délai de paiement, notes internes ; la
  fiche montre le solde et les documents.
- **Produits et services** : référence, nom, unité, prix de vente et d'achat, taxe, seuil
  d'alerte. Import/export CSV (`;`) : l'import est **tout ou rien** (les erreurs sont
  listées par ligne et rien n'est importé tant qu'il en reste).
- **Stock** : les quantités ne changent **que par des mouvements** (vente, achat, avoir,
  ajustement avec motif obligatoire, transfert entre dépôts selon l'offre). Onglet
  Mouvements : historique complet avec l'auteur.
- **Achats** : brouillon → **Réceptionner la marchandise** (entrée en stock, prix d'achat
  mis à jour) → paiement.
- **Dépenses** : montant TTC, taxe déductible, catégorie, fournisseur, déjà payée ou non,
  **justificatif** (PDF/PNG/JPEG, 5 Mo).

## 5. Rapports

Journal des ventes (avec « Émis par »), activité par employé, paiements (avec
« Enregistré par »), dépenses, ventes par produit (marge **estimée** au prix d'achat
actuel), mouvements de stock, créances et dettes à ce jour. Période au choix, export
**CSV** ou **PDF**. Les montants sont toujours présentés **par devise**, jamais convertis
ni additionnés entre devises.

## 6. Qui a fait quoi — journal d'activité

- **Propriétaire, administrateur, comptable** : *Journal d'activité* — toutes les actions
  de l'établissement (connexions, documents, paiements, stock, produits, contacts,
  équipe, paramètres, caisse, interventions du support Nhill), avec la date, l'auteur,
  l'objet et le détail. Filtres par **employé**, **type d'action** (dont « Caisse » et
  « Support Nhill » en 2.0) et **période** ; export **CSV/PDF**. Un clic sur une ligne
  ouvre le document concerné.
- **Rapport « Activité par employé »** : pour chaque personne et chaque devise, documents
  créés, factures et avoirs émis, montants encaissés, dépenses saisies, corrections de
  stock et dernière activité.
- **Autres rôles** : *Mon activité* — uniquement leurs propres actions.
- Le journal est tenu par le serveur et **ne peut être ni modifié ni effacé**, y compris
  par le propriétaire. Informez-en votre personnel.

## 7. Paramètres

| Onglet | Contenu | Qui |
|---|---|---|
| Établissement | Identité, identifiants légaux, **logo**, langue des documents, délai de paiement, arrondi des taxes, instructions de paiement, pied de page, stock négatif | Propriétaire, administrateur |
| Code d'établissement | Afficher/masquer, copier, **renouveler maintenant** | Propriétaire, administrateur, gérant (lecture) |
| Équipe | Membres, rôles, désactivation, **invitations** (code d'activation affiché **une seule fois** : remettez-le en main propre, par téléphone ou SMS), **réinitialisation du mot de passe** d'un membre (§ 9) | Propriétaire, administrateur |
| Abonnement | Offre, échéance, offres par durée et devise, code promo, paiement, historique | Propriétaire, administrateur |
| Taxes et comptes | Taux de taxe (non modifiables : créer un nouveau code), caisses et comptes, **taux de change** | Propriétaire, administrateur |
| Point de vente | Caisses et caissiers, moyens de paiement, remise maximale, ticket, codes promo de caisse, sessions et rapports X/Z | Propriétaire, administrateur ; gérant et comptable : sessions et rapports |
| Mon compte | Nom, téléphone, langue, thème, **mot de passe** (le changer déconnecte vos autres appareils), **Se souvenir de moi sur cet appareil**, déconnexion | Tous |

Rôles : **Propriétaire**, **Administrateur**, **Gérant**, **Vendeur / caissier**,
**Comptable**, **Responsable de caisse**, **Caissier**, **Lecture seule**.

## 8. Abonnement (propriétaire et administrateurs)

- Essai gratuit (30 jours par défaut, une seule fois par entreprise).
- **Choisir une offre** : durée et devise proposées par Nhill, puis **Choisir cette
  offre**. Le récapitulatif calculé par le serveur indique le prix, la remise éventuelle,
  le crédit au prorata et le total.
- **Offre supérieure** : effet immédiat, partie non utilisée déduite. **Offre
  inférieure** : appliquée à la fin de la période. **Renouvellement** : aucun jour perdu.
- **Paiement** : carte ou PayPal quand Nhill les a activés, sinon virement / Mobile Money
  avec la référence indiquée, confirmé par l'équipe de facturation. L'abonnement change
  quand le paiement est confirmé, jamais avant. Une demande non payée expire après
  48 heures.
- Après la période de grâce sans paiement : **lecture seule** (consultation et export
  restent possibles).

## 9. Aide et assistance

Menu **Aide et assistance** : envoyer une question ou un signalement à l'équipe Nhill,
suivre les réponses. N'y indiquez jamais de mot de passe, de code d'établissement ni de
numéro de carte.

### Mot de passe oublié

L'équipe Nhill ne connaît pas votre mot de passe et ne vous le demandera jamais. Selon
votre rôle :

**Vous êtes employé (vendeur, caissier, comptable, gérant…).** Demandez à l'administrateur
ou au propriétaire de votre établissement. Dans **Paramètres › Équipe**, il choisit votre
nom puis **Réinitialiser le mot de passe** : Nhill crée un mot de passe temporaire
(12 caractères, par exemple `k7Rm-x2Qa-Hp9w`), affiché **une seule fois**, qu'il vous
remet **en personne ou par téléphone** (jamais par e-mail ni dans un groupe de discussion).
Vos sessions ouvertes sont fermées immédiatement. À la connexion suivante, avec ce mot de
passe temporaire, Nhill vous demande d'en choisir un nouveau (8 caractères minimum,
différent du temporaire) avant d'ouvrir l'application. Le mot de passe temporaire expire
après 72 heures : passé ce délai, demandez-en un autre.

Un administrateur ne peut réinitialiser que les comptes de **son** établissement, jamais
celui du propriétaire ni le sien ; seul le propriétaire peut réinitialiser un
administrateur. Si votre compte sert aussi dans une **autre** entreprise, aucun de ses
administrateurs ne peut le réinitialiser : utilisez la procédure par e-mail ci-dessous.

**Vous êtes propriétaire ou administrateur** (ou votre compte sert dans plusieurs
entreprises). Sur l'écran de connexion, choisissez **Mot de passe oublié ?**, saisissez
votre adresse e-mail puis **Envoyer le code**. Vous recevez un code à 8 chiffres
(`1234-5678`), valable **20 minutes** et utilisable **une seule fois** ; saisissez-le avec
votre nouveau mot de passe. Après 5 codes erronés, le code est annulé : demandez-en un
nouveau. Une nouvelle demande annule le code précédent. Le message affiché est le même que
l'adresse corresponde à un compte ou non (personne ne peut ainsi découvrir qui utilise
Nhill) ; si votre compte est un compte d'employé, l'e-mail reçu vous indique qui contacter.
Une fois le mot de passe changé, **toutes vos sessions sont fermées** et un e-mail vous
prévient du changement.

Changer de mot de passe en étant connecté reste possible (Paramètres › Mon compte).

## 10. Connexion et sécurité

- **Se souvenir de moi** : à cocher à la connexion, uniquement sur un appareil que vous ne
  partagez pas. L'application se rouvre alors connectée ; le jeton de session est gardé
  dans le stockage sécurisé du système (Gestionnaire d'identification de Windows, Keystore
  d'Android, trousseau Secret Service ou KWallet sous Linux), jamais en clair et jamais le
  mot de passe. **Se déconnecter** efface cette session ; décocher l'option (à la connexion
  ou dans Paramètres › Mon compte) l'efface aussi. Si le système refuse de la garder (par
  exemple un Linux sans trousseau), l'application le signale et redemandera la connexion.
- Un appareil perdu ou volé : changez votre mot de passe depuis un autre appareil (toutes
  les autres sessions sont fermées) ou demandez à votre administrateur de réinitialiser
  votre accès.
- Hors ligne : la consultation et les écritures sont suspendues ; rien n'est perdu.
- Plusieurs entreprises avec le même compte : choix à la connexion.
- Après plusieurs essais de connexion erronés, le serveur ralentit les tentatives.

## 11. Périmètre de Nhill (à connaître avant de décider)

Ces points sont présentés de façon positive sur le site ; voici leur formulation exacte.

| Sujet | Ce que fait Nhill | Ce qu'il ne fait pas |
|---|---|---|
| Obligations fiscales | taux de taxe, identifiants légaux et mentions sur chaque document ; profil `CD` (RDC, brouillon à faire valider par un fiscaliste) et profil `INTL` générique ; référence d'un document officiel (par exemple DEF de la DGI) enregistrée sur la facture Nhill | n'est **certifié dans aucun pays** : ni caisse certifiée, ni facture électronique réglementaire, ni dispositif fiscal |
| Comptabilité | journaux et exports CSV/PDF (ventes, paiements, dépenses, créances, montants à payer) | ne tient pas la comptabilité générale (grand livre, plan OHADA) |
| Paiements | espèces, carte, virement, chèque, Mobile Money enregistrés avec leur référence | n'est relié ni aux opérateurs Mobile Money ni aux terminaux de carte : le paiement se vérifie sur le téléphone ou le terminal |
| Envoi des documents | PDF à partager depuis l'ordinateur ou le téléphone | pas d'envoi direct par e-mail ou SMS depuis l'application |
| Connexion | une connexion mobile suffit ; une vente interrompue est renvoyée sans doublon | sans connexion, on ne peut ni vendre ni facturer |
| Appareils | Nhill Business : Windows, Linux, Android ; Nhill POS : Windows, Linux | pas de caisse sur Android ni sur iOS |

---

# Partie B — Nhill Console (créateurs)

Application séparée, réservée aux créateurs de Nhill. Chaque action est inscrite au
journal d'audit. La console ne donne **aucun accès aux factures, clients ou stocks** des
établissements.

## 1. Accès

- **Installer un nouveau serveur** (une fois) : voir le guide d'installation § 4.
- **Premier accès** : e-mail, **jeton d'initialisation** (valable 30 minutes), mot de
  passe de **8 caractères minimum**, puis ajout de la clé dans une application
  d'authentification (TOTP) et saisie d'un code à 6 chiffres. La double authentification
  reste **obligatoire** pour la console (accès à toute la plateforme) ; choisissez de
  préférence une phrase de passe longue.
- **Connexion** : e-mail + mot de passe + code TOTP (chaque code ne sert qu'une fois). La
  session n'est pas mémorisée.
- Rôles : **superadmin**, **billing** (offres, paiements, rapports), **support**
  (établissements, suspensions, codes, rattachements, annonces), **auditor** (lecture
  seule : journal, statistiques, événements prestataires).

## 2. Écrans

| Écran | Usage |
|---|---|
| Vue d'ensemble | Établissements, abonnements par état, utilisateurs actifs, activité sur 30 jours, revenus confirmés |
| Établissements | Recherche ; fiche : identité, abonnement, codes (sans leur valeur), statistiques ; **Suspendre / Réactiver / Réémettre le code**, **Abonnement…** — motif obligatoire |
| Établissements à supprimer | Filtre « À supprimer (90 jours écoulés) » ; sur la fiche, bandeau indiquant la fin de l'accès et la date à partir de laquelle les données doivent être supprimées ; **Supprimer…** (super-administrateur seulement) : motif, saisie exacte du nom commercial ; avant les 90 jours, seulement si le client l'a demandé et jamais tant que l'établissement a un accès complet. Tout est supprimé (documents, clients, produits, stock, paiements, journal d'activité, comptes utilisés uniquement dans cet établissement) ; les commandes d'abonnement payées sont archivées 10 ans pour la comptabilité ; la suppression est inscrite au journal de la plateforme |
| Rapports | Rapport global par établissement et par devise — CSV/PDF |
| Demandes de rattachement | Vérifier l'identité, accepter/refuser avec une note |
| Notifications | Assistance, paiements reçus ou en écart, erreurs techniques |
| Demandes d'assistance | Prendre en charge, répondre, note interne, changer l'état |
| Transactions | Demandes de paiement, rapprochement, **confirmation manuelle** d'un virement (référence + motif) |
| Offres et tarifs | Offres, plafonds et **prix par durée et devise** (aucun prix codé en dur) |
| Codes promo | Pourcentage ou montant, dates, utilisations, offres éligibles |
| Prestataires de paiement | Stripe, PayPal, Payoneer (configuration seulement), générique ; secrets **chiffrés, jamais réaffichés** ; mode test/production |
| Erreurs techniques | Erreurs regroupées, à marquer résolues |
| Journal d'audit | Événements de plateforme et de sécurité |
| Annonces | Messages aux établissements |
| Paramètres de la plateforme | Essai, grâce, rotation des codes, limites ; inviter un administrateur |

Les **statistiques d'un établissement** sont des agrégats calculés par la base ; aucun
nom de client, produit ou numéro de document n'est transmis, et chaque consultation
apparaît dans le journal de l'établissement.

---

# Partie C — Nhill POS (caisse)

Reliée à un établissement Nhill Business : mêmes produits, prix, taxes et stock. Chaque
caissier se connecte avec **son propre compte** (rôle Caissier ou Responsable de caisse).

## 1. Préparer (Nhill Business → Paramètres → Point de vente)

1. **Nouvelle caisse** : code imprimé sur les tickets (ex. `C1`), nom, emplacement, dépôt.
2. **Qui peut ouvrir cette caisse** : cocher les caissiers.
3. **Moyens de paiement** : Espèces (automatique), Carte, M-Pesa… Nhill enregistre le
   montant ; il ne pilote ni terminal de carte ni opérateur.
4. **Règles et ticket** : remise maximale sans validation, largeur (58/80 mm), message.
5. **Codes promo en caisse**.

## 2. Ouvrir la caisse

Se connecter, choisir sa caisse, **compter le fond de caisse**, **Ouvrir la session**.
« Se souvenir de moi » et « Mot de passe oublié ? » fonctionnent comme dans Nhill Business
(Partie A, § 9 et § 10) ; sur un poste de caisse partagé par plusieurs caissiers, laissez
« Se souvenir de moi » décoché.

## 3. Vendre

| Touche / bouton | Action |
|---|---|
| `F2` | aller à la recherche / au code-barres |
| Entrée (après un code-barres) | ajouter le produit |
| − / + | diminuer / augmenter la quantité d'une ligne |
| `%` | remise sur la ligne |
| `F9` ou **Encaisser** | ouvrir l'encaissement |

- Un ou plusieurs moyens de paiement ; pour les espèces, saisir le **montant remis** : la
  monnaie à rendre s'affiche. **Imprimer le ticket** (PDF au format rouleau).
- Au-delà de la remise autorisée, un superviseur saisit **son** e-mail et **son** mot de
  passe ; l'autorisation ne sert qu'une fois et est journalisée.
- Coupure réseau pendant la validation : appuyer de nouveau sur Valider ; la vente ne
  sera pas enregistrée deux fois.
- Écran étroit : le ticket passe sous la grille des produits.

## 4. Tickets, retours, annulations, espèces

**Tickets et caisse** : liste des tickets de la session (colonne « Date / heure »),
totaux, ventes par moyen de paiement.

- Sur un ticket : **Imprimer**, **Retourner les quantités** ou **Annuler le ticket**
  (même session). Motif obligatoire ; validation d'un superviseur si nécessaire.
- **Entrée / sortie** d'espèces ; une sortie ne peut dépasser les espèces présentes.
- **Lecture X** : rapport intermédiaire sans clôturer.

## 5. Clôturer

**Clôturer la caisse** : compter le tiroir (montant **obligatoire**) et reporter les
totaux des terminaux. Le caissier ne voit pas le montant attendu ; le serveur calcule
l'écart. Le **rapport Z** est téléchargeable ensuite ; une session clôturée ne peut plus
être modifiée. Les écarts sont notifiés aux responsables.

Les tickets sont des documents de gestion ; Nhill POS n'est pas une caisse certifiée.
