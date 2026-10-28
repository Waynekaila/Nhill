# Modèles de messages (WhatsApp, e-mail)

*English: [../en/06-message-templates.md](../en/06-message-templates.md)*

Règles communes : envoyer seulement à une personne qui a donné son accord pour ce canal, ou
qui vous a écrit la première ; signer avec un prénom ; au plus deux relances (J+3, puis
J+10), puis arrêt ; mention de désinscription dans les envois commerciaux ; ne jamais
annoncer un prix, une date de disponibilité ou une fonction qui ne figure pas dans la
proposition écrite ou dans le logiciel.

## Champs à fusionner

Chaque message est un modèle : les champs entre accolades sont remplacés, au moment de
l'envoi, par les informations du destinataire. Ce ne sont pas des informations manquantes
du document ; leur source est indiquée ci-dessous.

| Champ | Contenu | Source |
|---|---|---|
| `{contact}` | prénom de la personne, ou « Madame » / « Monsieur » suivi du nom | suivi des prospects, colonne `contact_nom` |
| `{expediteur}` | prénom et nom de la personne de l'équipe qui écrit | — |
| `{etablissement}` | nom du magasin, de l'entreprise ou de l'organisation | colonne `etablissement` |
| `{rendez_vous}` | jour, heure et lieu convenus | colonnes `prochaine_action`, `date_prochaine_action` |
| `{creneaux}` | deux créneaux libres de l'équipe | agenda de l'équipe |
| `{date_echange}` | date de la rencontre précédente | colonne `derniere_action` |
| `{date_atelier}` | date proposée pour l'atelier | agenda de l'équipe |
| `{places}` | nombre de places réservées à l'essai pour cette organisation | décision de l'équipe, notée dans le suivi des partenaires |
| `{debut_essai}`, `{fin_essai}` | premier et dernier jour des 30 jours d'essai | conditions de l'essai acceptées |
| `{interets}`, `{inquietudes}`, `{reponses}` | points notés pendant la démonstration | fiche de démonstration ([`07`](07-script-demonstration.md)) |
| `{benefice}` | bénéfice cité par l'établissement, avec ses mots | questionnaire de fin ([`08`](08-suivi-et-processus.md) § 5) |
| `{bilan}` | mesures avant et après, telles que mesurées | rapport d'essai ([`02`](02-programme-pilote.md) § 4) |
| `{corrections}` | corrections faites grâce aux retours de l'établissement | journal des problèmes de l'essai |
| `{fin_acces}` | date de fin de l'accès complet | Nhill Console, fiche de l'établissement |

## 1. WhatsApp — après une visite (accord donné)

> Bonjour {contact}, c'est {expediteur} de Nhill, nous nous sommes vus aujourd'hui à
> {etablissement}. Merci pour votre accueil.
> Voici la fiche de présentation promise (en pièce jointe). Pour la démonstration gratuite
> sur vos produits (15 min), dites-moi simplement le jour qui vous convient.
> Pour ne plus recevoir de messages de Nhill, répondez STOP.

Pièce jointe : `marketing/visuals/export/fiche-essai-a5-fr.pdf` (essai gratuit accompagné).

## 2. WhatsApp — confirmation de démonstration

> Bonjour {contact}, je confirme notre démonstration : {rendez_vous}. Si vous pouvez,
> préparez la liste de 10 à 20 articles que vous vendez le plus : je vous montrerai Nhill
> avec eux. À bientôt !

## 3. WhatsApp — relance (J+3, puis J+10 sans réponse)

> Bonjour {contact}, je reviens vers vous au sujet de Nhill. Souhaitez-vous toujours voir la
> démonstration ? Si ce n'est pas le moment, aucun souci, dites-le-moi et je ne vous
> relancerai pas.

## 4. E-mail — premier contact après un formulaire du site

**Objet** : Votre demande de démonstration Nhill

> Bonjour {contact},
>
> Merci pour votre demande de démonstration de Nhill pour {etablissement}.
>
> Je vous propose 30 minutes, à votre magasin ou en visioconférence, sur vos propres
> produits. Voici deux créneaux : {creneaux}. Si aucun ne convient, proposez-m'en un autre.
>
> Pour préparer, pouvez-vous m'indiquer : le nombre de personnes qui vendent, les appareils
> disponibles (ordinateur, tablette Android), et ce que vous aimeriez améliorer en priorité
> (caisse, stock, ventes à crédit) ?
>
> Bien cordialement,
> {expediteur} — Nhill
> info@nhill.online · nhill.online

