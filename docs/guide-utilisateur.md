# Nhill — Guide utilisateur

> **Document de la version 0.1.0, remplacé** par [docs/fr/guide-utilisateur.md](fr/guide-utilisateur.md) (FR) et [docs/en/user-guide.md](en/user-guide.md) (EN) pour la 2.0. Changement principal : mots de passe de 8 caractères minimum, y compris pour la console.

Partie A : Nhill Business, l'application de l'établissement (Windows, Linux, Android).
Partie B : Nhill Console, la console d'administration des créateurs de Nhill.
Partie C : Nhill POS, la caisse reliée à un établissement (Windows, Linux).

Captures d'écran : `docs/captures/` (`pos-*` pour la caisse, `s*` pour l'installation).

---

# Partie A — Nhill Business (application de l'établissement)

## 1. Premier lancement

L'écran **Bienvenue** propose deux choix.

### 1.1 « Mon entreprise existe déjà » (rejoindre)

1. Saisissez le **code d'établissement** (`NH-XXXX-XXXX-XXXX`) remis par le propriétaire.
   Le code change automatiquement tous les 30 jours : s'il est refusé, demandez le
   nouveau. Le message d'erreur est volontairement le même dans tous les cas.
2. Le code ne suffit pas : soit **« J'ai un compte »** (votre e-mail et votre mot de
   passe), soit **« J'ai reçu une invitation »** (l'e-mail de l'invitation, le code
   d'activation à 8 chiffres donné par votre responsable et un mot de passe à choisir).

### 1.2 « Créer mon entreprise »

1. **Type d'activité** : *Entrepreneur individuel* (un seul utilisateur, écrans
   simplifiés : pas d'équipe ni de code à partager) ou *Entreprise ou organisation*
   (plusieurs utilisateurs, rôles, invitations).
2. **Identité** : nom commercial, raison sociale, secteur, pays, assujettissement à la
   TVA, identifiants légaux (RCCM, Id. Nat., NIF… uniquement ceux qui vous concernent).
3. **Coordonnées et préférences** : ville, adresse, téléphone, e-mail, devise principale,
   langue des documents.
4. **Votre compte** (propriétaire) puis **vérification et création**.

L'essai gratuit de 30 jours démarre, sans moyen de paiement. Notez le **code
d'établissement** affiché à la fin. Si une entreprise avec le même identifiant légal
existe déjà, Nhill ne la rattache jamais automatiquement : vous pouvez envoyer une
**demande de rattachement**, vérifiée par le propriétaire ou le support.

## 2. L'écran principal

Menu latéral (tiroir sur téléphone) : Tableau de bord, Ventes, Clients et fournisseurs,
Produits et services, Stock, Achats, Dépenses, Paiements, Rapports, Journal d'activité
(ou Mon activité), Paramètres. En haut : recherche globale (`Ctrl+K`), état de la
connexion (En ligne / Hors ligne / Problème serveur), état de l'abonnement, notifications.

Raccourcis bureau : `Ctrl+N` nouvelle facture, `Ctrl+1…4` sections principales, `F5`
actualiser, `Alt+←` (touche Retour du système) pour revenir.

Thème clair, sombre ou automatique et langue (français/anglais) : Paramètres › Mon compte.

## 3. Ventes

* **Nouvelle facture** (ou devis, commande) : client, devise, date, échéance, lignes
  (produit ou ligne libre, quantité, prix HT, remise, taxe). Le total affiché est une
  estimation ; le serveur recalcule et fait foi.
* **Enregistrer le brouillon** : modifiable, sans numéro, filigrane « BROUILLON » sur le PDF.
* **Émettre** : la facture reçoit son **numéro définitif** (ex. `FA-2026-000012`), le stock
  diminue, elle **ne peut plus être modifiée ni supprimée**. Pour corriger : **Avoir**
  (total ou partiel, avec remise en stock possible).
