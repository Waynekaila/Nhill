# Nhill Business 3.0 — Guide de l'application

*English: [../en/nhill-business.md](../en/nhill-business.md)*

Nhill Business sert à l'établissement : ventes, clients, produits, stock, achats, dépenses,
paiements, rapports, équipe et réglages. Plateformes : Windows 10 et 11 (64 bits), Linux
(Ubuntu 24.04, Debian 13), téléphones et tablettes Android 7 et plus récents. Installation :
[guide-installation.md](guide-installation.md) § 3. Autres applications :
[nhill-pos.md](nhill-pos.md) (caisse), [nhill-console.md](nhill-console.md) (exploitant).

> **Ce que vous voyez dépend de votre rôle et de la formule de l'établissement.** Un menu,
> un onglet ou un bouton n'apparaît que si votre rôle le permet **et** si la formule
> comprend la fonction (factures, devis, caisse, achats, dépenses, plusieurs dépôts,
> plusieurs devises, import du catalogue, rapports avancés). Une fonction hors formule
> n'est jamais affichée ; le serveur la refuse aussi. Quand la formule change, les droits
> se mettent à jour à la requête suivante.

Captures réelles de la version 3.0 (établissement de démonstration) : `docs/assets/v3/`.

## 1. Premier lancement

### 1.1 Adresse du serveur

Aucune adresse n'est enregistrée dans l'application. Au premier lancement, **Paramètres
du serveur** (écran Bienvenue) demande l'**Adresse du serveur (API)** fournie par votre
prestataire, par exemple `https://api.exemple.com`. **Tester la connexion** la vérifie
(serveur Nhill, version compatible, HTTPS obligatoire), puis **Utiliser ce serveur**
l'enregistre sur l'appareil. Elle reste modifiable depuis le même écran. Si le serveur
exige une version plus récente de l'application, installez d'abord la mise à jour.

### 1.2 « Mon entreprise existe déjà » (rejoindre)

1. Saisissez le **code d'établissement** (`NH-XXXX-XXXX-XXXX`) remis par le propriétaire.
   Le code change régulièrement (tous les 30 jours par défaut) : s'il est refusé,
   demandez le nouveau. Le message d'erreur est volontairement le même dans tous les cas.
2. Puis **« J'ai un compte »** (e-mail et mot de passe) ou **« J'ai reçu une
   invitation »** (e-mail de l'invitation, code d'activation donné par votre responsable,
   mot de passe d'au moins 8 caractères à choisir).

### 1.3 « Créer mon entreprise »

1. **Type d'activité** : entrepreneur individuel (un seul utilisateur) ou entreprise ou
   organisation (plusieurs utilisateurs, rôles, invitations).
2. **Identité** : nom commercial, raison sociale, secteur, pays, assujettissement à la TVA,
   identifiants légaux qui vous concernent (RCCM, Id. Nat., NIF…).
3. **Coordonnées et préférences** : ville, adresse, téléphone, e-mail, devise principale,
   langue des documents.
4. **Votre compte** (propriétaire), puis vérification et création.

Aucun moyen de paiement n'est demandé. La formule de départ (en général l'essai gratuit,
30 jours par défaut) est attribuée par les règles de l'exploitant ; le tableau de bord
indique les jours restants. Notez le **code d'établissement** affiché à la fin. Si une
entreprise avec le même identifiant légal existe déjà, Nhill ne vous y rattache jamais
automatiquement : vous pouvez envoyer une **demande de rattachement**.

## 2. L'écran principal

Menu : Tableau de bord, Ventes, Clients (et fournisseurs), Produits et services, Stock,
Achats, Dépenses, Paiements, Rapports, Journal d'activité (ou Mon activité), Paramètres,
Aide et assistance — chacun selon votre rôle et la formule. En haut : recherche globale,
état de la connexion, état de l'abonnement, notifications.

| Raccourci | Action |
|---|---|
| `Ctrl+K` | recherche globale |
| `Ctrl+N` | nouvelle facture (si votre rôle et la formule le permettent) |
| `Ctrl+1` … `Ctrl+4` | Tableau de bord, Ventes, Clients, Produits |
| `F5` | actualiser |
| `Alt+←` | écran précédent |

Le tableau de bord n'affiche que les indicateurs des fonctions comprises dans la formule ;
sur un écran étroit, les titres des indicateurs passent sur deux lignes.

## 3. Ventes

