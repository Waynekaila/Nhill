# Nhill Console 3.0 — Guide de l'exploitant

*English: [../en/nhill-console.md](../en/nhill-console.md)*

Nhill Console est l'application privée de l'exploitant de la plateforme (Nhill Solution) :
installation du serveur, établissements, formules et droits, facturation, assistance,
exploitation. Plateformes : Windows 10 et 11, Linux. Elle n'est pas distribuée aux
établissements.

La console ne donne **aucun accès aux factures, clients, produits ou stocks** des
établissements : seulement leur fiche plateforme, leur abonnement et des statistiques
**agrégées**, dont chaque consultation apparaît dans le journal de l'établissement.
Chaque action de la console est inscrite au journal d'audit de la plateforme.

## 1. Installer un nouveau serveur

1. Démarrer le serveur sans base : il passe en **mode installation** et écrit un **jeton
   d'initialisation** dans son journal (`journalctl -u nhill-server`), valable 60 minutes,
   renouvelé à chaque redémarrage.
2. Console › **Installer un nouveau serveur** : (1) adresse du serveur (HTTPS sauf serveur
   local) et jeton ; (2) base de données : PostgreSQL auto-hébergé ou Supabase (Session
   pooler), hôte, port, base, utilisateur administrateur, mot de passe (utilisé une fois,
   non conservé), TLS ; PostgreSQL 15 ou plus récent ; (3) premier administrateur ;
   (4) vérification.
3. La vérification affiche les contrôles : migrations, rôle applicatif sans privilèges et
   sans objet, sécurité au niveau des lignes forcée, schéma dédié `nhill`, fermeture aux
   rôles publics, fonctions à `search_path` fixé, aucune donnée visible ni écriture sans
   établissement, aucune modification du schéma par l'application. Le serveur redémarre
   ensuite en fonctionnement normal.

Sur le VPS de production, `deploy/vps/provision.sh` fait cette installation en ligne de
commande ([deploiement-vps-hostinger.md](deploiement-vps-hostinger.md)).

## 2. Accès

- **Adresse du serveur** : demandée au premier lancement, vérifiée avant d'être
  enregistrée ; **Changer de serveur** la modifie.
- **Premier accès** : e-mail, **jeton d'initialisation** (remis une seule fois, valable
  30 minutes), mot de passe (8 caractères minimum, pas un mot de passe trop courant), puis
  clé TOTP à ajouter dans une application d'authentification et code à 6 chiffres. La
  double authentification est **obligatoire**. Un jeton perdu ou expiré du premier
  administrateur se réémet avec `nhill-server admin-bootstrap` tant que le premier accès
  n'est pas terminé.
- **Connexion** : e-mail, mot de passe et code TOTP (un code ne sert qu'une fois). La
  session n'est pas mémorisée.
- **Rôles** : `superadmin` (tout, dont paramètres, prestataires, suppression
  d'établissements, invitations d'administrateurs), `billing` (formules, prix, droits,
  règles, transactions, codes promotionnels), `support` (établissements, suspensions,
  codes, rattachements, demandes d'assistance, erreurs, annonces), `auditor` (lecture :
  journal, transactions, événements des prestataires, rapports). Le détail par route est
  dans [reference-api.md](reference-api.md) (colonne « Rôles console »).

## 3. Écrans

| Écran | Usage |
|---|---|
| Vue d'ensemble | établissements, abonnements par état, utilisateurs actifs, activité, revenus confirmés |
| Notifications | assistance, paiements reçus ou en écart, erreurs techniques ; tout marquer comme lu |
| Établissements | recherche ; filtre **À supprimer (90 jours écoulés)** ; fiche : identité, abonnement, droits effectifs et usage, codes (sans leur valeur), statistiques agrégées ; **Suspendre**, **Réactiver**, **Réémettre le code**, **Abonnement…** (prolonger, suspendre, réactiver, annuler, changer de formule), **Supprimer…** — motif obligatoire |
| Rapports | rapport par établissement et par devise (CSV, PDF) ; rapport mensuel d'un établissement |
| Demandes de rattachement | vérifier l'identité, accepter ou refuser avec une note |
| Demandes d'assistance | prendre en charge, répondre, note interne (invisible pour l'établissement), état, gravité, personne assignée |
| Transactions | demandes de paiement d'abonnement, détail et références du prestataire, **confirmation manuelle** d'un virement (référence et motif) |
| Offres et tarifs | § 5 |
| Règles d'attribution | § 6 |
| Codes promo | pourcentage ou montant fixe dans une devise, dates, nombre d'utilisations, utilisations ; le code et la remise ne changent plus après création |
| Prestataires de paiement | Stripe, PayPal, générique signé (HMAC) ; Payoneer configurable sans adaptateur de paiement ; secrets **chiffrés, jamais réaffichés** ; mode test ou production |
| Événements des prestataires | notifications reçues et leur traitement |
| Erreurs techniques | erreurs regroupées des applications et du serveur, à marquer résolues |
| Journal d'audit | événements de la plateforme et de sécurité ; détails des actions métier masqués |
| Annonces | messages publiés aux établissements |
| Paramètres de la plateforme | essai, période de grâce, rappels, rotation des codes, limites de tentatives, durée des invitations, utilisateurs pendant l'essai, mot de passe oublié, instructions de paiement (FR/EN) ; **inviter un administrateur** (jeton affiché une seule fois) |

