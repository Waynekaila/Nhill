# Étude de marché — Nhill à Kinshasa (RDC)

*English: [../en/01-market-study.md](../en/01-market-study.md)*

| | |
|---|---|
| Version | 2.0.0 — 27 octobre 2026 |
| Zone | Kinshasa d'abord, puis Lubumbashi et Goma |
| Méthode | recherche documentaire (sources § 8), sans enquête de terrain propre à Nhill ; les chiffres sont ceux des sources, avec leur date |
| Limite | aucune donnée de marché n'a été inventée ; là où une donnée manque, le document dit comment l'obtenir |

## 1. Résumé

- Les micro, petites et moyennes entreprises (MPME) congolaises s'équipent : dans une
  enquête de 914 MPME (Kinshasa, Goma, Lubumbashi), **51 %** utilisent des outils
  numériques, **46 %** des micro-entreprises et **71 %** des petites [S2].
- Parmi les MPME équipées, **94 %** utilisent un smartphone, **86 %** un ordinateur
  portable, **72 %** un logiciel de comptabilité mais seulement **19 %** un logiciel de
  gestion de stock [S2] : la gestion des ventes, du stock et de la caisse est le maillon
  peu outillé, c'est l'espace de Nhill.
- **Le coût** est le premier frein cité (54 % des micro-entreprises) [S2] ; la connexion
  et l'énergie suivent (coupures régulières pour 16 % des MPME, 33 % sans solution de
  secours) [S2]. Nhill doit être abordable, sobre en données et tolérant aux coupures.
