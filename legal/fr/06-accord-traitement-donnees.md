# 06 — Accord de traitement des données (sous-traitance)

*English: [../en/06-data-processing-agreement.md](../en/06-data-processing-agreement.md)*

| Cartouche | |
|---|---|
| Éditeur | Nhill Solution, Kinshasa (République démocratique du Congo) |
| Version | 3.0.0 — 28 octobre 2026 |
| Statut | rédigé sans relecture par un avocat ; points de relecture recommandés : V-05, V-06, V-07 ([00](00-recherche-juridique.md) § 5) |
| Hypothèses | H-03, H-04, H-05, H-14, H-15, H-19 ([00](00-recherche-juridique.md) § 2) |
| Place | document contractuel des conditions de service (document 02) ; accepté avec elles |

## 1. Parties et rôles

L'établissement client (« le Client ») est responsable du traitement des données qu'il
enregistre dans Nhill. **Nhill Solution** (« l'Éditeur ») agit comme sous-traitant : il
traite ces données uniquement pour fournir le Service et selon les instructions
documentées du Client, que constituent le contrat, le présent accord et les réglages faits
dans l'application.

## 2. Description du traitement

| Élément | Contenu |
|---|---|
| Objet | gestion commerciale et caisse : ventes, achats, stock, dépenses, paiements, rapports, équipe |
| Personnes concernées | clients et fournisseurs du Client, ses employés et utilisateurs |
| Données | identité et coordonnées, identifiants fiscaux des tiers, historique d'achats et de paiements, pièces jointes, données de connexion et journal d'activité des utilisateurs |
| Données sensibles | **aucune n'est prévue** ; le Client s'engage à ne pas en enregistrer dans les champs libres |
| Opérations | enregistrement, conservation, consultation, calcul, export, suppression |
| Durée | durée du contrat, puis 90 jours d'export avant suppression (document 02, article 9) |
| Lieu | serveur situé en France (Union européenne), exploité à distance depuis Kinshasa |

## 3. Obligations de l'Éditeur

1. Ne traiter les données que sur instruction du Client ; l'informer si une instruction lui
   semble contraire à la loi.
2. Garantir la confidentialité : personnes habilitées, tenues au secret, accès limité aux
   besoins de l'exploitation et de l'assistance. L'équipe ne consulte pas les clients,
   produits ni documents du Client ; les chiffres globaux d'utilisation qu'elle consulte
   sont inscrits au journal d'activité du Client.
3. Mettre en œuvre les mesures de sécurité de l'article 6.
4. Ne pas recourir à un autre sous-traitant sans information préalable du Client
   (article 5).
5. Aider le Client à répondre aux demandes d'exercice des droits (exports, rectification
   par le Client lui-même dans l'application).
6. Notifier au Client toute violation de données le concernant **dans les meilleurs
   délais** après en avoir pris connaissance, avec les informations disponibles (nature,
   catégories et volume approximatif, conséquences probables, mesures prises), pour lui
   permettre de notifier l'autorité dans le délai légal. Procédure : document 14.
7. Supprimer les données à la fin du contrat (document 02, article 9) et confirmer la
   suppression par écrit sur demande.
8. Mettre à disposition du Client les informations nécessaires pour démontrer le respect
   du présent accord.

## 4. Obligations du Client

Le Client dispose d'une base légale pour chaque traitement, informe les personnes
concernées (dont ses employés : document 17), accomplit les formalités qui lui incombent
auprès de l'autorité de protection des données, attribue les droits d'accès dans
l'application et retire ceux des personnes qui le quittent.

## 5. Sous-traitants ultérieurs

| Sous-traitant | Service | Lieu |
|---|---|---|
| Hostinger International Ltd, 61 Lordou Vironos str., 6023 Larnaca, Chypre | serveur privé virtuel (serveur applicatif, base de données, sauvegardes du serveur) | France (Union européenne) |

À la date du présent accord, aucun autre sous-traitant n'a accès aux données du Client.
L'Éditeur informe le Client avant d'ajouter ou de remplacer un sous-traitant (par exemple
un service d'envoi des e-mails automatiques) ; le Client peut s'y opposer pour un motif
légitime et, à défaut d'accord, mettre fin au contrat sans pénalité. Les prestataires de
paiement de l'abonnement ne reçoivent pas les données du Client enregistrées dans Nhill.

## 6. Mesures de sécurité

- chiffrement des échanges (HTTPS) entre les applications et le serveur ;
- séparation des données de chaque établissement dans la base (sécurité au niveau des
  lignes imposée par la base, rôle applicatif sans privilège de contournement) ;
- base de données inaccessible depuis Internet ; pare-feu n'ouvrant que les ports web et
  d'administration ;
- mots de passe conservés sous forme d'empreinte (Argon2id), sessions courtes avec
  rotation, limitation des tentatives ;
- contrôle d'accès par rôle, journal d'activité non modifiable depuis l'application ;
- double authentification obligatoire pour les administrateurs de la plateforme ;
- sauvegarde chiffrée quotidienne, restauration d'essai sur une base séparée ;
- secrets de l'infrastructure hors du code et des applications.

Ces mesures sont revues au moins une fois par an. Aucune mesure ne garantit l'absence
totale de vulnérabilité.

## 7. Transfert hors de la RDC

Le serveur est situé en France. Pour un Client établi en RDC, l'hébergement constitue un
transfert de données hors du pays. L'Éditeur impose à son hébergeur des obligations de
sécurité et de confidentialité, et coopère avec le Client pour les formalités qui
s'appliquent à ce transfert.

## 8. Durée

Le présent accord s'applique tant que l'Éditeur traite des données pour le Client.
