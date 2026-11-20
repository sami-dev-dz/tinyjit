# TinyJIT

TinyJIT est un petit environnement d'exécution écrit en C11 pour un langage de script minimal, le langage Tiny. Il a été réalisé pour le module Compilation.

Un compilateur en une passe traduit le source en bytecode. Une machine virtuelle à pile l'interprète et compte les appels de chaque fonction. Quand une fonction atteint 1000 appels, elle est compilée en code machine x86-64 avec libgccjit (`-O2`), et les appels suivants passent directement par ce code natif.

Le rapport complet, avec les explications détaillées, est le document PDF remis avec le projet. Ce fichier ne donne que l'essentiel pour compiler et lancer le programme.

## Résultats

Banc d'essai : `sum_to(10000)` appelée 5000 fois, soit 50 millions de tours de boucle. Mesures relevées sous Windows (MSYS2 UCRT64, GCC 16.2, `-O2`) avec `QueryPerformanceCounter`.

| Mode | Temps | Accélération |
| :--- | ---: | ---: |
| Interpréteur seul | 5 480,3 ms | 1,0 |
| Hybride (seuil de 1000 appels) | 1 814,8 ms | 3,0 |
| Code natif seul | 23,6 ms | 232,2 |

Ces valeurs varient d'un lancement à l'autre. Elles valent comme ordres de grandeur, sur un seul programme : une boucle d'arithmétique entière sans appel de fonction.

## Compiler

Sous Windows avec MSYS2 (UCRT64) et libgccjit installé :

```powershell
.\build.ps1
```

Le script produit `my_jit_vm.exe`.

`main.c` utilise l'horloge de Windows (`QueryPerformanceCounter`). Le `Makefile` ne fonctionne donc pas sous Linux sans remplacer cette fonction de mesure.

## Utiliser

```powershell
.\my_jit_vm.exe                          # quatre tests intégrés, puis le banc d'essai
.\my_jit_vm.exe examples\fibonacci.tiny  # exécute la fonction main du fichier
```

Options, à placer à côté d'un fichier `.tiny` :

- `--disasm` (ou `--dump-bytecode`) affiche le bytecode de chaque fonction.
- `--dump-asm` affiche l'assembleur généré par libgccjit.
- `--no-jit` (ou `--interp`) désactive la compilation.

`--bench` (ou `--benchmark`) lance seulement le banc d'essai.

Les programmes du dossier `examples/` et leurs résultats : `fibonacci.tiny` donne 6765, `factorial.tiny` 3 628 800, `sum_to.tiny` 50 005 000, `sum_of_squares.tiny` 400 et `collatz.tiny` 111.

## Le langage Tiny

Un seul type, l'entier signé de 64 bits. Un programme est une suite de fonctions et commence par `main()`.

```
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

## Organisation du code

| Fichier | Rôle |
| :--- | :--- |
| `src/compiler.c` | analyse lexicale et syntaxique, émission du bytecode |
| `src/vm.c` | interpréteur à pile, compteur d'appels, bascule vers le natif |
| `src/bytecode.c` | les 13 opcodes, constantes, table des fonctions |
| `src/jit.c` | blocs de base, pile virtuelle, génération du code avec libgccjit |
| `src/main.c` | ligne de commande, tests intégrés, banc d'essai |
| `studio/`, `studio.py` | interface web locale, voir plus bas |

## TinyJIT Studio

Interface web locale qui affiche le bytecode, l'assembleur, les blocs de base et les résultats du banc d'essai. Elle appelle `my_jit_vm.exe`, elle ne fait pas partie du compilateur.

```powershell
pip install fastapi uvicorn
python studio.py        # ou .\gui.ps1
```

La page s'ouvre sur http://127.0.0.1:8765. Elle charge Chart.js depuis Internet.

## Limites

- Un seul type de données, pas de flottants ni de chaînes.
- La division par zéro est détectée par l'interpréteur. Le code natif ne la contrôle pas.
- Chaque appel interprété réserve environ 10 Ko de pile C, ce qui borne la profondeur de récursion.
- La compilation est synchrone : le programme attend la fin de la compilation.
- Le seuil de 1000 appels est fixe.

## Références

- J. Aycock, « A Brief History of Just-In-Time », ACM Computing Surveys, 35(2), 2003.
- Documentation de libgccjit : https://gcc.gnu.org/onlinedocs/jit/
- A. V. Aho, M. S. Lam, R. Sethi, J. D. Ullman, Compilers: Principles, Techniques, and Tools, 2e éd., Pearson, 2006.