- Accès numérique du pays : 114 millions d'habitants, 34,7 millions d'internautes (30,5 %),
  64,7 millions de connexions mobiles (56,9 % de la population), 10,4 millions d'identités
  sur les réseaux sociaux, 48,8 % de population urbaine (données d'octobre 2025) [S1].
- **Contrainte réglementaire décisive** : les assujettis à la TVA doivent émettre des
  factures normalisées avec un logiciel ou dispositif homologué par la DGI (sanctions
  annoncées à partir du 15 mai 2026) [S4]. Nhill n'est pas homologué. Le premier marché
  est donc celui des **commerces non assujettis à la TVA** et des usages de **gestion**
  (stock, caisse, suivi par employé) en complément du dispositif fiscal pour les
  assujettis.

## 2. Le problème observé

| Constat | Source | Ce que Nhill apporte |
|---|---|---|
| Faible équipement en gestion de stock (19 % des MPME équipées) | S2 | stock par mouvements, alertes de seuil, inventaire, historique par auteur |
| Coût, premier frein | S2 | abonnement par établissement, essai gratuit, pilote gratuit ; prix à fixer après le pilote |
| Coupures d'électricité et d'internet | S2 | reprise sans double saisie après coupure, application de bureau légère, Android |
| Recrutement par le bouche-à-oreille (79 % des MPME) : équipes de confiance, peu formalisées | S2 | un compte par personne, journal d'activité non modifiable, clôture de caisse « à l'aveugle » |
| 31 % des MPME de l'échantillon non immatriculées au RCCM | S2 | création possible sans identifiants légaux, ajout plus tard |
| Usage massif du mobile | S1, S2 | Nhill Business sur Android, écrans adaptés au téléphone |

## 3. Segments

| Segment | Description | Intérêt pour Nhill | Priorité |
|---|---|---|---|
| A. Boutiques et commerces de détail non assujettis à la TVA (Gombe, Limete, Ngaliema, Kintambo, marchés structurés) | 1 à 5 personnes, une caisse, stock de quelques centaines d'articles | caisse + stock + suivi des vendeurs ; décision rapide par le propriétaire | **1** |
| B. Quincailleries, dépôts de matériaux, grossistes | stock important, ventes à crédit, plusieurs vendeurs | stock, créances clients, achats fournisseurs | **1** |
| C. Pharmacies et parapharmacies (hors ordonnances) | nombreuses références, contrôle de caisse | caisse, stock, seuils | 2 |
| D. Prestataires de services (ateliers, imprimeries, agences) | devis et factures, peu de stock | devis → factures, paiements | 2 |
| E. PME assujetties à la TVA | doivent utiliser le dispositif DGI | Nhill en complément (stock, caisse, rapports), pas pour la facture normalisée | 3 — après clarification fiscale |

## 4. Concurrence et alternatives

| Alternative | Forces | Faiblesses pour la cible | Position de Nhill |
|---|---|---|---|
| Cahier, carnet de reçus, calculatrice | gratuit, connu, aucun matériel | pas de stock fiable, pas de contrôle des vendeurs, erreurs | même simplicité, avec le contrôle |
| Tableur (Excel) | souple, déjà installé | pas de traçabilité, fichiers perdus, pas multi-utilisateurs | données sur serveur, un compte par personne |
| Logiciels de gestion internationaux (ERP en ligne) | fonctionnels, intégrations | prix en devises, complexité, peu adaptés au français local et au CDF | simple, CDF et USD, français et anglais, accompagnement local |
| Logiciels de caisse locaux | présence locale, installateurs | souvent mono-poste, peu de contrôle à distance | multi-postes, consultation à distance, journal d'activité |
| Logiciels et dispositifs homologués DGI | conformes pour la facture normalisée | centrés sur la conformité fiscale | complémentaire (Nhill ne remplace pas la facture normalisée) |

Les prix publics des concurrents changent et dépendent des régions : relevez-les sur leur
site le jour de la décision de prix (tableau de veille dans `trackers/`), sans les
reproduire de mémoire.

## 5. Proposition de valeur

> **Nhill : vos ventes, votre stock et votre caisse, au clair — même quand vous n'êtes pas
> au magasin.**

Trois bénéfices, vérifiables dans le logiciel :

1. **Savoir ce qui se vend et ce qui reste** : chaque vente diminue le stock, les alertes
   préviennent avant la rupture.
2. **Savoir qui a fait quoi** : un compte par employé, un journal que personne ne peut
   effacer, une clôture de caisse où le caissier ne voit pas le montant attendu.
3. **Travailler malgré les coupures** : une opération interrompue se relance sans être
   enregistrée deux fois.

À ne jamais dire : « conforme DGI », « facture normalisée », « certifié », « hors ligne
complet » (Nhill a besoin d'une connexion pour enregistrer), « sans bug », chiffres de
gains non mesurés.

## 6. Prix (à fixer après le pilote)

Aucun prix n'est arrêté ni publié sur le site ; propositions chiffrées dans l'étude financière (document 16). Méthode proposée :

1. Pendant le pilote, mesurer le temps gagné et les écarts évités chez chaque pilote.
2. Interroger les pilotes sur leur dépense actuelle (papeterie, tableur, logiciel) et un
   prix acceptable (questionnaire de fin de pilote).
3. Relever les prix publics des alternatives (§ 4) à la même date.
4. Fixer 2 ou 3 offres (utilisateurs, caisses, dépôts) en CDF et en USD, avec un taux de
   conversion publié et daté (taux officiel BCC).

Repère de change pour les budgets de ce dossier : **1 USD = 2 264,0000 CDF**, taux officiel
de la Banque centrale du Congo du **26/10/2026** [S3].

## 7. Estimer la taille du marché (à faire, sans inventer)

Le nombre d'établissements de la cible à Kinshasa n'a pas été trouvé dans une source
fiable. Méthode :

| Étape | Source à consulter | Résultat attendu |
|---|---|---|
| 1 | Guichet unique de création d'entreprise, RCCM de Kinshasa : nombre d'immatriculations par forme et secteur | entreprises formelles |
| 2 | Fédération des entreprises du Congo (FEC), associations de commerçants | répartition par secteur et taille |
| 3 | Comptage terrain sur 3 artères commerçantes par segment (A, B, C) | densité de boutiques par rue |
| 4 | Part équipée d'un ordinateur ou d'une tablette Android (questionnaire de prospection) | marché adressable |
| 5 | Taux de conversion du pilote | marché atteignable à 12 mois |

## 8. Sources

| Réf. | Source | Date |
|---|---|---|
| S1 | [DataReportal — Digital 2026: The Democratic Republic of the Congo](https://datareportal.com/reports/digital-2026-democratic-republic-of-the-congo) | publié le 8 novembre 2025, données d'octobre 2025 |
| S2 | [Malabar, Innogence Consulting, Fondation Roi Baudouin — « Transformation digitale en RDC : état des lieux des MPME »](https://www.microsave.net/fr/wp-content/uploads/2025/09/Transformation-digitale-en-RDC-E%CC%81tat-des-lieux-des-MPME.pdf) ; présentation : [Fondation Roi Baudouin](https://kbs-frb.be/en/study-digital-transformation-smes-drc) | avril 2025 ; terrain du 16 octobre 2024 au 14 janvier 2025 ; 914 MPME (Kinshasa 270, Goma 334, Lubumbashi 270), **échantillon non représentatif** selon les auteurs |
| S3 | [Banque centrale du Congo — cours de change officiel](https://www.bcc.cd/marche-des-changes/cours-de-change) | 26/10/2026 : 1 USD = 2 264,0000 CDF |
| S4 | [DGI — Réforme de la facture normalisée](https://dgi.gouv.cd/reforme-de-la-facture-normalisee/) ; [La Prospérité — sanctions à partir du 15 mai 2026](https://www.laprosperite.cd/rdc-le-ministere-des-finances-met-fin-au-moratoire-sur-la-facture-normalisee-les-sanctions-debutent-le-15-mai-2026/) | consulté le 26/10/2026 |
| S5 | [GSMA — State of the Industry Report on Mobile Money 2026](https://www.gsma.com/sotir/wp-content/plugins/plugin_gsma_sotir/reports/The-State-of-the-Industry-Report-2026_French.pdf) : Afrique subsaharienne 2025, 1,2 milliard de comptes enregistrés, 341 millions de comptes actifs à 30 jours, 1 400 Md$ de transactions (pas de chiffre propre à la RDC) | 2026 |

Données consultées les 26 et 27 octobre 2026.
