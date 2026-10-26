# Modèles de documents et profils fiscaux

## 1. Modèles de documents (PDF)

Un seul moteur (`server/src/pdf/pdf.cpp`, libharu) produit les documents de vente
(facture, avoir, devis, bon de commande, ticket de caisse) et les rapports ; la mise en page est
identique, les libellés viennent de `server/i18n/fr.json` / `en.json` selon la langue
des documents de l'établissement. Exemples générés par les tests :
`docs/exemples/exemple-facture.pdf`, `exemple-facture-brouillon.pdf`,
`exemple-journal-ventes.pdf` et `.csv` (données marquées EXEMPLE).

Structure d'une page A4 :

1. **En-tête** : logo de l'établissement à gauche (dimensions calculées pour conserver
   le rapport largeur/hauteur, jamais déformé ; sans logo : nom commercial en texte),
   type et numéro du document, dates (émission, échéance ou validité).
2. **Vendeur** : nom commercial, raison sociale, adresse, contacts et identifiants
   légaux marqués `printed` dans le profil (RCCM, Id. Nat., NIF…).
3. **Client** (encadré) : nom, adresse, NIF le cas échéant.
4. **Lignes** : désignation, quantité et unité, prix unitaire HT, remise, taxe,
   total HT ; pagination automatique avec répétition de l'en-tête du tableau.
5. **Totaux** : HT, détail des taxes par taux, TTC, payé, reste dû ; devise du document ;
   taux de change figé à l'émission s'il y a lieu (taux, date, source).
6. **Mentions** : mention du profil (ex. document de gestion pour les assujettis TVA en
   RDC), référence de la facture d'origine (avoir), référence DEF si enregistrée.
7. **Notes** : conditions et instructions de paiement, notes du document.
8. **Pied de page** : « Généré par Nhill · Empreinte … », pagination.

Brouillon : filigrane « BROUILLON — SANS VALEUR » et aucun numéro. Document émis :
empreinte SHA-256 du PDF enregistrée à l'émission et affichée dans l'application ; les
réimpressions utilisent les coordonnées du vendeur et du client **figées à
l'émission** (instantanés `seller_snapshot` / `buyer_snapshot`) et des dates
déterministes ; seul le logo est celui en vigueur au moment de la réimpression.

Personnalisation par établissement (Paramètres) : logo, langue des documents,
instructions de paiement, pied de page, délai de paiement par défaut, mode d'arrondi.

### 1.1 Tickets de caisse et rapports X/Z (Nhill POS)

Même moteur, format **rouleau** 58 ou 80 mm (Paramètres → Point de vente), hauteur
calculée selon le contenu : logo centré (proportions conservées), nom, adresse,
téléphone, identifiants légaux ; numéro (`TK-2026-000001` en français, `RCP-…` en
anglais ; retours et annulations : avoir `AV-…`), date, caisse et session, caissier,
client s'il n'est pas « Client comptoir » ; lignes (quantité × prix, remise) ; remise,
sous-total HT, taxes par taux, total ; paiements par moyen, monnaie rendue ; message de
l'établissement ; « Merci de votre visite » ; mention de document de gestion ; empreinte.
Rapport X (session ouverte) ou Z (clôturée) : ventes et retours (nombre, montants),
ventes nettes, remises, taxes par taux, encaissements par moyen, fond de caisse, entrées
et sorties ; pour un superviseur, espèces attendues, comptées et écart. Bilingue selon
la langue des documents de l'établissement.

## 2. Profils fiscaux

Fichiers `server/profiles/<CODE>.json`, versionnés et synchronisés en base au démarrage.
Chaque document émis enregistre `profile_code` et `profile_version`.

| Champ | Rôle |
|---|---|
| `validation_status` | État de validation du profil — affiché, jamais présenté comme une certification |
| `legal_ids` | Identifiants demandés à la création, obligatoires selon le type d'établissement ou l'assujettissement, imprimés ou non |
| `tax_rates` | Taux proposés à la création de l'établissement (le taux par défaut dépend de l'assujettissement) |
| `tax_rounding` | Mode d'arrondi par défaut |
| `numbering` | Modèle de numéro, remise à zéro, préfixes par langue |
| `seller_fields` / `buyer_fields` | Mentions imprimées |
| `document_notices` | Mentions obligatoires conditionnelles (fr/en) |
| `fiscal_device` | Dispositif fiscal requis, **implémenté ou non**, champs de référence à saisir |
| `retention_years` | Durée de conservation affichée |
| `sources` | Sources officielles utilisées pour établir le profil |

### Profil `CD` (République démocratique du Congo) — version 1

État : **brouillon à faire relire par un fiscaliste congolais**. Identifiants RCCM,
Id. Nat., NIF (NIF obligatoire si assujetti à la TVA) ; taux TVA 16 %, 0 % export,
exonéré, non assujetti ; numérotation `FA-2026-000001` ; mention « Document de gestion —
ne remplace pas la facture normalisée délivrée par le dispositif électronique fiscal
(DEF) » pour les assujettis ; `fiscal_device.implemented = false` ; conservation 10 ans.
Le taux de 8 % dit « TVA sociale » n'est pas inclus (texte non identifié, voir
`docs/conformite/conformite.md`) : il peut être ajouté par l'établissement comme taux
personnalisé.

### Profil `INTL` — version 1

Facture commerciale générique (numéro d'immatriculation, numéro de TVA/fiscal, aucun
taux prédéfini autre que « Sans taxe »). **Aucune conformité spécifique n'est
revendiquée**, notamment aucun format de facturation électronique européen.

### Ajouter un pays

1. Copier `INTL.json` vers `<CODE>.json`, renseigner `validation_status` honnêtement
   et les `sources` officielles (texte, date, portée).
2. Faire valider par un professionnel local ; consigner la validation (date, auteur).
3. Ajouter le pays à la liste du client (`Labels.countries()`) et un test de création.
4. Toute modification ultérieure incrémente `version`.
