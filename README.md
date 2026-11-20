# TinyJIT

TinyJIT est une machine virtuelle à bytecode avec compilation à la volée, écrite en C11 pour un langage de script minimal, le langage Tiny. Elle a été réalisée pour le module Compilation.

Le programme interprète d'abord le bytecode et compte les appels de chaque fonction. Quand une fonction atteint 1000 appels, elle est compilée en code machine x86-64 avec libgccjit (`-O2`), et les appels suivants passent directement par ce code natif.

Une interface web locale, TinyJIT Studio, permet de regarder le bytecode, l'assembleur généré et les blocs de base. Elle est décrite plus bas.

## Résultats

Banc d'essai : `sum_to(10000)` appelée 5000 fois, soit 50 millions de tours de boucle. Mesures relevées sous Windows (MSYS2 UCRT64, GCC 16.2, `-O2`).

| Mode | Temps | Accélération |
| :--- | ---: | ---: |
| Interpréteur seul | 5 480,3 ms | 1,0 |
| Hybride (seuil de 1000 appels) | 1 814,8 ms | 3,0 |
| Code natif seul | 23,6 ms | 232,2 |

Ces valeurs varient d'un lancement à l'autre. Elles ne portent que sur une boucle d'arithmétique entière sans appel de fonction : ce sont des ordres de grandeur, pas une mesure générale.

## Architecture

```
 source .tiny
      |
      v
 compilateur en une passe (src/compiler.c) --> bytecode
                                                  |
                                                  v
                                   machine virtuelle (src/vm.c)  <---+
                                                  |                  | non
                                   1000 appels atteints ? -----------+
                                                  | oui
                                                  v
                              JIT (src/jit.c, libgccjit) --> code x86-64
```

- **Compilateur.** Analyse lexicale et descente récursive en une seule passe. Il émet le bytecode directement, sans arbre syntaxique, et résout les sauts en avant par backpatching.
- **Machine virtuelle.** Interpréteur à pile. Chaque appel a ses propres tableaux de pile et de variables locales sur la pile C, ce qui permet la récursion. Un compteur d'appels par fonction déclenche le JIT.
- **JIT.** Découpe le bytecode en blocs de base, simule la pile d'évaluation pendant la compilation, puis construit le code avec libgccjit.
- **Appels depuis le code natif.** Un `OP_CALL` dans le code généré passe par l'adresse absolue de `vm_run`, injectée avec `gcc_jit_context_new_call_through_ptr`. Cette méthode évite de dépendre de la résolution des symboles de l'exécutable sous Windows (PE/COFF). Un appel récursif natif repasse donc par `vm_run`, qui redirige vers le code natif.

## Le langage Tiny

Un seul type de données, l'entier signé de 64 bits. Un programme est une suite de fonctions et commence par `main()`.

```c
fn fib(n) {
    if (n < 2) {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

fn main() {
    return fib(20);
}
```

Instructions : `var x = e;`, `x = e;`, `if (e) { ... } else { ... }`, `while (e) { ... }`, `return e;`. Opérateurs : `+ - * / < ==`. Une fonction ne peut appeler qu'elle-même ou une fonction définie plus haut dans le fichier.

Le dossier `examples/` contient cinq programmes et leurs résultats :

| Fichier | Résultat |
| :--- | ---: |
| `fibonacci.tiny` | 6765 |
| `factorial.tiny` | 3 628 800 |
| `sum_to.tiny` | 50 005 000 |
| `sum_of_squares.tiny` | 400 |
| `collatz.tiny` | 111 |

## Jeu d'instructions

La machine manipule des entiers de 64 bits sur une pile d'évaluation. Il y a 13 opcodes.

| Opcode | Argument | Effet |
| :--- | :---: | :--- |
| `OP_CONST` | index | empile `constants[index]` |
| `OP_LOAD` | slot | empile la variable locale `locals[slot]` |
| `OP_STORE` | slot | dépile et range la valeur dans `locals[slot]` |
| `OP_ADD`, `OP_SUB`, `OP_MUL` | | dépile deux valeurs, empile le résultat |
| `OP_DIV` | | division entière, avec contrôle de la division par zéro |
| `OP_LT`, `OP_EQ` | | comparaison, empile 1 ou 0 |
| `OP_JUMP` | cible | saut inconditionnel |
| `OP_JUMP_IF_FALSE` | cible | dépile la condition, saute si elle vaut 0 |
| `OP_CALL` | indice de fonction | appelle la fonction avec ses arguments, empile le résultat |
| `OP_RET` | | termine la fonction et renvoie la valeur du sommet |

## Prérequis et compilation

Le projet se compile sous Windows avec MSYS2 (UCRT64). Dans le terminal UCRT64 :

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-libgccjit
```

Puis, dans PowerShell, à la racine du projet :

```powershell
.\build.ps1
```

Le script produit `my_jit_vm.exe`.

`main.c` utilise l'horloge de Windows (`QueryPerformanceCounter`). Le `Makefile` ne fonctionne donc pas sous Linux sans remplacer cette fonction de mesure.

## Utilisation

```powershell
.\my_jit_vm.exe                           # tests intégrés, puis banc d'essai
.\my_jit_vm.exe examples\fibonacci.tiny   # exécute la fonction main du fichier
.\my_jit_vm.exe --bench                   # banc d'essai seul
```

Options, à placer avec un fichier `.tiny` :

| Option | Effet |
| :--- | :--- |
| `--disasm` (ou `--dump-bytecode`) | affiche le bytecode de chaque fonction |
| `--dump-asm` | affiche l'assembleur généré par libgccjit |
| `--no-jit` (ou `--interp`) | désactive la compilation, tout est interprété |

## TinyJIT Studio

Interface web locale qui affiche le bytecode, l'assembleur, les blocs de base et les résultats du banc d'essai. Un serveur FastAPI (`studio/server.py`) appelle `my_jit_vm.exe` ; l'interface ne fait pas partie du compilateur.

```powershell
pip install fastapi uvicorn
python studio.py        # ou .\gui.ps1
```

La page s'ouvre sur http://127.0.0.1:8765. Elle charge Chart.js depuis Internet.

## Organisation du dépôt

```
tinyjit/
├── build.ps1, gui.ps1, studio.py, Makefile
├── README.md, RAPPORT.md, PRESENTATION.md
├── examples/     programmes .tiny
├── include/      bytecode.h, compiler.h, jit.h, vm.h
├── src/          bytecode.c, compiler.c, jit.c, main.c, vm.c
└── studio/       server.py, static/index.html
```

`RAPPORT.md` résume le projet, `PRESENTATION.md` est le diaporama (format Marp).

## Limites

- Un seul type de données, pas de flottants ni de chaînes.
- La division par zéro est détectée par l'interpréteur. Le code natif ne la contrôle pas.
- Chaque appel interprété réserve environ 10 Ko de pile C (1024 valeurs de pile et 256 variables locales de 8 octets), ce qui borne la profondeur de récursion.
- La compilation est synchrone : le programme attend la fin de la compilation.
- Le seuil de 1000 appels est fixe et n'a pas été comparé à d'autres valeurs.
- Une fonction dont la dernière instruction est un `return` placé dans un `if` n'a pas de retour implicite final. Sa compilation JIT échoue et elle reste interprétée.

## Références

- J. Aycock, « A Brief History of Just-In-Time », ACM Computing Surveys, 35(2), 2003.
- Documentation de libgccjit : https://gcc.gnu.org/onlinedocs/jit/
- A. V. Aho, M. S. Lam, R. Sethi, J. D. Ullman, Compilers: Principles, Techniques, and Tools, 2e éd., Pearson, 2006.