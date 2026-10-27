# 15 — Politique de conservation et de suppression des données

*English: [../en/15-retention-policy.md](../en/15-retention-policy.md)*

> **MODÈLE — À FAIRE VALIDER.** Les durées entre crochets sont des propositions ; les
> réglages techniques correspondants sont indiqués quand ils existent.

| Cartouche | |
|---|---|
| Juridiction | RDC — Code du numérique (durée limitée) ; OHADA (pièces comptables : 10 ans) |
| Hypothèses | H-03, H-14 |
| Sources | S01, S15 — voir [00-recherche-juridique.md](00-recherche-juridique.md) § 7 |
| Clauses à valider | V-07 |
| Version | [NUMÉRO] du [DATE] |

## 1. Principe

Une donnée n'est conservée que le temps nécessaire à sa finalité, puis supprimée ou rendue
anonyme. La suppression couvre aussi les sauvegardes, qui expirent selon leur propre cycle.

## 2. Durées

| Données | Durée | Mise en œuvre |
|---|---|---|
| Demandes reçues par le site (sans suite) | [365] jours | `SITE_RETENTION_DAYS`, purge quotidienne (`nhill-site-purge.timer`) |
| Empreinte IP des formulaires | [30] jours | `SITE_IP_RETENTION_DAYS` |
| Compteurs anti-abus du site | 2 jours | purge quotidienne |
| Consentement à la prospection | jusqu'au retrait ; preuve du retrait [3] ans | modèle 16 ; `cli unsubscribe` |
| Données d'un établissement (clients, ventes, stock…) | durée du contrat + [90] jours de réversibilité | export par le client ; suppression par l'exploitation |
| Pièces comptables de l'établissement | 10 ans **à la charge de l'établissement** (il exporte ses données) | export CSV/PDF |
| Factures d'abonnement émises par l'Éditeur | 10 ans | comptabilité de l'Éditeur |
| Comptes utilisateurs inactifs | [24] mois sans connexion → avertissement puis suppression [À DÉCIDER] | procédure manuelle |
| Journal d'audit et de sécurité | [12] mois minimum, [5] ans maximum **[À VALIDER : V-07]** | — |
| Demandes d'assistance | [24] mois après clôture | — |
| Sauvegardes de la base | rotation : [7] quotidiennes, [4] hebdomadaires, [12] mensuelles | `deploy/backup/` |
| Journaux techniques du serveur | [30] jours | configuration de journald |

## 3. Suppression à la fin du contrat

1. Fin de l'abonnement → établissement en lecture seule ; le propriétaire est invité à
   exporter ses données.
2. J+[90] → suppression des données de l'établissement de la base de production ;
   enregistrement de la suppression (date, opérateur) dans le registre.
3. J+[90] + durée de rotation des sauvegardes → les données ont disparu des sauvegardes.
4. Sur demande, attestation écrite de suppression.

## 4. Demandes d'effacement

Traitées selon le modèle 19, sous [30] jours, sauf obligation légale de conservation (dans
ce cas : conservation limitée, accès restreint, et information de la personne).

## 5. Revue

Cette politique est revue une fois par an et à chaque nouveau traitement (registre,
modèle 18).
