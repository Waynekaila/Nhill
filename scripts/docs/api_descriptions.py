"""Descriptions of the Nhill API routes (French, English), grouped by section.

Key: "METHOD /path" as declared in the server (setup-mode routes: "SETUP METHOD /path").
gen_api_reference.py fails when a declared route has no description here, or when a
description names a route that the server no longer declares.
"""

SECTIONS = [
    # (id, French title, English title, predicate on (mode, path))
    ("setup", "Mode installation (serveur sans base)", "Setup mode (server without a database)",
     lambda mode, p: mode == "setup"),
    ("service", "Service et référentiels", "Service and reference data",
     lambda mode, p: p in ("/health", "/api/v1/meta") or p.startswith("/api/v1/countries")),
    ("onboarding", "Inscription et rattachement", "Sign-up and joining",
     lambda mode, p: p.startswith("/api/v1/establishments") or p.startswith("/api/v1/join")),
    ("auth", "Authentification et compte", "Authentication and account",
     lambda mode, p: p.startswith("/api/v1/auth") or p.startswith("/api/v1/me") or p.startswith("/api/v1/notifications")),
    ("establishment", "Établissement, équipe et droits", "Business, team and rights",
     lambda mode, p: p.startswith("/api/v1/establishment") or p.startswith("/api/v1/members")
     or p.startswith("/api/v1/invitations") or p == "/api/v1/entitlements"),
    ("subscription", "Abonnement et paiement", "Subscription and payment",
     lambda mode, p: p.startswith("/api/v1/plans") or p.startswith("/api/v1/subscription")
     or p.startswith("/payment") or p.startswith("/api/v1/webhooks")),
    ("support", "Assistance", "Support",
     lambda mode, p: p.startswith("/api/v1/support") or p.startswith("/api/v1/client-errors")),
    ("catalog", "Catalogue", "Catalogue",
     lambda mode, p: p.startswith("/api/v1/tax-rates") or p.startswith("/api/v1/product")),
    ("parties", "Clients et fournisseurs", "Customers and suppliers",
     lambda mode, p: p.startswith("/api/v1/customers") or p.startswith("/api/v1/suppliers") or p.startswith("/api/v1/parties")),
    ("sales", "Documents de vente", "Sales documents", lambda mode, p: p.startswith("/api/v1/sales-documents")),
    ("purchases", "Achats", "Purchases", lambda mode, p: p.startswith("/api/v1/purchases")),
    ("stock", "Stock", "Stock", lambda mode, p: p.startswith("/api/v1/warehouses") or p.startswith("/api/v1/stock")),
    ("expenses", "Dépenses", "Expenses", lambda mode, p: p.startswith("/api/v1/expense")),
    ("payments", "Paiements, caisses et taux de change", "Payments, cash accounts and exchange rates",
     lambda mode, p: p.startswith("/api/v1/payments") or p.startswith("/api/v1/cash-accounts")
     or p.startswith("/api/v1/exchange-rates")),
    ("pos", "Caisse (Nhill POS)", "Till (Nhill POS)", lambda mode, p: p.startswith("/api/v1/pos")),
    ("reports", "Rapports et journal d'activité", "Reports and activity log",
     lambda mode, p: p.startswith("/api/v1/reports") or p.startswith("/api/v1/activity")),
    ("admin-access", "Console — accès des administrateurs", "Console — administrator access",
     lambda mode, p: p.startswith("/api/v1/admin/auth") or p in ("/api/v1/admin/me", "/api/v1/admin/admins")),
    ("admin-est", "Console — établissements", "Console — businesses",
     lambda mode, p: p.startswith("/api/v1/admin/establishments") or p.startswith("/api/v1/admin/attachment-requests")),
    ("admin-plans", "Console — formules et droits", "Console — plans and rights",
     lambda mode, p: p.startswith("/api/v1/admin/plan") or p.startswith("/api/v1/admin/capabilities")),
    ("admin-billing", "Console — facturation", "Console — billing",
     lambda mode, p: any(p.startswith("/api/v1/admin/" + x) for x in
                         ("orders", "transactions", "payment-providers", "promo-codes", "webhook-events"))),
    ("admin-ops", "Console — assistance et exploitation", "Console — support and operations",
     lambda mode, p: p.startswith("/api/v1/admin/")),
]

