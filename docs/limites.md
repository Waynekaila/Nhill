# Limites connues — Nhill 0.1.0

Ce qui n'est **pas** fait ou **pas** vérifié, à lire avant toute mise en service.

## Plateformes

| Application | Linux | Windows | Android |
|---|---|---|---|
| Nhill Business | Compilée, testée, **paquet .deb construit et installé** dans un Ubuntu 24.04 vierge | Script + installateur Inno Setup fournis, **jamais exécutés** | Manifeste, icônes, script fournis, **jamais compilée** |
| Nhill POS | Idem Linux (vente, retour, clôture vérifiés avec le paquet installé) | Idem Windows, non exécuté | Non prévue (application de bureau) |
| Nhill Console | Idem Linux | Idem Windows, non exécuté | Non prévue (par choix) |
| Serveur | Paquet .deb installé, installation par l'API vérifiée | Non prévu | — |

* Aucune chaîne MSVC, ni Qt pour Windows ou Android, ni SDK Android n'était disponible ou
  téléchargeable dans l'environnement de développement.
* Qt testé : 6.4.2 (Ubuntu 24.04). Les scripts Windows/Android visent Qt 6.5+, non testé.
* Le paquet .deb vise Ubuntu 24.04 (dépendances `t64`) ; Debian 12 ou Ubuntu 22.04
  demandent une reconstruction sur la distribution cible.
* Le service systemd n'a pas pu être démarré dans l'environnement de test (pas de
  systemd) : le serveur a été lancé à la main avec la même commande que l'unité.

## Réglementation

* **Pas de facture normalisée (RDC)** : Nhill n'est pas un SFE homologué et n'est
  connecté à aucun DEF. Les factures, tickets et devis sont des **documents de gestion** ;
  ils ne sont présentés comme conformes à aucune réglementation nationale. Vérification
  des exigences applicables à faire avec un professionnel avant tout usage fiscal.
* **Tickets de caisse** : pas de certification de caisse (type NF525 ou équivalent),
  pas de signature électronique des tickets ni de journal inaltérable certifié. Les
  sessions clôturées sont immuables en base et les opérations sont journalisées, ce qui
  ne vaut pas certification.
* Pas de facturation électronique européenne (EN 16931, UBL/CII, plateformes agréées).
* Profils fiscaux `CD` et `INTL` non validés par un professionnel.
* Conditions générales, politique de confidentialité, information des employés sur le
  journal d'activité : **à rédiger** par l'exploitant.
* Le nom « Nhill » n'a fait l'objet d'**aucune recherche d'antériorité** de marque ni de
  nom de domaine ; sa disponibilité juridique n'est pas établie (voir `brand/identite.md`).

## Paiements

* **Aucun paiement réel n'a été effectué.** Les adaptateurs Stripe (cartes Visa,
  Mastercard…) et PayPal suivent la documentation publique et sont testés **contre des
  réponses simulées** uniquement. Avant le mode production : paiement de test avec un
  compte du prestataire, vérification de l'adresse de webhook publique.
* **Payoneer** : fiche de configuration seulement ; l'activation est refusée
  (`provider_not_implemented`) faute de documentation d'API accessible et d'identifiants
  de test.
* Mobile Money (M-Pesa, Airtel Money, Orange Money) : pas d'intégration d'opérateur ; au
  POS, le caissier vérifie le paiement sur son téléphone/terminal et Nhill enregistre le
  montant. Les abonnements peuvent être réglés par virement / Mobile Money confirmé à la
  main par l'équipe de facturation (audité).
* Pas de prélèvement automatique ; le renouvellement est une nouvelle commande.
* La facture de l'abonnement (exploitant → établissement) n'est pas générée.

## Point de vente

* Pas de pilote d'imprimante de tickets (ESC/POS) : le ticket est un PDF au format
  rouleau 58 ou 80 mm à imprimer par le système ; pas d'ouverture de tiroir-caisse.
* Lecteur de codes-barres : pris en charge comme clavier (saisie + Entrée) ; pas de
  lecture par la caméra.
* Pas de mode hors ligne : une caisse sans réseau ne vend pas (choix : stock, numérotation
  et droits arbitrés par le serveur). Une vente dont la réponse est perdue est rejouée
  sans doublon grâce à sa clé d'idempotence.
* Remboursements de retours en espèces seulement depuis l'application (l'API accepte
  d'autres moyens).
* Codes promo de caisse : pourcentage sur tout le ticket uniquement.

## Fonctionnel

* Export **XLSX non implémenté** (CSV et PDF seulement).
* Taux de change saisis à la main (pas de flux BCC automatique).
* Valeur du stock et marges estimées au prix d'achat actuel (pas de coût moyen pondéré).
* Pas de comptabilité générale (plan OHADA, écritures, grand livre).
* Pas d'authentification à deux facteurs pour les utilisateurs des établissements.
* **Pas d'envoi d'e-mails** (transport `log` uniquement) ni de SMS.

## Sécurité et exploitation

* Pas d'audit externe ni de test d'intrusion (audit interne : `securite.md`).
* Tables de plateforme sans RLS, isolées par les requêtes du serveur (`securite.md` § 4.2).
* Pas d'archivage continu des WAL (perte possible jusqu'à 24 h avec la seule sauvegarde
  quotidienne) ; sur Supabase, les sauvegardes dépendent de l'offre du projet.
* Pas de supervision intégrée au-delà de `/health` et des « Erreurs techniques » de la
  console.
* Traductions : interfaces en français et anglais ; relecture par un locuteur natif
  non faite.
