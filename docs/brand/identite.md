# Nhill — Identité visuelle

## 1. Nom

**Nhill** rend hommage à **Napoleon Hill** (*Réfléchissez et devenez riche*) : la
croissance, viser plus haut, atteindre son objectif. Le mot évoque aussi *hill*, la
colline que l'on gravit. Il s'écrit « Nhill » (N majuscule), sans article.

Trois produits, toujours nommés en entier : **Nhill Business**, **Nhill POS**,
**Nhill Console**.

> **Disponibilité du nom : non établie.** Aucune recherche d'antériorité de marque n'a
> été faite (OAPI, EUIPO, INPI, USPTO, registre congolais), ni de vérification des noms
> de domaine ou des magasins d'applications. Il existe notamment une localité « Nhill »
> en Australie (Victoria). **Faire réaliser une recherche d'antériorité par un conseil
> en propriété industrielle avant tout dépôt ou lancement commercial.** Le nom est
> centralisé (`common/include/nhill/version.hpp`, traductions, ressources générées) pour
> pouvoir être changé.

## 2. Logo

Le symbole est un **monogramme « N »** dessiné d'un seul trait aux angles arrondis, dont
le jambage droit monte plus haut que le gauche — une ascension — et porte à son sommet
un **fanion doré** : le sommet atteint, l'objectif réalisé. Il reste lisible de 16 px à
l'impression.

| Fichier (`assets/brand/`) | Usage |
|---|---|
| `nhill-logo.svg` / `.png` | Logo principal (symbole + « Nhill »), fonds clairs |
| `nhill-logo-reverse.svg` / `.png` | Fonds sombres, barre latérale |
| `nhill-logo-mono.svg` / `.png` | Monochrome (impression noir et blanc, tampons) |
| `nhill-logo-business.svg`, `-console.svg`, `-pos.svg` (+ `-reverse`) | Logo de chaque application (« Nhill » + nom du produit) |
| `nhill-logo-document.svg` | Version discrète pour les documents de Nhill (les PDF des établissements portent seulement la mention « Généré par Nhill ») |
| `nhill-mark.svg` | Symbole seul |
| `nhill-icon-business.svg`, `-console.svg`, `-pos.svg`, `assets/icons/*` | Icônes d'application 16 → 1024 px, `.ico` Windows, icônes adaptatives Android (Nhill Business) |

Les trois icônes partagent le symbole sur un fond propre à chaque produit, pour les
distinguer dans une barre des tâches. Tout est généré par `scripts/brand/build_brand.py`
(source unique, mot-symbole vectorisé depuis IBM Plex Sans SemiBold ; ne pas retoucher
les PNG à la main).

### Règles d'usage

* Zone de protection : la hauteur du fanion tout autour du logo.
* Taille minimale : 24 px de haut à l'écran, 8 mm à l'impression ; en dessous, symbole
  seul.
* Interdits : déformer, changer les couleurs hors palette, ajouter ombres ou contours,
  séparer le fanion du N, poser le logo principal sur un fond chargé (utiliser la
  version inverse ou monochrome).
* Le logo **d'un établissement** sur ses factures et tickets n'est jamais déformé : le
  moteur PDF conserve son rapport largeur/hauteur.

## 3. Couleurs

| Nom | Hex | Usage | Contraste (WCAG, calculé) |
|---|---|---|---|
| Nuit 900 | `#0E2236` | Barre latérale, fonds sombres | blanc dessus 16,1:1 |
| **Nuit 700** | `#173F5F` | Couleur primaire (« bleu Nhill ») : boutons, titres, symbole | sur blanc 11,0:1, sur papier 10,1:1 (AAA) |
| Nuit 500 | `#2D6A8F` | États actifs, graphiques | sur blanc 5,9:1 (AA) |
| Nuit 300 | `#9BC2DA` | Accents en mode sombre | — |
| **Or 600** | `#B7862A` | Fanion du logo, mises en avant graphiques | sur blanc 3,3:1 : **éléments graphiques et grand texte seulement** |
| Or 400 | `#D9A945` | Accent en mode sombre | — |
| Or texte | `#8E6519` | Texte d'accent dans l'interface | sur blanc 5,2:1 (AA) |
| Encre 900 | `#14181F` | Texte principal, mot-symbole | sur papier 16,3:1 |
| Encre 600 | `#56606B` | Texte secondaire, nom du produit dans le logo | sur papier 5,9:1 (AA) |
| Papier | `#F7F5F0` | Fond des applications et des documents | — |

Les couleurs d'état (succès, avertissement, danger, information) et les variantes
sombres sont définies dans `client/qml/Nhill/Ui/Theme.qml`.

## 4. Typographie

* **IBM Plex Sans** (Regular, Medium, SemiBold, Bold) : interfaces et documents.
* **IBM Plex Mono** : numéros de documents et de tickets, codes, références.
* Licence SIL Open Font License 1.1 ; polices intégrées aux applications et aux PDF.
* Hiérarchie écran : titre de page 26 px SemiBold, section 16 px, texte 14 px, libellés
  12,5 px, légendes 11 px.

## 5. Ton

Clair, factuel, rassurant : on dit ce qui s'est passé et ce que l'utilisateur peut faire
(« Connexion perdue : appuyez de nouveau sur Valider, la vente ne sera pas enregistrée
deux fois »). Pas de promesse non vérifiable (« certifié », « homologué », « 100 %
conforme »).
