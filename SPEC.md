# Spec — macOS Calculator 12.0 (com.apple.calculator), mode Basic

Extraite par REA (AX tree, screenshots, scénarios de clics, observe-native-calls)
+ frappes osascript. Fenêtre mesurée : 230×408 px fixe.

## Layout (px, repère fenêtre)

- Fond `#252323`
- Affichage aligné à droite :
  - expression : y≈64–80, ~16–17 px, `#A09F9F`
  - résultat : y≈97–119, ~32 px, `#E4E3E3` (visible uniquement après `=`)
- Boutons : disques 48 px, grille 4×5
  - x = 10, 64, 118, 172 (pas 54, marge 10)
  - y = 133, 187, 241, 295, 349
- Coins fenêtre arrondis (~10 px)
- Barre de titre custom : 3 pastilles trafic (rouge #FF5F57, jaune #FEBC2E, vert #28C840) en haut-gauche, 2 icônes en haut-droite (historique, convertisseur)

## Couleurs boutons

Faces **plates** au centre (profils pixel : pas de dégradé interne, ±1).

| Groupe | Face | Glyphe |
|---|---|---|
| chiffres, `±`, `,` | `#5D5C5C` | blanc |
| `⌫`, `AC`, `%` | `#858584` | blanc |
| `÷`, `×`, `−`, `+`, `=` | `#FF8D00` | blanc |

### Liquid glass (mesuré sur capture Retina 2×)

L'effet "verre" n'est pas un dégradé de face mais un **éclairage de bord** :
bande spéculaire ~1 px collée au bord interne du disque, maximale en
haut-centre, fondue vers les côtés (lobe ~gaussien σ≈45°), + rebond faible en
bas (σ≈40°). Intensité **additive** (préserve la teinte) :

- chiffre : face `(93,92,92)` → rim haut `~130` (+37), bas `~99` (+6)
- `C`/top : face `(134,133,133)` → rim haut `~149` (+15)
- opérateur : face `(255,141,0)` → rim haut `(255,156,0)`, bas `(255,176,0)`

### État pressé (mesuré par CGEvent mouse-down + capture)

- Face éclaircie : chiffre `(93,92,92)` → `(131,128,128)` (~mix 25 % vers blanc)
- Le disque **grossit** légèrement (~+7 % diamètre) pendant le maintien
- Restauration au relâchement

- Pastilles trafic : rouge `#FF5F56`, jaune `#FEBC2E`, **3e grise `#585654`** (zoom désactivé, taille fixe)
- Toolbar top-right : 2 disques `(53,51,51)` ~22 px, centres ~(115,17) et (205,17), icônes claires ~`#DAD9D9` (sidebar + calculatrice)
- Label dynamique : **`C` pendant la construction d'une expression** (saisie
  ou opérateur committé) ; `AC` à l'état vierge **et après `=`** (résultat ou
  Undefined affiché)

## Grille

```
⌫  AC  %  ÷
7  8   9  ×
4  5   6  −
1  2   3  +
±  0   ,  =
```

## Comportement (observé)

- Saisie chiffres : concat ; `005` → `5` ; groupement milliers `9 999 999` (espace fine insécable)
- `,` décimal : une seule par nombre (`5,5,5` → `5,55`)
- Opérateurs en chaîne avec **priorité standard** : `2 + 3 × 4 =` → `14`
- `%` postfixe : seul → ÷100 (`5 %` → `0,05`) ; après `+`/`−` → x % du premier opérande (`200 + 10 %` → `220`) ; après `×`/`÷` → ÷100 (`200 × 10 %` → `20`) ; imbricable
- `=` : évalue ; l'expression reste en ligne du haut, résultat en dessous. `=` répète la dernière opération sur le résultat (`1+2==` → `5` ; `1÷7==` → `0,02040816…`). Un chiffre après `=` démarre une nouvelle expression.
- `AC`/`C` : label `C` pendant la saisie ; 1er appui efface l'entrée courante (`5+9` → `5+`), puis redevient `AC` ; 2e appui remet à `0`. Après `=` (résultat ou Undefined) le label redevient `AC`
- `⌫` : supprime le dernier token (`12+3` → `12+`) ; sur `0` reste `0`
- `±` : négative l'opérande/résultat courant (`5` → `−5`), marche après `=`
- `5 ÷ 0` → **"Undefined"** (grand texte clair)
- `1 ÷ 3` → `0,33333333…` — 8 décimales + `…` si tronqué
- `99999999²` → `9 999 999 800 000 001` — arithmétique exacte
- `−` : `5 − 8` → `−3`
- `+` tout seul sur `0` → `0 +`
- ⌫ disparaît quand l'affichage est `0` (grille dynamique) — vérifié dans l'arbre AX

## Notes

- Locale FR : `,` décimal, ` ` milliers, `Undefined` en anglais même sur système FR
- Séparateur expression : ` ÷ ` espacé (`5 ÷ 0`), mais collé dans le rendu (`5÷0` vu sur capture dense)
- Fenêtre inactive = boutons tous gris (pas d'orange)
