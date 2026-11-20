---
marp: true
theme: default
paginate: true
header: "TinyJIT : machine virtuelle et compilation à la volée"
footer: "Module Compilation, novembre 2026"
---

# TinyJIT

## Une machine virtuelle à bytecode qui compile à la volée avec libgccjit

Projet du module Compilation

C11, GCC 16.2, libgccjit, x86-64 sous Windows

---

## Le problème

Un interpréteur démarre tout de suite, mais décode chaque instruction à chaque exécution et passe par une pile en mémoire. Sur une boucle qui tourne des millions de fois, ce coût domine.

Un compilateur produit du code rapide, mais il faut compiler avant d'exécuter, et compiler prend du temps.

Le JIT combine les deux : on interprète d'abord, on compte les appels, et on ne compile que les fonctions appelées souvent, les points chauds.

---

## Ce que le projet devait faire

1. Compiler un petit langage de script (Tiny) vers du bytecode.
2. Interpréter ce bytecode dès le premier appel, en comptant les appels de chaque fonction.
3. Compiler en mémoire toute fonction qui atteint 1000 appels, avec libgccjit.
4. Continuer l'exécution en natif, y compris au milieu d'une récursion.

---

## Architecture

```
 script .tiny
      |
      v
 compilateur (une passe) --> bytecode
                                |
                                v
                      machine virtuelle  <--------+
                                |                 | non
                    1000 appels atteints ? -------+
                                | oui
                                v
                    libgccjit (-O2) --> code x86-64
                                           |
        les appels suivants passent directement par le code natif
```

Cinq modules C : compilateur, machine virtuelle, bytecode, JIT, pilote.

---

## Le langage Tiny

Un seul type : l'entier signé de 64 bits. Opérateurs `+ - * / < ==`, variables locales, `if/else`, `while`, fonctions, récursion.

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

Une fonction ne peut appeler qu'elle-même ou une fonction définie plus haut.

---

## Le bytecode

13 opcodes, une instruction porte au plus un opérande.

| Opcode | Effet |
| :--- | :--- |
| `CONST`, `LOAD`, `STORE` | constante, lecture et écriture d'une variable locale |
| `ADD`, `SUB`, `MUL`, `DIV` | arithmétique sur les deux valeurs du sommet de pile |
| `LT`, `EQ` | comparaison, résultat 1 ou 0 |
| `JUMP`, `JUMP_IF_FALSE` | sauts |
| `CALL`, `RET` | appel et retour |

Pour `fib`, le compilateur produit 16 instructions, de `LOAD 0` à `RET`.

---

## Sauts en avant : le backpatching

Quand le compilateur émet un `if` ou un `while`, il ne connaît pas encore l'adresse de fin du bloc.

```c
size_t jump_false_idx = emit(p, OP_JUMP_IF_FALSE, 0);  // cible provisoire
parse_block(p);                                        // corps du bloc
p->fn->code[jump_false_idx].arg = (int32_t)p->fn->code_count;
```

Il émet le saut avec la cible 0, compile le corps, puis écrit la vraie adresse. Pas d'arbre syntaxique : chaque règle émet ses instructions tout de suite.

---

## L'interpréteur et le compteur d'appels

```c
if (fn->native_fn != NULL)                 // déjà compilée
    return ((JitNativeFn)fn->native_fn)(args);

fn->call_count++;
if (fn->call_count == HOTSPOT_THRESHOLD)   // 1000
    jit_compile(fn);
```

La pile et les variables locales sont des tableaux de la pile C : chaque appel a son propre cadre, donc la récursion fonctionne. Un cadre pèse environ 10 Ko (1024 + 256 entiers de 8 octets).

---

## Du bytecode au code natif : blocs de base

libgccjit attend un graphe de blocs, le bytecode est une suite linéaire.

Un bloc commence à l'instruction 0, à chaque cible de saut et après chaque saut. Chaque bloc libgccjit se termine par un saut, un saut conditionnel ou un retour.

---

## La pile devient virtuelle

Pendant la compilation, le JIT garde une pile d'expressions (256 au maximum). Une addition dépile deux expressions et en crée une nouvelle.

```c
case OP_ADD: {
    gcc_jit_rvalue *b = vstack[--vsp];
    gcc_jit_rvalue *a = vstack[--vsp];
    vstack[vsp++] = gcc_jit_context_new_binary_op(
        ctxt, NULL, GCC_JIT_BINARY_OP_PLUS, t_int64, a, b);
    break;
}
```

