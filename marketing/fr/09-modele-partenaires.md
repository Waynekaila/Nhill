# Modèle de partenariat

*English: [../en/09-partner-model.md](../en/09-partner-model.md)*

Contrat type : [`legal/fr/10-contrat-partenariat.md`](../../legal/fr/10-contrat-partenariat.md).
Page publique : `website/src/pages/fr/partenaires.html`.

## 1. Types de partenaires

| Type | Qui | Ce qu'il fait | Rémunération | Ce que Nhill fournit |
|---|---|---|---|---|
| Apporteur | commerçant influent, comptable indépendant, agent de terrain | présente Nhill, enregistre le prospect avant tout contact de Nhill | commission sur les sommes effectivement encaissées ; taux et durée fixés dans la lettre d'accord | fiche partenaire, lien du formulaire, relevé de chaque paiement de commission |
| Intégrateur | prestataire informatique, revendeur de matériel (imprimantes de tickets, tablettes) | installe, importe le catalogue, assistance de premier niveau | facture ses prestations au client ; commission éventuelle | formation technique de 2 h, documentation, accès de démonstration |
| Formateur | école de gestion, centre de formation, formateur indépendant | forme les utilisateurs | facture sa formation ; supports Nhill gratuits | supports de formation, établissement de démonstration |
| Organisation d'accompagnement | association de commerçants, incubateur, structure d'appui aux PME, fédération | fait connaître Nhill, accueille des ateliers | pas de commission sauf mention dans la lettre d'accord ; places réservées à l'essai gratuit accompagné, atelier gratuit pour les membres | atelier clé en main, places réservées |
| Utilisateur de référence | établissement client satisfait | reçoit d'autres commerçants, **seulement s'il le souhaite** | aucune, sauf contrepartie écrite dans la lettre d'accord, mentionnée à côté de tout témoignage publié ([`legal/fr/20`](../../legal/fr/20-autorisation-temoignage-image.md)) | — |

## 2. Règles

- Aucun partenaire n'est annoncé publiquement sans contrat signé et accord sur l'usage du nom.
- Un partenaire ne promet que ce que Nhill fait (liste « ce que Nhill ne fait pas » dans
  `07-script-demonstration.md`) et ne présente jamais Nhill comme homologué DGI.
- Un partenaire ne demande jamais de mot de passe à un commerçant ; il crée ses propres
  accès si l'établissement l'invite.
- Les contacts transmis le sont avec l'accord des personnes (`legal/fr/16`).

## 3. Parcours d'un partenaire

| Étape | Délai |
|---|---|
| Premier échange (30 min) : réseau, motivation, type de partenariat | — |
| Démonstration et formation (2 h) | sous 1 semaine |
| Signature du contrat | sous 2 semaines |
| Premier prospect enregistré | sous 3 semaines |
| Point mensuel : prospects, conversions, retours, relevé de commissions | chaque mois |

## 4. Suivi

`marketing/trackers/suivi-partenaires.csv` : `id`, `nom`, `type`, `commune`, `date_contrat`,
`prospects_enregistres`, `demos`, `pilotes`, `clients`, `encaisse_usd`, `commission_due_usd`,
`commission_payee_usd`, `derniere_rencontre`, `notes`.

## 5. Indicateurs partenaires

| Indicateur | Cible à 90 jours |
|---|---|
| Partenaires signés | 3 |
| Part des pilotes venant de partenaires | ≥ 30 % |
| Délai moyen de paiement des commissions | ≤ 10 jours après encaissement |
