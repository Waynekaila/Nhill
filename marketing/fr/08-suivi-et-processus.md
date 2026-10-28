# Processus commercial, suivi des prospects et questionnaires

*English: [../en/08-sales-process-and-tracking.md](../en/08-sales-process-and-tracking.md)*

## 1. Étapes du processus (conversion)

| N° | Statut (colonne `statut`) | Entrée dans l'étape | Sortie | Délai cible |
|---|---|---|---|---|
| 1 | `identifié` | établissement repéré (terrain, recommandation, formulaire) | premier contact fait | — |
| 2 | `contacté` | visite, appel ou message (avec accord) | démo fixée, ou refus | 7 jours |
| 3 | `démo fixée` | date confirmée | démo faite | 7 jours |
| 4 | `démo faite` | compte rendu rempli, score calculé | essai proposé ou relance | 2 jours |
| 5 | `pilote proposé` | conditions de l'essai envoyées | acceptées par e-mail, ou refus | 7 jours |
| 6 | `pilote en cours` | mise en route faite | bilan à J30 | 30 jours |
| 7 | `proposition` | proposition écrite de suite envoyée | accepte / refuse | 14 jours |
| 8 | `client` | premier paiement confirmé | — | — |
| — | `perdu` | refus ou absence de réponse après 2 relances | motif noté (`motif_perte`) | — |
| — | `plus tard` | intérêt mais pas maintenant | relance datée | — |

Taux de passage à suivre (indicateurs `10`) : contacté → démo, démo → pilote,
pilote → client.

## 2. Fichier de suivi des prospects

`marketing/trackers/suivi-prospects.csv` (UTF-8, séparateur `;`, ouvrable dans un tableur)
et l'onglet « Prospects » de `trackers/kpi-tableau-de-bord.xlsx`.

| Colonne | Contenu |
|---|---|
| `id` | P-0001… |
| `date_entree` | AAAA-MM-JJ |
| `etablissement`, `secteur`, `commune` | — |
| `contact_nom`, `contact_role` | — |
| `telephone`, `email` | seulement si fournis par la personne |
| `consentement_canal`, `consentement_date` | ex. `WhatsApp`, `2026-11-03` (vide = aucun envoi commercial) |
| `source` | terrain / recommandation / site / partenaire:[ID] / événement |
| `score` | grille `04` § 3 (/20) |
| `statut` | voir § 1 |
| `derniere_action`, `prochaine_action`, `date_prochaine_action` | — |
| `motif_perte` | prix / pas d'appareil / TVA / pas le moment / autre |
| `notes` | sans donnée sensible |

Le fichier contient des données personnelles : accès limité à l'équipe commerciale,
conservation selon `legal/fr/15-politique-conservation.md`, jamais partagé publiquement.

## 3. Rituel hebdomadaire (lundi, 30 minutes)

1. Mettre à jour les statuts ; supprimer les lignes des personnes qui l'ont demandé.
2. Lister les actions de la semaine (`date_prochaine_action`).
3. Calculer les indicateurs (`10`), comparer à l'objectif du plan (`03`).
4. Une décision : quoi arrêter, quoi renforcer.

## 4. Questionnaire de satisfaction — mi-parcours (S4, 5 minutes)

1. Combien de jours par semaine utilisez-vous Nhill ? (0-1 / 2-3 / 4-6)
2. Qu'est-ce qui vous sert le plus ? (caisse, stock, factures, rapports, journal d'activité, autre)
3. Qu'est-ce qui vous gêne le plus ? (réponse libre)
4. Avez-vous eu un problème bloquant ? A-t-il été résolu ? En combien de temps ?
5. La formation était-elle suffisante pour chaque utilisateur ? (oui / en partie / non)
6. Note de 0 à 10 : recommanderiez-vous Nhill à un autre commerçant ?
7. Une chose à changer en priorité ?

## 5. Questionnaire de fin d'essai (J30, 10 minutes)

1. Reprise des questions 1, 2, 3 et 6 de mi-parcours.
2. Par rapport au début, la clôture de caisse prend : plus / autant / moins de temps.
   (À compléter par la mesure chronométrée.)
3. Les écarts de caisse ou de stock : ont augmenté / sont stables / ont diminué / je ne sais pas.
4. Que dépensez-vous aujourd'hui pour gérer vos ventes (cahiers, tableur, logiciel,
   personne dédiée) ? (réponse libre, montant facultatif)
5. À quel prix mensuel Nhill vous paraîtrait : trop cher pour l'envisager / cher mais
   envisageable / une bonne affaire / si bas que vous douteriez de la qualité ?
   (montants libres, en CDF ou USD)
6. Souhaitez-vous continuer après le pilote ? (oui / oui sous conditions : … / non : pourquoi)
7. Acceptez-vous que nous citions votre retour ? (non / oui, sans nom / oui, avec nom —
   formulaire d'accord séparé à signer, convention article 7)

## 6. Modèle de relance et de recommandation

Voir `06-modeles-messages.md` § 3 (relance), § 7 (recommandation), § 9 (fin de pilote).
