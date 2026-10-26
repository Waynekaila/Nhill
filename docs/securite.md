# Nhill — Sécurité et audit

Ce document décrit le modèle de sécurité de Nhill (serveur, Nhill Business, Nhill POS,
Nhill Console) et consigne l'**audit interne** réalisé le 26 octobre 2026 : points
contrôlés, preuves (tests automatisés, requêtes de vérification), défauts trouvés et
corrigés, risques résiduels. Il ne remplace pas un audit externe ni un test d'intrusion,
qui n'ont pas été réalisés.

## 1. Principes

* **Le serveur décide.** Rôle, permission, appartenance, état de l'abonnement, plafonds
  de l'offre, autorisations de superviseur : tout est vérifié côté serveur. Masquer un
  bouton dans une application n'est jamais une mesure de sécurité. Par exemple, la page
  « Abonnement » n'est visible que des administrateurs de l'entreprise, et l'API refuse
  aussi `GET /subscription` aux autres rôles (test
  `Subscription.PlanDetailsAreReservedToOwnersAndAdministrators`).
* **Un identifiant n'est pas une autorisation.** Ni l'identifiant d'un établissement,
  ni son code renouvelable, ni un code promotionnel, ni l'identifiant d'une session de
  caisse ou d'un ticket ne donne accès à quoi que ce soit (§ 4).
* **Aucun secret dans le dépôt ni dans les applications.** Les applications ne
  contiennent ni identifiants de base de données, ni clé d'API, ni secret de
  prestataire de paiement. **La clé Supabase `service_role` n'est jamais utilisée**,
  même côté serveur : Nhill se connecte à PostgreSQL avec ses propres rôles.

## 2. Trois identifiants à ne jamais confondre

| Notion | Nature | Rôle | Ce qu'il ne permet pas |
|---|---|---|---|
| Identifiant de l'établissement | UUID interne et stable | Clé technique (RLS, journaux, console) | Aucun accès : jamais accepté comme preuve |
| Code d'établissement | `NH-XXXX-XXXX-XXXX`, 60 bits, renouvelable (30 jours ou manuellement) | Permet à un employé de **trouver** son entreprise | Rejoindre exige le mot de passe d'un membre ou une invitation à usage unique |
| Abonnement | Enregistrement `subscriptions` (offre, prix, période, état) | Droits d'écriture et plafonds (utilisateurs, caisses, dépôts) | Ne change pas en renouvelant le code ; le code ne change pas l'abonnement |

Le code est stocké en HMAC (recherche) et chiffré (libsodium `secretbox`). Les
administrateurs de la plateforme ne voient jamais sa valeur. Tests :
`Codes.CodeAloneGivesNoAccess`, `Codes.ValidationIsGenericForEveryFailure`,
`Subscription.AnnualPlanStaysAnnualDespiteMonthlyCodeRotation`.

## 3. Authentification et sessions

