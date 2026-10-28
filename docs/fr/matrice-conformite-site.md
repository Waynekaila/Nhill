# Matrice de conformité : site vitrine et application (3.0.0)

*English: [../en/website-compliance-matrix.md](../en/website-compliance-matrix.md)*

| | |
|---|---|
| Version | 3.0.0 — 28 octobre 2026 |
| Objet | chaque affirmation du site vitrine (`website/src/pages/fr`, `website/src/pages/en`) confrontée au code, aux tests ou à une décision de la direction |
| Méthode | lecture du code serveur, des migrations et des écrans QML ; tests automatisés (`tests/server`, 121 tests dont 120 réussis et 1 ignoré en local le 28/10/2026) ; essais de bout en bout des outils de déploiement sur une machine Ubuntu 24.04 neuve en intégration continue |
| Légende | **Vérifié** : présent dans le code et, quand c'est indiqué, couvert par un test · **Outillé** : les outils existent et sont testés, l'exploitation réelle n'a pas commencé · **Engagement** : promesse de service humaine, non vérifiable dans le code, à tenir par l'équipe · **Corrigé** : écart trouvé pendant cette revue et corrigé |

## 1. Caisse (Nhill POS)

| Affirmation du site | État | Preuve |
|---|---|---|
| Chaque caissier a son compte et ne voit que sa caisse et sa session | Vérifié | rôles `cashier` et `cash_manager` (`db/migrations/0009_pos.sql`), affectation `pos_register_users` ; tests `tests/server/test_pos.cpp` |
| Ouverture avec fond de caisse ; vente au toucher ou au lecteur de codes-barres USB | Vérifié | `client/qml/Nhill/Pos/pages/PosHome.qml` (`openSession`), champ de recherche validé par la touche Entrée envoyée par le lecteur (`PosSale.qml`) |
| Plusieurs moyens de paiement sur une même vente (espèces, carte, Mobile Money, virement, chèque), monnaie à rendre | Vérifié | `pos_payment_methods.kind` (`0009_pos.sql`), `PosPayDialog.qml` |
| Ticket ou facture A4 au nom du client, même modèle que Nhill Business | Vérifié | `document: "invoice"` dans `PosPayDialog.qml` ; modèles communs (tâche « factures POS identiques à Business ») ; tests `test_documents.cpp` |
| Remise au-delà du plafond, retour, annulation : accord d'un responsable avec son mot de passe, valable une fois | Vérifié | `pos_approvals` (`0009_pos.sql`), `PosApprovalDialog.qml`, `PosNav.sendApproved` |
| Entrées et sorties d'espèces motivées, lecture X | Vérifié | `pos_cash_movements.kind` (`cash_in`, `cash_out`), rapport X (`server/src/services/pos.cpp`) |
| Clôture à l'aveugle, écart calculé et gardé dans le rapport Z | Vérifié | le montant attendu est retiré des réponses avant la clôture (`pos.cpp`, champs `_cash_expected_minor`) ; session close immuable (`nh_guard_pos_session`) |
| Tickets de 58 ou 80 mm ; raccourcis F2 et F9 | Vérifié | `receipt_width_mm IN (58, 80)` (`0009_pos.sql`) ; `Shortcut { sequence: "F2" }`, `"F9"` (`PosSale.qml`) |
| Une vente interrompue par une coupure se valide de nouveau sans être comptée deux fois | Vérifié | clé d'idempotence conservée pour le nouvel essai (`PosPayDialog.qml`, `idempotency_keys`) |

## 2. Factures, stock, rapports

| Affirmation du site | État | Preuve |
|---|---|---|
| Devis, bons de commande, factures, avoirs numérotés par le serveur sans trou ni doublon | Vérifié | `doc_type IN ('quote','order','invoice','credit_note')`, `document_sequences` (`0002_business.sql`) |
| Devis transformé en facture en un clic ; l'avoir corrige une facture et remet en stock | Vérifié | `POST /api/v1/sales-documents/:id/convert`, `/credit-note` avec `restock` (`server/src/services/sales.cpp`) |
| 36 devises, taux de change enregistré avec le document | Vérifié | 36 codes ISO 4217 insérés dans `currencies` (migrations) ; `fx_rate`, `fx_rate_date`, `fx_rate_source` (`0002_business.sql`) |
| Taux de taxe réglables (profil RDC : 16 %, 0 %, exonéré) | Vérifié | `tax_rates`, profil de conformité `CD` |
| Champ pour la référence de la facture normalisée | Vérifié | colonne `fiscal_reference` (`sales_documents`) |
| Stock par dépôt, transferts, mouvements tracés, alertes de seuil, valeur estimée | Vérifié | `stock_movements.kind` (`transfer_in`, `transfer_out`, `adjustment`…), `server/src/services/stock.cpp` ; tableau de bord |
| Import et export du catalogue en CSV | Vérifié | `GET /api/v1/products/export.csv`, `POST /api/v1/products/import` (`catalog.cpp`) |
| Rapports : ventes, paiements, dépenses, produits avec marge estimée, mouvements de stock, créances, dettes ; export PDF et CSV | Vérifié | `server/src/services/reports.cpp` (routes `/api/v1/reports/...`) |
| Justificatifs de dépenses (photo ou PDF) | Vérifié | table `objects`, `purpose = 'receipt'`, types PNG, JPEG, PDF |
| Archive complète des données par le titulaire (CSV, JSON, pièces jointes) | Vérifié | `GET /api/v1/establishment/export` (`export.cpp`) |

## 3. Équipe, sécurité, comptes

