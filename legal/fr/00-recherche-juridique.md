# Recherche juridique — Nhill en République démocratique du Congo

*English: [../en/00-legal-research.md](../en/00-legal-research.md)*

| | |
|---|---|
| Version | 2.0.0 — 27 octobre 2026 |
| Juridiction principale | République démocratique du Congo (droit congolais et droit uniforme OHADA) |
| Objet | cadre applicable à l'édition et à l'exploitation du logiciel en ligne Nhill (SaaS de gestion commerciale et de caisse) et de son site vitrine ; base des modèles `01` à `19` |
| Méthode | lecture de textes officiels quand ils sont accessibles en ligne, sinon de sources secondaires identifiées comme telles (doctrine, presse spécialisée) ; consultation des sources du 26 au 27 octobre 2026 |
| Statut | **document de travail, non validé par un avocat**. Il ne constitue pas un avis juridique |

> **Avertissement.** Ce dossier a été préparé sans accès au Journal officiel de la RDC et
> sans consultation d'un avocat inscrit à un barreau congolais. Plusieurs textes récents
> (2024-2026) ne sont connus que par des commentaires ou la presse ; leur portée exacte,
> leur validité et leur articulation doivent être confirmées. Aucun élément propre à
> l'entreprise exploitante (forme juridique, RCCM, identification nationale, NIF, adresse,
> capital, dirigeants) n'est connu : ils apparaissent entre crochets dans les modèles et
> ne doivent jamais être inventés.

## 1. Synthèse pour la direction