Les sauts n'apparaissent qu'entre deux instructions, donc la pile est vide aux frontières de blocs. GCC place ensuite les valeurs dans les registres.

---

## Un point propre à Windows

Le code compilé en mémoire ne retrouve pas facilement les symboles de l'exécutable (format PE/COFF). Pour appeler `vm_run` depuis le code généré, TinyJIT y écrit son adresse absolue.

```c
gcc_jit_rvalue *r_vm_run_ptr = gcc_jit_context_new_rvalue_from_ptr(
    ctxt, t_vm_run_ptr, (void *)vm_run);
gcc_jit_rvalue *res = gcc_jit_context_new_call_through_ptr(
    ctxt, NULL, r_vm_run_ptr, 2, call_params);
```

Conséquence : un appel récursif natif repasse par `vm_run`, qui redirige vers le code natif.

---

## Le code produit pour fib

```asm
fib:
    pushq   %rsi
    pushq   %rbx
    subq    $56, %rsp
    movq    (%rcx), %rax      # n
    cmpq    $1, %rax          # n < 2 devient n <= 1
    jle     .L1
    ...
    call    *%rbx             # vm_run(fib, &(n - 1))
    ...
    call    *%rbx             # vm_run(fib, &(n - 2))
    addq    %rsi, %rax
.L1:
    ...
    ret
```

La pile de la VM a disparu : `n` et les résultats sont dans des registres. Il reste deux petits tableaux d'arguments sur la pile C.

---

## Mesures

`sum_to(10000)` appelée 5000 fois, soit 50 millions de tours de boucle (13 instructions bytecode par tour).

| Mode | Temps | Accélération |
| :--- | ---: | ---: |
| Interpréteur seul | 5 480,3 ms | 1,0 |
| Hybride (seuil 1000) | 1 814,8 ms | 3,0 |
| Natif seul | 23,6 ms | 232,2 |

Interpréteur : 109,6 ns par tour, soit 8,4 ns par instruction. Natif : 0,47 ns par tour.

---

## Ce que disent les chiffres

Le mode hybride interprète 1000 appels sur 5000, soit environ 1 096 ms, puis exécute le reste en natif pour environ 19 ms. Cela fait près de 1 115 ms hors compilation, pour 1 814,8 ms mesurés.

L'écart d'environ 700 ms est de l'ordre du coût de la compilation, que nous n'avons pas mesuré séparément.

Une seule boucle d'arithmétique a été mesurée. Les programmes récursifs comme `fib` gardent un coût par appel que ce banc d'essai ne voit pas.

---

## TinyJIT Studio

Une interface web locale pour regarder le JIT travailler.

- éditeur, bytecode, assembleur, blocs de base, banc d'essai ;
- un serveur FastAPI appelle `my_jit_vm.exe` ;
- lancement : `python studio.py`, page sur http://127.0.0.1:8765.

Ce n'est pas une partie du compilateur, seulement un outil pour l'explorer.

---

## Limites

- Un seul type de données, l'entier de 64 bits.
- La division par zéro est détectée par l'interpréteur, pas par le code natif.
- Chaque appel interprété réserve environ 10 Ko de pile, ce qui borne la récursion.
- La compilation est synchrone : le programme s'arrête le temps de compiler.
- Le seuil de 1000 appels est fixe et n'a pas été comparé à d'autres valeurs.

---

## Pistes

- Compiler dans un thread séparé.
- Ajouter d'autres types, par exemple avec du NaN-boxing.
- Intégrer les petites fonctions très appelées.
- Mesurer le temps de compilation et tester plusieurs seuils.

---

## Conclusion

TinyJIT interprète du bytecode, repère les fonctions appelées 1000 fois et les compile en x86-64 avec libgccjit. Sur la boucle mesurée, le code natif est environ 232 fois plus rapide que l'interpréteur, et le mode hybride reste 3 fois plus rapide.

Les résultats des programmes sont les mêmes avant et après la bascule.

---

## Références

1. J. Aycock, « A Brief History of Just-In-Time », ACM Computing Surveys, 35(2), 2003, p. 97-113.
2. Documentation de libgccjit, https://gcc.gnu.org/onlinedocs/jit/
3. A. V. Aho, M. S. Lam, R. Sethi, J. D. Ullman, Compilers: Principles, Techniques, and Tools, 2e éd., Pearson, 2006.
4. Microsoft, « x64 calling convention », Microsoft Learn.