| Affirmation du site | État | Preuve |
|---|---|---|
| Rôles : propriétaire, administrateur, gérant, vendeur, comptable, lecture seule, responsable de caisse, caissier | Vérifié | contrainte `memberships_role_check` (`0009_pos.sql`) |
| Code d'établissement renouvelé tous les 30 jours par défaut | Vérifié | paramètre `code_rotation_days = 30` (`0004_reference_data.sql`), tâche `rotateCodes` |
| Journal d'activité non modifiable, exportable | Vérifié | droits `SELECT, INSERT` seulement sur `audit_log` (`0003_security.sql`) ; export (`activity.cpp`) |
| Employé désactivé : ses sessions sont fermées | Vérifié | `members.cpp` (révocation `member_disabled`) |
| Mot de passe oublié : mot de passe temporaire donné par l'administrateur ; code par e-mail pour propriétaires et administrateurs | Vérifié | `password_resets.purpose IN ('email_code','forced_change')` (`0011_password_recovery.sql`), `auth.cpp` ; tests `test_password.cpp` |
| Données de chaque établissement séparées | Vérifié | sécurité au niveau des lignes forcée sur 30 tables (contrôle `row_level_security` de `nhill-server setup`) ; tests d'isolation |
| L'équipe Nhill ne voit que des chiffres globaux, chaque consultation inscrite au journal de l'établissement | Vérifié | fonctions d'agrégats `0006_activity_and_platform_stats.sql`, audit des consultations |
| Fonctions limitées à la formule de l'établissement | Vérifié | droits par formule (`0012_plan_entitlements.sql`, `entitlements.cpp`) ; tests `test_entitlements.cpp` |
| Export possible même en lecture seule | Vérifié | accès `read_only` = consultation et export (`nh_subscription_access`) |

## 4. Appareils et langues

| Affirmation du site | État | Preuve |
|---|---|---|
| Nhill Business : Windows 10 et 11 (64 bits), Linux (Debian 13, Ubuntu 24.04), Android 7 et plus | Vérifié | compilations Windows, paquets `.deb` Ubuntu 24.04 et Debian 13, APK `minSdk 24` (`client/CMakeLists.txt`) en intégration continue |
| Nhill POS : Windows 10 et 11, Linux | Vérifié | mêmes compilations |
| Interface, documents et rapports en français et en anglais | Vérifié | `client/i18n/nhill_fr.ts` (0 chaîne non traduite au contrôle `scripts/qa/qml-check.py`), modèles PDF bilingues |
| « Une connexion mobile (3G, 4G) suffit » | Corrigé | aucune mesure de débit : la mention « 3G, 4G » est retirée de la FAQ (FR et EN) |

## 5. Données, hébergement, conservation

| Affirmation du site | État | Preuve |
|---|---|---|
| Hébergement sur un serveur Hostinger situé en France | Outillé | décision de la direction ; kit `deploy/vps` testé sur une machine neuve ; le serveur de production n'est pas encore déployé (aucun accès fourni) |
| Sauvegarde chiffrée chaque jour | Outillé | `nhill-backup.timer`, chiffrement `age`, restauration d'essai hors serveur (`deploy/vps/tests/acceptance.sh`) |
| Données d'un établissement supprimées 90 jours après la fin du contrat | **Corrigé** | aucune suppression n'existait : ajout de la migration `0014_establishment_erasure.sql`, de l'action « Supprimer… » de Nhill Console et de 6 tests (`tests/server/test_erasure.cpp`) |
| Demandes du site supprimées après 12 mois | **Corrigé** | le site annonçait « 12 mois après le dernier échange » alors que la purge compte depuis l'envoi : texte aligné sur le code (`SITE_RETENTION_DAYS`) |
| Journaux techniques : 90 jours au plus | Vérifié | journald `MaxRetentionSec=90day`, Caddy `roll_keep_for 2160h` ; sessions terminées supprimées après 90 jours (tâche quotidienne) |
| Aucun cookie, aucun traceur | Vérifié | aucun `Set-Cookie`, aucun stockage dans le navigateur, aucune ressource externe (`website/assets/js/site.js` ; politique de sécurité du contenu dans `website/backend/deploy/Caddyfile.site`) |

## 6. Engagements de service (à tenir par l'équipe)

Ces promesses figurent sur le site ; elles ne dépendent pas du logiciel et ne peuvent pas
être vérifiées dans le code. La direction confirme qu'elle peut les tenir avant la mise en
ligne du site.

| Affirmation | Pages |
|---|---|
| Démonstration d'environ 45 minutes, en visioconférence ou sur place à Kinshasa | accueil, contact, démarrer |
| Mise en route en une demi-journée environ (installation, import du catalogue, formation) | accueil, essai gratuit, FAQ |
| Essai accompagné de 30 jours : point chaque semaine, assistance ; places limitées | essai gratuit, accueil |
| Proposition écrite avant la fin de l'essai | essai gratuit, conditions |
| Au plus un message de nouvelles par mois | formulaires, politique de confidentialité |

## 7. Contacts et domaine

| Élément | État au 28 octobre 2026 |
|---|---|
| `nhill.online` | le domaine répond en HTTPS (certificat Let's Encrypt valable jusqu'au 25 janvier 2027) mais affiche la page de domaine parqué de Hostinger : le site n'est pas encore publié ; pas de redirection HTTP vers HTTPS ni d'en-tête HSTS (`website/tools/check_live.py`, rapport de l'intégration continue `ci-results/domain-check`) |
| `info@nhill.online`, `support@nhill.online` | aucun enregistrement MX, SPF ni DMARC : ces adresses ne peuvent pas recevoir de courrier tant que la messagerie n'est pas configurée |
