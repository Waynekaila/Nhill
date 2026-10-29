# Conformité — RDC, Union européenne, international

> Nhill **n'est ni certifié ni homologué** par une administration. Ce document
> décrit ce que le logiciel fait, ce qu'il ne fait pas, et ce qui doit être validé par
> des professionnels (fiscaliste, avocat, expert-comptable) avant un usage en production.
> Sources consultées en octobre 2026 ; la réglementation évolue : vérifier les textes
> officiels à jour avant chaque décision.

## 1. Méthode

Pour chaque exigence : (1) source officielle ou, à défaut, secondaire identifiée comme
telle ; (2) date de consultation ; (3) portée (qui est concerné) ; (4) statut —
**obligatoire**, **bonne pratique** ou **non établi** ; (5) traitement dans Nhill et
test associé ; (6) point à faire valider. Aucune référence n'est citée sans avoir été
consultée ; ce qui n'a pas pu être vérifié est marqué comme tel.

## 2. République démocratique du Congo

| # | Exigence | Source | Statut | Traitement Nhill | Vérification |
|---|---|---|---|---|---|
| CD-1 | TVA : taux normal 16 %, 0 % à l'exportation | Ordonnance-loi n° 10/001 du 20 août 2010, art. 35 (texte consolidé, Leganet) | Obligatoire pour les assujettis | Taux TVA16 / TVA0 / EXO / NA dans le profil `CD` ; taux immuables une fois créés | Tests de calcul (lignes, arrondis, regroupement par taux) |
| CD-2 | Mentions de la facture fixées par voie réglementaire | Même texte, art. 58 | Obligatoire | Mentions vendeur (RCCM, Id. Nat., NIF), client, numéro, dates, détail par taux | **Liste exacte à faire valider** par un fiscaliste |
| CD-3 | Facture normalisée produite par un dispositif électronique fiscal (DEF), avec identifiant du DEF, code d'authentification, QR code ; systèmes de facturation (SFE) homologués par la DGI | Décret n° 23/010 du 3 mars 2023 et arrêtés 2023-2025, page DGI « Réforme de la facture normalisée » ; presse économique (Bankable 06/2025 et 10/2025, FEC 08/2025, La Prospérité : fin du moratoire, sanctions à partir du 15 mai 2026) | Obligatoire pour les assujettis à la TVA | **Non couvert** : Nhill n'est pas un SFE homologué ni connecté à un DEF. Mention « Document de gestion — ne remplace pas la facture normalisée… » sur les documents des assujettis ; saisie de la référence DEF (NID, code d'authentification, numéro) pour rapprochement ; aucun code DEF n'est jamais inventé ni imprimé | Tests : mention présente pour un assujetti, absente sinon ; référence DEF enregistrée et auditée |
| CD-4 | Taux de 8 % dit « TVA sociale » | Agence congolaise de presse, 28/07/2023 (source secondaire) | **Non établi** (texte et liste de produits non identifiés) | Non activé ; ajout possible comme taux personnalisé | À valider |
| CD-5 | Seuil d'assujettissement (80 millions de FC selon la presse) | Bankable, 17/10/2025 (secondaire) | Non établi officiellement ici | L'utilisateur déclare lui-même s'il est assujetti | À valider |
| CD-6 | Conservation des livres et pièces : 10 ans | Acte uniforme OHADA relatif au droit comptable et à l'information financière (2017) | Obligatoire (comptabilité) | Documents émis immuables, jamais supprimés ; avoirs pour corriger ; exports CSV/PDF ; politique de sauvegarde avec rétention annuelle 10 ans recommandée | Tests d'immuabilité (déclencheurs) ; restauration testée |
| CD-7 | Protection des données : base légale, sécurité, notification des violations (72 h), encadrement des transferts hors du pays | Ordonnance-loi n° 23/010 du 13 mars 2023 portant Code du numérique (résumé secondaire PACMAP) | Obligatoire (portée exacte à confirmer sur le texte officiel) | Isolation par établissement, chiffrement en transit, contrôle d'accès, journal ; **lieu d'hébergement et transferts à décider par l'exploitant** | Avocat : lecture du texte officiel, déclarations éventuelles, contrats |
| CD-9 | Tickets de caisse | Mêmes textes que CD-3 (la facture normalisée vise aussi les ventes au comptoir des assujettis, à confirmer) | À valider | Les tickets Nhill POS portent la mention de document de gestion ; pas de certification de caisse ni de journal fiscal certifié | Fiscaliste |
| CD-8 | Numérotation continue, sans doublon | Bonne pratique (et attendu de toute réglementation de facturation) | Bonne pratique | Séquence serveur par type et par année, transactionnelle | Test : 24 émissions concurrentes → 24 numéros distincts consécutifs |

**Feuille de route DEF/SFE** : ouvrir un compte développeur sur le portail de la DGI
(`developper.dgirdc.cd`, cité par la FEC), obtenir les spécifications techniques,
implémenter l'adaptateur de dispositif fiscal, déposer le dossier d'homologation. Tant
que ce n'est pas fait, la mention de document de gestion reste obligatoire pour les
assujettis.

## 3. Union européenne

| # | Exigence | Source | Statut | Traitement |
|---|---|---|---|---|
| EU-1 | Facturation électronique structurée : France (réception par toutes les entreprises et émission par les grandes et moyennes depuis septembre 2026), Belgique (B2B obligatoire depuis le 1er janvier 2026), directive ViDA 2025/516 (intra-UE en 2030) | Calendrier ViDA (eConnect, source secondaire) | Obligatoire dans les pays concernés | **Non couvert** : pas d'export EN 16931 (UBL/CII), pas de plateforme agréée. Profil `INTL` générique sans revendication de conformité |
| EU-2 | Règlement général sur la protection des données (UE 2016/679) pour des clients établis dans l'UE | Texte du règlement | Obligatoire si applicable | Mesures techniques décrites dans `docs/archive/securite.md` ; contrat de sous-traitance, registre, lieu d'hébergement, droits des personnes : **à établir par l'exploitant** |

Conclusion : **ne pas commercialiser Nhill comme solution de facturation conforme
dans un pays de l'UE** avant implémentation et validation d'EU-1.

## 4. International

Profil `INTL` : facture commerciale générique. Avant chaque nouveau pays, appliquer la
méthode du §1 et créer un profil (`docs/archive/modeles-et-profils.md`).

## 5. Traçabilité des employés et statistiques de la plateforme

* **Journal d'activité nominatif** : il enregistre les actions de chaque employé dans
  l'application (connexions, documents, paiements…). C'est un traitement de données
  personnelles des salariés : l'établissement doit **informer son personnel** (finalité :
  sécurité, contrôle interne, preuve des opérations ; durée de conservation). Nhill
  en limite la portée : il ne trace que les actions faites dans Nhill, pas l'activité
  hors application ; un employé ordinaire ne voit que ses propres actions.
  **À valider** : règles du droit du travail et de la protection des données du pays
  (en RDC, Code du numérique et Code du travail ; dans l'UE, RGPD et droit national).
* **Statistiques des créateurs** : l'exploitant de Nhill consulte des agrégats par
  établissement (nombres de documents, volumes facturés et encaissés par devise,
  utilisateurs actifs) pour exploiter le service, sans accès aux clients, produits ou
  contenus des documents ; chaque consultation est visible par l'établissement dans son
  journal. **À inscrire dans les conditions générales et la politique de
  confidentialité** (paramètre `platform_stats_notice` fourni comme texte de départ),
  et à faire valider par un avocat (finalité, base légale, durée).

## 6. Abonnements et paiements

* Pas de prélèvement sans consentement explicite enregistré (case à cocher datée,
  révocable, auditée) ; aucun prestataire réel n'est intégré.
* Prix exprimés hors taxe avec taux affiché ; la facturation des abonnements eux-mêmes
  (facture de l'exploitant à l'établissement) **n'est pas générée** par Nhill en V1.
* Conditions générales de vente et de résiliation : **à rédiger** par l'exploitant
  (politique de suspension affichée : lecture et export après 7 jours de grâce).

## 7. Sources

* DGI — Réforme de la facture normalisée : https://dgi.gouv.cd/reforme-de-la-facture-normalisee/
* Ordonnance-loi n° 10/001 du 20 août 2010 (TVA), texte consolidé : https://www.leganet.be/Legislation/Dfiscal/TVA/OL.10.001.20.2010.htm
* Bankable, homologation des systèmes de facturation (19/06/2025) : https://bankable.africa/en/climat-des-affaires/1906-1335-facturation-seuls-les-logiciels-homologues-autorises-des-le-1er-juillet-en-rdc
* Bankable, moratoire et seuil d'assujettissement (17/10/2025) : https://bankable.africa/fr/climat-des-affaires/1710-1854-facture-normalisee-nouveau-moratoire-sur-une-reforme-cle-de-la-tva-en-rdc
* FEC, procédure d'homologation (08/2025) : https://fec-rdc.com/mise-en-oeuvre-de-la-reforme-relative-a-la-facture-normalisee-le-cabinet-du-ministre-des-finances-et-la-dgi-rencontrent-les-preoccupations-des-entreprises-et-clarifient-la-procedure-dhomologation/
* La Prospérité, fin du moratoire (sanctions au 15 mai 2026) : https://www.laprosperite.cd/rdc-le-ministere-des-finances-met-fin-au-moratoire-sur-la-facture-normalisee-les-sanctions-debutent-le-15-mai-2026/
* ACP, TVA sociale de 8 % (28/07/2023) : https://acp.cd/economie/rdc-linstitution-dune-tva-sociale-a-8-rend-accessible-les-produits-de-premiere-necessite/
* Acte uniforme OHADA relatif au droit comptable (2017) : https://www.vie-publique.sn/documents/11026/acte-uniforme-droit-comptable-information-financiere-ohada-2017
* Code du numérique RDC (résumé) : https://pacmap.dev/regulation/cd-digital-code-2023
* Calendrier ViDA et pays européens : https://econnect.eu/en/docs/knowledge/regulations/europe/vida/timeline