## 5. E-mail — demande de rendez-vous à une organisation (association, incubateur)

**Objet** : Proposition de rencontre — outil de gestion pour vos membres commerçants

> Madame, Monsieur,
>
> Je suis {expediteur}, de Nhill, un logiciel de gestion des ventes, du stock et de la
> caisse conçu pour les commerçants et PME de la RDC (français et anglais, CDF et USD).
>
> Nous proposons à Kinshasa un essai gratuit accompagné de 30 jours, avec installation et
> formation. Il pourrait intéresser les membres de {etablissement} qui veulent mieux suivre
> leur caisse et leur stock.
>
> Accepteriez-vous un échange de 30 minutes pour voir si une collaboration a du sens ? Je
> peux venir à vos bureaux, au moment qui vous arrange.
>
> Précision importante : Nhill est un logiciel de gestion ; il n'est pas homologué par la
> DGI pour la facture normalisée.
>
> Bien cordialement,
> {expediteur} — Nhill Solution — info@nhill.online

## 6. Proposition de collaboration (après la rencontre)

**Objet** : Nhill × {etablissement} — proposition

> Bonjour {contact},
>
> Merci pour notre échange du {date_echange}. Comme convenu, voici notre proposition :
>
> 1. **Atelier** de 1 h 30 pour vos membres : bien suivre sa caisse et son stock, avec
>    démonstration de Nhill. Date proposée : {date_atelier}.
> 2. **Places réservées** à l'essai gratuit accompagné pour {places} de vos membres.
> 3. **Partenariat**, si vous le souhaitez : ses conditions (commission sur les abonnements
>    de vos membres, ou remise pour eux) sont fixées par écrit, dans une lettre d'accord,
>    avant tout démarrage.
>
> Ce que nous vous demandons : diffuser l'invitation à l'atelier à vos membres (sans nous
> transmettre leurs coordonnées sans leur accord).
>
> Je reste disponible pour en parler.
> {expediteur} — Nhill

Le point 3 renvoie aux conditions générales de partenariat
([`legal/fr/10-contrat-partenariat.md`](../../legal/fr/10-contrat-partenariat.md)).

## 7. Demande de recommandation (établissement satisfait, en S9)

> Bonjour {contact}, merci encore pour votre participation à l'essai. Vous m'avez dit que
> {benefice}. Connaissez-vous un ou deux commerçants à qui Nhill pourrait rendre le même
> service ? Si oui, demandez-leur d'abord s'ils sont d'accord, puis envoyez-moi leur nom et
> leur numéro, ou donnez-leur notre adresse : info@nhill.online.

## 8. Suivi après démonstration

**Objet** : Suite à notre démonstration — prochaines étapes

> Bonjour {contact},
>
> Merci pour votre temps aujourd'hui. Pour résumer :
> - ce qui vous intéresse : {interets} ;
> - ce qui vous inquiète : {inquietudes} — {reponses} ;
> - proposition : essai gratuit accompagné de 30 jours, du {debut_essai} au {fin_essai}.
>
> Je vous joins les conditions de l'essai gratuit accompagné, à lire tranquillement. Si
> elles vous conviennent, nous fixons la mise en route (une demi-journée).
>
> {expediteur} — Nhill

Pièce jointe : [`legal/pdf/fr/09-conditions-essai-accompagne.pdf`](../../legal/pdf/fr/09-conditions-essai-accompagne.pdf).

## 9. Fin d'essai — proposition de suite

**Objet** : Bilan de votre essai Nhill et proposition

> Bonjour {contact},
>
> Voici le bilan de vos 30 jours d'essai : {bilan}.
>
> Pour continuer, vous trouverez ci-jointe notre proposition écrite : formule, durée, prix,
> devise et moyens de paiement. Rien n'est facturé sans votre accord. Sans réponse de votre
> part, l'accès complet s'arrête le {fin_acces} ; Nhill passe alors en lecture seule et vous
> gardez 90 jours pour exporter vos données.
>
> Merci pour vos retours, qui ont permis de corriger {corrections}.
> {expediteur} — Nhill

## 10. Signature d'e-mail

Voir `marketing/visuals/signature-email-fr.html` (source modifiable).
