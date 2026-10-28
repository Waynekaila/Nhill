# Paramètres légaux de Nhill 3.0

*English: [../en/legal-parameters.md](../en/legal-parameters.md)*

Ce document indique où se trouvent les informations juridiques et commerciales de Nhill :
documents juridiques (`legal/`), site vitrine et réglages de Nhill Console. Analyse
détaillée, décisions et sources : [`legal/fr/00-recherche-juridique.md`](../../legal/fr/00-recherche-juridique.md).

> Rien dans ce dépôt ne constitue un avis juridique. Les documents ont été rédigés sans
> relecture par un avocat ; les points à faire relire sont listés au § 5 du dossier
> juridique.

## 1. Valeurs fixées par Nhill Solution (28 octobre 2026)

| Donnée | Valeur | Où elle apparaît |
|---|---|---|
| Éditeur | Nhill Solution (nom commercial), siège à Kinshasa (République démocratique du Congo) ; aucune autre donnée d'identification | documents 01 à 19, mentions légales, pied de page du site, métadonnées des installateurs |
| Directeur de la publication | Wayne TSHINGI, fondateur | mentions légales (document 05 et site) |
| Domaine officiel | `nhill.online` | site (`config/site.json`), documents |
| E-mail général | `info@nhill.online` | site, documents, protection des données |
| E-mail d'assistance | `support@nhill.online` | site, applications (« Aide et assistance »), e-mails automatiques (`NH_SUPPORT_EMAIL`), signalements de sécurité, `/.well-known/security.txt` |
| Téléphone | aucun publié | — |
| Hébergement du service | Hostinger International Ltd, 61 Lordou Vironos str., 6023 Larnaca, Chypre ; serveur privé virtuel situé en France | mentions légales, politique de confidentialité, accord de traitement, registre |
| Droit applicable | droit de la RDC ; juridictions compétentes de Kinshasa, sous réserve des règles impératives du pays de l'utilisateur | documents 01, 02, 09 à 12, page « Conditions » |
| Responsabilité | plafonnée aux sommes payées pendant les 12 mois précédant le fait générateur, hors faute lourde ou intentionnelle et dommage corporel | document 02, page « Conditions » |
| Niveau de service | aucun taux ni délai chiffré garanti | documents 02 et 07, page « Conditions » |
| Prix | aucun prix publié ; proposition écrite par établissement | site, documents 02 et 09 |
| Fin de contrat | 90 jours d'export en lecture seule, puis suppression | documents 02, 06, 15 ; Nhill Console (« Supprimer… ») |
| Référence de bonnes pratiques | normes européennes (RGPD), sans affirmation de conformité | documents 03, 06, 13, 14, 15, 19 |

## 2. Réglages de Nhill Console

| Paramètre | Valeur par défaut | Où |
|---|---|---|
| Durée de l'essai gratuit | 30 jours (`trial_days`) | Paramètres de la plateforme ; par formule |
| Période de grâce avant lecture seule | 7 jours (`grace_days`) | Paramètres de la plateforme ; par formule |
| Rappels avant l'échéance | 14, 7, 3 et 1 jours | Paramètres de la plateforme ; par formule |
| Renouvellement automatique | seulement avec l'accord exprès du titulaire, si la formule le permet | Offres (« Renouvellement automatique autorisé (avec consentement) ») |
| Formules, droits, plafonds, prix par durée et devise | aucun prix préconfiguré | Offres et tarifs |
| Validité du code d'établissement | 30 jours (`code_rotation_days`) | Paramètres de la plateforme |

## 3. Fiscalité dans l'application

| Élément | Situation 3.0 | Action |
|---|---|---|
| Profil fiscal RDC (`server/profiles/CD.json`) | TVA 16 %, 0 %, exonéré, non applicable | faire vérifier taux et mentions par un fiscaliste |
| Facture normalisée de la RDC | **Nhill n'est pas un dispositif de facturation agréé** ; mention « document de gestion » sur les documents des assujettis ; champ pour la référence de la facture officielle | ne jamais présenter Nhill comme un logiciel de facturation agréé |
| Profil international (`INTL`) | taux, identifiants et mentions saisis par l'établissement | l'établissement fait confirmer ses mentions par son comptable |

## 4. Textes affichés par l'application

| Paramètre | Contenu | Où |
|---|---|---|
| `platform_stats_notice` | texte FR/EN expliquant les chiffres globaux consultés par l'équipe Nhill | Paramètres de la plateforme ; repris dans la politique de confidentialité |
| Mention « document de gestion » | imprimée sur les documents des assujettis à la TVA | profils (`server/profiles/*.json`) |
| Information du personnel (journal d'activité) | document 17, à remettre par chaque établissement | à proposer lors de l'ouverture des comptes |

## 5. Pages juridiques du site

| Page FR / EN | Document de référence | État 3.0 |
|---|---|---|
| `mentions-legales` / `legal-notice` | document 05 | complète, sans champ à remplir ; la ligne « hébergeur du site vitrine » sera ajoutée quand l'hébergeur du site sera choisi |
| `confidentialite` / `privacy` | document 03 | complète, durées alignées sur le code |
| `conditions` / `terms` | documents 01 et 02 | complète |

## 6. Actions restant à l'exploitant

1. Faire relire les documents 01 à 19 par un avocat inscrit à un barreau de la RDC
   (points V-01 à V-13) et la position fiscale par un fiscaliste.
2. Vérifier la disponibilité du nom « Nhill » et décider d'un dépôt de marque.
3. Identifier l'autorité de protection des données compétente et accomplir les formalités
   (traitements, transfert vers la France).
4. Configurer la messagerie de `nhill.online` (MX, SPF, DKIM, DMARC) avant de publier les
   adresses.
5. Publier le site, puis contrôler le domaine (`website/tools/check_live.py --require-site
   --require-mail`).
