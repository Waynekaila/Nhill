# Indicateurs clés (KPI) et tableau de bord

*English: [../en/10-kpis.md](../en/10-kpis.md)*

Fichier de suivi : `marketing/trackers/kpi-tableau-de-bord.xlsx` (onglets « Tableau de
bord », « Hebdo », « Prospects », « Partenaires », « Budget », « Taux ») — formules
incluses, à remplir chaque lundi. Les objectifs sont ceux du plan de 90 jours (`03`).

## 1. Indicateurs

| Famille | Indicateur | Définition exacte | Source | Objectif 90 j |
|---|---|---|---|---|
| Acquisition | Prospects identifiés | lignes ajoutées au suivi des prospects | suivi | 150 |
| | Contacts réalisés | statut ≥ `contacté` | suivi | 90 |
| | Taux contact → démo | démos faites / contacts réalisés | suivi | ≥ 25 % |
| | Coût par démo | dépenses marketing / démos faites | budget + suivi | à mesurer |
| Activation | Démos faites | statut ≥ `démo faite` | suivi | 25 |
| | Taux démo → pilote | pilotes signés / démos faites | suivi | ≥ 40 % |
| | Pilotes actifs | pilotes avec ≥ 4 jours d'usage la semaine passée | console (agrégats) | 10 |
| | Délai de mise en route | jours entre signature et première vente dans Nhill | suivi | ≤ 3 j |
| Satisfaction | Note de recommandation moyenne | moyenne des réponses 0-10 | questionnaires | ≥ 7 |
| | Incidents bloquants > 48 h | nombre | assistance | 0 |
| Revenus | Taux pilote → client | clients / pilotes terminés | suivi | ≥ 40 % |
| | Revenu mensuel récurrent (MRR) | somme des abonnements actifs ramenés au mois, **par devise** | console (transactions) | à fixer avec les prix |
| | Encaissé | paiements confirmés, par devise | console | — |
| Partenaires | Partenaires signés | contrats signés | suivi partenaires | 3 |
| | Part des pilotes via partenaires | pilotes `source=partenaire` / pilotes | suivi | ≥ 30 % |
| Budget | Dépense cumulée vs enveloppe | dépenses / enveloppe du scénario | budget | ≤ 100 % |

Règles : les montants ne sont jamais additionnés entre USD et CDF (comme dans Nhill) ; une
conversion pour comparaison utilise le taux BCC daté de l'onglet « Taux ».

## 2. Lecture hebdomadaire

| Signal | Interprétation probable | Action |
|---|---|---|
| Beaucoup de contacts, peu de démos | message ou cible | revoir le script `05`, recentrer la cible `04` |
| Démos sans pilote | démonstration ou doute (prix, TVA, appareils) | revoir `07`, lister les objections |
| Pilotes inactifs | mise en route ou formation | appel de soutien, visite sur place |
| Satisfaction < 7 | produit | remonter les problèmes en priorité au développement |
