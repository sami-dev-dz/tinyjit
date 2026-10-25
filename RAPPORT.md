# TinyJIT — rapport d'état du projet

**Date :** 25 octobre 2026
**Dépôt :** <https://github.com/sami-dev-dz/tinyjit>
**Branche :** `main`

## 1. En bref

TinyJIT est un projet de petit langage dynamique avec deux modes d'exécution :

1. **interprétation** : le programme est traduit en bytecode, puis exécuté par
   une machine virtuelle à pile ;
2. **compilation à la volée (JIT)** : quand une fonction est appelée souvent,
   elle est compilée en code natif avec **libgccjit**, puis les appels suivants
   utilisent directement ce code machine.

Le dépôt contient aujourd'hui un **socle fonctionnel** : le format de bytecode,
la VM et une démonstration qui calcule la somme des entiers de 1 à `n`. En
revanche, **le front-end et le JIT ne sont pas encore écrits** : `src/lexer.py`,
`src/parser.py` et `src/jit.py` sont vides. Il n'y a donc pas encore de langage
source complet ni de compilation native.

> Phrase à retenir pour la soutenance : « Le socle bytecode + VM est
> fonctionnel ; le front-end et le JIT sont les prochaines étapes. »

## 2. Objectif du projet

L'objectif est de montrer, sur un exemple réduit, la chaîne complète d'un
langage compilé :

```text
source .src
   │
   ▼
lexer ──► parser ──► AST ──► compilateur ──► BytecodeFunction
                                                    │
                                    ┌───────────────┴───────────────┐
                                    ▼                               ▼
                               VM (interprète)                 JIT (compile)
                                                          code natif libgccjit
```

Le JIT ne compile pas tout le programme au démarrage. Il attend qu'une fonction
devienne « chaude », c'est-à-dire qu'elle ait été appelée un certain nombre de
fois. Ce compromis évite de payer le coût de compilation pour du code exécuté
une seule fois.

## 3. État actuel du dépôt

### 3.1 Ce qui est implémenté

| Fichier | Contenu | État |
| --- | --- | --- |
| `src/bytecode.py` | `OpCode`, `Instruction`, `BytecodeFunction` ; pool de constantes ; `emit()` ; `dump()` | ✅ |
| `src/vm.py` | Boucle *fetch-decode-execute* ; pile ; variables locales ; arithmétique ; comparaisons ; sauts ; `OP_RET` | ✅ |
| `src/main.py` | Construction manuelle du bytecode de `sum_to(n)` ; exécution et affichage du résultat | ✅ |
| `src/lexer.py` | Analyse lexicale | ⬜ vide |
| `src/parser.py` | Analyse syntaxique / AST | ⬜ vide |
| `src/jit.py` | Compilation native | ⬜ vide |
| `tests/add.src` | Programme de test « addition » | ⬜ vide |
| `tests/fibonacci.src` | Programme de test « fibonacci » | ⬜ vide |
| `tests/loop.src` | Programme de test « boucle » | ⬜ vide |

Il n'existe pas encore de `compiler.py` ni de `ast.py` dans la version Python.
Le bytecode est pour l'instant produit à la main dans `main.py`.

### 3.2 Ce qui n'existe pas encore

