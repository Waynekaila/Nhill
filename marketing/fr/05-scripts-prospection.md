# Scripts de prospection (visite, appel, WhatsApp vocal)

*English: [../en/05-prospecting-scripts.md](../en/05-prospecting-scripts.md)*

Principes : court, poli, concret ; parler du problème du commerçant avant de parler du
logiciel ; ne rien promettre que Nhill ne fait pas ; demander l'accord avant de noter un
numéro pour des envois (modèle `legal/fr/16-consentement-prospection.md`). Adapter la
langue : français, lingala ou anglais selon l'interlocuteur.

Les champs entre accolades se remplacent au moment de l'appel ou de la visite :
`{expediteur}` = votre prénom ; `{recommandant}` = nom de la personne qui a recommandé le
contact (suivi des prospects, colonnes `source` et `notes`). Les autres champs sont décrits
dans [`06-modeles-messages.md`](06-modeles-messages.md).

## 1. Visite en magasin (2 minutes)

> Bonjour Madame (ou Monsieur), je m'appelle {expediteur}, de Nhill. Je ne vous prends que
> deux minutes, je vois que vous avez des clients.
>
> Nhill est un logiciel qui permet aux commerçants de suivre leurs ventes, leur stock et
> leur caisse sur un ordinateur, une tablette ou un téléphone. Par exemple, le soir, vous
> voyez combien chaque vendeur a encaissé, sans refaire les calculs du cahier.
>
> Est-ce que la caisse du soir vous prend du temps, ou est-ce qu'il vous arrive d'avoir des
> écarts ?
>
> *Écouter. Poser 2 ou 3 questions de découverte (voir `04` § 4).*
>
> Nous proposons un essai gratuit accompagné de 30 jours, avec installation et
> formation. Je peux vous montrer en quinze minutes, avec vos propres produits, quand vous
> serez plus disponible. Quel jour vous arrange ?

**Si « je n'ai pas le temps »** : « Je comprends. Quand puis-je repasser : plutôt en début
ou en fin de semaine ? Ou je vous laisse cette fiche, et si vous m'autorisez à vous écrire
sur WhatsApp, je vous l'envoie aussi en PDF. »

**Si « c'est cher »** : « L'essai de 30 jours est gratuit et sans engagement. Ensuite, vous
recevez une proposition écrite adaptée à votre établissement ; rien n'est facturé sans
votre accord. »

**Si « je suis assujetti à la TVA »** : « Nhill ne remplace pas votre dispositif de facture
normalisée de la DGI. Il vous aide pour le stock, la caisse et le suivi des vendeurs ; vous
gardez votre dispositif pour la facture fiscale. »

**Si « et si internet coupe ? »** : « Nhill a besoin d'internet pour enregistrer. Si la
connexion coupe au milieu d'une vente, vous la relancez et elle n'est pas comptée deux fois.
Pour une longue coupure, gardez un carnet de secours ; on ressaisit ensuite. »

**Si « mes données ? »** : « Elles sont à vous. Chaque établissement est séparé des autres
dans la base, et notre équipe ne voit pas vos factures ni vos clients. Vous pouvez tout
exporter quand vous voulez. »

## 2. Appel téléphonique (après une recommandation)

> Bonjour Madame (ou Monsieur), {expediteur} de Nhill. {recommandant} m'a donné votre
> numéro et m'a dit que vous pourriez être intéressé(e) par un outil pour suivre la caisse
> et le stock. Avez-vous deux minutes ?
>
> *Si oui :* Comment gérez-vous la caisse aujourd'hui ? … Je vous propose une
> démonstration de quinze minutes à votre magasin, sur vos produits. Mardi ou jeudi ?
>
> *Si non :* Je peux vous envoyer notre fiche de présentation sur WhatsApp, si vous êtes
> d'accord ? *Noter l'accord, la date et le canal.*

Ne téléphoner que si le recommandant a demandé à la personne si elle acceptait d'être
appelée (document `legal/fr/16`, § 4).

## 3. Message vocal WhatsApp (30 secondes, seulement avec accord)

> Bonjour, c'est {expediteur} de Nhill, merci pour votre accord. Comme promis : Nhill vous
> permet de voir vos ventes, votre stock et la caisse de chaque vendeur, même quand vous
> n'êtes pas au magasin. Je vous envoie la fiche de présentation juste après. Si vous voulez
> la démonstration gratuite sur vos produits, répondez simplement avec un jour qui vous
> arrange. Bonne journée.

## 4. Après le contact

| Étape | Délai |
|---|---|
| Saisir le contact dans `trackers/suivi-prospects.csv` (statut, score, consentement) | le jour même |
| Envoyer ce qui a été promis (fiche de présentation `marketing/visuals/export/fiche-essai-a5-fr.pdf`) | dans les 24 h |
| Relance si pas de réponse | J+3, puis J+10, puis arrêt |
