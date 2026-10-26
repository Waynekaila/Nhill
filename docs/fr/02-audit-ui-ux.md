# Audit UI/UX — Nhill Business, Nhill POS, Nhill Console

> **Date :** 26 octobre 2026 · **Version auditée :** 0.1.0 · **Corrections :** 2.0.0
> **Sources de preuve :**
> 1. les 50 captures réelles de la 0.1.0 (`docs/captures/`, parcours complets avec un serveur
>    de démonstration) ;
> 2. l'exécution des binaires 0.1.0 livrés sous Xvfb à 1366×768 et 1024×600
>    (`docs/assets/audit/v010-*.png`) ;
> 3. la lecture des 95 fichiers QML et des composants `Nhill.Ui` ;
> 4. des mesures de contraste WCAG 2.1 sur les jetons de couleur du thème.
>
> **Limite :** les corrections 2.0.0 sont écrites dans le code et contrôlées statiquement
> (`scripts/qa/qml-check.py` : 0 erreur), mais **n'ont pas pu être exécutées** ici (pas de Qt de
> développement). Leurs captures seront produites par la CI (`scripts/qa/ui-screenshots.sh`,
> 6 tailles d'écran). Le statut de chaque point l'indique.

## 1. Synthèse

L'identité visuelle de Nhill est soignée et cohérente (bleu nuit, or, papier chaud, IBM Plex,
icônes au trait, cartes, puces d'état). Les parcours métier sont complets et compréhensibles.
Les défauts relèvent surtout de **l'adaptation à l'écran réel de l'utilisateur** et de
**l'accessibilité** :

* les trois fenêtres s'ouvrent à une **taille fixe** (1280×800, 1280×820, 1320×840), plus
  grande que la zone utile des ordinateurs portables 1366×768 très répandus, et sortent
  littéralement de l'écran sur un 1024×600 ;
* les **tableaux** répartissent les colonnes en proportions sans largeur minimale : sur une
  fenêtre moyenne, dates, numéros et montants sont coupés (« 26/10/2… », « AV-2026-0… ») ;
* plusieurs couleurs de texte et de bordure **n'atteignent pas les seuils de contraste**
  WCAG ; les tailles 11 px et 12,5 px sont fréquentes ;
* l'état **désactivé** des boutons (texte blanc sur fond bleu à 45 %) est peu lisible et
  n'explique pas pourquoi l'action est indisponible.

## 2. Méthode et barème

Chaque écran a été examiné selon la liste demandée (tailles, redimensionnement, débordements,
formulaires, boutons, textes tronqués, navigation, cohérence, tableaux, dialogues, mise à
l'échelle, clavier/focus/contraste, états de chargement/erreur/vide, performances).

* **Gravité :** Bloquante (empêche une tâche) · Haute (gêne forte ou risque d'erreur) ·
  Moyenne (gêne) · Faible (finition).
* **Statut :** ✅ corrigé dans le code 2.0.0 (non exécuté ici) · ⏳ à faire · 🔍 à vérifier sur
  les captures de la CI.

## 3. Problèmes transversaux

| # | Problème | Preuve | Gravité | Correction 2.0.0 | Statut |
|---|---|---|---|---|---|
| X1 | Fenêtres de taille fixe plus grandes que l'écran ; contenu coupé en bas et décentré sur 1024×600 ; sur 1366×768, la fenêtre (800 px) dépasse la zone utile (≈ 728 px avec la barre des tâches) | `v010-pos-ecran-1024x600.png`, `v010-pos-ecran-1366x768.png` ; `PosMain.qml`, `Main.qml`, `ConsoleMain.qml` | **Haute** | Taille initiale = min(taille idéale, 94 % × 92 % de l'écran), centrage, ouverture maximisée si l'écran fait moins de 1100×700 ; minimums bornés à la taille de l'écran | ✅ 🔍 |
| X2 | Minimum POS de 1000×640 : inutilisable sur les écrans 1024×600 des petites caisses | `PosMain.qml` | Haute | Minimum 860×560 ; écran de vente adaptatif (voir P2) | ✅ 🔍 |
| X3 | Colonnes de tableau sans largeur minimale : textes coupés sans moyen de les lire | captures `business-14`, `business-15`, `console-a05` | Haute | `DataTable` : largeur minimale par colonne (ou 72 px par unité), **défilement horizontal**, infobulle sur toute cellule coupée | ✅ 🔍 |
| X4 | Tableaux non utilisables au clavier | `DataTable.qml` (lignes non focalisables) | Moyenne | Tab entre dans le tableau, flèches/Page préc./suiv., Entrée ou Espace ouvre ; ligne courante encadrée | ✅ |
| X5 | Contraste insuffisant : texte « faint » #8A9097 sur #F7F5F0 = **2,96:1** (placeholders, aides, en-têtes secondaires) ; bordures de champs #CFC9BD sur blanc = **1,65:1** (WCAG 1.4.11 exige 3:1) ; thème sombre « faint » 3,7:1 | calcul WCAG (`Theme.qml`) | Haute (accessibilité) | `textFaint` #62686F (≥ 4,6:1 sur toutes les surfaces), `inputBorder` #8A8376 (3,45–3,76:1), sombre #8392A2 (≥ 5:1) et #5D7389 ; vert « succès » 5,27:1 | ✅ |
| X6 | Petites tailles de police (11 px, 12,5 px) pour les en-têtes de tableau, aides, sous-titres | `Theme.qml` | Moyenne | 12 px et 13 px | ✅ 🔍 |
| X7 | Bouton désactivé : texte blanc sur bleu à 45 % d'opacité, peu lisible ; aucune indication de la raison | captures `business-03`, `business-04`, `pos-p01` | Moyenne | Couleurs désactivées explicites (`disabledBg`/`disabledText`) ; message d'erreur sous le champ e-mail invalide ; Entrée dans le mot de passe ne déclenche l'envoi que si les champs sont remplis | ✅ |
| X8 | Anneau de focus invisible sur les boutons principaux (bordure #2D6A8F sur bleu #173F5F) | `FbButton.qml` | Moyenne | Anneau de 2 px **à l'extérieur** du bouton, visible sur tous les styles | ✅ |
| X9 | Noms accessibles : les champs annoncent leur *placeholder* (souvent vide) au lieu de leur libellé ; zones de texte qui capturent Tab | `FbTextField.qml`, `Field.qml`, `FbTextArea.qml` | Moyenne | Le libellé du `Field` devient le nom accessible ; Tab/Maj+Tab quittent les zones de texte ; boutons icônes nommés par leur infobulle | ✅ |
| X10 | Mots de passe : aucun moyen de vérifier la saisie | tous les écrans de connexion/création | Moyenne | Bouton « Afficher / Masquer le mot de passe » automatique sur tout champ mot de passe | ✅ |
| X11 | Menus latéraux qui débordent sur les fenêtres basses ; libellés coupés (« Paramètres de la plateform… », « Événements des prestataire… ») sans infobulle | captures `console-a*`, `Shell.qml` | Moyenne | Menu Business défilant ; infobulles sur les libellés coupés (Business et Console) ; focus clavier visible dans la console | ✅ |
| X12 | Champs de formulaire dont la largeur dépend du texte saisi : grilles déséquilibrées (« Raison sociale » réduite à « IPLE DÉMO SARL ») | capture `business-16` | Moyenne | Largeur préférée stable des champs (`Field`) : colonnes égales | ✅ 🔍 |
| X13 | Listes déroulantes, calendrier et notifications pouvant sortir de la fenêtre | `FbComboBox`, `DateField`, `NotificationsButton` | Moyenne | Marges de fenêtre sur ces popups (repositionnement automatique) | ✅ |
| X14 | Messages d'erreur fugaces (4,2 s) : un message d'échec peut disparaître avant lecture | `Notifier.qml` | Moyenne | Erreurs et avertissements 8 s, au plus 4 messages, cliquer pour fermer, rôle « alerte » pour les lecteurs d'écran | ✅ |
| X15 | Informations par la couleur seule ? Non : les puces d'état portent toujours un texte (« Payé », « Impayé ») | captures | — | Conforme | — |
| X16 | Animations | Transitions de 100–140 ms en opacité seulement | — | Conformes (sobres, sans coût notable) | — |

## 4. Nhill Business

| # | Écran | Problème | Gravité | Correction / recommandation | Statut |
|---|---|---|---|---|---|
| B1 | Accueil / connexion | Bouton « Se connecter » désactivé sans explication ; e-mail invalide signalé par une bordure rouge seule | Moyenne | Message « Saisissez une adresse e-mail valide (par exemple nom@entreprise.cd) » sous le champ | ✅ |
| B2 | Création / adhésion | Règle de mot de passe 10 caractères | — | 8 caractères (exigence), aide mise à jour, afficher/masquer | ✅ |
| B3 | Barre supérieure | Champ de recherche de largeur fixe 420 px | Faible | Largeur 160–420 px selon l'espace | ✅ |
| B4 | Barre latérale | Nom de l'établissement et de l'utilisateur coupés (« Quincaillerie Exem… ») | Faible | Infobulle avec le nom complet | ✅ |
| B5 | Paiements | Colonne « Date » répétant le moyen de paiement (« 26/10/2026 · Esp… ») alors qu'une colonne « Moyen » existe | Moyenne | Date seule ; largeurs minimales (date, document, montant) | ✅ |
| B6 | Achats | Colonne « Référence » répétant la date (coupée) | Faible | Référence seule ; date dans sa colonne ; sous-titre mobile = date | ✅ |
| B7 | Rapports | 11 colonnes : tout est coupé | Haute | Largeurs minimales par type de donnée (date 104 px, montant 128 px, numéro 150 px) + défilement horizontal | ✅ 🔍 |
| B8 | Journal d'activité | Actions brutes affichées (« action.pos.session_closed ») et détails techniques (« by_owner : oui · cash_counted : 25656… », UUID) | Haute (traçabilité lisible) | 51 libellés FR/EN ajoutés au serveur (caisse, assistance, console) ; UUID et montants en unités mineures masqués du résumé ; filtres « Point de vente » et « Demandes d'assistance » | ✅ |
| B9 | Nouvelle facture | Ligne de saisie dense (6 champs + taxe sur une 2e ligne) ; libellé « Ligne incomplète » discret | Moyenne | Recommandation : éditeur de ligne en carte sur fenêtre étroite ; non modifié (risque de régression sans exécution possible) | ⏳ |
| B10 | Paramètres | 7 onglets en ligne : débordement possible en fenêtre étroite | Faible | Recommandation : onglets défilants (`TabStrip`) | ⏳ |
| B11 | Tableau de bord | Indicateurs lisibles ; libellés de légende à 11 px | Faible | Tailles relevées (X6) | ✅ |
| B12 | États vides / chargement | Présents et cohérents (`EmptyState`, indicateurs) | — | Conforme | — |

## 5. Nhill POS

| # | Écran | Problème | Gravité | Correction | Statut |
|---|---|---|---|---|---|
| P1 | Connexion | Fenêtre plus grande que l'écran, formulaire décentré et coupé sur 1024×600 | Haute | X1/X2 | ✅ 🔍 |
| P2 | Vente | Panneau ticket fixe de 420 px ; barre supérieure qui déborde sous 1200 px | Haute | Panneau 340–480 px (38 % de la largeur) ; nom de l'utilisateur masqué et boutons Aide / Déconnexion en icônes (avec infobulle) sous 1200 px | ✅ 🔍 |
| P3 | Ticket | Noms d'articles coupés (« Ciment 50 kg … », « Livraison (dé… ») | Moyenne | Noms sur deux lignes, hauteur de ligne adaptée | ✅ |
| P4 | Ticket | Quantité modifiée par des chevrons ‹ › (sens ambigu) | Moyenne | Boutons « − » et « + », infobulles « Diminuer la quantité » / « Retirer la ligne » / « Augmenter la quantité », quantité annoncée aux lecteurs d'écran | ✅ |
| P5 | Vente | Aucun raccourci clavier pour une caisse | Moyenne | **F2** : retour au champ de recherche/scan ; **F9** : encaisser (si le ticket n'est pas vide) ; rappel dans le champ et l'infobulle | ✅ |
| P6 | Session | En-tête avec 5 boutons + titre : débordement sous ≈ 1180 px ; 4 indicateurs sur une ligne | Moyenne | Boutons secondaires en icônes, titre réduit et abrégé si nécessaire ; indicateurs sur 2 colonnes sous 1000 px | ✅ 🔍 |
| P7 | Session | Colonne « Heure » affichant date et heure | Faible | Colonne « Date et heure », largeur minimale | ✅ |
| P8 | Ticket (retour) | **TypeError** si « Retourner » est déclenché avant le chargement du ticket (rapportée par la console en 0.1.0) | Moyenne | Garde ; aucune action tant que le détail n'est pas chargé | ✅ |
| P9 | Clôture | Clôture sans saisie des espèces : le serveur refuse (`cash_count_required`) avec un message générique | Moyenne | Contrôle avant envoi + message explicite « Saisissez les espèces comptées dans le tiroir (0 si le tiroir est vide) » | ✅ |
| P10 | Dialogue de retour | Placeholder « Qté à r… » coupé | Faible | Recommandation : libellé de colonne au-dessus des champs | ⏳ |

## 6. Nhill Console

| # | Écran | Problème | Gravité | Correction | Statut |
|---|---|---|---|---|---|
| C1 | Toutes | Fenêtre 1320×840 fixe | Haute | X1 | ✅ 🔍 |
| C2 | Menu | 15 entrées, libellés coupés, pas d'anneau de focus | Moyenne | X11 | ✅ |
| C3 | Connexion | Code TOTP obligatoire (A2F) | — | Conservé : comptes à privilèges de plate-forme (voir audit sécurité) ; mot de passe minimum 8 | ✅ |
| C4 | Demandes, erreurs, audit | Colonnes coupées (« Quincaillerie Exemple … », « qrc:/Nhill/Pos/pages/… ») | Moyenne | X3 (infobulles, défilement) | ✅ 🔍 |
| C5 | Installation du serveur | Assistant en 4 étapes clair ; textes d'aide à 11 px | Faible | X6 | ✅ |

## 7. Système de design (2.0.0)

| Élément | Règle |
|---|---|
| Couleurs | Jetons `Theme.qml` : surfaces (`bg`, `surface`, `surfaceAlt`, `sunken`), texte (`text` 16:1, `textMuted` ≥ 5,8:1, `textFaint` ≥ 4,6:1), marque (`primary` #173F5F, `accent` #8E6519), états (`success`, `warning`, `danger`, `info` + versions *soft*), `inputBorder` ≥ 3:1, `disabledBg/Text`, `focus` |
| Typographie | IBM Plex Sans / Mono embarquées ; échelle 12 / 13 / 14 / 16 / 20 / 26 px ; graisses 400/500/600/700 |
| Espacements | 4 / 8 / 12 / 16 / 24 / 32 px (`s1`…`s6`) ; rayons 8 et 12 px |
| Contrôles | Hauteur 38 px (44 px en mode compact/tactile) ; boutons primaire / secondaire / fantôme / danger ; icônes 18–20 px |
| Focus | Anneau `focus` de 2 px, extérieur pour les boutons, bordure pour les champs et lignes de tableau |
| États | Désactivé explicite ; occupé (indicateur dans le bouton) ; vide (`EmptyState` avec action) ; erreur (texte sous le champ + message 8 s) |
| Tableaux | Largeur minimale par colonne, défilement horizontal, infobulles, clavier, cartes sur téléphone |
| Fenêtres | Taille ajustée à l'écran, centrée, minimums ≤ écran, plein écran sous 1100×700 (1024×680 pour Business) |
| Dialogues | Largeur ≤ fenêtre − 48 px, hauteur ≤ fenêtre − 48 px, contenu défilant, plein écran en mode téléphone |
| Langue | Toute chaîne visible passe par `qsTr` et existe en français (contrôle automatique) |

## 8. Ce qui reste à faire (priorité)

1. 🔍 Examiner les captures produites par la CI aux 6 tailles d'écran et corriger les écarts.
2. ⏳ Éditeur de lignes de facture en carte sur fenêtre étroite (B9).
3. ⏳ Onglets défilants dans les paramètres (B10) ; libellés de colonnes dans le dialogue de retour (P10).
4. ⏳ Test avec un lecteur d'écran (Orca sous Linux, NVDA sous Windows) et avec un mise à
   l'échelle système de 125 % et 150 %.
5. ⏳ Séances d'observation avec 3 à 5 commerçants (programme pilote) : temps pour émettre
   une facture, encaisser une vente, clôturer une caisse.
