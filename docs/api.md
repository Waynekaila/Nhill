# Nhill — Référence de l'API v1

> Référence de l'API `v1`, inchangée en 2.0 (sauf longueur minimale des mots de passe : 8 caractères).

Générée à partir des déclarations de routes du serveur (`server/src/services/*.cpp`)
le 25 octobre 2026 : 126 points d'accès.

## Conventions

* Base : `https://<serveur>/api/v1`. JSON UTF-8. Langue des messages et des PDF :
  en-tête `Accept-Language: fr|en`.
* Authentification : `Authorization: Bearer <jeton d'accès>` (15 min), renouvelé par
  `POST /auth/refresh`. La console utilise un jeton admin distinct (non interchangeable).
* **Montants : chaînes décimales** (`"1234.50"`), quantités : chaînes (`"2.5"`), taux :
  points de base entiers (`1600`). Dates `YYYY-MM-DD`, horodatages ISO 8601 UTC.
* Écritures sensibles : en-tête `Idempotency-Key` (UUID) obligatoire ; rejeu →
  même réponse + `Idempotent-Replayed: true`.
* Pagination : `limit` (≤ 200), `offset`. Rapports : `from`, `to`, `format=json|csv|pdf`.
* Erreurs : `{"error": {"code", "message", "details", "request_id"}}` (voir
  `docs/technique.md` §11). Écriture refusée si abonnement suspendu : `402`.
* Accès : **Public** (sans jeton), **User** (membre d'un établissement ; la colonne
  « Permission » indique le droit requis), **Admin** (console), **Webhook** (signature HMAC).

Permissions par rôle : propriétaire et administrateur — toutes ; gérant — catalogue,
tiers, ventes, émission, achats, stock, dépenses, paiements, rapports, voir le code ;
vendeur/caissier — tiers, ventes, émission, paiements ; comptable — dépenses, paiements,
rapports, journal ; lecture seule — aucune écriture.


## Public / intégration