| Sujet | Ce que l'on sait | Conséquence pour Nhill | Niveau de certitude |
|---|---|---|---|
| Données personnelles | Code du numérique (Ordonnance-loi n° 23/010 du 13 mars 2023), Livre III : principes, droits des personnes, sécurité, délégué à la protection des données, formalités préalables, transferts hors RDC encadrés | Nhill est **responsable de traitement** pour ses prospects, clients-payeurs et administrateurs, **sous-traitant** pour les données que les établissements saisissent | Élevé sur l'existence du cadre ; moyen sur les détails |
| Autorité de contrôle | Missions de l'autorité de protection des données confiées à titre provisoire à l'ARPTC (2024) ; la presse et la doctrine indiquent ensuite son remplacement par une nouvelle autorité, l'**ARPTIC** (2026) | Identifier l'autorité **compétente au jour du dépôt** et la procédure en vigueur | **Faible : à vérifier en priorité** |
| Formalités et frais | Décision n° 039/ARPTC/CLG/2025 du 11 septembre 2025 (déclaration, autorisation, transfert, frais, durées) ; sa légalité est contestée par la doctrine | Budget et calendrier des formalités ; l'hébergement hors RDC (ex. Supabase) relève a priori d'une **autorisation de transfert** | Moyen (source secondaire), validité discutée |
| Activités et services numériques | Code du numérique, art. 13 à 17 (régimes) ; arrêtés n° 004 et n° 005 du 11 mars 2026 (autorisation / déclaration) ; arrêté interministériel de 2026 sur les frais, startups exonérées selon la presse | Déterminer si l'activité de Nhill relève de la déclaration ou de l'autorisation (« hébergement d'applications ») et si l'exploitant peut bénéficier d'une exonération | Faible à moyen |
| Commerce électronique | Code du numérique, art. 48 à 72 : informations précontractuelles, contenu de l'offre en ligne, rétractation, publicité, prospection directe (consentement préalable) | Mentions du site et des conditions de service ; opt-in pour les envois commerciaux | Moyen |
| Consommation | Pas de code de la consommation adopté identifié ; un projet de loi sur la protection des consommateurs est en discussion ; textes sectoriels anciens et loi n° 18/020 du 9 juillet 2018 (prix et concurrence) | Les clients de Nhill sont des professionnels : régime B2B principal ; prudence pour les entrepreneurs individuels | Moyen |
| Facturation et TVA | TVA 16 % (Ordonnance-loi n° 10/001 du 20 août 2010) ; facture normalisée produite par un dispositif électronique fiscal, logiciels homologués seuls autorisés pour les assujettis, sanctions à partir du 15 mai 2026 selon la presse | Nhill **n'est pas homologué** : documents de gestion seulement ; cibler d'abord les non-assujettis ou un usage en complément du dispositif DGI | Moyen à élevé |
| Droit des sociétés et comptabilité | RDC membre de l'OHADA (adhésion déposée le 13 juillet 2012) ; actes uniformes applicables (sociétés, droit commercial général, comptabilité : conservation 10 ans) | Forme sociale et RCCM de l'exploitant à créer ; durées de conservation des pièces | Élevé |
| Propriété intellectuelle | Loi n° 82-001 du 7 janvier 1982 (propriété industrielle, dont marques) ; Ordonnance-loi n° 86-033 du 5 avril 1986 (droit d'auteur, dont logiciels selon sa rédaction, à confirmer) | Dépôt de la marque « Nhill » à vérifier (disponibilité non vérifiée) ; clauses de licence | Moyen |
| Droit du travail (journal d'activité) | Le journal nominatif des actions des employés est un traitement de données des salariés | Note d'information au personnel fournie aux établissements (modèle 17) | Moyen |

## 2. Registre des hypothèses

Chaque modèle renvoie aux hypothèses qu'il suppose vraies. Une hypothèse invalidée impose de
revoir les modèles cités.

| N° | Hypothèse | Modèles concernés | Comment la vérifier | État |
|---|---|---|---|---|
| H-01 | L'exploitant est une société de droit congolais (forme OHADA à choisir : SARL, SAS, SA…) immatriculée au RCCM de Kinshasa | tous | statuts, extrait RCCM | **ouvert** — aucune société identifiée |
| H-02 | Les clients de Nhill sont des professionnels (commerçants, entreprises, associations) agissant pour leur activité ; il n'y a pas de vente à des consommateurs | 01, 02, 07, 09 | modèle commercial ; avocat : qualification d'un entrepreneur individuel | à valider |
| H-03 | Nhill est responsable de traitement pour les données des visiteurs, prospects, comptes utilisateurs et facturation, et sous-traitant pour les données saisies par les établissements | 03, 06, 15, 18 | avocat ; Code du numérique, Livre III | à valider |
| H-04 | La base de production peut être hébergée hors de RDC (Supabase, région à choisir) : transfert international soumis à autorisation préalable | 03, 06, 14, 18 | choix de l'hébergeur ; autorité compétente | **ouvert** — hébergeur et région non choisis |
| H-05 | L'autorité de protection des données compétente est l'ARPTC à titre provisoire, ou l'ARPTIC si sa création et la reprise des missions sont effectives | 03, 06, 14, 18, 19 | textes de création ; site de l'autorité | **ouvert — prioritaire** |
| H-06 | Les frais et procédures de la décision n° 039/ARPTC/CLG/2025 restent applicables malgré les contestations et le changement d'autorité | checklist § 6 | autorité ; avocat | ouvert |
| H-07 | L'activité de Nhill (logiciel en ligne hébergé pour des tiers) peut relever du régime d'**autorisation** des arrêtés n° 004 et 005/2026 si elle est qualifiée d'hébergement d'applications ; une jeune entreprise pourrait relever de la déclaration ou d'une exonération | checklist § 6 | texte des arrêtés ; ministère / autorité | ouvert |
| H-08 | La prospection commerciale par e-mail, SMS et WhatsApp exige le consentement préalable de la personne (opt-in) ; le silence ne vaut pas consentement | 03, 16 | Code du numérique, art. 68 à 72 | à valider |
| H-09 | Les documents émis par Nhill ne sont pas des factures normalisées ; un assujetti à la TVA doit utiliser un dispositif ou logiciel homologué par la DGI | 01, 02 | DGI ; fiscaliste | élevé, à confirmer |
| H-10 | Le droit congolais et les juridictions de Kinshasa (tribunal de commerce de Kinshasa/Gombe) sont choisis dans les contrats ; l'arbitrage CCJA OHADA est une option | 01, 02, 05, 09, 10, 11, 12 | avocat | choix à faire |
| H-11 | Les prix sont fixés en USD et/ou en CDF ; la conversion se fait au taux indiqué sur la facture | 02, 09 | politique commerciale ; loi n° 18/020 ; réglementation des changes | à valider |
| H-12 | La disponibilité du nom « Nhill » comme marque et dénomination n'a pas été vérifiée | 01, 05, 12 | recherche d'antériorité (RDC, OAPI non applicable, international) | **ouvert** |
| H-13 | Le site vitrine ne dépose aucun cookie et n'utilise aucun outil de mesure tiers | 04 | contrôle technique du site | vrai en 2.0.0 (vérifié dans le code) |
| H-14 | Les données des établissements sont conservées pendant l'abonnement puis 90 jours après la fin pour permettre l'export, sauf obligation légale contraire | 02, 06, 15 | politique produit ; avocat (obligation de conservation 10 ans à la charge de l'établissement) | proposition |
| H-15 | Une violation de données est notifiée à l'autorité et, si nécessaire, aux personnes, dans les délais du Code du numérique (72 heures selon un résumé secondaire) | 06, 14 | texte officiel | à confirmer |
| H-16 | Le programme pilote est gratuit, limité dans le temps et sans engagement d'achat | 09 | politique commerciale | proposition |
| H-17 | Les applications sont fournies sous licence d'utilisation non exclusive, liée à l'abonnement ; le code reste propriété de l'éditeur | 01, 12 | statuts / cession des droits des développeurs | à valider |
| H-18 | Les partenaires (apporteurs, intégrateurs, formateurs) sont des indépendants, rémunérés à la commission, sans pouvoir de signer au nom de l'exploitant | 10 | avocat ; fiscaliste (retenues) | à valider |

## 3. Constats par domaine

### 3.1 Données personnelles

- **Texte** : Ordonnance-loi n° 23/010 du 13 mars 2023 portant Code du numérique, Livre III
  (art. 183 à 270 selon la version consultée) [S01]. Principes de licéité, finalité,
  proportionnalité, exactitude, durée limitée, sécurité ; droits d'information, d'accès,
  de rectification, d'opposition, d'effacement ; obligation de sécurité ; délégué à la
  protection des données (art. 222) ; transferts hors RDC encadrés (art. 187, 201 et 202
  selon les commentaires disponibles) [S01, S07].
- **Autorité** : missions confiées provisoirement à l'ARPTC [S03] ; plusieurs sources de
  2026 font état d'une nouvelle autorité, l'ARPTIC, présentée comme autorité unique pour la
  régulation du numérique, la certification électronique et la protection des données, et
  de la disparition de l'ARPTC [S04]. **Le statut exact n'a pas pu être lu dans un texte
  officiel** (H-05).
- **Formalités et frais** : décision n° 039/ARPTC/CLG/2025 du 11 septembre 2025, d'après un
  commentaire d'avocats [S02] : déclaration (250 USD de dossier + 250 USD), autorisation
  (500 + 500 USD), autorisation de transfert (750 + 750 USD) ; validité de 3, 2 et 1 an
  respectivement ; amendes de 8 à 200 millions de francs congolais. Cette décision est
  contestée (légalité, constitutionnalité) par la doctrine [S02b]. **Montants et durées à
  reconfirmer auprès de l'autorité** (H-06).
- **Rôle de Nhill** : responsable de traitement pour ses propres fichiers (site, prospects,
  comptes, facturation, assistance) ; sous-traitant pour les données des établissements
  (clients, ventes, employés) — d'où l'accord de traitement (modèle 06).

### 3.2 Activités et services numériques

- Code du numérique, art. 13 à 17 : régimes applicables aux activités et services
  numériques (déclaration, autorisation) [S01].
- Arrêtés ministériels n° 004 et n° 005 du 11 mars 2026 : conditions d'autorisation et de
  déclaration des activités et services numériques ; selon les commentaires, l'hébergement
  d'applications relève de l'autorisation et les startups de la déclaration, avec une
  période transitoire jusqu'au 30 juin 2026 [S05].
- Arrêté interministériel de 2026 sur les frais : maintenu en vigueur, startups exonérées
  selon la presse ; des montants différents sont cités pour l'hébergement selon qu'il est
  local ou étranger [S06]. **Texte non lu** (H-07).

### 3.3 Commerce électronique et prospection

- Code du numérique, art. 48 à 72 [S01] : informations à fournir (identité du
  prestataire, prix, conditions), contenu de l'offre en ligne (art. 53-54), droit de
  rétractation (art. 61, principalement pour les consommateurs), publicité identifiable
  (art. 66-67), prospection directe soumise au consentement préalable, le silence ne
  valant pas consentement (art. 68 à 72).
- Conséquences : mentions légales et conditions accessibles sur le site ; case de
  consentement marketing séparée et non cochée (déjà en place sur le site) ; lien de
  désinscription dans chaque envoi ; registre des consentements.

### 3.4 Consommation, prix et concurrence

- Aucun code de la consommation adopté n'a été identifié ; un projet de loi portant
  protection des consommateurs a été examiné [S11]. Textes de référence cités par la
  doctrine : loi n° 18/020 du 9 juillet 2018 relative à la liberté des prix et à la
  concurrence [S10] et textes anciens sur le commerce.
- Nhill vise des professionnels (H-02) ; pour un entrepreneur individuel, prévoir des
  clauses équilibrées (pas de clause abusive évidente, résiliation simple).

### 3.5 Fiscalité et facturation

- TVA au taux de 16 % [S09]. Facture normalisée : dispositif électronique fiscal,
  homologation des systèmes de facturation, logiciels homologués seuls autorisés depuis le
  1er juillet 2025, sanctions annoncées à partir du 15 mai 2026 [S08].
- Conséquence : les conditions de service doivent dire clairement que Nhill n'est **pas** un
  système de facturation homologué (déjà affiché dans le logiciel pour les assujettis).
- Facturation de l'abonnement Nhill par l'exploitant : si l'exploitant est assujetti à la
  TVA, ses propres factures doivent être normalisées (fiscaliste).

### 3.6 Sociétés, comptabilité, contrats

- La RDC applique le droit OHADA depuis son adhésion (instruments déposés le 13 juillet
  2012) [S14] : Acte uniforme relatif au droit des sociétés commerciales, au droit
  commercial général, au droit comptable (conservation des pièces : 10 ans) [S15].
- Choix du for : tribunal de commerce de Kinshasa ; option d'arbitrage (CCJA) à discuter
  (H-10).

### 3.7 Propriété intellectuelle

- Marques : loi n° 82-001 du 7 janvier 1982 portant propriété industrielle [S12].
- Droit d'auteur : Ordonnance-loi n° 86-033 du 5 avril 1986 [S13] ; protection du logiciel
  et titularité des droits sur le code écrit par des développeurs ou prestataires : clause
  de cession à prévoir dans leurs contrats (hors du présent dossier).
- Code du numérique, art. 49 (cité dans les recherches antérieures pour le commerce
  électronique) : à relire.
- Disponibilité de « Nhill » : **non vérifiée** (H-12).

### 3.8 Travail

- Le journal d'activité nominatif traite des données des employés de l'établissement :
  information préalable du personnel (finalité, données, durée, droits). Modèle 17 fourni
  aux établissements ; articulation avec le Code du travail à faire valider.

## 4. Liste des modèles

| N° | Modèle (FR) | Modèle (EN) | Usage |
|---|---|---|---|
| 01 | [Conditions générales d'utilisation](01-conditions-generales-utilisation.md) | [Terms of use](../en/01-terms-of-use.md) | utilisation du site et des applications |
| 02 | [Conditions de service SaaS](02-conditions-service-saas.md) | [SaaS terms of service](../en/02-saas-terms-of-service.md) | contrat d'abonnement avec l'établissement |
| 03 | [Politique de confidentialité](03-politique-confidentialite.md) | [Privacy policy](../en/03-privacy-policy.md) | information des personnes |
| 04 | [Politique relative aux cookies et traceurs](04-politique-cookies.md) | [Cookie policy](../en/04-cookie-policy.md) | site vitrine |
| 05 | [Mentions légales](05-mentions-legales.md) | [Legal notice](../en/05-legal-notice.md) | site vitrine |
| 06 | [Accord de traitement des données](06-accord-traitement-donnees.md) | [Data processing agreement](../en/06-data-processing-agreement.md) | annexe au contrat (sous-traitance) |
| 07 | [Engagement de niveau de service](07-engagement-niveau-service.md) | [Service level agreement](../en/07-service-level-agreement.md) | disponibilité, assistance |
| 08 | [Politique d'utilisation acceptable](08-politique-utilisation-acceptable.md) | [Acceptable use policy](../en/08-acceptable-use-policy.md) | usages interdits |
| 09 | [Convention de programme pilote](09-convention-programme-pilote.md) | [Pilot programme agreement](../en/09-pilot-programme-agreement.md) | essai encadré |
| 10 | [Contrat de partenariat commercial](10-contrat-partenariat.md) | [Partnership agreement](../en/10-partnership-agreement.md) | apporteurs, intégrateurs, formateurs |
| 11 | [Accord de confidentialité](11-accord-confidentialite.md) | [Non-disclosure agreement](../en/11-non-disclosure-agreement.md) | échanges avec partenaires, prestataires |
| 12 | [Licence d'utilisation des applications](12-licence-applications.md) | [Application licence](../en/12-application-licence.md) | applications installées (Windows, Linux, Android) |
| 13 | [Politique de sécurité (résumé public)](13-politique-securite.md) | [Security policy (public summary)](../en/13-security-policy.md) | confiance, appels d'offres |
| 14 | [Procédure de gestion des violations de données](14-procedure-violation-donnees.md) | [Data breach procedure](../en/14-data-breach-procedure.md) | interne |
| 15 | [Politique de conservation et de suppression](15-politique-conservation.md) | [Retention and deletion policy](../en/15-retention-policy.md) | interne et contractuelle |
| 16 | [Formulaire et registre de consentement à la prospection](16-consentement-prospection.md) | [Marketing consent form and register](../en/16-marketing-consent.md) | prospection, WhatsApp, e-mail |
| 17 | [Note d'information du personnel](17-information-personnel.md) | [Staff information notice](../en/17-staff-notice.md) | fournie aux établissements (journal d'activité) |
| 18 | [Registre des activités de traitement](18-registre-traitements.md) | [Record of processing activities](../en/18-processing-register.md) | interne |
| 19 | [Formulaire d'exercice des droits](19-demande-exercice-droits.md) | [Data subject request form](../en/19-data-subject-request.md) | accès, rectification, effacement |

Chaque modèle commence par un cartouche : juridiction, hypothèses, sources, clauses à
valider. Les champs à compléter sont entre crochets `[…]` ; les clauses à faire valider
portent la mention **[À VALIDER]**.

## 5. Clauses nécessitant une validation juridique

| Réf. | Clause | Modèles | Question pour l'avocat |
|---|---|---|---|
| V-01 | Limitation de responsabilité (plafond = sommes payées sur 12 mois) | 02, 07, 12 | validité et opposabilité en droit congolais / OHADA ; exceptions (faute lourde, dol) |
| V-02 | Loi applicable et juridiction (Kinshasa) ou arbitrage | 01, 02, 09, 10, 11, 12 | for le plus adapté, clause compromissoire |
| V-03 | Prix en USD, paiement en CDF au taux du jour | 02, 10 | réglementation des changes, loi 18/020 |
| V-04 | Suspension pour défaut de paiement après période de grâce, lecture seule | 02 | délais de mise en demeure |
| V-05 | Transfert des données hors RDC et garanties | 03, 06 | autorisation préalable, autorité compétente (H-04, H-05) |
| V-06 | Délai de notification d'une violation (72 h) | 06, 14 | délai exact du Code du numérique |
| V-07 | Durées de conservation | 03, 15 | prescription, obligations comptables |
| V-08 | Consentement à la prospection (opt-in) et preuve | 16 | art. 68 à 72 |
| V-09 | Information du personnel sur le journal d'activité | 17 | Code du travail, représentation du personnel |
| V-10 | Rétractation pour les entrepreneurs individuels | 02 | applicabilité de l'art. 61 |
| V-11 | Commission des partenaires, non-concurrence, non-sollicitation | 10 | validité, fiscalité (retenue à la source) |
| V-12 | Clause de non-responsabilité fiscale (documents de gestion) | 01, 02 | suffisance au regard de la réforme de la facture normalisée |
| V-13 | Désignation d'un délégué à la protection des données | 03, 18 | obligation (art. 222) et seuils |

## 6. Liste de validation avant le lancement commercial

| # | Action | Responsable | Préalable à |
|---|---|---|---|
| 1 | Constituer la société exploitante (forme OHADA), obtenir RCCM, identification nationale, NIF | fondateurs + avocat / guichet unique | tout contrat |
| 2 | Vérifier la disponibilité du nom et déposer la marque « Nhill » | avocat / conseil en PI | communication publique |
| 3 | Identifier l'autorité de protection des données compétente (ARPTC / ARPTIC) et la procédure en vigueur | avocat | dépôts |
| 4 | Choisir l'hébergeur et la région ; si hors RDC, demander l'autorisation de transfert | direction technique + avocat | mise en production |
| 5 | Déposer les déclarations / autorisations de traitement (prospects, clients, employés de l'exploitant, sous-traitance) | DPO / avocat | collecte de données |
| 6 | Qualifier l'activité au regard des arrêtés n° 004 et 005/2026 ; vérifier l'éligibilité à une exonération startup | avocat | facturation des abonnements |
| 7 | Désigner un délégué à la protection des données ou justifier son absence | direction | dépôts |
| 8 | Compléter tous les champs `[…]` des modèles 01 à 19 et faire relire les clauses V-01 à V-13 | avocat | publication |
| 9 | Valider la position fiscale : Nhill = documents de gestion ; facturation de l'exploitant (TVA, facture normalisée) | fiscaliste | premier encaissement |
| 10 | Publier mentions légales, CGU, politique de confidentialité sur le site ; retirer les bandeaux « projet » | direction | ouverture des formulaires |
| 11 | Mettre en place le registre des traitements (modèle 18) et la procédure de violation (modèle 14) | DPO | mise en production |
| 12 | Fournir le modèle 17 aux établissements dès l'activation du journal d'activité | équipe produit | premiers clients |
| 13 | Réexaminer ce dossier tous les 6 mois et à chaque nouveau texte | DPO / avocat | — |

## 7. Sources

Consultées les 26 et 27 octobre 2026. « Secondaire » : commentaire, presse ou doctrine,
pas le texte officiel.

| Réf. | Source | Nature |
|---|---|---|
| S01 | Ordonnance-loi n° 23/010 du 13 mars 2023 portant Code du numérique — [texte PDF publié par Droit-Numérique.cd](https://droitnumerique.cd/wp-content/uploads/2024/08/Code-du-numerique-I-RD-Congo.pdf) ; [page de présentation](https://droitnumerique.cd/code-du-numerique-congolais-rdc-pdf/) | texte (copie non officielle) |
| S02 | [Avocats.cd — conformité des entreprises en matière de données personnelles (décision 039/ARPTC/CLG/2025)](https://avocats.cd/blog/donnees-personnelles-en-rdc-comment-les-entreprises-doivent-se-conformer) | secondaire |
| S02b | [Droit-Numérique.cd — réflexions sur l'illégalité de la décision de l'ARPTC](https://droitnumerique.cd/inconstitutionnalite-de-la-decision-de-larptc/) ; [PDF « Illégalité de la décision de l'ARPTC »](https://droitnumerique.cd/wp-content/uploads/2026/05/Illegalite-de-la-decision-de-lARPTC-Brozeck-Kandolo-1.pdf) | doctrine |
| S03 | [Droit-Numérique.cd — ARPTIC/ARPTC investie des missions de l'autorité de protection des données](https://droitnumerique.cd/arptic-arptc-investie-des-missions-de-lautorite-de-protection-des-donnees-en-rd-congo/) | secondaire |
| S04 | [Droit-Numérique.cd — l'ARPTIC investie autorité unique](https://droitnumerique.cd/regulation-du-numerique-en-rdc-larptic-investie-autorite-unique-pour-la-regulation-du-numerique-la-certification-electronique-et-la-protection-des-donnees/) ; [Ouragan.cd — « Déjà abrogée, l'ARPTC n'existe plus » (janvier 2026)](https://ouragan.cd/2026/01/deja-abrogee-larptc-nexiste-plus) ; [Rights Empower — l'ARPTIC remplace l'ARPTC](https://rightsempower.org/fr/reforme-dans-le-secteur-de-telecommunications-une-nouvelle-autorite-de-regulation-des-postes-des-telecommunications-et-des-technologies-de-linformation-et-de-la-communication-arptic-remplace-larptc) | secondaire |
| S05 | [Droit-Numérique.cd — arrêtés n° 004 et 005 du 11 mars 2026](https://droitnumerique.cd/arretes-n-004-et-n-005-du-11-mars-2026-relatifs-aux-activites-et-services-numeriques-en-rdc/) ; [Leganews — arrêté n° 005/2026 (déclaration)](https://leganews.pro/articles/019cfd07-43b2-7af3-914b-4d345972970d) ; [Numerico.cd — limites de l'arrêté](https://numerico.cd/2026/03/25/encadrement-des-services-numeriques-honore-mvula-pointe-les-limites-du-nouvel-arrete/) | secondaire |
| S06 | [Mediacongo — décryptage du nouvel arrêté (taxes, agréments, sanctions)](https://www.mediacongo.net/article-actualite-166525_taxes_agrements_sanctions_decryptage_du_nouvel_arrete_qui_encadre_le_numerique_en_rdc.html) ; [Journal de Kinshasa — startups exonérées](https://www.journaldekinshasa.com/rdc-fiscalite-numerique-startups-exonerees-arrete-2026/) ; [Afrique IT News](https://afriqueitnews.com/news/rdc-nouvelles-taxes-numeriques-entrent-vigueur-start-up-exonerees/) | presse |
| S07 | [Droit-Numérique.cd — le transfert des données personnelles hors de la RDC](https://droitnumerique.cd/le-transfert-des-donnees-personnelles-hors-de-la-rd-congo/) | secondaire |
| S08 | [DGI — Réforme de la facture normalisée](https://dgi.gouv.cd/reforme-de-la-facture-normalisee/) ; [Bankable — logiciels homologués seuls autorisés](https://bankable.africa/en/climat-des-affaires/1906-1335-facturation-seuls-les-logiciels-homologues-autorises-des-le-1er-juillet-en-rdc) ; [La Prospérité — fin du moratoire, sanctions au 15 mai 2026](https://www.laprosperite.cd/rdc-le-ministere-des-finances-met-fin-au-moratoire-sur-la-facture-normalisee-les-sanctions-debutent-le-15-mai-2026/) | officiel + presse |
| S09 | [Ordonnance-loi n° 10/001 du 20 août 2010 (TVA), texte consolidé — Leganet](https://www.leganet.be/Legislation/Dfiscal/TVA/OL.10.001.20.2010.htm) | texte |
| S10 | [Loi n° 18/020 du 9 juillet 2018 relative à la liberté des prix et à la concurrence — FAOLEX](https://faolex.fao.org/docs/pdf/Cng189580.pdf) | texte |
| S11 | [ACP — passage en revue du projet de loi portant protection des consommateurs](https://acp.cd/economie/rdc-passage-en-revue-du-projet-de-loi-portant-protection-des-consommateurs/) | presse officielle |
| S12 | [Loi n° 82-001 du 7 janvier 1982 portant propriété industrielle — WIPO Lex](https://www.wipo.int/wipolex/en/legislation/details/7499) | texte |
| S13 | [Ordonnance-loi n° 86-033 du 5 avril 1986 (droits d'auteur et droits voisins) — WIPO Lex](https://www.wipo.int/wipolex/en/legislation/details/7511) | texte |
| S14 | [OHADA — dépôt des instruments d'adhésion de la RDC, Dakar, 13 juillet 2012](https://www.ohada.com/actualite/1614/depot-des-instruments-dadhesion-de-la-rdc-a-lohada-dakar-13-juillet-2012.html) | officiel |
| S15 | [Acte uniforme OHADA relatif au droit comptable et à l'information financière (2017)](https://www.vie-publique.sn/documents/11026/acte-uniforme-droit-comptable-information-financiere-ohada-2017) | texte |
| S16 | [DLA Piper — Data protection laws of the world : République démocratique du Congo](https://www.dlapiperdataprotection.com/index.html?t=law&c=CD) | secondaire |

Limite de la recherche : les articles cités du Code du numérique proviennent de la copie
PDF S01 et de commentaires ; les textes de 2024-2026 (arrêtés, décision, création de
l'ARPTIC) n'ont pas été lus dans leur version officielle publiée au Journal officiel.