## 4. Supprimer un établissement

Engagement des conditions de service : à la fin du contrat, les données restent lisibles
et exportables pendant 90 jours, puis elles sont supprimées.

1. Établissements › filtre **À supprimer (90 jours écoulés)**. La fiche indique la fin de
   l'accès et la date à partir de laquelle la suppression est due.
2. **Supprimer…** (super-administrateur) : motif, saisie exacte du nom commercial. Avant les
   90 jours, seulement si le client l'a demandé (case à cocher), et jamais tant que
   l'établissement a un accès complet.
3. Sont supprimés : documents, clients, fournisseurs, produits, stock, paiements, fichiers,
   journal d'activité, codes, sessions et comptes utilisés uniquement dans cet
   établissement. Les commandes d'abonnement payées ou remboursées sont archivées 10 ans
   pour la comptabilité. La suppression est inscrite au journal de la plateforme. Elle est
   définitive (les sauvegardes disparaissent à leur rotation).

## 5. Formules et droits (Offres et tarifs)

- **Identité** : code, noms et descriptions FR/EN, type (payée, essai, gratuite, sur
  mesure), public (tout le monde, entrepreneurs individuels, entreprises), visibilité
  (publique ou privée : une formule privée ne s'attribue que depuis la console), niveau
  (supérieur = montée immédiate au prorata ; inférieur = changement en fin de période),
  ordre d'affichage.
- **Conditions** : libre-service, renouvellement automatique autorisé (avec accord exprès
  du client), délai de grâce, rappels (jours avant la fin), durée lors de l'attribution,
  formule appliquée à l'expiration à la place de la lecture seule.
- **Fonctions** : factures et avoirs, devis et commandes, caisse Nhill POS, achats,
  dépenses, plusieurs dépôts et transferts, plusieurs devises, import du catalogue,
  rapports avancés. **Une fonction désactivée est invisible dans les applications et
  refusée par le serveur.**
- **Limites** : utilisateurs, caisses, dépôts, produits, documents par mois, stockage
  (vide = illimité).
- **Prix par durée** (1, 3, 6, 12, 24 ou 36 mois) **et par devise**, HT, avec taux de taxe ;
  aucun prix n'est préconfiguré. Retirer ou proposer à nouveau un prix ; les changements ne
  s'appliquent qu'aux futures demandes de paiement.
- **Publiée** (visible par les établissements) ou **archivée** (plus de nouvelle
  souscription ; les abonnements en cours continuent).

Après enregistrement, les établissements de la formule reçoivent leurs nouveaux droits à
leur prochaine requête.

## 6. Règles d'attribution

Formule attribuée automatiquement à un nouvel établissement : la première règle active qui
correspond (type d'établissement, pays), par priorité croissante, s'applique ; comme
**essai** ou comme **période accordée** (durée en jours, ou sans fin pour une formule
gratuite ou sur mesure). Sans règle, les nouveaux établissements reçoivent l'essai standard
des paramètres de la plateforme.
