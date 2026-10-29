# Nhill 3.0 — Guide utilisateur

*English: [../en/user-guide.md](../en/user-guide.md)*

Depuis la version 3.0, chaque application a son propre guide :

| Application | Pour qui | Plateformes | Guide |
|---|---|---|---|
| **Nhill Business** | l'établissement : ventes, clients, stock, achats, dépenses, rapports, équipe, réglages | Windows, Linux, Android | [nhill-business.md](nhill-business.md) |
| **Nhill POS** | les caissiers d'un établissement | Windows, Linux | [nhill-pos.md](nhill-pos.md) |
| **Nhill Console** | l'exploitant de la plateforme (application privée) | Windows, Linux | [nhill-console.md](nhill-console.md) |

Captures réelles de la version 3.0 (établissement de démonstration, interface en
français) : `docs/assets/v3/`.

## 1. Nouveautés de la version 3.0

- **Formules** : chaque établissement voit seulement les fonctions comprises dans sa
  formule ; une fonction hors formule n'apparaît nulle part et le serveur la refuse. Les
  limites (utilisateurs, caisses, dépôts, produits, documents par mois, stockage) et leur
  usage s'affichent dans Paramètres › Abonnement.
- **Adresse du serveur** : saisie et vérifiée au premier lancement de chaque application,
  modifiable ensuite (plus aucune adresse n'est intégrée aux applications).
- **Factures en caisse** : Nhill POS peut établir une facture A4 au nom d'un client
  identifié, avec la même numérotation et le même modèle que Nhill Business. Tous les
  documents (A4 ou ticket de 58/80 mm) utilisent un seul modèle.
- **Mentions sur les documents** : l'établissement rédige les mentions imprimées sur chaque
  document (Paramètres › Établissement).
- **Export complet** : le propriétaire télécharge une archive ZIP de toutes les données, à
  tout moment, même en lecture seule.
- **Android** : bouton **Partager** sur un document.
- **Fin de contrat** : 90 jours de lecture seule pour exporter, puis suppression définitive
  par l'exploitant.
- **Recherches** : `%` et `_` sont cherchés tels quels.

Versions précédentes : [changelog.md](changelog.md).

## 2. Règles communes

- Mots de passe : 8 caractères au moins, pas un mot de passe trop courant ; aucun
  changement périodique imposé. Le bouton en forme d'œil affiche ou masque la saisie.
- « Se souvenir de moi » garde un jeton de session dans le stockage sécurisé du système,
  jamais le mot de passe.
- Une opération interrompue par une coupure se relance sans être enregistrée deux fois.
- Langue (français, anglais) et thème (clair, sombre, automatique) : Paramètres › Mon
  compte.

## 3. Ce que Nhill fait et ne fait pas

| Sujet | Ce que fait Nhill | Ce qu'il ne fait pas |
|---|---|---|
| Obligations fiscales | taux de taxe, identifiants légaux et mentions sur chaque document ; profil `CD` (RDC, à faire valider par un fiscaliste) et profil `INTL` générique ; référence d'un document officiel (par exemple DEF de la DGI) enregistrée sur la facture | n'est **certifié dans aucun pays** : ni caisse certifiée, ni facture électronique réglementaire, ni dispositif fiscal |
| Comptabilité | journaux et exports CSV/PDF (ventes, paiements, dépenses, créances, dettes) | ne tient pas la comptabilité générale |
| Paiements | espèces, carte, virement, chèque, Mobile Money enregistrés avec leur référence | n'est relié ni aux opérateurs Mobile Money ni aux terminaux de carte |
| Envoi des documents | PDF à enregistrer ou à partager (bouton Partager sur Android) | pas d'envoi direct par e-mail ou SMS depuis l'application |
| Connexion | une connexion mobile suffit ; une opération interrompue est renvoyée sans doublon | sans connexion, on ne peut ni vendre ni facturer |
| Appareils | Nhill Business : Windows, Linux, Android ; Nhill POS : Windows, Linux | pas de caisse sur Android ni sur iOS |