- **Nouvelle facture** (ou devis, commande) : client, devise, date, échéance, lignes
  (produit ou ligne libre, quantité, prix HT, remise, taxe). Le serveur recalcule les
  totaux et fait foi.
- **Brouillon** : modifiable, sans numéro ni valeur fiscale.
- **Émettre** : numéro définitif (par exemple `FA-2026-000012`), stock diminué, empreinte
  SHA-256 du PDF enregistrée ; le document **ne peut plus être modifié ni supprimé**. Pour
  corriger : **Avoir** (total ou partiel, remise en stock possible, motif).
- **Devise étrangère** : le taux de change est demandé à l'émission, enregistré avec sa
  date et sa source, jamais recalculé (fonction « plusieurs devises » de la formule).
- **Devis** : accepté ou refusé, puis **Convertir en facture**. **Commande** : convertie
  en facture.
- **Paiement** : « Enregistrer un paiement » (espèces, Mobile Money, carte, virement,
  chèque…, référence), jamais au-delà du reste dû ; sur un avoir, « Enregistrer le
  remboursement ».
- **PDF A4** ou **Ticket** (58 ou 80 mm) : même document, même modèle que Nhill POS ; le
  fichier est enregistré dans le dossier de téléchargement de l'appareil, sous `Nhill`.
  Sur Android, **Partager** envoie le PDF vers la messagerie ou l'e-mail.
- **Coupure pendant une émission ou un paiement** : **Réessayer** ; l'opération n'est
  jamais enregistrée deux fois.
- **RDC, assujetti à la TVA** : les documents Nhill sont des documents de gestion, pas des
  factures normalisées. Émettez la facture normalisée avec votre dispositif électronique
  fiscal (DEF), puis **Enregistrer la référence DEF** (NID, code d'authentification,
  numéro) sur la facture Nhill pour le rapprochement. Nhill ne certifie rien.
- La limite mensuelle de documents de la formule, s'il y en a une, ne bloque jamais un
  avoir.

## 4. Clients, produits, stock, achats, dépenses

- **Clients et fournisseurs** : coordonnées, NIF, délai de paiement, notes ; la fiche
  montre le solde et les documents (fournisseurs : si la formule comprend achats ou
  dépenses).
- **Produits et services** : référence, nom, unité, prix de vente et d'achat, taxe, code-
  barres, seuil d'alerte. Export CSV ; **import CSV tout ou rien** (erreurs listées par
  ligne) si la formule le comprend. La formule peut limiter le nombre de produits.
- **Stock** : les quantités ne changent que par des mouvements (vente, achat reçu, avoir,
  ajustement avec motif, transfert entre dépôts si la formule permet plusieurs dépôts),
  chacun avec son auteur.
