# Nhill POS 3.0 — Guide de la caisse

*English: [../en/nhill-pos.md](../en/nhill-pos.md)*

Nhill POS est la caisse d'un établissement Nhill : mêmes produits, prix, taxes, stock et
numérotation que Nhill Business. Plateformes : Windows 10 et 11 (64 bits), Linux
(Ubuntu 24.04, Debian 13) ; lecteur de codes-barres USB possible (il se comporte comme un
clavier). Pas de caisse sur Android ni sur iOS. La caisse n'est disponible que si la
formule de l'établissement la comprend.

Chaque caissier se connecte avec **son propre compte**, créé dans Nhill Business (rôle
Caissier ou Responsable de caisse ; propriétaire, administrateur et gérant peuvent aussi
vendre).

## 1. Préparer la caisse (Nhill Business › Paramètres › Point de vente)

1. **Nouvelle caisse** : code imprimé sur les tickets (par exemple `C1`), nom, emplacement,
   dépôt dont le stock est utilisé. Le nombre de caisses est limité par la formule.
2. **Caissiers autorisés** : cocher les membres qui peuvent ouvrir cette caisse.
3. **Moyens de paiement** : Espèces (toujours présent), carte, Mobile Money, etc. Nhill
   enregistre le montant et la référence ; il ne pilote ni terminal de carte ni opérateur.
4. **Règles et ticket** : remise maximale sans validation, prix modifiable ou non, largeur
   du ticket (58 ou 80 mm), message de pied de ticket.
5. **Codes promotionnels de caisse** : pourcentage, montant minimal, validité, nombre
   d'utilisations.
6. **Sessions et rapports** : liste des sessions, lectures X et rapports Z.

## 2. Se connecter et ouvrir la session

1. Premier lancement : **Connectez cette caisse à votre serveur Nhill** — saisir l'adresse
   du serveur fournie par votre prestataire (HTTPS), vérifiée avant d'être enregistrée sur
   le poste. **Changer de serveur** reste possible ; la session en cours de la caisse se
   termine alors.
2. Se connecter (e-mail, mot de passe). « Mot de passe oublié ? » et « Se souvenir de moi »
   fonctionnent comme dans Nhill Business ([nhill-business.md](nhill-business.md) § 8) ;
   sur un poste partagé par plusieurs caissiers, laissez « Se souvenir de moi » décoché.
3. **Choisissez votre caisse** (un caissier ne voit que les caisses qui lui sont
   attribuées ; un superviseur — responsable de caisse, gérant, propriétaire,
   administrateur — les voit toutes),
   comptez le **fond de caisse**, puis **Ouvrir la session de caisse**.

## 3. Vendre

| Touche ou bouton | Action |
|---|---|
| `F2` | revenir à la recherche / au code-barres |
| Entrée (après un code-barres ou une recherche) | ajouter le produit |
| − / + | diminuer / augmenter la quantité d'une ligne |
| Remise | remise en pourcentage sur la ligne |
| `F9` ou **Encaisser** | ouvrir l'encaissement |

- Le stock du dépôt de la caisse s'affiche sur chaque produit ; un article sans stock est
  signalé.
- **Client** : client comptoir par défaut, ou un client recherché par nom ou téléphone.
- **Code promo** : vérifié et appliqué par le serveur à l'encaissement.
- **Encaissement** : un ou plusieurs moyens de paiement ; pour les espèces, saisir le
  **montant remis** : la **monnaie à rendre** s'affiche. **Valider**.
- **Facture au lieu d'un ticket** : pour un client identifié, cocher « Établir une facture
  au nom de … » (si la formule comprend les factures). La facture a le même type, la même
  numérotation et le même modèle que dans Nhill Business ; **Imprimer la facture (A4)**.
  Sinon **Imprimer le ticket** (PDF au format rouleau).
- **Au-delà de la remise autorisée** (ou pour un prix modifié, un retour, une annulation,
  une sortie d'espèces, la clôture de la session d'un autre caissier sans la permission) :
  un superviseur saisit **son propre** e-mail et **son** mot de passe ; la validation ne
  sert qu'une fois et est inscrite au journal avec son nom.
- **Coupure réseau pendant la validation** : appuyer de nouveau sur **Valider** ; la vente
  n'est pas enregistrée deux fois.

## 4. Tickets et caisse

Écran **Tickets et caisse** : ventes, retours et annulations, ventes nettes, remises,
espèces attendues (vue superviseur), mouvements de caisse, liste des tickets et factures
de la session (date et heure, numéro, type, client, total, statut).

- Sur un ticket : **Imprimer**, **Retourner les quantités** (saisir la quantité à retourner
  sur chaque ligne concernée ; un retour crée un avoir, rembourse en espèces et remet la
  marchandise en stock) ou **Annuler le ticket** (ticket entier, même session). Motif
  obligatoire.
- **Entrée / sortie** d'espèces (monnaie, apport, dépôt en banque, dépense) avec motif ; une
  sortie ne peut pas dépasser les espèces présentes.
- **Lecture X** : rapport intermédiaire sans clôturer.

## 5. Clôturer

**Clôturer la caisse** : compter le tiroir (montant **obligatoire**, 0 si vide) et reporter
le total de chaque terminal ou relevé. Le caissier compte sans voir le montant attendu ; le
serveur calcule l'écart, contrôlé ensuite par un responsable. Le **rapport Z** se télécharge
après la clôture ; une session clôturée ne peut plus être modifiée.

Les tickets sont des documents de gestion : Nhill POS n'est pas une caisse certifiée ni un
dispositif de facturation fiscale agréé.
