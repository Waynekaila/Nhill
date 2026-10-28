# 13 — Politique de sécurité de l'information (résumé public)

*English: [../en/13-security-policy.md](../en/13-security-policy.md)*

| Cartouche | |
|---|---|
| Éditeur | Nhill Solution, Kinshasa (République démocratique du Congo) |
| Version | 3.0.0 — 28 octobre 2026 |
| Statut | résumé destiné aux clients et partenaires ; chaque affirmation est vérifiée dans le code et les outils d'exploitation de la version 3.0.0 et doit être relue à chaque version (référence technique : `docs/fr/securite.md`, `docs/fr/03-audit-securite.md`, `docs/fr/deploiement-vps-hostinger.md`) |
| Hypothèses | H-03, H-04, H-15, H-19 ([00](00-recherche-juridique.md) § 2) |

## Nos engagements

1. **Vos données restent séparées** : la base de données isole chaque établissement
   (sécurité au niveau des lignes, imposée par la base elle-même) ; nos tests vérifient
   qu'un établissement ne peut pas lire les données d'un autre.
2. **Vos échanges sont chiffrés** : les applications communiquent avec le serveur en
   HTTPS ; la base de données n'est pas accessible depuis Internet.
3. **Vos mots de passe ne sont jamais stockés en clair** : ils sont conservés sous forme
   d'empreinte (Argon2id). Minimum 8 caractères, mots de passe trop courants refusés,
   tentatives limitées. Aucun changement périodique n'est imposé.
4. **Chaque action est tracée** : le journal d'activité de votre établissement enregistre
   qui a fait quoi ; aucune fonction de Nhill ne permet de le modifier ou de l'effacer, ni
   à vous ni à notre équipe (le rôle de base de données utilisé par l'application n'en a
   pas le droit).
5. **Notre équipe n'accède pas à vos documents** : la console d'administration ne donne
   accès ni à vos factures ni à vos clients ; l'accès direct au serveur est réservé à
   l'exploitation technique (maintenance, sauvegardes) ; nous consultons seulement des
   chiffres globaux d'utilisation, et chaque consultation apparaît dans votre journal.
6. **Nos administrateurs utilisent une double authentification** obligatoire.
7. **Sauvegardes chiffrées** chaque jour, conservées 14 jours sur le serveur ; la clé qui
   permet de les déchiffrer n'est pas conservée sur le serveur ; une restauration d'essai
   sur une base séparée vérifie qu'elles sont utilisables.
8. **Aucun secret dans les applications** : elles ne contiennent ni mot de passe de base
   de données ni clé d'accès.

## Ce que nous ne promettons pas

Aucun logiciel n'est exempt de failles. Nhill n'a pas encore fait l'objet d'un audit
externe ni d'un test d'intrusion indépendant ; nous le prévoyons avant un déploiement à
grande échelle et en publierons la date.

## Ce que nous attendons de vous

Un compte par personne ; retirer l'accès des employés qui partent ; un mot de passe propre
à Nhill (une courte phrase se retient facilement) ; un appareil à jour, protégé par un
antivirus sous Windows ; nous signaler toute activité suspecte.

## Incidents

En cas de violation de données vous concernant, nous vous prévenons dans les meilleurs
délais, avec ce que nous savons et les mesures prises (procédure : document 14).

## Signaler une faille

support@nhill.online, objet « Sécurité ». Merci de ne pas exploiter ni divulguer la faille
avant sa correction.