| Méthode | Chemin | Accès | Permission | Description |
|---|---|---|---|---|
| GET | `/health` | Public | — | État du service et de la base (200 ok / 503 dégradé) |
| GET | `/api/v1/meta` | Public | — | Version, version minimale du client, pays, devises, langues |
| GET | `/api/v1/countries/:cc/profile` | Public | — | Profil pays : identifiants légaux, taxes, mentions, numérotation |
| POST | `/api/v1/establishments` | Public | — | Option B : crée l'établissement, le propriétaire, l'essai et le code ; refuse un doublon d'identifiant légal (`establishment_exists`) |
| POST | `/api/v1/establishments/attachment-requests` | Public | — | Demande de rattachement à une entreprise existante (réponse identique que l'entreprise existe ou non) |
| POST | `/api/v1/join/check-code` | Public | — | Option A, étape 1 : code d'établissement → ticket de 10 min (erreur générique, limité par IP) |
| POST | `/api/v1/join/login` | Public | — | Option A : ticket + e-mail + mot de passe d'un membre |
| POST | `/api/v1/join/activate` | Public | — | Option A : ticket + e-mail + code d'invitation → création du compte et de la session |
| POST | `/api/v1/webhooks/payments/:provider` | Webhook | — | Webhook prestataire : `X-Nhill-Timestamp`, `X-Nhill-Signature: v1=HMAC-SHA256(secret, ts.corps)`, ±300 s, idempotent par `event_id` |

## Authentification et compte

| Méthode | Chemin | Accès | Permission | Description |
|---|---|---|---|---|
| POST | `/api/v1/auth/login` | Public | — | Connexion (e-mail, mot de passe, établissement si plusieurs) |
| POST | `/api/v1/auth/refresh` | Public | — | Rotation du jeton de rafraîchissement (réutilisation détectée → révocation) |
| POST | `/api/v1/auth/logout` | User | — | Fermeture de la session courante |
| GET | `/api/v1/me` | User | — | Utilisateur, rôle, permissions, établissement, abonnement, autres établissements |
| PATCH | `/api/v1/me` | User | — | Nom, téléphone, langue |
| POST | `/api/v1/me/password` | User | — | Changement de mot de passe (ferme les autres sessions) |
| POST | `/api/v1/me/switch` | User | — | Changer d'établissement sans ressaisir le mot de passe |
| GET | `/api/v1/notifications` | User | — | Notifications de l'utilisateur |
| POST | `/api/v1/notifications/:id/read` | User | — | Marquer une notification comme lue |
| GET | `/api/v1/members` | User | users.manage | Membres, invitations en attente, limite d'utilisateurs de l'offre |
| PATCH | `/api/v1/members/:id` | User | users.manage | Changer le rôle ou désactiver un membre (sessions révoquées) |

## Établissement, équipe, abonnement

| Méthode | Chemin | Accès | Permission | Description |
|---|---|---|---|---|
| GET | `/api/v1/establishment` | User | — | Fiche de l'établissement, paramètres, mention de conformité |
| PATCH | `/api/v1/establishment` | User | settings.manage | Modifier la fiche |
| PUT | `/api/v1/establishment/legal-ids` | User | settings.manage | Remplacer les identifiants légaux (validés selon le profil) |
| PATCH | `/api/v1/establishment/settings` | User | settings.manage | Paramètres de documents (langue, délai, arrondi, pied de page, stock négatif) |
| PUT | `/api/v1/establishment/logo` | User | settings.manage | Téléverser le logo (PNG/JPEG, corps brut, vérification du contenu réel) |
| GET | `/api/v1/establishment/logo` | User | — | Lire le logo |
| DELETE | `/api/v1/establishment/logo` | User | settings.manage | Supprimer le logo |
| GET | `/api/v1/establishment/code` | User | code.view | Code d'établissement en cours et date de renouvellement |
| POST | `/api/v1/establishment/code/rotate` | User | code.rotate | Renouveler le code immédiatement |
| GET | `/api/v1/establishment/audit` | User | audit.read | Journal brut (ancien point d'accès, conservé ; préférer /activity) |
| POST | `/api/v1/invitations` | User | users.manage | Inviter (e-mail, rôle) ; renvoie le code d'activation une seule fois |
| DELETE | `/api/v1/invitations/:id` | User | users.manage | Révoquer une invitation |
| GET | `/api/v1/plans` | Public | — | Offres publiées avec leurs prix par durée et devise, plafonds et fonctionnalités |
| GET | `/api/v1/subscription` | User | subscription.manage | Abonnement (offre, prix, échéance, changement prévu, échecs), commandes, historique. `/auth/me` ne donne aux autres rôles que `{status, write_allowed}` |
| POST | `/api/v1/subscription/quote` | User | subscription.manage | Devis calculé par le serveur : `{price_id, promo_code}` → type (new/renewal/upgrade/downgrade), prix, remise, crédit au prorata, total |
| POST | `/api/v1/subscription/orders` | User | subscription.manage | Créer la commande (`price_id`, `promo_code`, `provider`) ; total nul → appliquée immédiatement |
| POST | `/api/v1/subscription/orders/:id/checkout` | User | subscription.manage | Créer la session de paiement chez le prestataire → `checkout_url` |
| POST | `/api/v1/subscription/orders/:id/cancel` | User | subscription.manage | Annuler une commande non payée (code promo libéré) |
| POST | `/api/v1/subscription/auto-renew` | User | subscription.manage | Donner ou retirer le consentement au renouvellement automatique |
| POST | `/api/v1/webhooks/payments/:provider` | Webhook | — | Notification du prestataire (signature vérifiée, dédoublonnage, relecture chez le prestataire) |
| GET | `/payment/return` | Public | — | Page HTML de retour après paiement (n'active rien) |

## Assistance

| Méthode | Chemin | Accès | Permission | Description |
|---|---|---|---|---|
| GET | `/api/v1/support/tickets` | User | — | Demandes : les siennes ; toutes celles de l'établissement avec `users.manage` |
| POST | `/api/v1/support/tickets` | User | — | Nouvelle demande `{category: bug|question|billing|feature, subject, body}` (20 par jour) |
| GET | `/api/v1/support/tickets/:id` | User | — | Demande et messages (notes internes exclues) |
| POST | `/api/v1/support/tickets/:id/messages` | User | — | Répondre |
| POST | `/api/v1/support/tickets/:id/close` | User | — | Fermer |
| POST | `/api/v1/client-errors` | User | — | Rapport d'erreur technique d'une application (données personnelles masquées) |

## Point de vente (Nhill POS)

| Méthode | Chemin | Accès | Permission | Description |
|---|---|---|---|---|
| GET | `/api/v1/pos/config` | User | pos.sell | Caisses accessibles, moyens de paiement, règles, permissions, session ouverte de l'utilisateur |
| GET | `/api/v1/pos/catalog?register_id=` | User | pos.sell | Produits actifs avec prix, taxe et stock du dépôt de la caisse |
| GET | `/api/v1/pos/registers` | User | pos.report | Caisses, moyens de paiement, règles ; plafond de l'offre (avec pos.manage) |
| POST / PATCH | `/api/v1/pos/registers`, `/:id` | User | pos.manage | Créer (plafond de l'offre) / modifier / désactiver une caisse |
| PUT | `/api/v1/pos/registers/:id/users` | User | pos.manage | Caissiers autorisés `{user_ids}` (membres actifs seulement) |
| POST / PATCH | `/api/v1/pos/payment-methods`, `/:id` | User | pos.manage | Moyens de paiement (un moyen espèces reste actif) |
| PATCH | `/api/v1/pos/settings` | User | pos.manage | `max_discount_bp`, `allow_price_override`, `receipt_footer`, `receipt_width_mm` (58/80) |
| GET / POST / PATCH | `/api/v1/pos/promo-codes` | User | pos.manage | Codes promo de caisse (pourcentage) |
| POST | `/api/v1/pos/approvals` | User | pos.sell | Autorisation de superviseur `{email, password, action, session_id}` → jeton à usage unique (3 min) |
| POST | `/api/v1/pos/sessions` | User | pos.sell | Ouvrir une session `{register_id, opening_float}` |
| GET | `/api/v1/pos/sessions`, `/:id` | User | pos.sell ou pos.report | Sessions (les siennes ; toutes avec pos.report), totaux, moyens, tickets ; attendu et écart avec pos.supervise |
| POST | `/api/v1/pos/sessions/:id/cash-movements` | User | pos.sell | Entrée / sortie d'espèces (sortie : pos.cash ou autorisation) |
| POST | `/api/v1/pos/sessions/:id/close` | User | pos.sell | Clôture `{counted: {moyen: montant}, note}` (espèces obligatoires) |
| GET | `/api/v1/pos/sessions/:id/report.pdf` | User | pos.sell ou pos.report | Lecture X (ouverte) ou rapport Z (clôturée) |
| POST | `/api/v1/pos/sales` | User | pos.sell | Vente `{session_id, lines, customer_id?, promo_code?, payments, approval_token?}` → `{receipt, change}` |
| GET | `/api/v1/pos/sales/:id/receipt.pdf` | User | pos.sell | Ticket (58/80 mm) |
| POST | `/api/v1/pos/sales/:id/return` | User | pos.sell | Retour `{session_id, reason, lines[{line_no, quantity}], refunds?, restock}` (pos.refund ou autorisation) |
| POST | `/api/v1/pos/sales/:id/void` | User | pos.sell | Annulation du ticket dans sa session (pos.void ou autorisation) |
| GET | `/api/v1/pos/receipts?number=` | User | pos.sell | Retrouver un ticket par numéro |

Sans la permission requise, une opération sensible répond `403 approval_required`
avec `details.action` ; l'application demande alors l'autorisation et rejoue la requête
avec `approval_token`.

## Installation (mode installation uniquement)

| Méthode | Chemin | Description |
|---|---|---|
| GET | `/health` | `{"status":"setup","database":false}` tant que le serveur n'est pas installé |
| GET | `/api/v1/setup/status` | État du mode installation |
| POST | `/api/v1/setup/check` | `{setup_token, database}` → sondage (version, droits, schéma existant) |
| POST | `/api/v1/setup/apply` | `{setup_token, database, first_admin}` → rôles, migrations, vérification, configuration, jeton du premier administrateur ; le serveur redémarre |

## Traçabilité

| Méthode | Chemin | Accès | Permission | Description |
|---|---|---|---|---|
| GET | `/api/v1/activity` | User | — | Journal d'activité : filtres `user_id`, `action` (préfixe), `from`, `to`, pagination, `format=csv|pdf`. Sans `audit.read` : propres actions uniquement |
| GET | `/api/v1/reports/staff` | User | audit.read | Activité par employé et par devise (`from`, `to`, `format`) |

## Catalogue, tiers, stock

| Méthode | Chemin | Accès | Permission | Description |
|---|---|---|---|---|
| GET | `/api/v1/tax-rates` | User | — | Taux de taxe |
| POST | `/api/v1/tax-rates` | User | settings.manage | Créer un taux |
| PATCH | `/api/v1/tax-rates/:id` | User | settings.manage | Libellé, actif, défaut (le taux lui-même est immuable) |
| GET | `/api/v1/product-categories` | User | — | Catégories |
| POST | `/api/v1/product-categories` | User | catalog.write | Créer une catégorie |
| DELETE | `/api/v1/product-categories/:id` | User | catalog.write | Supprimer une catégorie vide |
| GET | `/api/v1/products` | User | — | Produits (recherche, catégorie, stock bas, pagination) |
| GET | `/api/v1/products/export.csv` | User | — | Export CSV du catalogue |
| GET | `/api/v1/products/:id` | User | — | Produit |
| POST | `/api/v1/products` | User | catalog.write | Créer un produit ou service |
| PATCH | `/api/v1/products/:id` | User | catalog.write | Modifier un produit |
| POST | `/api/v1/products/import` | User | catalog.write | Import CSV tout ou rien (erreurs par ligne) |
| GET | `/api/v1/customers` | User | — | Clients (recherche, soldes) |
| GET | `/api/v1/suppliers` | User | — | Fournisseurs |
| GET | `/api/v1/parties/:id` | User | — | Tiers, soldes, documents |
| POST | `/api/v1/parties` | User | parties.write | Créer un client et/ou fournisseur |
| PATCH | `/api/v1/parties/:id` | User | parties.write | Modifier un tiers |
| GET | `/api/v1/warehouses` | User | — | Dépôts (et si l'offre permet plusieurs dépôts) |
| POST | `/api/v1/warehouses` | User | stock.adjust | Créer un dépôt (selon l'offre) |
| GET | `/api/v1/stock` | User | — | Niveaux de stock et valeur estimée |
| GET | `/api/v1/stock/movements` | User | — | Mouvements de stock |
| POST | `/api/v1/stock/adjustments` | User | stock.adjust | Ajustement ou stock d'ouverture avec motif obligatoire (idempotent) |
| POST | `/api/v1/stock/transfers` | User | stock.adjust | Transfert entre dépôts (idempotent) |

## Ventes, achats, paiements, dépenses

| Méthode | Chemin | Accès | Permission | Description |
|---|---|---|---|---|
| GET | `/api/v1/expense-categories` | User | — | Catégories de dépenses |
| POST | `/api/v1/expense-categories` | User | expenses.write | Créer une catégorie |
| GET | `/api/v1/expenses` | User | — | Dépenses (période, catégorie, impayées) |
| POST | `/api/v1/expenses` | User | expenses.write | Créer une dépense, éventuellement payée (idempotent) |
| PUT | `/api/v1/expenses/:id/receipt` | User | expenses.write | Joindre un justificatif (PDF/PNG/JPEG, 5 Mo) |
| GET | `/api/v1/expenses/:id/receipt` | User | — | Lire le justificatif |
| GET | `/api/v1/payments` | User | — | Paiements (sens, période) |
| POST | `/api/v1/payments` | User | payments.write | Paiement d'une facture, d'un avoir (remboursement), d'un achat ou d'une dépense — jamais au-delà du reste dû (idempotent) |
| GET | `/api/v1/cash-accounts` | User | — | Caisses et comptes, solde enregistré |
| POST | `/api/v1/cash-accounts` | User | settings.manage | Créer une caisse ou un compte |
| GET | `/api/v1/exchange-rates` | User | — | Taux de change enregistrés |
| POST | `/api/v1/exchange-rates` | User | settings.manage | Enregistrer un taux (date et source) |
| GET | `/api/v1/purchases` | User | — | Achats |
| GET | `/api/v1/purchases/:id` | User | — | Achat |
| POST | `/api/v1/purchases` | User | purchases.write | Créer un achat brouillon (idempotent) |
| PUT | `/api/v1/purchases/:id` | User | purchases.write | Modifier un brouillon |
| POST | `/api/v1/purchases/:id/receive` | User | purchases.write | Réceptionner : entrées de stock, prix d'achat mis à jour (idempotent) |
| POST | `/api/v1/purchases/:id/cancel` | User | purchases.write | Annuler un brouillon |
| GET | `/api/v1/sales-documents` | User | — | Documents (type, statut, tiers, période, recherche, retard) |
| GET | `/api/v1/sales-documents/:id` | User | — | Document complet, paiements, historique |
| GET | `/api/v1/sales-documents/:id/pdf` | User | — | PDF du document (`inline` pour affichage) |
| POST | `/api/v1/sales-documents` | User | sales.write | Créer un brouillon (idempotent) |
| PUT | `/api/v1/sales-documents/:id` | User | sales.write | Modifier un brouillon |
| DELETE | `/api/v1/sales-documents/:id` | User | sales.write | Supprimer un brouillon (jamais un document émis) |
| POST | `/api/v1/sales-documents/:id/issue` | User | sales.issue | Émettre : numéro définitif, stock, taux de change figé, empreinte du PDF (idempotent) |
| POST | `/api/v1/sales-documents/:id/status` | User | sales.write | Transition contrôlée (devis accepté/refusé, commande annulée) |
| POST | `/api/v1/sales-documents/:id/convert` | User | sales.write | Devis/commande → commande/facture (brouillon) |
| POST | `/api/v1/sales-documents/:id/credit-note` | User | sales.issue | Brouillon d'avoir sur une facture émise (option remise en stock) |
| PUT | `/api/v1/sales-documents/:id/fiscal-reference` | User | sales.issue | Enregistrer la référence de la facture normalisée DEF (rapprochement, aucune certification) |

## Rapports

| Méthode | Chemin | Accès | Permission | Description |
|---|---|---|---|---|
| GET | `/api/v1/reports/dashboard` | User | — | Tableau de bord (`from`, `to`) ; périmètre personnel sans `reports.read` |
| GET | `/api/v1/reports/sales` | User | reports.read | Journal des ventes (avec « Émis par ») |
| GET | `/api/v1/reports/expenses` | User | reports.read | Dépenses |
| GET | `/api/v1/reports/stock-movements` | User | reports.read | Mouvements de stock |
| GET | `/api/v1/reports/receivables` | User | reports.read | Créances clients à ce jour |
| GET | `/api/v1/reports/payables` | User | reports.read | Dettes à ce jour |
| GET | `/api/v1/reports/payments` | User | reports.read | Paiements (avec « Enregistré par ») |
| GET | `/api/v1/reports/products` | User | reports.read | Ventes par produit, marge estimée |

## Console d'administration (jeton admin)

| Méthode | Chemin | Accès | Permission | Description |
|---|---|---|---|---|
| POST | `/api/v1/admin/auth/setup` | Public | — | Premier accès : jeton d'initialisation + mot de passe (≥ 14) → secret TOTP |
| POST | `/api/v1/admin/auth/setup/confirm` | Public | — | Premier accès : code TOTP → compte actif |
| POST | `/api/v1/admin/auth/login` | Public | — | Connexion admin : e-mail + mot de passe + TOTP (code non rejouable) |
| POST | `/api/v1/admin/auth/logout` | Admin | — | Fermer la session admin |
| GET | `/api/v1/admin/me` | Admin | — | Administrateur courant |
| POST | `/api/v1/admin/admins` | Admin | — | Inviter un administrateur (superadmin) ; jeton d'initialisation affiché une fois |
| GET | `/api/v1/admin/establishments` | Admin | — | Liste des établissements (recherche, statut d'abonnement) |
| GET | `/api/v1/admin/establishments/:id` | Admin | — | Fiche plateforme : abonnement, codes (sans valeur), usage sur 30 jours |
| POST | `/api/v1/admin/establishments/:id/suspend` | Admin | — | Suspendre avec motif (support) ; sessions révoquées |
| POST | `/api/v1/admin/establishments/:id/reactivate` | Admin | — | Réactiver avec motif (support) |
| POST | `/api/v1/admin/establishments/:id/codes/reissue` | Admin | — | Réémettre le code avec motif (support) ; le code n'est pas montré à l'admin |
| GET | `/api/v1/admin/plans` | Admin | — | Offres |
| POST | `/api/v1/admin/plans` | Admin | — | Créer une offre (billing) |
| PATCH | `/api/v1/admin/plans/:id` | Admin | — | Modifier une offre (billing) ; s'applique aux futures demandes |
| POST | `/api/v1/admin/plans/:id/prices` | Admin | billing | Ajouter ou remplacer un prix (durée, devise) |
| PATCH | `/api/v1/admin/plan-prices/:id` | Admin | billing | Modifier ou retirer un prix |
| POST | `/api/v1/admin/establishments/:id/subscription` | Admin | billing | `{action: extend|suspend|reactivate|cancel|set_plan, reason}` — motif obligatoire, audité, établissement notifié |
| POST | `/api/v1/admin/establishments/:id/orders` | Admin | billing | Créer une demande de paiement pour un établissement |
| GET | `/api/v1/admin/transactions`, `/:id` | Admin | billing, auditor | Commandes et paiements des prestataires, événements reçus |
| GET / POST / PATCH | `/api/v1/admin/payment-providers` | Admin | billing (lecture), superadmin (écriture) | Prestataires ; secrets en écriture seule |
| GET / POST / PATCH | `/api/v1/admin/promo-codes` | Admin | billing | Codes promo d'abonnement ; `/:id/redemptions` |
| GET / PATCH | `/api/v1/admin/tickets`, `/:id` | Admin | support | Demandes d'assistance ; `POST /:id/messages` (réponse ou note interne) |
| GET | `/api/v1/admin/errors` | Admin | support, auditor | Erreurs techniques regroupées ; `POST /:id/resolve` |
| GET | `/api/v1/admin/notifications` | Admin | — | Notifications de la console ; `POST /read` |
| GET | `/api/v1/admin/overview` | Admin | — | Indicateurs d'exploitation (abonnements, paiements, demandes, erreurs) |
| GET | `/api/v1/admin/orders` | Admin | — | Demandes de paiement d'abonnement |
| POST | `/api/v1/admin/orders/:id/confirm-manual` | Admin | — | Confirmation manuelle d'un virement (billing) : référence + motif, auditée |
| GET | `/api/v1/admin/webhook-events` | Admin | — | Événements prestataires reçus (billing, auditor) |
| GET | `/api/v1/admin/audit` | Admin | — | Journal plateforme ; détails des actions métier masqués (auditor, support) |
| GET | `/api/v1/admin/stats` | Admin | — | Chiffres globaux de la plateforme |
| GET | `/api/v1/admin/announcements` | Admin | — | Annonces |
| POST | `/api/v1/admin/announcements` | Admin | — | Publier une annonce (support) |
| GET | `/api/v1/admin/settings` | Admin | — | Paramètres de plateforme |
| PUT | `/api/v1/admin/settings/:key` | Admin | — | Modifier un paramètre (superadmin, liste blanche) |
| GET | `/api/v1/admin/attachment-requests` | Admin | — | Demandes de rattachement (support) |
| POST | `/api/v1/admin/attachment-requests/:id/resolve` | Admin | — | Accepter/refuser avec note (support) |
| GET | `/api/v1/admin/establishments/:id/stats` | Admin | — | Statistiques **agrégées** sur une période (billing, auditor) ; consultation auditée et visible par l'établissement |
| GET | `/api/v1/admin/reports/establishments` | Admin | — | Rapport plateforme : une ligne par établissement et devise (billing, auditor) — JSON/CSV/PDF |
| GET | `/api/v1/admin/reports/establishments/:id` | Admin | — | Rapport mensuel agrégé d'un établissement (billing, auditor) — JSON/CSV/PDF |