D = {
    # ------------------------------------------------------------------ setup mode
    "SETUP GET /health": (
        "Serveur en attente d'installation : `{\"status\":\"setup\",\"database\":false}`",
        "Server waiting for installation: `{\"status\":\"setup\",\"database\":false}`"),
    "SETUP GET /api/v1/meta": (
        "Identité du serveur (produit, version, API, version minimale du client) avec `mode: setup` ; les applications "
        "l'utilisent pour vérifier une adresse avant de l'enregistrer",
        "Server identity (product, version, API, minimum client version) with `mode: setup`; the applications use it to "
        "check an address before saving it"),
    "SETUP GET /api/v1/setup/status": (
        "`setup` ou `restarting` (installation terminée, redémarrage en cours) et version",
        "`setup` or `restarting` (installation done, restart in progress) and version"),
    "SETUP POST /api/v1/setup/check": (
        "Teste la connexion à la base `{setup_token, database:{…}}` sans rien écrire ; jeton affiché dans le journal du "
        "serveur, valable 60 min, 20 essais par 15 min et par adresse IP",
        "Tests the database connection `{setup_token, database:{…}}` without writing anything; token printed in the "
        "server log, valid 60 min, 20 attempts per 15 min per IP address"),
    "SETUP POST /api/v1/setup/apply": (
        "Installe : enregistre la configuration, applique les migrations, crée éventuellement le premier "
        "administrateur `{first_admin:{email, full_name}}` (jeton de premier accès renvoyé une fois), puis le serveur "
        "redémarre en mode normal ; `409 already_installed` ensuite",
        "Installs: saves the configuration, applies the migrations, optionally creates the first administrator "
        "`{first_admin:{email, full_name}}` (first-access token returned once), then the server restarts in normal "
        "mode; `409 already_installed` afterwards"),
    # ------------------------------------------------------------------ service
    "GET /health": (
        "État du service et de la base (200 `ok`, 503 dégradé)",
        "Service and database status (200 `ok`, 503 degraded)"),
    "GET /api/v1/meta": (
        "Version, version minimale du client, pays, devises (36), langues",
        "Version, minimum client version, countries, currencies (36), languages"),
    "GET /api/v1/countries/:cc/profile": (
        "Profil pays : identifiants légaux, taxes, mentions, numérotation",
        "Country profile: legal identifiers, taxes, notices, numbering"),
    # ------------------------------------------------------------------ onboarding
    "POST /api/v1/establishments": (
        "Création d'un établissement : établissement, propriétaire, formule attribuée (essai ou règle d'attribution) et "
        "code ; refuse un doublon d'identifiant légal (`establishment_exists`)",
        "Creates a business: business, owner, assigned plan (trial or assignment rule) and code; refuses a duplicate "
        "legal identifier (`establishment_exists`)"),
    "POST /api/v1/establishments/attachment-requests": (
        "Demande de rattachement à une entreprise existante (réponse identique que l'entreprise existe ou non)",
        "Request to join an existing company (same answer whether the company exists or not)"),
    "POST /api/v1/join/check-code": (
        "Rejoindre, étape 1 : code d'établissement → ticket de 10 min (erreur générique, limité par adresse IP)",
        "Join, step 1: business code → 10-minute ticket (generic error, rate-limited per IP address)"),
    "POST /api/v1/join/login": (
        "Rejoindre : ticket + e-mail + mot de passe d'un membre",
        "Join: ticket + e-mail + password of a member"),
    "POST /api/v1/join/activate": (
        "Rejoindre : ticket + e-mail + code d'invitation → création du compte et de la session",
        "Join: ticket + e-mail + invitation code → account and session created"),
    # ------------------------------------------------------------------ auth
    "POST /api/v1/auth/login": (
        "Connexion (e-mail, mot de passe, établissement si plusieurs). Avec un mot de passe temporaire : "
        "`{\"password_change_required\":true,\"change_token\",\"expires_at\"}` au lieu d'une session ; temporaire expiré "
        "→ `401 temporary_password_expired`",
        "Login (e-mail, password, business when several). With a temporary password: "
        "`{\"password_change_required\":true,\"change_token\",\"expires_at\"}` instead of a session; expired temporary "
        "password → `401 temporary_password_expired`"),
    "POST /api/v1/auth/refresh": (
        "Rotation du jeton de rafraîchissement (réutilisation détectée → révocation de la session)",
        "Refresh-token rotation (reuse detected → session revoked)"),
    "POST /api/v1/auth/logout": ("Fermeture de la session courante", "Closes the current session"),
    "POST /api/v1/auth/password/forgot": (
        "Mot de passe oublié `{email}` → **toujours** `202`. Code à 8 chiffres par e-mail pour un propriétaire, un "
        "administrateur ou un compte de plusieurs établissements ; consigne « contactez votre administrateur » pour un "
        "employé d'un seul établissement ; rien pour une adresse inconnue. 3 demandes par heure et par adresse, 10 par "
        "heure et par IP",
        "Forgotten password `{email}` → **always** `202`. 8-digit code by e-mail for an owner, an administrator or an "
        "account of several businesses; \"contact your administrator\" message for an employee of a single business; "
        "nothing for an unknown address. 3 requests per hour per address, 10 per hour per IP"),
    "POST /api/v1/auth/password/reset": (
        "`{email, code, new_password}` → 204 ; code faux, expiré ou épuisé → `400 invalid_reset_code` (même erreur "
        "dans tous les cas) ; toutes les sessions sont fermées",
        "`{email, code, new_password}` → 204; wrong, expired or used-up code → `400 invalid_reset_code` (same error in "
        "every case); every session is closed"),
    "POST /api/v1/auth/password/change-required": (
        "`{change_token, new_password}` après une connexion avec mot de passe temporaire ; ticket de 15 min à usage "
        "unique (`400 invalid_change_token`) ; `422 password_reused` si identique au temporaire ; même réponse qu'une "
        "connexion",
        "`{change_token, new_password}` after a login with a temporary password; single-use 15-minute ticket "
        "(`400 invalid_change_token`); `422 password_reused` if identical to the temporary one; same answer as a login"),
    "GET /api/v1/me": (
        "Utilisateur, rôle, permissions, version des droits, établissement, abonnement (détail pour le propriétaire et "
        "les administrateurs, état d'accès pour les autres), droits de la formule et usage, autres établissements",
        "User, role, permissions, rights version, business, subscription (details for the owner and administrators, "
        "access state for the others), plan rights and usage, other businesses"),
    "PATCH /api/v1/me": ("Nom, téléphone, langue", "Name, phone, language"),
    "POST /api/v1/me/password": (
        "Changement de mot de passe (ferme les autres sessions)",
        "Password change (closes the other sessions)"),
    "POST /api/v1/me/switch": (
        "Changer d'établissement sans ressaisir le mot de passe",
        "Switch business without typing the password again"),
    "GET /api/v1/notifications": ("Notifications de l'utilisateur", "The user's notifications"),
    "POST /api/v1/notifications/:id/read": ("Marquer une notification comme lue", "Mark a notification as read"),
    # ------------------------------------------------------------------ establishment
    "GET /api/v1/establishment": (
        "Fiche de l'établissement, paramètres, mention de conformité",
        "Business record, settings, compliance notice"),
    "PATCH /api/v1/establishment": ("Modifier la fiche", "Edit the record"),
    "PUT /api/v1/establishment/legal-ids": (
        "Remplacer les identifiants légaux (validés selon le profil pays)",
        "Replace the legal identifiers (validated against the country profile)"),
    "PATCH /api/v1/establishment/settings": (
        "Paramètres des documents : langue, délai de paiement, arrondi des taxes, pied de page, instructions de "
        "paiement, mentions (FR/EN), stock négatif",
        "Document settings: language, payment terms, tax rounding, footer, payment instructions, notices (FR/EN), "
        "negative stock"),
    "PUT /api/v1/establishment/logo": (
        "Téléverser le logo (PNG ou JPEG, corps brut, contenu réel vérifié)",
        "Upload the logo (PNG or JPEG, raw body, actual content checked)"),
    "GET /api/v1/establishment/logo": ("Lire le logo", "Read the logo"),
    "DELETE /api/v1/establishment/logo": ("Supprimer le logo", "Delete the logo"),
    "GET /api/v1/establishment/code": (
        "Code d'établissement en cours et date de renouvellement",
        "Current business code and renewal date"),
    "POST /api/v1/establishment/code/rotate": ("Renouveler le code immédiatement", "Renew the code now"),
    "GET /api/v1/establishment/audit": (
        "Journal brut (point d'accès conservé ; préférer `/api/v1/activity`)",
        "Raw log (endpoint kept; prefer `/api/v1/activity`)"),
    "GET /api/v1/establishment/export": (
        "Archive ZIP complète des données de l'établissement (JSON, CSV, pièces jointes, manifeste avec empreintes "
        "SHA-256) ; propriétaire seulement, même abonnement suspendu ou expiré ; 3 par 15 min",
        "Full ZIP archive of the business's data (JSON, CSV, attachments, manifest with SHA-256 hashes); owner only, "
        "even with a suspended or expired subscription; 3 per 15 min"),
    "GET /api/v1/entitlements": (
        "Droits de l'établissement : fonctions comprises dans la formule (jamais celles qui n'y sont pas), limites et "
        "usage, libellés FR/EN, version des droits",
        "The business's rights: features included in the plan (never the others), limits and usage, FR/EN labels, "
        "rights version"),
    "GET /api/v1/members": (
        "Membres, invitations en attente, limite d'utilisateurs de la formule",
        "Members, pending invitations, user limit of the plan"),
    "PATCH /api/v1/members/:id": (
        "Changer le rôle ou désactiver un membre (sessions révoquées)",
        "Change a member's role or deactivate them (sessions revoked)"),
    "POST /api/v1/members/:id/password-reset": (
        "Mot de passe temporaire pour un membre de **cet** établissement, affiché une seule fois ; refusé pour soi-même, "
        "le propriétaire, un administrateur si l'auteur n'est pas propriétaire, un membre désactivé (`409 "
        "member_disabled`), un compte actif dans un autre établissement (`409 account_shared`) ; 10 par 15 min et par "
        "auteur",
        "Temporary password for a member of **this** business, shown only once; refused for oneself, the owner, an "
        "administrator if the author is not the owner, a deactivated member (`409 member_disabled`), an account active "
        "in another business (`409 account_shared`); 10 per 15 min per author"),
    "POST /api/v1/invitations": (
        "Inviter (e-mail, rôle) dans la limite d'utilisateurs ; code d'activation renvoyé une seule fois",
        "Invite (e-mail, role) within the user limit; activation code returned only once"),
    "DELETE /api/v1/invitations/:id": ("Révoquer une invitation", "Revoke an invitation"),
    # ------------------------------------------------------------------ subscription
    "GET /api/v1/plans": (
        "Formules publiées avec leurs prix par durée et devise, limites et fonctions",
        "Published plans with their prices per duration and currency, limits and features"),
    "GET /api/v1/subscription": (
        "Abonnement (formule, prix, échéance, changement prévu, échecs), commandes, historique",
        "Subscription (plan, price, due date, scheduled change, failures), orders, history"),
    "POST /api/v1/subscription/quote": (
        "Devis calculé par le serveur `{price_id, promo_code}` → type (nouveau, renouvellement, montée, descente), "
        "prix, remise, crédit au prorata, total",
        "Server-computed quote `{price_id, promo_code}` → kind (new, renewal, upgrade, downgrade), price, discount, "
        "prorated credit, total"),
    "POST /api/v1/subscription/orders": (
        "Créer la commande (`price_id`, `promo_code`, `provider`) ; total nul → appliquée immédiatement",
        "Create the order (`price_id`, `promo_code`, `provider`); zero total → applied at once"),
    "POST /api/v1/subscription/orders/:id/checkout": (
        "Créer la session de paiement chez le prestataire → `checkout_url`",
        "Create the payment session with the provider → `checkout_url`"),
    "POST /api/v1/subscription/orders/:id/cancel": (
        "Annuler une commande non payée (code promotionnel libéré)",
        "Cancel an unpaid order (promo code released)"),
    "POST /api/v1/subscription/auto-renew": (
        "Donner ou retirer l'accord exprès au renouvellement automatique",
        "Give or withdraw express consent to automatic renewal"),
    "GET /payment/return": (
        "Page HTML de retour après paiement (n'active rien)",
        "HTML return page after payment (activates nothing)"),
    "POST /api/v1/webhooks/payments/:provider": (
        "Notification du prestataire de paiement : signature vérifiée, dédoublonnage, relecture chez le prestataire",
        "Payment provider notification: signature checked, deduplicated, re-read from the provider"),
    # ------------------------------------------------------------------ support
    "GET /api/v1/support/tickets": (
        "Demandes d'assistance : les siennes ; toutes celles de l'établissement avec `users.manage`",
        "Support requests: one's own; all of the business's with `users.manage`"),
    "POST /api/v1/support/tickets": (
        "Nouvelle demande `{category: bug|question|billing|feature, subject, body}` (20 par jour)",
        "New request `{category: bug|question|billing|feature, subject, body}` (20 per day)"),
    "GET /api/v1/support/tickets/:id": (
        "Demande et messages (notes internes de l'équipe exclues)",
        "Request and messages (the team's internal notes excluded)"),
    "POST /api/v1/support/tickets/:id/messages": ("Répondre", "Reply"),
    "POST /api/v1/support/tickets/:id/close": ("Fermer la demande", "Close the request"),
    "POST /api/v1/client-errors": (
        "Rapport d'erreur technique d'une application (données personnelles masquées)",
        "Technical error report from an application (personal data masked)"),
    # ------------------------------------------------------------------ catalog
    "GET /api/v1/tax-rates": ("Taux de taxe", "Tax rates"),
    "POST /api/v1/tax-rates": ("Créer un taux", "Create a rate"),
    "PATCH /api/v1/tax-rates/:id": (
        "Libellé, actif, défaut (le taux lui-même ne change pas)",
        "Label, active, default (the rate itself never changes)"),
    "GET /api/v1/product-categories": ("Catégories de produits", "Product categories"),
    "POST /api/v1/product-categories": ("Créer une catégorie", "Create a category"),
    "DELETE /api/v1/product-categories/:id": ("Supprimer une catégorie vide", "Delete an empty category"),
    "GET /api/v1/products": (
        "Produits et services (recherche, catégorie, stock bas, pagination)",
        "Products and services (search, category, low stock, pagination)"),
    "GET /api/v1/products/export.csv": ("Export CSV du catalogue", "Catalogue CSV export"),
    "GET /api/v1/products/:id": ("Produit", "Product"),
    "POST /api/v1/products": (
        "Créer un produit ou un service (dans la limite de produits de la formule)",
        "Create a product or service (within the plan's product limit)"),
    "PATCH /api/v1/products/:id": ("Modifier un produit", "Edit a product"),
    "POST /api/v1/products/import": (
        "Import CSV tout ou rien (erreurs par ligne)",
        "All-or-nothing CSV import (errors per line)"),
    # ------------------------------------------------------------------ parties
    "GET /api/v1/customers": ("Clients (recherche, soldes)", "Customers (search, balances)"),
    "GET /api/v1/suppliers": ("Fournisseurs", "Suppliers"),
    "GET /api/v1/parties/:id": ("Tiers, soldes, documents", "Party, balances, documents"),
    "POST /api/v1/parties": ("Créer un client et/ou un fournisseur", "Create a customer and/or supplier"),
    "PATCH /api/v1/parties/:id": ("Modifier un tiers", "Edit a party"),
    # ------------------------------------------------------------------ sales
    "GET /api/v1/sales-documents": (
        "Documents (type, statut, tiers, période, recherche, retard) ; seulement les types compris dans la formule",
        "Documents (type, status, party, period, search, overdue); only the types included in the plan"),
    "GET /api/v1/sales-documents/:id": (
        "Document complet, paiements, historique",
        "Full document, payments, history"),
    "GET /api/v1/sales-documents/:id/pdf": (
        "PDF du document : `format=a4|ticket`, `width=58|80` (même document dans les deux mises en page ; `inline` "
        "pour l'affichage)",
        "Document PDF: `format=a4|ticket`, `width=58|80` (the same document in both layouts; `inline` for display)"),
    "POST /api/v1/sales-documents": ("Créer un brouillon (idempotent)", "Create a draft (idempotent)"),
    "PUT /api/v1/sales-documents/:id": ("Modifier un brouillon", "Edit a draft"),
    "DELETE /api/v1/sales-documents/:id": (
        "Supprimer un brouillon (jamais un document émis)",
        "Delete a draft (never an issued document)"),
    "POST /api/v1/sales-documents/:id/issue": (
        "Émettre : numéro définitif, stock, taux de change figé, empreinte du PDF (idempotent ; limite de documents "
        "par mois, jamais pour un avoir)",
        "Issue: final number, stock, frozen exchange rate, PDF hash (idempotent; monthly document limit, never for a "
        "credit note)"),
    "POST /api/v1/sales-documents/:id/status": (
        "Transition contrôlée (devis accepté ou refusé, commande annulée)",
        "Controlled transition (quote accepted or declined, order cancelled)"),
    "POST /api/v1/sales-documents/:id/convert": (
        "Devis ou commande → commande ou facture (brouillon)",
        "Quote or order → order or invoice (draft)"),
    "POST /api/v1/sales-documents/:id/credit-note": (
        "Brouillon d'avoir sur une facture émise (option de remise en stock)",
        "Credit-note draft for an issued invoice (restock option)"),
    "PUT /api/v1/sales-documents/:id/fiscal-reference": (
        "Enregistrer la référence de la facture normalisée émise par le dispositif agréé (rapprochement, aucune "
        "certification)",
        "Record the reference of the standardised invoice issued by the approved device (reconciliation, no "
        "certification)"),
    # ------------------------------------------------------------------ purchases
    "GET /api/v1/purchases": ("Achats", "Purchases"),
    "GET /api/v1/purchases/:id": ("Achat", "Purchase"),
    "POST /api/v1/purchases": ("Créer un achat brouillon (idempotent)", "Create a draft purchase (idempotent)"),
    "PUT /api/v1/purchases/:id": ("Modifier un brouillon", "Edit a draft"),
    "POST /api/v1/purchases/:id/receive": (
        "Réceptionner : entrées de stock, prix d'achat mis à jour (idempotent)",
        "Receive: stock entries, purchase prices updated (idempotent)"),
    "POST /api/v1/purchases/:id/cancel": ("Annuler un brouillon", "Cancel a draft"),
    # ------------------------------------------------------------------ stock
    "GET /api/v1/warehouses": (
        "Dépôts (et indication du droit à plusieurs dépôts)",
        "Warehouses (and whether several warehouses are allowed)"),
    "POST /api/v1/warehouses": (
        "Créer un dépôt (dans la limite de dépôts de la formule)",
        "Create a warehouse (within the plan's warehouse limit)"),
    "GET /api/v1/stock": ("Niveaux de stock et valeur estimée", "Stock levels and estimated value"),
    "GET /api/v1/stock/movements": ("Mouvements de stock", "Stock movements"),
    "POST /api/v1/stock/adjustments": (
        "Ajustement ou stock d'ouverture, motif obligatoire (idempotent)",
        "Adjustment or opening stock, reason required (idempotent)"),
    "POST /api/v1/stock/transfers": ("Transfert entre dépôts (idempotent)", "Transfer between warehouses (idempotent)"),
    # ------------------------------------------------------------------ expenses
    "GET /api/v1/expense-categories": ("Catégories de dépenses", "Expense categories"),
    "POST /api/v1/expense-categories": ("Créer une catégorie", "Create a category"),
    "GET /api/v1/expenses": ("Dépenses (période, catégorie, impayées)", "Expenses (period, category, unpaid)"),
    "POST /api/v1/expenses": (
        "Créer une dépense, éventuellement payée (idempotent)",
        "Create an expense, possibly paid (idempotent)"),
    "PUT /api/v1/expenses/:id/receipt": (
        "Joindre un justificatif (PDF, PNG ou JPEG, 5 Mo ; limite de stockage de la formule)",
        "Attach a receipt (PDF, PNG or JPEG, 5 MB; storage limit of the plan)"),
    "GET /api/v1/expenses/:id/receipt": ("Lire le justificatif", "Read the receipt"),
    # ------------------------------------------------------------------ payments
    "GET /api/v1/payments": ("Paiements (sens, période)", "Payments (direction, period)"),
    "POST /api/v1/payments": (
        "Paiement d'une facture, d'un avoir (remboursement), d'un achat ou d'une dépense — jamais au-delà du reste dû "
        "(idempotent)",
        "Payment of an invoice, a credit note (refund), a purchase or an expense — never more than the amount due "
        "(idempotent)"),
    "GET /api/v1/cash-accounts": ("Caisses et comptes, solde enregistré", "Cash and bank accounts, recorded balance"),
    "POST /api/v1/cash-accounts": ("Créer une caisse ou un compte", "Create a cash or bank account"),
    "GET /api/v1/exchange-rates": ("Taux de change enregistrés", "Recorded exchange rates"),
    "POST /api/v1/exchange-rates": ("Enregistrer un taux (date et source)", "Record a rate (date and source)"),
    # ------------------------------------------------------------------ POS
    "GET /api/v1/pos/config": (
        "Caisses accessibles, moyens de paiement, règles, permissions, session ouverte de l'utilisateur",
        "Accessible tills, payment methods, rules, permissions, the user's open session"),
    "GET /api/v1/pos/catalog": (
        "`?register_id=` : produits actifs avec prix, taxe et stock du dépôt de la caisse",
        "`?register_id=`: active products with price, tax and stock of the till's warehouse"),
    "GET /api/v1/pos/registers": (
        "Caisses, moyens de paiement, règles ; limite de caisses de la formule",
        "Tills, payment methods, rules; till limit of the plan"),
    "POST /api/v1/pos/registers": (
        "Créer une caisse (code, nom, emplacement, dépôt) dans la limite de caisses de la formule",
        "Create a till (code, name, location, warehouse) within the plan's till limit"),
    "PATCH /api/v1/pos/registers/:id": ("Modifier, activer ou désactiver une caisse", "Edit, activate or deactivate a till"),
    "PUT /api/v1/pos/registers/:id/users": (
        "Caissiers autorisés `{user_ids}` (membres actifs seulement)",
        "Authorised cashiers `{user_ids}` (active members only)"),
    "POST /api/v1/pos/payment-methods": ("Ajouter un moyen de paiement", "Add a payment method"),
    "PATCH /api/v1/pos/payment-methods/:id": (
        "Modifier, activer ou désactiver un moyen de paiement",
        "Edit, activate or deactivate a payment method"),
    "PATCH /api/v1/pos/settings": (
        "`max_discount_bp`, `allow_price_override`, `receipt_footer`, `receipt_width_mm` (58 ou 80)",
        "`max_discount_bp`, `allow_price_override`, `receipt_footer`, `receipt_width_mm` (58 or 80)"),
    "GET /api/v1/pos/promo-codes": (
        "Codes promotionnels de vente de l'établissement et nombre d'utilisations",
        "The business's sales promo codes and number of uses"),
    "POST /api/v1/pos/promo-codes": (
        "Créer un code promotionnel de vente (pourcentage, montant minimal, validité, nombre maximal)",
        "Create a sales promo code (percentage, minimum amount, validity, maximum uses)"),
    "PATCH /api/v1/pos/promo-codes/:id": (
        "Modifier la validité ou activer / désactiver un code",
        "Change the validity or activate / deactivate a code"),
    "POST /api/v1/pos/approvals": (
        "Autorisation d'un responsable `{email, password, action, session_id}` → jeton à usage unique (3 min)",
        "Supervisor approval `{email, password, action, session_id}` → single-use token (3 min)"),
    "POST /api/v1/pos/sessions": ("Ouvrir une session `{register_id, opening_float}`", "Open a session `{register_id, opening_float}`"),
    "GET /api/v1/pos/sessions": (
        "Sessions de caisse : les siennes, toutes avec `pos.report` (filtre `register_id`, pagination)",
        "Till sessions: one's own, all with `pos.report` (`register_id` filter, pagination)"),
    "GET /api/v1/pos/sessions/:id": (
        "Détail d'une session : ventes et retours, encaissements par moyen, taxes, mouvements d'espèces, espèces "
        "attendues et écart, documents émis",
        "Session detail: sales and returns, takings per method, taxes, cash movements, expected cash and difference, "
        "documents issued"),
    "POST /api/v1/pos/sessions/:id/cash-movements": (
        "Entrée ou sortie d'espèces (sortie : `pos.cash` ou autorisation)",
        "Cash in or out (out: `pos.cash` or approval)"),
    "POST /api/v1/pos/sessions/:id/close": (
        "Clôture `{counted: {moyen: montant}, note}` (espèces obligatoires)",
        "Closing `{counted: {method: amount}, note}` (cash required)"),
    "GET /api/v1/pos/sessions/:id/report.pdf": (
        "Lecture X (session ouverte) ou rapport Z (session clôturée)",
        "X reading (open session) or Z report (closed session)"),
    "POST /api/v1/pos/sales": (
        "Vente `{session_id, lines, document: receipt|invoice, customer_id?, promo_code?, payments, approval_token?}` → "
        "`{receipt, change}` ; une facture exige un client identifié",
        "Sale `{session_id, lines, document: receipt|invoice, customer_id?, promo_code?, payments, approval_token?}` → "
        "`{receipt, change}`; an invoice requires an identified customer"),
    "GET /api/v1/pos/sales/:id/receipt.pdf": (
        "PDF de la vente : ticket 58/80 mm ou facture A4, même modèle que Nhill Business",
        "Sale PDF: 58/80 mm receipt or A4 invoice, same template as Nhill Business"),
    "POST /api/v1/pos/sales/:id/return": (
        "Retour `{session_id, reason, lines[{line_no, quantity}], refunds?, restock}` (`pos.refund` ou autorisation)",
        "Return `{session_id, reason, lines[{line_no, quantity}], refunds?, restock}` (`pos.refund` or approval)"),
    "POST /api/v1/pos/sales/:id/void": (
        "Annulation du ticket dans sa session (`pos.void` ou autorisation)",
        "Void the receipt within its session (`pos.void` or approval)"),
    "GET /api/v1/pos/receipts": (
        "`?number=` : retrouver un ticket ou une facture de caisse par son numéro",
        "`?number=`: find a till receipt or invoice by its number"),
    # ------------------------------------------------------------------ reports
    "GET /api/v1/activity": (
        "Journal d'activité : filtres `user_id`, `action` (préfixe), `from`, `to`, pagination, `format=csv|pdf` ; sans "
        "`audit.read`, ses propres actions seulement",
        "Activity log: `user_id`, `action` (prefix), `from`, `to` filters, pagination, `format=csv|pdf`; without "
        "`audit.read`, one's own actions only"),
    "GET /api/v1/reports/staff": (
        "Activité par employé et par devise (`from`, `to`, `format`)",
        "Activity per employee and per currency (`from`, `to`, `format`)"),
    "GET /api/v1/reports/dashboard": (
        "Tableau de bord (`from`, `to`) ; périmètre personnel sans `reports.read` ; indicateurs limités aux fonctions "
        "de la formule",
        "Dashboard (`from`, `to`); personal scope without `reports.read`; indicators limited to the plan's features"),
    "GET /api/v1/reports/sales": ("Journal des ventes (avec « Émis par »)", "Sales journal (with \"Issued by\")"),
    "GET /api/v1/reports/expenses": ("Dépenses", "Expenses"),
    "GET /api/v1/reports/stock-movements": ("Mouvements de stock", "Stock movements"),
    "GET /api/v1/reports/receivables": ("Créances clients à ce jour", "Customer receivables to date"),
    "GET /api/v1/reports/payables": ("Dettes à ce jour", "Payables to date"),
    "GET /api/v1/reports/payments": ("Paiements (avec « Enregistré par »)", "Payments (with \"Recorded by\")"),
    "GET /api/v1/reports/products": ("Ventes par produit, marge estimée", "Sales per product, estimated margin"),
    # ------------------------------------------------------------------ console: access
    "POST /api/v1/admin/auth/setup": (
        "Premier accès : jeton d'initialisation + mot de passe (8 caractères au moins, pas un mot de passe trop "
        "courant) → secret TOTP",
        "First access: setup token + password (at least 8 characters, not a too-common password) → TOTP secret"),
    "POST /api/v1/admin/auth/setup/confirm": ("Premier accès : code TOTP → compte actif", "First access: TOTP code → account active"),
    "POST /api/v1/admin/auth/login": (
        "Connexion : e-mail + mot de passe + code TOTP (code non rejouable)",
        "Login: e-mail + password + TOTP code (code cannot be replayed)"),
    "POST /api/v1/admin/auth/logout": ("Fermer la session d'administration", "Close the administration session"),
    "GET /api/v1/admin/me": ("Administrateur courant", "Current administrator"),
    "POST /api/v1/admin/admins": (
        "Inviter un administrateur ; jeton d'initialisation affiché une seule fois",
        "Invite an administrator; setup token shown only once"),
    # ------------------------------------------------------------------ console: businesses
    "GET /api/v1/admin/establishments": (
        "Liste des établissements (recherche, état d'abonnement, `erasable=true` : établissements à supprimer)",
        "List of businesses (search, subscription state, `erasable=true`: businesses to erase)"),
    "GET /api/v1/admin/establishments/:id": (
        "Fiche plateforme : abonnement, codes (sans leur valeur), usage sur 30 jours, état de suppression",
        "Platform record: subscription, codes (without their value), 30-day usage, erasure state"),
    "POST /api/v1/admin/establishments/:id/suspend": (
        "Suspendre avec motif ; sessions révoquées",
        "Suspend with a reason; sessions revoked"),
    "POST /api/v1/admin/establishments/:id/reactivate": ("Réactiver avec motif", "Reactivate with a reason"),
    "POST /api/v1/admin/establishments/:id/erase": (
        "Suppression définitive `{reason, confirm_trade_name, customer_request}` : possible 90 jours après la fin de "
        "l'accès, plus tôt seulement à la demande du client et jamais pendant un accès complet (`409 not_erasable`) ; "
        "commandes payées archivées 10 ans ; inscrite au journal de la plateforme",
        "Permanent erasure `{reason, confirm_trade_name, customer_request}`: possible 90 days after access ends, earlier "
        "only at the customer's request and never during full access (`409 not_erasable`); paid orders archived 10 "
        "years; recorded in the platform log"),
    "POST /api/v1/admin/establishments/:id/codes/reissue": (
        "Réémettre le code avec motif ; le code n'est pas montré à l'administrateur",
        "Reissue the code with a reason; the code is not shown to the administrator"),
    "POST /api/v1/admin/establishments/:id/subscription": (
        "`{action: extend|suspend|reactivate|cancel|set_plan, reason}` — motif obligatoire, inscrit au journal, "
        "établissement notifié",
        "`{action: extend|suspend|reactivate|cancel|set_plan, reason}` — reason required, logged, business notified"),
    "POST /api/v1/admin/establishments/:id/orders": (
        "Créer une demande de paiement pour un établissement",
        "Create a payment request for a business"),
    "GET /api/v1/admin/establishments/:id/stats": (
        "Statistiques **agrégées** sur une période ; consultation inscrite au journal et visible par l'établissement",
        "**Aggregated** statistics over a period; consultation logged and visible to the business"),
    "GET /api/v1/admin/establishments/:id/entitlements": (
        "Droits effectifs de l'établissement (toutes les fonctions, activées ou non) et compteurs d'usage, jamais le "
        "contenu",
        "The business's effective rights (every feature, on or off) and usage counters, never content"),
    "GET /api/v1/admin/attachment-requests": ("Demandes de rattachement", "Join requests"),
    "POST /api/v1/admin/attachment-requests/:id/resolve": (
        "Accepter ou refuser avec une note",
        "Accept or refuse with a note"),
    # ------------------------------------------------------------------ console: plans
    "GET /api/v1/admin/plans": ("Formules", "Plans"),
    "POST /api/v1/admin/plans": (
        "Créer une formule (code, noms et descriptions FR/EN, public visé, niveau, limites, fonctions)",
        "Create a plan (code, FR/EN names and descriptions, audience, tier, limits, features)"),
    "PATCH /api/v1/admin/plans/:id": (
        "Modifier une formule et ses conditions (essai, période de grâce, rappels, souscription en libre-service, "
        "renouvellement automatique, formule de repli à l'échéance, archivage) ; s'applique aux futures demandes",
        "Edit a plan and its terms (trial, grace period, reminders, self-service subscription, automatic renewal, "
        "fallback plan at expiry, archiving); applies to future requests"),
    "POST /api/v1/admin/plans/:id/prices": (
        "Ajouter ou remplacer un prix (durée de 1, 3, 6, 12, 24 ou 36 mois, devise)",
        "Add or replace a price (1, 3, 6, 12, 24 or 36 months, currency)"),
    "PATCH /api/v1/admin/plan-prices/:id": ("Modifier ou retirer un prix", "Edit or withdraw a price"),
    "GET /api/v1/admin/plans/:id/entitlements": (
        "Valeurs de la formule pour chaque fonction et limite, avec le catalogue",
        "The plan's value for each feature and limit, with the catalogue"),
    "PUT /api/v1/admin/plans/:id/entitlements": (
        "Remplacer toutes les valeurs de la formule (une entrée absente prend la valeur par défaut du catalogue) ; "
        "les établissements abonnés reçoivent leurs nouveaux droits à leur prochaine requête ; avant/après inscrits au "
        "journal",
        "Replace every value of the plan (a missing entry takes the catalogue default); subscribed businesses get their "
        "new rights at their next request; before/after logged"),
    "GET /api/v1/admin/capabilities": (
        "Catalogue des fonctions et des limites (la liste suit le code du serveur)",
        "Catalogue of features and limits (the list follows the server's code)"),
    "PATCH /api/v1/admin/capabilities/:key": (
        "Modifier les libellés et descriptions FR/EN d'une fonction ou d'une limite",
        "Edit the FR/EN labels and descriptions of a feature or limit"),
    "GET /api/v1/admin/plan-rules": ("Règles d'attribution de formule", "Plan assignment rules"),
    "POST /api/v1/admin/plan-rules": (
        "Créer une règle `{plan_id, mode: trial|grant, audience, country_code, duration_days, priority}` ; une "
        "attribution sans durée est réservée aux formules gratuites ou sur mesure",
        "Create a rule `{plan_id, mode: trial|grant, audience, country_code, duration_days, priority}`; an open-ended "
        "grant is reserved to free or custom plans"),
    "PATCH /api/v1/admin/plan-rules/:id": ("Modifier une règle", "Edit a rule"),
    "DELETE /api/v1/admin/plan-rules/:id": ("Supprimer une règle", "Delete a rule"),
    # ------------------------------------------------------------------ console: billing
    "GET /api/v1/admin/orders": ("Demandes de paiement d'abonnement", "Subscription payment requests"),
    "POST /api/v1/admin/orders/:id/confirm-manual": (
        "Confirmation manuelle d'un virement : référence + motif, inscrite au journal",
        "Manual confirmation of a transfer: reference + reason, logged"),
    "GET /api/v1/admin/transactions": (
        "Transactions d'abonnement (statut, prestataire, établissement, période, pagination)",
        "Subscription transactions (status, provider, business, period, pagination)"),
    "GET /api/v1/admin/transactions/:id": (
        "Détail d'une transaction et références chez le prestataire",
        "Transaction detail and provider references"),
    "GET /api/v1/admin/payment-providers": (
        "Prestataires de paiement configurés (secrets jamais renvoyés) et adresse publique de retour",
        "Configured payment providers (secrets never returned) and public return address"),
    "POST /api/v1/admin/payment-providers": (
        "Ajouter un prestataire (`stripe`, `paypal`, `payoneer` sans adaptateur, `generic_hmac`) ; secrets chiffrés",
        "Add a provider (`stripe`, `paypal`, `payoneer` without adapter, `generic_hmac`); secrets encrypted"),
    "PATCH /api/v1/admin/payment-providers/:id": (
        "Modifier un prestataire, ses secrets ou son activation",
        "Edit a provider, its secrets or its activation"),
    "GET /api/v1/admin/promo-codes": ("Codes promotionnels d'abonnement", "Subscription promo codes"),
    "POST /api/v1/admin/promo-codes": (
        "Créer un code (pourcentage ou montant fixe dans une devise)",
        "Create a code (percentage or fixed amount in a currency)"),
    "PATCH /api/v1/admin/promo-codes/:id": (
        "Modifier la validité, les limites ou l'activation (code et remise non modifiables)",
        "Edit validity, limits or activation (code and discount cannot change)"),
    "GET /api/v1/admin/promo-codes/:id/redemptions": ("Utilisations d'un code", "Uses of a code"),
    "GET /api/v1/admin/webhook-events": ("Événements reçus des prestataires", "Events received from providers"),
    # ------------------------------------------------------------------ console: operations
    "GET /api/v1/admin/tickets": (
        "Demandes d'assistance de tous les établissements (statut, catégorie, établissement, ouvertes)",
        "Support requests of every business (status, category, business, open)"),
    "GET /api/v1/admin/tickets/:id": ("Demande et messages, notes internes comprises", "Request and messages, internal notes included"),
    "POST /api/v1/admin/tickets/:id/messages": (
        "Répondre, ou ajouter une note interne invisible pour l'établissement",
        "Reply, or add an internal note the business cannot see"),
    "PATCH /api/v1/admin/tickets/:id": ("Statut, gravité, personne assignée", "Status, severity, assignee"),
    "GET /api/v1/admin/errors": ("Erreurs techniques regroupées", "Grouped technical errors"),
    "POST /api/v1/admin/errors/:id/resolve": ("Marquer une erreur comme résolue", "Mark an error as resolved"),
    "GET /api/v1/admin/notifications": ("Notifications de la console", "Console notifications"),
    "POST /api/v1/admin/notifications/read": ("Tout marquer comme lu", "Mark everything as read"),
    "GET /api/v1/admin/overview": (
        "Indicateurs d'exploitation (abonnements, paiements, demandes, erreurs)",
        "Operating indicators (subscriptions, payments, requests, errors)"),
    "GET /api/v1/admin/audit": (
        "Journal de la plateforme ; détails des actions métier masqués",
        "Platform log; details of business actions masked"),
    "GET /api/v1/admin/stats": ("Chiffres globaux de la plateforme", "Platform-wide figures"),
    "GET /api/v1/admin/announcements": ("Annonces", "Announcements"),
    "POST /api/v1/admin/announcements": ("Publier une annonce", "Publish an announcement"),
    "GET /api/v1/admin/settings": ("Paramètres de la plateforme", "Platform settings"),
    "PUT /api/v1/admin/settings/:key": (
        "Modifier un paramètre de la liste autorisée : `trial_days` 0–365, `grace_days` 0–60, `reminder_days_before` "
        "1–60, `code_rotation_days` 1–365, `code_check_max_per_15min` et `login_max_per_15min` 3–100, "
        "`invitation_days` 1–60, `trial_max_users` 1–1000, `password_reset_minutes` 5–60, "
        "`password_reset_max_attempts` 1–10, `temporary_password_hours` 1–168, `payment_instructions` (objet FR/EN)",
        "Change a setting from the allowed list: `trial_days` 0–365, `grace_days` 0–60, `reminder_days_before` 1–60, "
        "`code_rotation_days` 1–365, `code_check_max_per_15min` and `login_max_per_15min` 3–100, `invitation_days` "
        "1–60, `trial_max_users` 1–1000, `password_reset_minutes` 5–60, `password_reset_max_attempts` 1–10, "
        "`temporary_password_hours` 1–168, `payment_instructions` (FR/EN object)"),
    "GET /api/v1/admin/reports/establishments": (
        "Rapport de la plateforme : une ligne par établissement et par devise (JSON, CSV, PDF)",
        "Platform report: one row per business and per currency (JSON, CSV, PDF)"),
    "GET /api/v1/admin/reports/establishments/:id": (
        "Rapport mensuel agrégé d'un établissement (JSON, CSV, PDF)",
        "Aggregated monthly report of a business (JSON, CSV, PDF)"),
}