* **Document en devise étrangère** : un taux de change est demandé à l'émission ; il est
  enregistré avec sa date et sa source et n'est jamais recalculé.
* **Perte de connexion pendant l'émission ou un paiement** : un bandeau propose
  **Réessayer**. Utilisez-le sans crainte : l'opération ne sera jamais enregistrée deux fois.
* **Devis** : Envoyer → Accepté / Refusé → Convertir en facture. **Commande** :
  Confirmer → Convertir en facture.
* **Paiement** : depuis la facture, « Enregistrer un paiement » (espèces, Mobile Money,
  carte, virement, chèque…, référence). Le montant ne peut pas dépasser le reste dû.
* **PDF** : bouton PDF ; le fichier est enregistré dans `Téléchargements/Nhill` et ouvert.
* **RDC, assujetti à la TVA** : vos documents Nhill sont des **documents de gestion**.
  Émettez la facture normalisée avec votre dispositif électronique fiscal (DEF), puis
  « Enregistrer la référence DEF » sur la facture pour le rapprochement.

## 4. Clients, produits, stock, achats, dépenses

* **Clients et fournisseurs** : coordonnées, NIF, délai de paiement, notes internes ;
  la fiche montre le solde et les documents.
* **Produits et services** : référence, nom, unité, prix de vente et d'achat, taxe,
  seuil d'alerte. Import/export CSV (`;`) : l'import est **tout ou rien** (les erreurs
  sont listées par ligne et rien n'est importé tant qu'il en reste).