- **Achats** : brouillon → **réception** (entrée en stock, prix d'achat mis à jour) →
  paiement.
- **Dépenses** : montant, taxe, catégorie, fournisseur, payée ou non, **justificatif**
  (PDF, PNG ou JPEG, 5 Mo), dans la limite de stockage de la formule.

## 5. Rapports et journal d'activité

- **Rapports** (permission « rapports ») : journal des ventes (« Émis par »), paiements
  (« Enregistré par »), dépenses, mouvements de stock, créances et dettes à ce jour ; avec
  les rapports avancés de la formule : ventes par produit (marge **estimée**) et activité
  par employé. Période au choix, export **CSV** ou **PDF**. Les montants sont toujours
  présentés **par devise**, jamais convertis ni additionnés entre devises.
- **Journal d'activité** (propriétaire, administrateur, comptable) : toutes les actions
  de l'établissement, filtres par employé, type d'action et période, export CSV/PDF. Les
  autres rôles voient **Mon activité**. Le journal est tenu par le serveur et ne peut être
  ni modifié ni effacé, y compris par le propriétaire ; les consultations de statistiques
  par l'exploitant y apparaissent aussi.

## 6. Paramètres

| Onglet | Contenu | Qui |
|---|---|---|
| Établissement | identité, fuseau horaire (dates des documents et des sessions), TVA, identifiants légaux, logo (PNG ou JPEG, 2 Mo), langue des documents, délai de paiement, arrondi des taxes, instructions de paiement, **mentions imprimées sur chaque document** (vous les rédigez ; Nhill ne les écrit ni ne les vérifie), pied de page, vente sous stock nul ; **Exporter toutes mes données** (propriétaire) | propriétaire, administrateur |
| Code d'établissement | afficher, copier, renouveler maintenant | propriétaire, administrateur ; gérant en lecture |
| Équipe | membres, rôles, désactivation, invitations (code d'activation affiché **une seule fois**), mot de passe temporaire d'un membre (§ 8), nombre d'utilisateurs autorisé par la formule | propriétaire, administrateur |
| Abonnement | § 7 | propriétaire, administrateur |
| Taxes et comptes | taux de taxe (un taux ne se modifie pas : en créer un autre), caisses et comptes, taux de change | propriétaire, administrateur |
| Point de vente | caisses et caissiers, moyens de paiement, remise maximale, ticket, codes promotionnels de caisse, sessions et rapports X/Z ([nhill-pos.md](nhill-pos.md) § 1) | selon le rôle, si la formule comprend la caisse |
| Mon compte | nom, téléphone, langue, thème, mot de passe, « Se souvenir de moi sur cet appareil », déconnexion | tous |

**Exporter toutes mes données** : une archive ZIP avec toutes les données de
l'établissement (fichiers CSV et JSON), les fichiers enregistrés (logo, justificatifs) et
une liste du contenu avec empreintes. Disponible à tout moment pour le propriétaire, y
compris en lecture seule. L'archive contient des données personnelles : gardez-la en lieu
sûr.

Rôles : propriétaire, administrateur, gérant, vendeur, comptable, lecture seule,
responsable de caisse, caissier. Détail des permissions :
[reference-api.md](reference-api.md) (« Permissions par rôle »).

## 7. Abonnement (propriétaire et administrateurs)

- **Abonnement en cours** : formule, période et jours restants, fonctions **comprises dans
  votre formule** et usage des limites (par exemple « 3 sur 5 utilisés »).
- **Offres** : formules publiées par l'exploitant, prix par durée et par devise. Si aucune
  offre n'est publiée, l'écran l'indique.
- **Choisir cette offre** : récapitulatif calculé par le serveur (prix, code promotionnel,
  crédit des jours non utilisés, taxe, total). Offre supérieure : effet immédiat, partie
  non utilisée déduite. Offre inférieure : payée maintenant, appliquée à la fin de la
  période. Renouvellement : la nouvelle période commence à la fin de l'actuelle.
- **Paiement** : par un prestataire en ligne quand l'exploitant l'a activé, sinon virement
  ou Mobile Money avec la référence indiquée, confirmé par l'équipe de facturation. Rien
  n'est facturé avant la confirmation ; une demande non payée expire.
- **Renouvellement automatique** : seulement avec votre accord exprès (case à cocher),
  quand la formule le permet et qu'un prestataire compatible est connecté.
- **Rappels** avant l'échéance, puis **période de grâce** si le paiement n'est pas confirmé ;
  ensuite, selon la formule, passage à une formule de repli ou **lecture seule**
  (consultation et export restent possibles).
- **Fin du contrat** : 90 jours de lecture seule pour exporter vos données, puis
  suppression définitive par l'exploitant.

## 8. Mot de passe, connexion et sécurité

- **Employé** : en cas d'oubli, l'administrateur ou le propriétaire crée un **mot de passe
  temporaire** (Paramètres › Équipe › Réinitialiser le mot de passe), affiché une seule
  fois et remis en personne ou par téléphone ; vos sessions sont fermées ; à la connexion
  suivante, vous choisissez un nouveau mot de passe (8 caractères au moins). Un
  administrateur ne peut pas réinitialiser le propriétaire ni lui-même ; seul le
  propriétaire réinitialise un administrateur ; un compte utilisé dans un autre
  établissement passe par la procédure par e-mail.
- **Propriétaire, administrateur, compte de plusieurs établissements** : **Mot de passe
  oublié ?** sur l'écran de connexion envoie un code à 8 chiffres par e-mail (usage unique,
  durée limitée) ; le message affiché est le même que l'adresse existe ou non ; toutes les
  sessions sont fermées après le changement.
- **Se souvenir de moi** : jeton de session gardé dans le stockage sécurisé du système
  (Gestionnaire d'identification de Windows, Keystore d'Android, trousseau Linux), jamais
  le mot de passe ; à cocher seulement sur un appareil personnel ; la déconnexion l'efface.
- Hors connexion, on ne peut ni vendre ni facturer ; rien n'est perdu.
- N'indiquez jamais de mot de passe, de code d'établissement ni de numéro de carte dans
  **Aide et assistance**.

## 9. Ce que Nhill ne fait pas

Voir [guide-utilisateur.md](guide-utilisateur.md) § 3 (fiscalité, comptabilité générale,
paiements, envoi des documents, connexion, appareils).
