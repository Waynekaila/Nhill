# 18 — Registre des activités de traitement (Nhill Solution)

*English: [../en/18-processing-register.md](../en/18-processing-register.md)*

| Cartouche | |
|---|---|
| Éditeur | Nhill Solution, Kinshasa (République démocratique du Congo) — contact : info@nhill.online |
| Version | 3.0.0 — 28 octobre 2026 |
| Statut | document interne, tenu à jour par la direction et présenté à l'autorité sur demande ; rédigé sans relecture par un avocat ; points de relecture recommandés : V-05, V-07, V-13 ([00](00-recherche-juridique.md) § 5) |
| Hypothèses | H-03, H-04, H-05, H-06 ([00](00-recherche-juridique.md) § 2) |
| Formalités | déclarations ou autorisations auprès de l'autorité de protection des données : à déterminer avec l'avocat (liste de validation, [00](00-recherche-juridique.md) § 6) ; chaque formalité accomplie est ajoutée à l'historique ci-dessous avec sa référence |

| N° | Traitement | Rôle | Finalité | Personnes | Données | Base | Destinataires | Lieu | Durée | Sécurité |
|---|---|---|---|---|---|---|---|---|---|---|
| T1 | Formulaires du site | responsable | répondre aux demandes | prospects | identité, contact, organisation, message, choix de consentement | démarches précontractuelles | équipe commerciale | serveur du site | 12 mois après l'envoi | base dédiée, rôle web limité à l'insertion, empreinte IP (HMAC) effacée après 30 jours |
| T2 | Nouvelles de Nhill | responsable | envois commerciaux | personnes ayant donné leur accord | e-mail, téléphone, accord | consentement | équipe commerciale, prestataire d'envoi | serveur du site | jusqu'au retrait | registre (document 16), désinscription |
| T3 | Comptes utilisateurs | responsable | authentification, sécurité | utilisateurs | identité, e-mail, empreinte du mot de passe, sessions | contrat | assistance | France (Hostinger) | durée du contrat de l'établissement + 90 jours | Argon2id, jetons courts, trousseau de l'appareil |
| T4 | Abonnements et paiements | responsable | gérer l'abonnement, facturer | titulaires et représentants des établissements | établissement, offre, paiements | contrat ; obligations comptables | direction, prestataire de paiement (pour le seul paiement) | France (Hostinger) | durée du contrat ; pièces comptables 10 ans | accès restreint, notifications de paiement vérifiées |
| T5 | Assistance | responsable | répondre, corriger | utilisateurs | messages, détails techniques sans mot de passe | contrat | assistance | France (Hostinger) | durée du contrat de l'établissement + 90 jours | accès par rôle |
| T6 | Journal d'administration de la plateforme | responsable | sécurité, preuve | administrateurs de la plateforme | actions, date, adresse IP | intérêt légitime | direction | France (Hostinger) | durée d'exploitation ; entrées liées à un établissement supprimées avec lui | ajout seul, double authentification des administrateurs |
| T7 | Données des établissements | **sous-traitant** | fournir le Service | clients, fournisseurs, employés des établissements | voir document 06 | celle de l'établissement | — | France (Hostinger) : **transfert hors de RDC** | durée du contrat + 90 jours | sécurité au niveau des lignes imposée par la base, HTTPS, sauvegardes chiffrées |
| T8 | Statistiques d'exploitation | responsable | exploiter le Service | établissements (chiffres globaux) | nombres, volumes par devise | intérêt légitime | direction, assistance | France (Hostinger) | calculées à la demande, non conservées | aucune donnée nominative ; consultation inscrite au journal de l'établissement |
| T9 | Journaux techniques du serveur | responsable | sécurité, diagnostic | utilisateurs, visiteurs | adresse IP, date, requête | intérêt légitime | exploitation | France (Hostinger) | 90 jours au plus | accès administrateur du serveur seulement |
| T10 | Suivi des prospects et des partenaires | responsable | donner suite aux rencontres et recommandations, suivre les partenaires | prospects, partenaires | nom, fonction, établissement, commune, coordonnées données, accord et canal, notes, sommes encaissées et commissions des partenaires | intérêt légitime ; consentement pour les envois ; contrat (partenaires) | équipe commerciale | fichiers de suivi de l'équipe | prospects : 12 mois après le dernier échange ; partenaires : durée du contrat puis pièces comptables 10 ans | accès limité à l'équipe commerciale, jamais publié (`marketing/fr/08-suivi-et-processus.md`) |
| T11 | Témoignages, photos et vidéos | responsable | présenter Nhill | personnes qui témoignent ou apparaissent, établissements | nom, fonction, texte, image, voix, résultat chiffré approuvé | consentement écrit (document 20) | public des supports choisis | site, réseaux, documents | tant que le contenu est utilisé, puis jusqu'à la fin du délai de réclamation | registre du document 20, retrait sur demande |

Historique des modifications et des formalités :

| Date | Modification ou formalité (référence) | Auteur |
|---|---|---|
| 28 octobre 2026 | version 3.0.0 : hébergement Hostinger en France, durées alignées sur la politique de confidentialité ; ajout du suivi des prospects et partenaires (T10) et des témoignages (T11) | Nhill Solution |
