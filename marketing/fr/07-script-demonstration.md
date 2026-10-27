# Script de démonstration (15 à 30 minutes)

*English: [../en/07-demo-script.md](../en/07-demo-script.md)*

Matériel : ordinateur ou tablette avec Nhill Business et Nhill POS installés, connectés à
un **établissement de démonstration** (données fictives, toutes marquées « (démo) », jamais
les données d'un vrai client). Avant la visite : ajouter 10 à 20 articles du prospect
(noms et prix qu'il a donnés), vérifier la connexion, charger l'appareil.

Ne montrer que ce qui existe dans la version installée. Ne jamais saisir de vraie donnée
personnelle d'un tiers pendant la démo.

## Déroulé

| Min. | Étape | Ce que vous faites | Ce que vous dites |
|---|---|---|---|
| 0-2 | Reprendre la douleur | rappeler ce que le prospect a dit (caisse, stock, vendeurs) | « Vous m'avez dit que la caisse du soir vous prend une heure. Regardons ce que ça donne avec Nhill. » |
| 2-6 | Une vente à la caisse | Nhill POS : ouvrir la caisse avec un fond, scanner ou toucher 3 articles du prospect, `F9`, paiement espèces avec monnaie rendue, ticket PDF | « Le stock a déjà baissé, la vente est au nom du caissier. » |
| 6-9 | Le contrôle | tenter une remise au-delà du plafond → demande de validation d'un superviseur ; clôture : le caissier saisit son comptage sans voir l'attendu ; rapport Z avec l'écart | « Le caissier ne peut pas ajuster son comptage pour tomber juste. » |
| 9-12 | Le stock | Nhill Business → Stock : quantités, seuil d'alerte, mouvements avec l'auteur ; un achat réceptionné | « Vous savez ce qui va manquer avant la rupture. » |
| 12-14 | Le propriétaire absent | sur un deuxième appareil (ou téléphone) : tableau de bord, rapport « activité par employé », journal d'activité | « Depuis chez vous, vous voyez ce qui s'est passé au magasin. » |
| 14-15 | Coupure | expliquer (sans simuler de fausse coupure) : une opération interrompue se relance sans doublon ; carnet de secours si longue coupure | « Nhill a besoin d'internet pour enregistrer ; il ne compte jamais deux fois. » |
| 15-20 | Questions | répondre ; noter les objections | voir réponses types dans `05` § 1 |
| 20-25 | Option : factures et crédit | facture à un client, paiement partiel, solde dû | « Vous savez qui vous doit combien. » |
| 25-30 | Conclusion | proposer le pilote, dates, convention | « On peut installer [jour]. Il faut une demi-journée et la liste de vos articles. » |

## Points de vigilance

- **TVA** : si le prospect est assujetti, le dire avant la fin : Nhill ne produit pas la
  facture normalisée DGI.
- **Prix** : pas encore fixés ; pilote gratuit ; proposition écrite avant toute facturation (pistes : document 16, non publiées).
- **Plates-formes** : vérifier ce qui est réellement livré (Windows, Linux, Android) dans
  `release/V<version>/` avant d'en parler.
- **Ce que Nhill ne fait pas en 2.1** : envoi direct des documents par e-mail ou SMS,
  paiement intégré des ventes par Mobile Money, comptabilité générale, paie,
  fonctionnement sans internet, certification fiscale. (Depuis la 2.1 : mot de passe
  oublié par l'administrateur ou par code e-mail, session mémorisée.)

## Compte rendu de démonstration (à remplir le jour même)

| Champ | Réponse |
|---|---|
| Date, lieu, durée | |
| Établissement, secteur, commune | |
| Personnes présentes (rôle) | |
| Douleurs confirmées | |
| Réactions positives (citations exactes) | |
| Objections et réponses données | |
| Questions sans réponse → à qui, pour quand | |
| Équipement constaté (appareils, internet, électricité) | |
| Score de qualification (`04` § 3) | /20 |
| Décision | pilote / relance le [DATE] / pas d'intérêt |
| Consentement aux messages (canal, date) | |
| Prochaine action et date | |

Copie dans `trackers/suivi-prospects.csv` (colonnes « score », « statut », « prochaine
action »).
