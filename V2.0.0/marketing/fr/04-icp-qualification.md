# Client idéal (ICP) et grille de qualification

*English: [../en/04-icp-qualification.md](../en/04-icp-qualification.md)*

## 1. Profil de client idéal

| Dimension | Profil |
|---|---|
| Activité | commerce de détail, quincaillerie, dépôt, grossiste, parapharmacie, prestataire de services avec devis |
| Lieu | Kinshasa (Gombe, Limete, Ngaliema, Kintambo, Lingwala, Barumbu, Masina en priorité selon le terrain), puis Lubumbashi et Goma |
| Taille | 2 à 15 personnes ; 1 à 3 points de vente ou caisses |
| Décideur | propriétaire ou gérant présent au magasin, qui gère lui-même la caisse le soir |
| Équipement | au moins un ordinateur (Windows 10/11 ou Linux) ou une tablette Android 7+ ; smartphone ; internet mobile |
| Fiscalité | non assujetti à la TVA (cible prioritaire) ; assujetti seulement s'il dispose déjà d'un dispositif DGI pour ses factures normalisées |
| Douleur | écarts de caisse, vols ou erreurs de stock, ne sait pas ce qui se vend, dépend d'un vendeur de confiance, cahiers perdus |
| Déclencheur | ouverture d'un 2e point de vente, recrutement de vendeurs, inventaire catastrophique, absence prolongée du propriétaire |

**Personas**

- **Mama Nzeba, propriétaire d'une boutique d'alimentation (4 personnes)** : passe chaque
  soir une heure sur le cahier, soupçonne des écarts mais ne peut pas les prouver. Veut :
  savoir combien elle a vendu sans être là.
- **Patrick, gérant d'une quincaillerie (8 personnes, ventes à crédit)** : clients qui
  paient en plusieurs fois, stock de milliers d'articles. Veut : savoir qui doit combien et
  ce qu'il reste en dépôt.

*Personas inventés pour illustrer le profil : ce ne sont ni des clients ni des témoignages.*

## 2. Hors cible (pour l'instant)

- Entreprises qui exigent une **facture normalisée** produite par Nhill (non homologué).
- Activités sans aucun appareil compatible ni connexion.
- Pharmacies délivrant des médicaments sur ordonnance (traçabilité réglementaire non
  couverte).
- Grandes entreprises demandant intégration ERP, comptabilité générale complète, paie.

## 3. Grille de qualification (score sur 20)

| Critère | 0 point | 1 point | 2 points | Coef. |
|---|---|---|---|---|
| Douleur exprimée (caisse, stock, contrôle des vendeurs) | aucune | vague | précise, avec un exemple récent | ×2 |
| Décideur rencontré | non | influence | décide seul | ×2 |
| Équipement compatible | aucun | smartphone seul | ordinateur ou tablette Android + internet | ×1 |
| Fiscalité | assujetti TVA sans dispositif DGI | assujetti avec dispositif DGI | non assujetti | ×1 |
| Disponibilité pour la mise en route | aucune | dans le mois | cette semaine | ×1 |
| Taille | 1 personne seule | 2 à 3 | 4 à 15 | ×1 |
| Potentiel de recommandation (réseau, association) | aucun | quelques contacts | membre actif d'une association ou d'un marché | ×1 |

Maximum : 2×2 + 2×2 + 2 + 2 + 2 + 2 + 2 = 18, plus **2 points bonus** si le prospect a
déjà essayé un tableur ou un logiciel (il sait ce qu'il veut) → 20.

| Score | Décision |
|---|---|
| ≥ 14 | proposer le pilote |
| 9 à 13 | démonstration, puis relance dans 2 semaines |
| ≤ 8 | garder dans la liste, informer (avec consentement) |

## 4. Questions de découverte (10 minutes)

1. Comment notez-vous vos ventes aujourd'hui ? Qui les note ?
2. Comment faites-vous la caisse le soir ? Combien de temps cela prend-il ?
3. La dernière fois que la caisse ne tombait pas juste, qu'avez-vous fait ?
4. Comment savez-vous qu'un article va manquer ?
5. Vendez-vous à crédit ? Comment suivez-vous ce qu'on vous doit ?
6. Combien de personnes vendent ? Ont-elles chacune un accès ?
7. Quels appareils avez-vous au magasin ? Internet est-il stable ?
8. Êtes-vous assujetti à la TVA ? Utilisez-vous un dispositif DGI ?
9. Qu'avez-vous déjà essayé (cahier, Excel, logiciel) ? Qu'est-ce qui n'a pas marché ?
10. Si cela marchait, qu'est-ce qui changerait pour vous dans un mois ?

Noter les réponses dans le suivi des prospects (`trackers/suivi-prospects.csv`).
