# 03 — Politique de confidentialité

*English: [../en/03-privacy-policy.md](../en/03-privacy-policy.md)*

> **MODÈLE — NE PAS PUBLIER EN L'ÉTAT.** À compléter et à faire valider (avocat, délégué à la
> protection des données). La version publiée sur le site (`website/src/pages/fr/confidentialite.html`)
> doit être alignée sur ce texte une fois validé.

| Cartouche | |
|---|---|
| Juridiction | RDC — Code du numérique, Livre III |
| Hypothèses | H-01, H-03, H-04, H-05, H-08, H-13, H-15 |
| Sources | S01, S02, S03, S04, S07, S16 — voir [00-recherche-juridique.md](00-recherche-juridique.md) § 7 |
| Clauses à valider | V-05, V-07, V-08, V-13 |
| Version | [NUMÉRO] du [DATE] |

## 1. Qui est responsable

[RAISON SOCIALE], [FORME JURIDIQUE], RCCM [RCCM], siège [ADRESSE] (« Nhill », « nous »).
Contact protection des données : info@nhill.online. Délégué à la protection des données :
[NOM ou « non désigné — motif »] **[À VALIDER : V-13]**.

Cette politique concerne : les visiteurs du site, les personnes qui remplissent un
formulaire, les clients et leurs utilisateurs (comptes), les personnes qui écrivent à
l'assistance.

Elle ne concerne pas les données que chaque établissement enregistre dans Nhill sur ses
propres clients, fournisseurs et employés : l'établissement en est responsable et Nhill les
traite pour son compte (accord de traitement, modèle 06).

## 2. Données, finalités, bases légales, durées

| Traitement | Données | Finalité | Base légale **[À VALIDER]** | Durée |
|---|---|---|---|---|
| Formulaires du site (démonstration, pilote, partenariat, question) | nom, organisation, e-mail, téléphone, ville, secteur, message, choix de consentement | répondre à la demande | mesures précontractuelles / consentement | [12] mois après le dernier échange (paramètre `SITE_RETENTION_DAYS`) |
| Protection du site contre les abus | empreinte HMAC non réversible de l'adresse IP, type de navigateur | sécurité, limitation des envois | intérêt légitime | [30] jours pour l'empreinte |
| Nouvelles et offres (si case cochée) | e-mail, nom, date et texte du consentement | prospection | **consentement préalable** (art. 68 à 72 du Code du numérique) | jusqu'au retrait, puis [3] ans pour la preuve du retrait |
| Comptes utilisateurs | nom, e-mail, téléphone, langue, rôle, mot de passe haché (Argon2id), sessions | fournir le Service, sécurité | exécution du contrat | durée du compte + [12] mois |
| Journal de sécurité et d'audit | actions, date, adresse IP, appareil | sécurité, preuve, lutte contre la fraude | obligation légale / intérêt légitime | [12] mois **[À VALIDER : V-07]** |
| Facturation des abonnements | identité de l'établissement, identifiants légaux, paiements | facturation, comptabilité | obligation légale | 10 ans (pièces comptables, OHADA) |
| Assistance | messages, pièces jointes, erreurs techniques masquées | répondre, corriger | exécution du contrat | [24] mois après clôture |
| Statistiques d'exploitation | agrégats par établissement (nombres, volumes par devise, utilisateurs actifs) | piloter et améliorer le Service | intérêt légitime | [24] mois |

Nhill ne vend pas de données personnelles et n'en fait pas de profilage publicitaire. Le site
ne dépose **aucun cookie** et n'utilise aucun outil de mesure d'audience tiers (modèle 04).

## 3. Destinataires

- l'équipe Nhill habilitée, selon son rôle (support, facturation) ;
- les sous-traitants techniques : hébergement [HÉBERGEUR, PAYS], envoi d'e-mails
  [PRESTATAIRE ou « aucun »], prestataires de paiement [STRIPE / PAYPAL / AUTRE, PAYS] pour
  les seuls paiements ;
- les autorités, sur demande légale.

## 4. Transferts hors de la RDC

Les données sont hébergées [en RDC / en [PAYS] chez [HÉBERGEUR]]. Un hébergement hors de la
RDC constitue un transfert international encadré par le Code du numérique ; Nhill le
réalise après [autorisation de l'autorité de protection des données n° [N°] du [DATE]]
et avec des garanties contractuelles. **[À VALIDER : V-05, H-04, H-05.]**

## 5. Sécurité

Échanges chiffrés (HTTPS), mots de passe hachés, isolation des données de chaque
établissement au niveau de la base, contrôle d'accès par rôle, journal d'audit, sauvegardes
chiffrées. Résumé : modèle 13. En cas de violation présentant un risque, Nhill informe
l'autorité et les personnes concernées dans les délais légaux (modèle 14).

## 6. Vos droits

Vous pouvez demander l'accès à vos données, leur rectification, leur effacement, vous
opposer à un traitement fondé sur l'intérêt légitime, retirer à tout moment votre
consentement à la prospection (lien de désinscription ou message à info@nhill.online).
Formulaire : modèle 19. Réponse sous [30] jours. Une pièce justificative peut être demandée
en cas de doute sur l'identité.

Vous pouvez saisir l'autorité de protection des données : [NOM ET COORDONNÉES DE
L'AUTORITÉ COMPÉTENTE — ARPTC à titre provisoire ou ARPTIC, à vérifier] **[À VALIDER :
H-05]**.

## 7. Employés des établissements

Le journal d'activité de Nhill enregistre les actions de chaque utilisateur dans
l'application. L'établissement employeur est responsable de ce traitement et doit en
informer son personnel (modèle 17).

## 8. Mineurs

Le Service est destiné aux professionnels ; il n'est pas conçu pour des enfants.

## 9. Modifications

Toute modification substantielle est annoncée sur le site et dans l'application [30] jours
avant son entrée en vigueur. Historique des versions : [LIEN].
