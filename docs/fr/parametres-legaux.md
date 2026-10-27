# Paramètres légaux de Nhill 2.0

*English: [../en/legal-parameters.md](../en/legal-parameters.md)*

Ce document liste **toutes les informations juridiques et fiscales à fournir** avant le
lancement commercial, et l'endroit où chacune doit être saisie : modèles juridiques, site
vitrine, réglages de Nhill Console et de Nhill Business. Aucune de ces valeurs n'est
connue à ce jour ; **aucune ne doit être inventée**. Analyse détaillée et sources :
[`legal/fr/00-recherche-juridique.md`](../../legal/fr/00-recherche-juridique.md).

> Rien dans ce dépôt ne constitue un avis juridique. Les modèles doivent être relus par un
> avocat inscrit à un barreau de la RDC et, pour la fiscalité, par un fiscaliste.

## 1. Identité de l'exploitant (champ → où il apparaît)

| Champ | Exemple de format | Modèles | Site vitrine | Application |
|---|---|---|---|---|
| `[RAISON SOCIALE]` | dénomination exacte du RCCM | 01, 02, 03, 05, 06, 09-12, 18 | `mentions-legales`, `confidentialite`, `conditions` (FR/EN) | facture d'abonnement (Nhill Console → paramètres de la plateforme, instructions de paiement) |
| `[FORME JURIDIQUE]`, `[CAPITAL]` | SARL / SAS / SA (OHADA), montant et devise | 01, 02, 05 | `mentions-legales` | — |
| `[RCCM]` | numéro du Registre du commerce et du crédit mobilier | 01, 02, 03, 05, 09, 18 | `mentions-legales` | — |
| `[ID. NAT.]` | identification nationale | 01, 02, 05 | `mentions-legales` | — |
| `[NIF]` | numéro d'impôt | 01, 02, 05 | `mentions-legales` | factures d'abonnement |
| `[ADRESSE]` | siège social complet | tous | `mentions-legales`, `contact` | — |
| `[TÉLÉPHONE]`, `[E-MAIL]` | contact professionnel | 01, 05 | `contact`, pied de page (`SITE_CONTACT_EMAIL`) | Aide et assistance |
| `[NOM, FONCTION]` | représentant légal / directeur de la publication | 02, 05, 06, 09-11 | `mentions-legales` | — |
| `[E-MAIL DPO]`, `[NOM DU DPO]` | contact protection des données | 03, 04, 14, 16, 18, 19 | `confidentialite` | — |
| `[E-MAIL DE SÉCURITÉ]` | signalement de vulnérabilités (ex. `security@…`) | 05, 08, 13 | `mentions-legales`, futur `/.well-known/security.txt` | — |

## 2. Hébergement et transferts

| Champ | Décision à prendre | Où |
|---|---|---|
| `[HÉBERGEUR]`, `[PAYS / RÉGION]` du serveur d'API | fournisseur et pays | modèles 03, 05, 06, 18 ; `mentions-legales` |
| Hébergeur de la base (Supabase ou PostgreSQL auto-hébergé), région | si hors RDC : **autorisation de transfert** (hypothèse H-04) | modèles 03, 06, 18 |
| Hébergeur du site vitrine | — | modèle 05 ; `website/README.md` |
| Référence de l'autorisation de transfert | n°, date, autorité | modèles 03, 06, 18 |
| Autorité de protection des données compétente | ARPTC (provisoire) ou ARPTIC (H-05) : **à vérifier en priorité** | modèles 03, 14, 19 ; page `confidentialite` (mentionne aujourd'hui l'ARPTC) |
| Références des déclarations / autorisations de traitement | n° et date par traitement | modèle 18 |
| Qualification de l'activité numérique (arrêtés n° 004 et 005/2026) | déclaration, autorisation ou exonération (H-07) | modèle 05 (« autorisation / déclaration d'activité numérique ») |

## 3. Conditions commerciales

| Paramètre | Valeur proposée | Où le régler | À valider |
|---|---|---|---|
| Durée de l'essai gratuit | 30 jours (défaut) | Nhill Console → Paramètres de la plateforme | — |
| Période de grâce avant lecture seule | 7 jours | Nhill Console → Paramètres de la plateforme | V-04 |
| Offres, plafonds, prix par durée et devise | **aucun prix préconfiguré** | Nhill Console → Offres et tarifs ; modèle 02 annexe A | V-03 |
| Devise de facturation, TVA de l'abonnement | `[USD / CDF]`, `[HT/TTC]` | modèle 02 § 3 | fiscaliste |
| Renouvellement tacite ou exprès | `[À CHOISIR]` | modèle 02 § 2.2 | avocat |
| Réversibilité après la fin | 90 jours | modèles 02, 06, 15 | V-07 |
| Commission partenaires | `[N] %` sur 12 mois | modèle 10 | V-11 |
| Remise « pilote » | `[N] %` pendant `[N]` mois | modèle 09 ; Nhill Console → Codes promo | — |
| Heures d'assistance, canaux | `[JOURS ET HORAIRES]`, WhatsApp professionnel, e-mail | modèles 02, 07 | — |

## 4. Fiscalité dans l'application

| Élément | Situation 2.0 | Action |
|---|---|---|
| Profil fiscal RDC (`server/profiles/CD.json`) | TVA 16 %, 0 %, exonéré, non applicable | faire vérifier taux et mentions par un fiscaliste |
| Facture normalisée (DEF / SFE homologué) | **Nhill n'est pas homologué** ; mention « document de gestion » sur les documents des assujettis ; saisie de la référence DEF | ne pas présenter Nhill comme logiciel de facturation conforme ; feuille de route d'homologation : `docs/conformite/conformite.md` |
| Taux dit « TVA sociale » 8 % | non établi, non activé | à valider |

## 5. Textes affichés par l'application

| Paramètre | Contenu actuel | Où |
|---|---|---|
| `platform_stats_notice` | texte FR/EN expliquant les statistiques agrégées consultées par l'équipe Nhill (migration 0006) | Nhill Console → Paramètres de la plateforme ; à reprendre dans la politique de confidentialité (modèle 03, tableau des traitements) |
| Mention « document de gestion » | imprimée sur les documents des assujettis à la TVA | profils (`server/profiles/*.json`) |
| Information du personnel (journal d'activité) | modèle 17 à remettre par chaque établissement | à proposer dans l'aide de Nhill Business et lors de l'activation des comptes |

## 6. Site vitrine : pages à finaliser

| Page FR / EN | Source à aligner | État 2.0 |
|---|---|---|
| `mentions-legales` / `legal-notice` | modèle 05 | bandeau « projet », champs entre crochets |
| `confidentialite` / `privacy` | modèle 03 | bandeau « projet » |
| `conditions` / `terms` | modèles 01 et 02 | bandeau « projet » |

Les pages portent un bandeau « Projet à compléter et à faire valider par un juriste. Ne pas
publier en l'état. » à retirer seulement après validation.

## 7. Ordre conseillé

1. Société, RCCM, NIF, ID. Nat. (section 1).
2. Choix de l'hébergement ; vérification de l'autorité compétente ; dépôts (section 2).
3. Relecture des modèles 01-19 par l'avocat (clauses V-01 à V-13).
4. Saisie des offres et prix dans Nhill Console ; annexe A du modèle 02.
5. Finalisation des pages du site ; retrait des bandeaux « projet ».
6. Ouverture des formulaires du site (`SITE_API_BASE_URL`), puis des inscriptions.
