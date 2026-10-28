# Programme pilote Nhill — Kinshasa

*English: [../en/02-pilot-programme.md](../en/02-pilot-programme.md)*

Objectif : valider, chez **8 à 12 établissements réels**, que Nhill règle un problème qu'ils
sont prêts à payer, et fixer les prix à partir de mesures, pas d'hypothèses. Contrat :
[`legal/fr/09-conditions-essai-accompagne.md`](../../legal/fr/09-conditions-essai-accompagne.md).
Page publique : `website/src/pages/fr/essai-gratuit.html` (« Essai gratuit accompagné »).

## 1. Offre aux pilotes (identique au site)

| Nous apportons | Nous demandons |
|---|---|
| Nhill Business (et Nhill POS si caisse) gratuits pendant 30 jours | utiliser Nhill pour une partie réelle de l'activité, chaque jour ouvré si possible |
| installation sur les appareils, import du catalogue | une personne référente, 20 à 30 minutes par semaine |
| formation de départ pour chaque utilisateur | signaler les problèmes, répondre à 2 courts questionnaires |
| point hebdomadaire, assistance aux heures de travail | une connexion internet sur les appareils |
| export des données (CSV, PDF) à la fin | — |

Pas de prix promis pendant le pilote : les conditions de suite sont présentées par écrit
avant la fin.

## 2. Critères de sélection

| Critère | Requis |
|---|---|
| Segment | A (détail), B (quincaillerie, dépôt) en priorité ; 1 ou 2 de C ou D |
| Taille | 1 à 20 utilisateurs |
| Équipement | au moins un ordinateur Windows/Linux ou une tablette Android 7+ ; internet |
| Fiscalité | de préférence non assujetti à la TVA ; un assujetti seulement s'il garde son dispositif DGI |
| Engagement | référent identifié, disponibilité pour la mise en route (une demi-journée) |
| Diversité | au moins 3 communes de Kinshasa différentes |

Grille de notation : [`04-icp-qualification.md`](04-icp-qualification.md) § 3.

## 3. Déroulé (30 jours)

Les premiers essais accompagnés servent de pilote : même offre que le site, avec des
mesures avant et après.

| Moment | Étape | Livrable |
|---|---|---|
| J0 | démonstration sur les produits de l'établissement ; acceptation des conditions de l'essai (`legal/fr/09-conditions-essai-accompagne.md`) ; mesure de départ (voir § 4) | acceptation par e-mail, fiche de départ |
| J0 | mise en route : installation, import du catalogue, création des comptes, formation | établissement opérationnel |
| semaine 1 | suivi rapproché : 2 appels de 10 minutes, passage sur place si besoin | journal des problèmes |
| semaine 2 | point de 20 minutes ; **questionnaire de mi-parcours** ([`08-suivi-et-processus.md`](08-suivi-et-processus.md) § 4) | réponses |
| semaine 3 | point de 20 minutes ; corrections livrées | — |
| J30 | **bilan** : mesures de fin, questionnaire de fin, proposition écrite de suite | rapport d'essai |

## 4. Mesures (avant / après)

| Mesure | Comment la mesurer | Qui |
|---|---|---|
| Temps de clôture de caisse quotidienne | chronomètre, 3 jours avant l'essai, 3 jours la dernière semaine | référent |
| Écart de caisse moyen | relevé du pilote avant ; rapports Z de Nhill pendant | Nhill |
| Écarts de stock à l'inventaire | inventaire de 20 articles à J0 et à J30 | référent + Nhill |
| Part des ventes saisies dans Nhill | ventes Nhill / ventes totales déclarées par le pilote | référent |
| Utilisation | jours avec au moins une vente, utilisateurs actifs (statistiques agrégées de la console, visibles dans le journal du pilote) | Nhill |
| Satisfaction | note de 0 à 10 « recommanderiez-vous Nhill ? » + verbatim (avec accord) | questionnaire |
| Disposition à payer | questions ouvertes de fin de pilote | questionnaire |

Aucun résultat n'est publié sans l'accord écrit du pilote et tel que mesuré.

## 5. Critères de réussite du programme

| Indicateur | Seuil de décision |
|---|---|
| Pilotes actifs la dernière semaine (au moins 4 jours d'usage par semaine) | ≥ 60 % des pilotes |
| Note de recommandation moyenne | ≥ 7 / 10 |
| Pilotes prêts à payer au prix proposé | ≥ 40 % |
| Incidents bloquants non résolus en 48 h | 0 |

Si les seuils ne sont pas atteints : ne pas lancer la vente large ; corriger, puis
deuxième vague de pilotes.

## 6. Organisation

| Rôle | Charge |
|---|---|
| Responsable du pilote (1 personne) | recrutement, conditions de l'essai, points hebdomadaires, rapport |
| Technicien d'installation (peut être un partenaire intégrateur) | mise en route, assistance sur place |
| Développement | corrections, suivi des erreurs techniques remontées |

Capacité : 1 responsable suit au plus 6 pilotes en parallèle ; au-delà, deux vagues.

## 7. Risques

| Risque | Parade |
|---|---|
| Coupures d'électricité / internet chez le pilote | test de connexion à la sélection ; procédure papier de secours ; reprise sans double saisie |
| Abandon après 2 semaines | référent engagé, appels de la première semaine, objectifs choisis par le pilote |
| Pilote assujetti à la TVA qui croit Nhill homologué | message clair dès la démonstration et dans les conditions de l'essai |
| Données importées incorrectes | import « tout ou rien » avec liste d'erreurs ; vérification à deux |
| Version non construite pour une plate-forme | vérifier les livrables disponibles (`release/V3.0.0/checksums/MISSING.txt`) avant de promettre Windows ou Android |
