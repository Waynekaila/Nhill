# 18 — Registre des activités de traitement (Éditeur)

*English: [../en/18-processing-register.md](../en/18-processing-register.md)*

> **MODÈLE — DOCUMENT INTERNE.** À tenir à jour par le délégué à la protection des données
> et à présenter à l'autorité sur demande. Les formalités (déclaration, autorisation)
> sont indiquées par traitement.

| Cartouche | |
|---|---|
| Juridiction | RDC — Code du numérique, Livre III ; décision n° 039/ARPTC/CLG/2025 (formalités, à reconfirmer) |
| Hypothèses | H-03, H-04, H-05, H-06 |
| Sources | S01, S02, S07 — voir [00-recherche-juridique.md](00-recherche-juridique.md) § 7 |
| Clauses à valider | V-05, V-07, V-13 |
| Version | [NUMÉRO] du [DATE] |

Responsable : [RAISON SOCIALE], [ADRESSE], [RCCM]. DPO : [NOM, CONTACT].

| N° | Traitement | Rôle de Nhill | Finalité | Personnes | Données | Base légale [À VALIDER] | Destinataires | Hébergement / transfert | Durée | Sécurité | Formalité (type, n°, date) |
|---|---|---|---|---|---|---|---|---|---|---|---|
| T1 | Formulaires du site | responsable | répondre aux demandes | prospects | identité, contact, organisation, message | précontractuel / consentement | équipe commerciale | [PAYS] | [365] j | base dédiée, rôle web en insertion seule, HMAC IP | [ ] |
| T2 | Prospection | responsable | envois commerciaux | prospects consentants | e-mail, téléphone, consentement | consentement | équipe commerciale | [ ] | jusqu'au retrait | registre (modèle 16) | [ ] |
| T3 | Comptes utilisateurs | responsable | authentification, sécurité | utilisateurs | identité, e-mail, mot de passe haché, sessions | contrat | assistance | [PAYS] | compte + [12] mois | Argon2id, jetons courts | [ ] |
| T4 | Facturation des abonnements | responsable | facturer, encaisser | représentants des clients | identité de l'établissement, paiements | obligation légale | comptabilité, prestataire de paiement | [ ] | 10 ans | accès restreint | [ ] |
| T5 | Assistance | responsable | répondre, corriger | utilisateurs | messages, erreurs masquées | contrat | support | [ ] | [24] mois | masquage automatique | [ ] |
| T6 | Journal d'audit plateforme | responsable | sécurité, preuve | administrateurs, utilisateurs | actions, IP | intérêt légitime | sécurité | [ ] | [12] mois | ajout seul | [ ] |
| T7 | Données des établissements | **sous-traitant** | fournir le Service | clients, fournisseurs, employés des établissements | voir modèle 06 | celle du client | — | [PAYS] → **transfert hors RDC si hébergement étranger** | contrat + [90] j | RLS forcée, TLS, sauvegardes chiffrées | autorisation de transfert : [ ] |
| T8 | Statistiques d'exploitation | responsable | piloter le service | établissements (agrégats) | nombres, volumes par devise | intérêt légitime | direction | [ ] | [24] mois | agrégats sans données nominatives | [ ] |
| T9 | Ressources humaines de l'Éditeur | responsable | gestion du personnel | salariés | [à compléter] | obligation légale | [ ] | [ ] | [ ] | [ ] | [ ] |

Historique des modifications :

| Date | Modification | Auteur |
|---|---|---|
| [DATE] | création | [NOM] |