- Un vrai langage source : aucun fichier `.src` n'est lisible.
- Les tokens, l'AST et la génération de bytecode depuis une syntaxe.
- `OP_CALL` dans la VM (l'opcode est déclaré mais la VM ne le traite pas).
- Le JIT, les seuils de chauffe et le passage bytecode → code natif.
- Les tests automatiques et les benchmarks.
- La gestion des erreurs de compilation avec numéro de ligne.

## 4. Le bytecode

Le bytecode est une suite d'instructions pour une **machine à pile** :

- les opérandes sont empilées et dépilées ;
- les variables locales sont stockées dans un tableau `locals` indexé par un
  entier ;
- les sauts utilisent des **index absolus** dans le tableau d'instructions ;
- les constantes entières sont stockées dans un pool propre à chaque fonction.

### 4.1 Jeu d'instructions

Il y a 13 opcodes dans `src/bytecode.py` :

| Opcode | Effet | Argument |
| --- | --- | --- |
| `OP_CONST` | empile `constants[arg]` | index dans le pool |
| `OP_LOAD` | empile `locals[arg]` | index de variable locale |
| `OP_STORE` | `locals[arg] = pile.pop()` | index de variable locale |
| `OP_ADD` | dépile `b`, `a`, empile `a + b` | — |
| `OP_SUB` | dépile `b`, `a`, empile `a - b` | — |
| `OP_MUL` | dépile `b`, `a`, empile `a * b` | — |
| `OP_DIV` | dépile `b`, `a`, empile `a // b` | — |
| `OP_LT` | empile `1` si `a < b`, sinon `0` | — |
| `OP_EQ` | empile `1` si `a == b`, sinon `0` | — |
| `OP_JUMP` | `ip = arg` | index cible |
| `OP_JUMP_IF_FALSE` | dépile la condition ; si elle vaut `0`, `ip = arg` | index cible |
| `OP_CALL` | appel de fonction | nombre d'arguments (non implémenté dans la VM) |
| `OP_RET` | dépile et retourne la valeur | — |

Remarques :

- `OP_DIV` correspond à la division entière **plancher** de Python (`//`), pas
  à la division C qui tronque vers zéro. Cette différence devra être traitée
  dans le JIT.
- Les comparaisons produisent des entiers `0`/`1` ; il n'y a pas de type
  booléen séparé.
- Le bytecode ne gère que des entiers. Les flottants, chaînes, tableaux et
  objets ne font pas partie du prototype actuel.

### 4.2 `BytecodeFunction`

Chaque fonction compilée est représentée par un objet `BytecodeFunction` :

| Champ | Rôle |
| --- | --- |
| `name` | nom de la fonction |
| `num_args` | nombre d'arguments attendus |
| `num_locals` | nombre total de variables locales (arguments inclus) |
| `constants` | pool de constantes entières |
| `code` | liste d'instructions `Instruction(op, arg)` |
| `call_count` | nombre d'appels, prévu pour détecter les fonctions chaudes |
| `native_fn` | pointeur vers le code natif une fois la fonction compilée |

Les deux derniers champs (`call_count`, `native_fn`) sont déjà présents dans la
structure : ils préparent le JIT, même si `jit.py` est encore vide.

## 5. La machine virtuelle

La VM de `src/vm.py` est volontairement simple :

```python
self.stack = []          # pile d'opérandes
self.locals = []         # variables locales
ip = 0                   # instruction pointer
while ip < len(fn.code):
    inst = fn.code[ip]
    ip += 1
    # dispatch sur inst.op
```

Au démarrage :

```python
self.locals = list(args) + [0] * (fn.num_locals - len(args))
```

Les arguments occupent donc les premiers slots, et les autres variables locales
sont initialisées à `0`.

### 5.1 Exemple : `sum_to(n)`

La fonction construite dans `src/main.py` calcule :

```text
acc = 0
while 0 < n:
    acc = acc + n
    n = n - 1
return acc
```

Bytecode produit (17 instructions) :

```text
0000: OP_CONST           0     # constante 0
0001: OP_STORE           1     # acc = 0
0002: OP_CONST           0     # début de boucle
0003: OP_LOAD            0     # n
0004: OP_LT                    # 0 < n ?
0005: OP_JUMP_IF_FALSE   15    # si faux, sortir vers 0015
0006: OP_LOAD            1     # acc
0007: OP_LOAD            0     # n
0008: OP_ADD                   # acc + n
0009: OP_STORE           1     # acc = ...
0010: OP_LOAD            0     # n
0011: OP_CONST           1     # 1
0012: OP_SUB                   # n - 1
0013: OP_STORE           0     # n = ...
0014: OP_JUMP            2     # retour au début de la boucle
0015: OP_LOAD            1     # acc
0016: OP_RET
```

L'instruction `0005` est d'abord émise avec l'argument `0`, puis **retouchée**
(*backpatch*) une fois la cible connue :

```python
jump_exit = fn.emit(OpCode.OP_JUMP_IF_FALSE, 0)
...
fn.code[jump_exit].arg = len(fn.code)   # 15
```

Pour `n = 10`, la VM retourne `55`. Pour `n = 3`, le déroulement est :

| Tour | `n` | `acc` avant | `acc` après |
| --- | --- | --- | --- |
| 1 | 3 | 0 | 3 |
| 2 | 2 | 3 | 5 |
| 3 | 1 | 5 | 6 |
| fin | 0 | 6 | 6 |

## 6. Ce qui reste à faire

### 6.1 Front-end

1. **`lexer.py`** : reconnaître les entiers, identifiants, mots-clés
   (`if`, `while`, `def`, `return`, ...), opérateurs (`+ - * / < ==`) et
   ponctuations (`( ) { } , ;`). Conserver la ligne et la colonne pour les
   messages d'erreur.
2. **`parser.py`** : analyse descendante récursive ; produire un AST
   (déclarations de fonctions, appels, affectations, `if`, `while`, `return`,
   expressions).
3. **Compilateur** : parcourir l'AST et émettre le bytecode. Il faudra gérer
   les portées, l'allocation des slots locaux, les sauts et le *backpatch*.

### 6.2 VM

- Implémenter `OP_CALL` et les **frames d'appel**. La VM actuelle a une seule
  pile et un seul tableau `locals` par instance : elle ne peut donc pas gérer
  la récursion ni les appels imbriqués.
- Définir proprement les erreurs d'exécution (division par zéro, pile vide,
  argument manquant, etc.).