| Élément | Mesure |
|---|---|
| Mots de passe | Argon2id (libsodium, 3 passes, 64 Mio) ; ≥ 10 caractères (≥ 14 pour la console) ; mots de passe courants, répétitifs ou égaux à l'e-mail refusés |
| Comparaison | Temps constant ; hachage factice pour un e-mail inconnu (pas d'énumération par le temps de réponse) |
| Jetons | Opaques (32 octets), seul leur SHA-256 est stocké ; accès 15 min ; rafraîchissement 30 jours avec rotation ; réutilisation d'un ancien jeton → révocation de toute la famille + audit |
| Stockage côté client | Jeton de rafraîchissement dans le trousseau du système (QtKeychain : Secret Service, Credential Manager, Keystore) ; **aucun repli en clair** |
| Changement de mot de passe | Ferme les autres sessions |
| Limitation des tentatives | Connexion (8 / 15 min par e-mail, 40 par IP) ; code d'établissement ; invitations ; création d'établissement ; console (5 par compte, 20 par IP) ; codes promo (15 par établissement, 30 par IP) ; autorisations de superviseur POS (10 par caissier, 8 par e-mail de superviseur) ; demandes d'assistance (20 par jour) ; rapports d'erreur (60) |
| Console | Exécutable séparé (Windows, Linux), comptes distincts, mot de passe + TOTP (RFC 6238, code non rejouable), rôles `superadmin`, `billing`, `support`, `auditor` ; jeton en mémoire uniquement |

## 4. Isolation des établissements

### 4.1 Base de données

* Schéma dédié `nhill`, fermé à `PUBLIC`, `anon`, `authenticated` et `service_role`
  (aucun droit ; vérifié sur Supabase : 0 privilège accordé à ces rôles).
* Rôle applicatif `nhill_app` : `NOSUPERUSER`, `NOBYPASSRLS`, ne possède aucun objet,
  ne peut pas créer de table. Le rôle propriétaire `nhill_owner` ne sert qu'aux
  migrations.
* **RLS forcée** (`FORCE ROW LEVEL SECURITY`) sur les **30 tables métier** — dont les
  9 tables du point de vente (caisses, sessions, mouvements, autorisations, moyens de
  paiement, codes promo de vente…). Politique unique
  `establishment_id = (SELECT nh_current_establishment())`, contexte positionné par
  transaction ; clés étrangères composites `(establishment_id, id)` qui interdisent de
  référencer l'objet d'un autre établissement.
* Toutes les fonctions du schéma ont un `search_path` fixé (vérifié : 0 exception).
* Vérification automatique à l'installation (§ 9) : rôle applicatif sans privilège,
  0 objet possédé, RLS forcée, 0 ligne visible sans établissement, écriture refusée hors
  établissement, aucun DDL possible.

### 4.2 Tables de plateforme (sans RLS, par conception)

Dix-huit tables qui portent un `establishment_id` ne sont **pas** sous RLS, car le
serveur doit les lire avant de connaître l'établissement ou pour la console :
`memberships`, `sessions`, `invitations`, `join_tickets`, `establishment_codes`,
`subscriptions`, `subscription_orders`, `subscription_events`, `promo_redemptions`,
`trial_registry`, `usage_daily`, `audit_log`, `notifications`, `support_tickets`,
`attachment_requests`, `admin_notifications`, `error_events`,
`establishment_identifiers`. Pour elles, l'isolation repose sur les requêtes du serveur.

**Revue effectuée** : chaque requête des services destinés aux établissements sur ces
tables a été relue ; elles filtrent toutes par l'établissement de la session ou passent
par un objet déjà chargé sous RLS. **Une requête a été durcie** (recherche de la monnaie
rendue d'un ticket dans `audit_log`, désormais filtrée aussi par établissement).
Risque résiduel : une future requête mal écrite sur ces tables ne serait pas rattrapée
par la base. Mesure : tests d'isolation (§ 4.3) à étendre à chaque nouvelle route.

### 4.3 Tests d'isolation

* `Isolation.OtherEstablishmentsDataIsUnreachable` : clients, documents, produits,
  paiements, stock d'un autre établissement inaccessibles par l'API et par la base.
* `Isolation.PosSupportAndBillingIdentifiersOfAnotherEstablishmentAreUseless` (nouveau) :
  avec les identifiants d'une session de caisse, d'un ticket, d'une caisse et d'une
  demande d'assistance d'un établissement A, l'établissement B obtient 404 sur la
  lecture, le PDF, le retour, l'annulation, les mouvements d'espèces, la clôture, la
  vente, la modification de la caisse et de ses caissiers, le catalogue, la demande et
  ses messages ; rien n'est écrit chez A.
* `PlatformStats.AggregateFunctionsKeepTenantIsolation`,
  `Migrations.EverythingLivesInTheNhillSchemaClosedToOtherRoles`.

## 5. Rôles et permissions

| Rôle | Points clés |
|---|---|
| Propriétaire, administrateur | Tout, dont équipe, abonnement, paramètres, configuration du POS |
| Gérant | Ventes, stock, rapports ; au POS : vendre, rembourser, superviser, rapports |
| Vendeur | Ventes ; au POS : vendre sur les caisses qui lui sont attribuées |
| Comptable | Lecture et rapports ; au POS : sessions et rapports Z, **sans vendre ni configurer** (test `Pos.AccountantReadsSessionsButCannotSell`) |
| Responsable de caisse | Toutes les permissions POS sauf la configuration |
| Caissier | Vendre sur sa caisse attribuée, dans **sa** session uniquement |
| Lecteur | Lecture seule |

Permissions POS : `pos.sell`, `pos.refund`, `pos.void`, `pos.cash`, `pos.supervise`,
`pos.manage`, `pos.report`. Un caissier ne peut ni ouvrir une caisse non attribuée, ni
agir dans la session d'un autre (test `Pos.CashierIsLimitedToTheAssignedRegisterAndOwnSession`).

## 6. Point de vente : opérations sensibles

* **Autorisations de superviseur** : remise au-delà du plafond, modification de prix,
  retour, annulation, sortie d'espèces, clôture de la session d'un autre. Le superviseur
  saisit **son propre** e-mail et mot de passe ; le serveur vérifie son rôle, refuse
  l'auto-validation, émet un jeton à usage unique (seul son SHA-256 est stocké), valable
  3 minutes, lié au demandeur, à l'action et à la session. Accordées et refusées sont
  journalisées (test `Pos.DiscountAboveTheLimitNeedsASupervisorApprovalUsedOnce`).
* **Clôture à l'aveugle** : le caissier déclare ses comptages sans voir l'attendu ;
  attendu et écart sont calculés par le serveur et ne sont montrés qu'aux superviseurs.
  Une session clôturée est **immuable** (déclencheur en base). Un écart crée une
  notification pour les responsables.
* **Ventes** : prix, taxes, remises et totaux recalculés par le serveur ; la somme des
  paiements doit égaler le total ; monnaie rendue seulement sur des espèces ; clé
  d'idempotence (une coupure réseau ne crée pas deux ventes) ; stock du dépôt de la
  caisse décrémenté dans la même transaction.
* Retours et annulations : avoirs liés au ticket, quantités plafonnées à ce qui reste
  retournable, remise en stock, journalisés avec le motif.

## 7. Paiements des abonnements

* **Aucun montant ni statut venant d'une application n'est cru.** Une commande est créée
  par le serveur à partir du prix publié (offre × durée × devise), avec code promo
  vérifié côté serveur (validité, plafonds, devise, offres et durées éligibles, nouveaux
  clients, une utilisation par établissement, réservation pendant le paiement, libération
  à l'expiration ou à l'échec).
* Une commande n'est payée que par : (a) un webhook vérifié — Stripe : signature
  `Stripe-Signature` HMAC-SHA256 avec tolérance de 300 s **puis relecture de la session
  auprès de Stripe** ; PayPal : API officielle `verify-webhook-signature` puis capture ;
  générique : HMAC `X-Nhill-Signature` ; événements dédoublonnés `(prestataire,
  identifiant)` — ou (b) une confirmation manuelle par un administrateur `billing`,
  auditée. Montant et devise sont recontrôlés ; un écart est signalé et n'active rien.
* Changements d'offre calculés par le serveur : passage à une offre supérieure immédiat
  au prorata (même devise), offre inférieure payée et appliquée à l'échéance,
  renouvellement à la fin de la période. Échecs comptés, période de grâce avant lecture
  seule.
* **État réel** : les adaptateurs Stripe et PayPal suivent la documentation publique et
  sont testés **uniquement contre des réponses simulées**. Payoneer : configuration
  seulement (aucune API vérifiée), activation refusée. Aucun paiement réel n'a été
  effectué.

## 8. Secrets

| Secret | Où il vit | Protection |
|---|---|---|
| `NH_SECRET_KEY` (32 octets) | `/etc/nhill/server.env` (0600, utilisateur `nhill`), généré sur l'hôte à l'installation | Dérive les clés des codes, TOTP, empreintes d'essai et **secrets des prestataires** ; à conserver dans un coffre, séparément des sauvegardes |
| Mots de passe des rôles PostgreSQL | `server.env`, générés aléatoirement par l'installation | Jamais affichés (sauf le compte de sauvegarde, une fois) |
| Secrets des prestataires (clés API, secrets de webhook) | Base, colonnes chiffrées (`secretbox`, clé dérivée « NHPAYSEC ») | API en **écriture seule** : jamais renvoyés, ni à la console ; l'audit ne note que les noms des champs modifiés |
| Mot de passe d'administration de la base (installation) | Utilisé une fois en mémoire | Jamais enregistré |

C'est le « mécanisme sécurisé équivalent » retenu à la place d'un coffre externe. Pour
un coffre dédié (Vault, KMS), il suffit de fournir `NH_SECRET_KEY` par l'environnement
du service (systemd `LoadCredential=` ou équivalent) : non intégré à ce jour.

## 9. Installation par la console (mode installation)

* Sans base configurée, le serveur démarre en **mode installation** : seules
  `/health` et `/api/v1/setup/*` répondent. Un **jeton à usage unique** (60 minutes) est
  écrit dans le journal du service : seule une personne ayant accès au serveur le lit.
* L'assistant de Nhill Console envoie les paramètres PostgreSQL ou Supabase (pooler de
  session, port 5432) ; le serveur crée `nhill_owner`, `nhill_app` et `nhill_backup`
  avec des mots de passe aléatoires, applique les migrations, **vérifie** isolation et
  privilèges, écrit la configuration (fichier 0600, écriture atomique) et redémarre en
  mode normal. En cas d'échec de vérification, rien n'est enregistré.
* Le premier administrateur de la console reçoit un jeton de premier accès (30 min) ;
  il définit son mot de passe et son TOTP. Aucun mot de passe administrateur en dur.

## 10. API et validation

* Corps JSON validés champ par champ (types, longueurs, énumérations, montants en
  chaînes décimales exactes, UUID) ; erreurs codées sans détail interne.
* Corps limités à 6 Mo ; fichiers vérifiés par leur contenu (PNG/JPEG/PDF), dimensions
  bornées ; exports CSV neutralisés contre l'injection de formules.
* Écritures protégées par clés d'idempotence ; aucune file de rejeu hors ligne.
* Les erreurs 5xx sont regroupées dans « Erreurs techniques » de la console ; les
  rapports envoyés par les applications sont **masqués** avant stockage (e-mails, suites
  de chiffres type carte ou téléphone, jetons longs) — test
  `Support.ErrorReportsMaskPersonalData`.

## 11. Transport et hôte

* Applications : HTTPS obligatoire (HTTP seulement vers la machine locale ou
  l'émulateur), certificats vérifiés, redirections vers un schéma moins sûr refusées ;
  Android : `usesCleartextTraffic=false`, `allowBackup=false`.
* Serveur sur la boucle locale derrière Caddy (TLS, HSTS, `nosniff`, `DENY`).
* Service systemd durci : `NoNewPrivileges`, `ProtectSystem=strict` (seul `/etc/nhill`
  est inscriptible, pour l'installation), `PrivateTmp`, `PrivateDevices`, filtre
  d'appels système, aucune capacité, `UMask=0077`.

## 12. Sauvegardes

Sauvegarde quotidienne chiffrée avec age (clé privée hors du serveur) par le rôle
`nhill_backup` : lecture seule, seul rôle qui contourne la RLS, à restreindre dans
`pg_hba.conf`. Restauration testée dans une base neuve (RLS préservée). Voir
`sauvegarde-restauration.md`. Le paquet ne supprime jamais `/etc/nhill` ni les
sauvegardes, même en purge.

## 13. Traçabilité

Journal d'audit en ajout seul (le rôle applicatif ne peut ni modifier ni supprimer).
Chaque action sensible est enregistrée : ventes, retours, annulations, autorisations de
superviseur (accordées et refusées), mouvements d'espèces, clôtures et écarts,
configuration du POS, actions de la console sur les abonnements (motif obligatoire),
prestataires (noms des champs), codes promo, demandes d'assistance, consultations des
statistiques d'un établissement (visibles par l'établissement lui-même).

> Un journal nominatif des actions des employés est un traitement de données
> personnelles : l'établissement doit en informer son personnel (à valider avec un
> juriste selon le pays).

## 14. Résultat de l'audit

| Point contrôlé | Résultat |
|---|---|
| `service_role` / clés Supabase dans les applications | Absentes (recherche de `service_role`, `eyJhbGci`, `sb_secret`, `sb_publishable` dans le code : seules occurrences = test qui crée un rôle `service_role` factice pour vérifier qu'il n'a aucun droit, et l'avertissement de l'assistant d'installation) ; jamais utilisées par le serveur |
| Conseiller sécurité Supabase (projet de production) | **0 alerte** après les migrations 0001–0010 |
| RLS forcée sur les tables métier | 30 / 30 (vérification de l'installation ; 0 table POS sans RLS forcée sur Supabase) |
| Requêtes sur les tables de plateforme | Relues ; 1 durcie |
| Accès par identifiant d'un autre établissement | Refusé (tests § 4.3) |
| Détails de l'abonnement réservés aux administrateurs | Corrigé (API + interface), testé |
| Données personnelles dans les rapports d'erreur | Corrigé (masquage), testé |
| Secrets des prestataires | Chiffrés, écriture seule, testés |
| Webhooks | Signature, horodatage, dédoublonnage, relecture chez le prestataire, montant |
| Mots de passe en dur, secrets dans le dépôt | Aucun (`config/*.example` sans valeur secrète) |

### Risques résiduels et actions recommandées

1. **Pas d'audit externe ni de test d'intrusion** : à commander avant la mise en service.
2. Tables de plateforme sans RLS (§ 4.2) : maintenir la revue et les tests d'isolation.
3. Pas de double authentification pour les utilisateurs des établissements (console
   seulement).
4. Paiements : adaptateurs non éprouvés en conditions réelles ; faire un paiement de test
   avec un compte du prestataire avant d'activer le mode production.
5. `NH_SECRET_KEY` sur le disque du serveur : envisager un coffre (§ 8).
6. Pas de WAF ni de supervision des intrusions ; pas de chiffrement applicatif des
   colonnes métier (protection par RLS, TLS, chiffrement du disque et des sauvegardes).
