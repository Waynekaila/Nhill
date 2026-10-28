# 15 — Politique de conservation et de suppression des données

*English: [../en/15-retention-policy.md](../en/15-retention-policy.md)*

| Cartouche | |
|---|---|
| Éditeur | Nhill Solution, Kinshasa (République démocratique du Congo) |
| Version | 3.0.0 — 28 octobre 2026 |
| Statut | rédigé sans relecture par un avocat ; point de relecture recommandé : V-07 ([00](00-recherche-juridique.md) § 5). Les durées sont celles publiées dans la politique de confidentialité et appliquées par les réglages indiqués |
| Hypothèses | H-03, H-14, H-19 ([00](00-recherche-juridique.md) § 2) |

## 1. Principe

Une donnée n'est conservée que le temps nécessaire à sa finalité, puis supprimée. La
suppression couvre aussi les sauvegardes, qui disparaissent à leur rotation.

## 2. Durées

| Données | Durée | Mise en œuvre |
|---|---|---|
| Demandes reçues par le site | 12 mois après l'envoi | `SITE_RETENTION_DAYS=365`, purge quotidienne (`nhill-site-purge.timer`) |
| Empreinte de l'adresse IP des formulaires | 30 jours | `SITE_IP_RETENTION_DAYS=30`, même purge |
| Compteurs anti-abus du site | 2 jours | même purge |
| Accord pour recevoir des nouvelles | jusqu'au retrait, au plus tard jusqu'à la suppression de la demande | `python3 -m nhill_site.cli unsubscribe --email …` ; registre (document 16) pour les accords recueillis hors du site |
| Données d'un établissement (comptes, clients, produits, documents, stock, paiements, journal d'activité, demandes d'assistance) | durée du contrat, puis 90 jours d'export en lecture seule | suppression définitive par la console d'administration, possible seulement après ces 90 jours (article 3) |
| Pièces comptables de l'établissement | 10 ans, **à la charge de l'établissement** | export CSV et PDF, archive complète des données |
| Factures d'abonnement émises par Nhill Solution | 10 ans | comptabilité de Nhill Solution |
| Journal d'administration de la plateforme (actions de l'équipe) | durée d'exploitation de la plateforme ; entrées liées à un établissement supprimées avec lui | journal en ajout seul |
| Sauvegardes chiffrées de la base | 14 jours sur le serveur | `NH_BACKUP_KEEP_DAYS=14` (`/etc/nhill/backup.env`) |
| Copies du serveur par l'hébergeur | selon le cycle de l'hébergeur (2 copies hebdomadaires par défaut) | panneau Hostinger |
| Copie externe des sauvegardes (si elle est mise en place) | durée choisie à sa mise en place, inscrite au registre (document 18) | `NH_BACKUP_OFFSITE_CMD` |
| Journaux techniques du serveur | 90 jours au plus | journald `MaxRetentionSec=90day` ; journaux de Caddy `roll_keep_for 2160h` |

## 3. Suppression à la fin du contrat

1. Fin de l'abonnement ou de l'essai, puis période de grâce éventuelle → l'établissement
   passe en lecture seule ; le titulaire est invité à exporter ses données.
2. 90 jours après la fin de l'accès complet → l'équipe supprime définitivement
   l'établissement depuis la console d'administration ; la suppression (date, auteur) est
   inscrite au journal d'administration.
3. 14 jours plus tard → les données ont disparu des sauvegardes conservées sur le serveur ;
   elles disparaissent des copies de l'hébergeur et de la copie externe à leur rotation.
4. Sur demande, Nhill Solution confirme la suppression par écrit.

## 4. Demandes d'effacement

Traitées selon le document 19, dans un délai d'un mois, sauf obligation légale de
conservation (dans ce cas : conservation limitée, accès restreint, information de la
personne).

## 5. Revue

Cette politique est revue une fois par an et à chaque nouveau traitement (registre,
document 18).