- Décider de la largeur des entiers (voir § 7.3).

### 6.3 JIT

- Compiler une `BytecodeFunction` en code natif avec libgccjit.
- Détecter les fonctions chaudes avec `call_count`.
- Appeler le code natif depuis la VM, puis mettre en cache le pointeur dans
  `native_fn`.
- Comparer les résultats VM / JIT sur les mêmes entrées.

### 6.4 Tests et outillage

- Remplir `tests/add.src`, `tests/fibonacci.src`, `tests/loop.src`.
- Ajouter un lanceur de tests qui exécute chaque programme sur la VM et, plus
  tard, sur le JIT, puis compare les sorties.
- Ajouter une interface en ligne de commande : `python3 src/main.py fichier.src`.
- Mesurer les performances (temps d'interprétation vs temps JIT, coût de
  compilation, seuil de chauffe).

## 7. Conception prévue du JIT

### 7.1 Principe

Le JIT repose sur libgccjit, la bibliothèque de compilation à la volée de GCC.
Elle permet de construire un graphe de code (fonctions, blocs de base,
expressions), de le compiler en mémoire, puis de récupérer un pointeur vers le
code machine.

Le déclenchement prévu :

```text
appel de fonction
      │
      ▼
call_count += 1
      │
      ├── call_count < SEUIL ──► interpréter normalement
      │
      └── call_count >= SEUIL ──► compiler avec libgccjit
                                       │
                                       ▼
                                 native_fn = pointeur
                                       │
                                       ▼
                              appels suivants : code natif
```

Un seuil typique serait `1000` appels, mais la valeur exacte sera choisie en
mesurant le coût de compilation.

### 7.2 Traduction bytecode → code natif

Les idées directrices :

- **Variables locales** : chaque slot `locals[i]` devient une variable locale
  du code natif (type `int64_t`).
- **Pile d'opérandes** : les valeurs intermédiaires deviennent des temporaires
  SSA produits par libgccjit. Une analyse de la pile permet de savoir combien
  de valeurs sont vivantes à chaque point.
- **Sauts** : chaque index de bytecode qui est la cible d'un saut commence un
  **bloc de base** libgccjit. `OP_JUMP` devient un saut inconditionnel ;
  `OP_JUMP_IF_FALSE` devient un saut conditionnel vers le bloc de la cible et
  un saut vers le bloc suivant.
- **`OP_RET`** : retourne la valeur du sommet de pile.
- **`OP_CALL`** : soit un appel direct vers une autre fonction native, soit un
  rappel (*callback*) vers un dispatcheur de la VM si la fonction appelée n'est
  pas encore compilée.