* **Stock** : les quantités ne changent **que par des mouvements** (vente, achat,
  avoir, ajustement avec motif obligatoire, transfert entre dépôts selon l'offre).
  Onglet Mouvements : historique complet avec l'auteur.
* **Achats** : brouillon → **Réceptionner la marchandise** (entrée en stock, prix
  d'achat mis à jour) → paiement.
* **Dépenses** : montant TTC, taxe déductible, catégorie, fournisseur, déjà payée ou
  non, **justificatif** (PDF/PNG/JPEG, 5 Mo).

## 5. Rapports

Journal des ventes (avec « Émis par »), activité par employé, paiements (avec
« Enregistré par »), dépenses, ventes par produit (marge **estimée** au prix d'achat
actuel), mouvements de stock, créances et dettes à ce jour. Période au choix, export
**CSV** ou **PDF**. Les montants sont toujours présentés **par devise**, jamais
convertis ni additionnés entre devises.

## 6. Qui a fait quoi — journal d'activité

* **Propriétaire, administrateur, comptable** : *Journal d'activité* — toutes les
  actions de l'établissement (connexions, documents créés et émis, paiements, stock,
  produits, contacts, équipe, paramètres, interventions du support Nhill), avec la
  date, l'auteur, l'objet (numéro de facture, produit, client…) et le détail. Filtres
  par **employé**, **type d'action** et **période** ; export **CSV/PDF**. Un clic sur une
  ligne ouvre le document concerné.
* **Rapport « Activité par employé »** (Rapports) : pour chaque personne et chaque
  devise, documents créés, factures et avoirs émis, montants encaissés, dépenses
  saisies, corrections de stock et dernière activité.
* **Autres rôles** (vendeur, gérant, lecture seule) : *Mon activité* — uniquement leurs
  propres actions. Le tableau de bord d'un vendeur affiche **ses** ventes et **ses**
  factures impayées (pas les dépenses ni la valeur du stock de l'entreprise).
* Le journal est tenu par le serveur et **ne peut être ni modifié ni effacé**, y compris
  par le propriétaire. Pensez à en informer votre personnel.

## 7. Paramètres

| Onglet | Contenu | Qui |
|---|---|---|
| Établissement / Mon entreprise | Identité, identifiants légaux, **logo** (PNG/JPEG, jamais déformé), langue des documents, délai de paiement, arrondi des taxes, instructions de paiement, pied de page, stock négatif | Propriétaire, administrateur |
| Code d'établissement | Afficher/masquer, copier, **renouveler maintenant** (le renouvellement ne déconnecte personne) | Propriétaire, administrateur, gérant (lecture) |
| Équipe | Membres, rôles, désactivation, **invitations** : le code d'activation n'est affiché **qu'une fois** — remettez-le en main propre, par téléphone ou SMS | Propriétaire, administrateur |
| Abonnement | Offre actuelle, échéance, offres par durée et devise, changement d'offre, code promo, paiement, historique — **visible seulement du propriétaire et des administrateurs** | Propriétaire, administrateur |
| Taxes et comptes | Taux de taxe (un taux ne se modifie pas : créer un nouveau code), caisses et comptes, **taux de change** (date et source) | Propriétaire, administrateur |
| Point de vente | Caisses et caissiers, moyens de paiement, remise maximale sans validation, ticket (58/80 mm, message), codes promo de caisse, sessions et rapports X/Z (voir Partie C) | Propriétaire, administrateur ; gérant et comptable : sessions et rapports |
| Mon compte | Nom, téléphone, langue, thème, **mot de passe** (le changer déconnecte vos autres appareils), déconnexion | Tous |

Rôles : **Propriétaire** (tout, non modifiable), **Administrateur** (tout sauf nommer
d'autres administrateurs), **Gérant** (ventes, achats, stock, catalogue, dépenses,
rapports), **Vendeur / caissier** (clients, ventes, émission, encaissements),
**Comptable** (dépenses, paiements, rapports, journal), **Responsable de caisse** et
**Caissier** (Nhill POS, Partie C), **Lecture seule**.

## 8. Abonnement (propriétaire et administrateurs)

Les autres membres ne voient ni l'offre ni les options d'abonnement ; ils sont seulement
prévenus si l'entreprise passe en lecture seule.

* Essai gratuit (30 jours par défaut, une seule fois par entreprise).
* **Choisir une offre** : durée (1, 3, 6, 12, 24 ou 36 mois selon ce que propose
  Nhill) et devise, puis **Choisir cette offre**. Le récapitulatif calculé par le
  serveur indique le prix, la remise du code promo éventuel, le crédit au prorata et le
  total à payer.
* **Offre supérieure** : effet immédiat, la partie non utilisée de la période en cours
  est déduite. **Offre inférieure** : payée maintenant, appliquée à la fin de la période
  en cours. **Renouvellement** : la nouvelle période commence à la fin de l'actuelle,
  aucun jour n'est perdu.
* **Paiement** : par carte (prestataire en ligne, page de paiement ouverte dans le
  navigateur) ou PayPal quand Nhill les a activés, sinon virement / Mobile Money avec la
  référence indiquée, confirmé par l'équipe de facturation. L'abonnement est mis à jour
  quand le paiement est confirmé, jamais avant. Une demande non payée expire après
  48 heures.
* Échec de paiement : l'abonnement n'est pas interrompu avant la fin de la période de
  grâce ; après, **lecture seule** (consultation et export restent possibles).

## 8 bis. Aide et assistance

Menu **Aide et assistance** : envoyer une question ou un signalement de bogue à l'équipe
Nhill, suivre les réponses. N'y indiquez jamais de mot de passe, de code d'établissement
ni de numéro de carte. Les erreurs techniques de l'application sont transmises
automatiquement, sans données personnelles.

## 9. Connexion et sécurité

* Restez connecté : la session est mémorisée dans le trousseau sécurisé de l'appareil.
* Hors ligne : la consultation et les écritures sont suspendues ; rien n'est perdu.
* Plusieurs entreprises avec le même compte : choix à la connexion.

---

# Partie B — Console d'administration (créateurs)

Application séparée **Nhill Console** (`nhill-console`, Windows/Linux), réservée aux
créateurs de Nhill. Chaque action y est inscrite au
journal d'audit. La console ne donne **aucun accès aux factures, clients ou stocks** des
établissements.

## 1. Accès

* **Installer un nouveau serveur** (une fois) : voir `deploiement-production.md` § 4 —
  jeton d'installation lu dans le journal du serveur, base Supabase ou PostgreSQL,
  premier administrateur, vérification automatique.
* **Premier accès** : e-mail, **jeton d'initialisation** (remis une fois, valable
  30 minutes), mot de passe de 14 caractères minimum, puis ajout de la clé dans une
  application d'authentification (TOTP) et saisie d'un code à 6 chiffres.
* **Connexion** : e-mail + mot de passe + code TOTP (chaque code ne sert qu'une fois).
  La session n'est pas mémorisée : fermer la console vous déconnecte.
* Rôles : **superadmin** (tout, paramètres, invitations d'administrateurs), **billing**
  (offres, paiements, statistiques et rapports), **support** (établissements,
  suspensions, codes, demandes de rattachement, annonces), **auditor** (journal,
  statistiques et rapports, événements prestataires, lecture seule).

## 2. Écrans

| Écran | Usage |
|---|---|
| Vue d'ensemble | Nombre d'établissements, abonnements par état, utilisateurs actifs, activité sur 30 jours, revenus d'abonnement confirmés |
| Établissements | Recherche, filtre par abonnement ; fiche : identité plateforme, abonnement et historique, codes (sans leur valeur), **statistiques d'activité** ; actions **Suspendre / Réactiver / Réémettre le code** et **Abonnement…** (prolonger, suspendre, réactiver, résilier, changer d'offre, demande de paiement) — motif obligatoire |
| Rapports | **Rapport global** : une ligne par établissement et par devise sur la période (offre, abonnement, utilisateurs, factures émises, facturé HT/TTC, avoirs, encaissé, dépenses) — CSV/PDF |
| Demandes de rattachement | Vérifier l'identité avec le propriétaire, puis accepter/refuser avec une note |
| Notifications | Nouvelles demandes d'assistance, réponses des clients, paiements reçus ou en écart, erreurs techniques |
| Demandes d'assistance | Prendre en charge, répondre, note interne (invisible du client), changer l'état (en cours, en attente du client, résolue, fermée) |
| Transactions | Demandes de paiement et paiements des prestataires, rapprochement ; **confirmation manuelle** d'un virement après vérification du relevé (référence + motif) |
| Offres et tarifs | Offres (niveau, utilisateurs, caisses POS, dépôts, fonctionnalités) et **prix par durée et devise** ; aucun prix n'est codé en dur |
| Codes promo | Pourcentage ou montant fixe, devise, dates, nombre d'utilisations total et par établissement, offres et durées éligibles, nouveaux clients seulement ; suivi des utilisations |
| Prestataires de paiement | Stripe (cartes Visa, Mastercard…), PayPal, Payoneer (configuration seulement), générique ; identifiants et secrets **chiffrés, jamais réaffichés** ; mode test/production ; adresse du webhook |
| Erreurs techniques | Pannes du serveur et erreurs des applications regroupées, à marquer résolues |
| Événements des prestataires | Notifications de paiement signées reçues (doublons ignorés) |
| Journal d'audit | Événements de plateforme et de sécurité (détails des actions métier masqués) |
| Annonces | Messages aux établissements (information, maintenance, nouvelle version) |
| Paramètres de la plateforme | Durée d'essai, grâce, rappel, rotation des codes, limites, instructions de paiement ; inviter un administrateur |

## 3. Statistiques d'un établissement

Fiche établissement › **Statistiques d'activité** : choisir la période puis Afficher.
Indicateurs : utilisateurs actifs et connexions, nombre d'actions et dernière activité,
taille du catalogue (produits, clients, fournisseurs), documents émis et brouillons,
répartition des rôles ; tableau **par devise** (factures émises, facturé HT et TTC,
avoirs, encaissé, dépenses, achats) et **par mois**. Export CSV/PDF du rapport mensuel.

Ces chiffres sont des **agrégats** calculés par la base de données ; aucun nom de
client, produit ou numéro de document n'est transmis. Chaque consultation apparaît dans
le journal de l'établissement (« Support Nhill — Statistiques agrégées consultées ») :
l'établissement sait quand et sur quelle période ses chiffres ont été consultés.

---

# Partie C — Nhill POS (caisse)

Application **Nhill POS** (`nhill-pos`, Windows/Linux), reliée à un établissement Nhill
Business : mêmes produits, prix, taxes et stock. Chaque caissier se connecte avec **son
propre compte** Nhill (rôle Caissier ou Responsable de caisse, créé dans Nhill Business
→ Paramètres → Équipe).

## 1. Préparer (Nhill Business → Paramètres → Point de vente)

1. **Nouvelle caisse** : code imprimé sur les tickets (ex. `C1`), nom, emplacement, dépôt
   dont le stock est utilisé. Le nombre de caisses actives dépend de l'offre (1 pendant
   l'essai).
2. Ouvrir la caisse → **Qui peut ouvrir cette caisse** : cocher les caissiers. Les
   responsables de caisse, gérants et administrateurs ouvrent toutes les caisses.
3. **Moyens de paiement** : Espèces (créé automatiquement), Carte bancaire, M-Pesa…
   Nhill enregistre le montant ; il ne pilote ni terminal de carte ni opérateur.
4. **Règles et ticket** : remise maximale sans validation, modification de prix (toujours
   avec validation), largeur du ticket, message en bas du ticket.
5. **Codes promo en caisse** : pourcentage sur tout le ticket, dates, utilisations.

## 2. Ouvrir la caisse

Se connecter, choisir sa caisse, **compter le fond de caisse** et l'indiquer, **Ouvrir la
session de caisse**. Une caisse n'a qu'une session ouverte, un caissier aussi.

## 3. Vendre

* Toucher un produit, ou scanner / saisir un code-barres puis Entrée.
* Quantités `<` `>`, remise par ligne (`%`), client (par défaut « Client comptoir »),
  code promo.
* **Encaisser** : un ou plusieurs moyens de paiement ; pour les espèces, saisir le
  **montant remis** : la monnaie à rendre s'affiche après validation. **Imprimer le
  ticket** (PDF au format rouleau).
* Au-delà de la remise autorisée, un superviseur saisit **son** e-mail et **son** mot de
  passe sur la caisse ; l'autorisation ne sert qu'une fois et est journalisée.
* Coupure réseau pendant la validation : appuyer de nouveau sur Valider ; la vente ne
  sera pas enregistrée deux fois.

## 4. Tickets, retours, annulations, espèces

Bouton **Tickets et caisse** : liste des tickets de la session, totaux, ventes par moyen
de paiement.

* Toucher un ticket : **Imprimer**, **Retourner les quantités** (avoir, remboursement en
  espèces, remise en stock) ou **Annuler le ticket** (même session, ticket non déjà
  retourné). Motif obligatoire ; validation d'un superviseur si vous n'avez pas le droit.
* **Entrée / sortie** : apport de monnaie, dépôt en banque, dépense ; une sortie demande
  une validation et ne peut dépasser les espèces présentes.
* **Lecture X** : rapport intermédiaire sans clôturer.

## 5. Clôturer

**Clôturer la caisse** : compter le tiroir et reporter les totaux des terminaux. Le
caissier ne voit pas le montant attendu ; le serveur calcule l'écart (espèces comptées −
fond de caisse − encaissements en espèces − entrées + sorties). Le **rapport Z** est
téléchargeable ensuite ; une session clôturée ne peut plus être modifiée. Les écarts
apparaissent dans Nhill Business (Paramètres → Point de vente → Sessions de caisse) et
sont notifiés aux responsables.

Les tickets sont des documents de gestion ; Nhill POS n'est pas une caisse certifiée
(voir `limites.md`).