- **Signature native** : une fonction à `n` arguments devient
  `int64_t f(int64_t a0, ..., int64_t a_{n-1})`. Le pointeur est encapsulé
  côté Python avec `ctypes.CFUNCTYPE`.

### 7.3 Points de vigilance sémantiques

1. **Division entière** : Python `//` arrondit vers moins l'infini, C `/`
   tronque vers zéro. Pour `-7 // 2`, Python donne `-4`, C donne `-3`. Le JIT
   devra soit reproduire la sémantique Python, soit définir la sémantique du
   langage comme une troncature et aligner la VM.
2. **Division par zéro** : la VM lève `ZeroDivisionError`. En code natif, une
   division par zéro provoque un signal `SIGFPE`. Il faut ajouter un test
   explicite et un chemin d'erreur (ou retomber sur l'interpréteur).
3. **Taille des entiers** : Python utilise des entiers de taille arbitraire,
   alors que le code natif utiliserait `int64_t`. Il faut choisir : soit le
   langage est défini sur 64 bits et la VM vérifie les débordements, soit le JIT
   ajoute des contrôles. Le plus simple pour le projet est de définir les
   entiers comme signés 64 bits.
4. **Appels et récursion** : il faut une convention d'appel claire et éviter de
   mélanger les piles de la VM et celles du code natif.
5. **Validation** : pour chaque fonction compilée, comparer les résultats du JIT
   et de la VM sur un ensemble d'entrées, y compris les cas limites.

### 7.4 Pourquoi libgccjit ?

- API C simple, adaptée à un projet pédagogique.
- Disponible avec GCC ; la machine de développement a la version 16.2.1.
- Permet de générer du code natif sans écrire un backend complet.
- Alternative possible : LLVM, plus puissant mais beaucoup plus lourd à mettre
  en place pour un prototype.

Côté Python, deux options :

1. appeler libgccjit directement avec `ctypes` (aucune dépendance externe) ;
2. écrire une petite passerelle en C, compilée avec GCC, puis l'exposer à
   Python via `ctypes`.

La deuxième option est souvent plus lisible, car l'API de libgccjit est
verbeuse.

## 8. Historique du dépôt

Le projet a d'abord été esquissé en C, puis réécrit en Python :

| Commit | Message | Contenu |
| --- | --- | --- |
| `99c7e18` | `chore: initial project structure` | Squelette C : `Makefile`, en-têtes `ast.h`, `parser.h`, `compiler.h`, `jit.h`, `vm.h`, sources vides |
| `0491ba5` | `feat: implement bytecode management` | Implémentation du bytecode en C (`include/bytecode.h`, `src/bytecode.c`) et démo `add(a, b)` |
| `f3a8a30` | `feat: initialisation du bytecode, de la VM et des tests en Python` | Portage en Python : `bytecode.py`, `vm.py`, `main.py` avec `sum_to` ; les autres fichiers deviennent des fichiers Python vides |

Le `Makefile` prévoyait déjà `-lgccjit` en commentaire pour l'étape JIT. Le
passage à Python privilégie la rapidité de prototypage et la lisibilité, au
prix de la performance de la VM (ce qui n'est pas gênant puisque l'objectif du
JIT est justement de produire du code natif).

L'ancien code C est toujours récupérable dans l'historique Git :

```bash
git show 0491ba5:src/bytecode.c
git show 0491ba5:src/main.c
```

## 9. Conclusion

TinyJIT n'est pas encore un JIT complet. C'est aujourd'hui un **interpréteur
de bytecode** avec une architecture pensée pour le JIT :

- le format de bytecode est défini ;
- la VM exécute arithmétique, comparaisons, sauts et retours ;
- les champs `call_count` et `native_fn` préparent la compilation à la volée ;
- la démonstration `sum_to(10) = 55` valide le socle.

La soutenance doit présenter honnêtement cet état : le socle fonctionne, le
front-end et le JIT restent à implémenter. Le plan technique du JIT est déjà
précis (seuil de chauffe, blocs de base, libgccjit, validation différentielle),
ce qui montre que la suite est préparée.
