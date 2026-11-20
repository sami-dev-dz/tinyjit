# Rapport de Projet — Compilation & Systèmes d'Exécution
## Conception et Implémentation de TinyJIT
### Machine virtuelle à bytecode avec détection de points chauds et compilation native à la volée via libgccjit

| Informations | Détails |
| :--- | :--- |
| **Auteur** | Sami Ghoul |
| **Encadrant** | Enseignant du module de Compilation |
| **Environnement** | C11 / GCC 16.2 / libgccjit (x86-64 Windows MSYS2 UCRT64) |
| **Dépôt GitHub** | https://github.com/sami-dev-dz/tinyjit |
| **Date** | Novembre 2026 |

---

### Résumé du projet
Ce rapport présente l'architecture et la réalisation de **TinyJIT**, un environnement d'exécution complet pour un langage de script impératif (`.tiny`). Le système combine un compilateur source-à-bytecode en une seule passe, un interpréteur à pile équipé d'un profileur d'exécution réentrant, et un backend de compilation Just-In-Time (JIT) reposant sur `libgccjit`. 

Lorsqu'une fonction franchit un seuil critique d'appels (point chaud), son bytecode est automatiquement découpé en blocs de base (*Basic Blocks*) et traduit à la volée en code machine natif **x86-64** optimisé (`-O2`). Sur des algorithmes intensifs, le passage au code natif procure une accélération mesurée supérieure à **$230\times$** par rapport à l'interpréteur classique.

---

### 1. Contexte et Objectifs du Projet

Les langages dynamiques modernes (tels que Python/PyPy, LuaJIT ou JavaScript V8) reposent sur un compromis fondamental entre rapidité de démarrage et performance en régime établi. Une exécution purement interprétée offre une réactivité immédiate sans temps de latence initial, mais souffre d'un surcoût intrinsèque élevé lié au décodage instruction par instruction (*dispatch overhead*) et à la gestion logicielle d'une pile d'opérandes en mémoire.

L'objectif de ce projet est de concevoir de zéro un moteur d'exécution hybride capable de :
1. **Compiler un langage de script minimaliste** (`.tiny` : variables locales, structures conditionnelles `if/else`, boucles `while` et fonctions récursives) vers un jeu d'instructions bytecode compact.
2. **Interpréter ce bytecode dès le premier appel** tout en comptabilisant dynamiquement la fréquence d'invocation de chaque fonction.
3. **Détecter les points chauds (*hotspots*)** au franchissement d'un seuil critique (`HOTSPOT_THRESHOLD = 1000` appels) et déclencher la compilation native en mémoire.
4. **Basculer de manière transparente vers le code natif x86-64**, y compris lors d'appels récursifs profonds, sans nécessiter d'édition de liens externe sur disque.

---

### 2. Architecture Globale de TinyJIT

Le projet est structuré en modules C11 modulaires, organisés selon une chaîne de traitement séquentielle allant du code source jusqu'au silicium :

| Module | Fichiers | Responsabilité technique |
| :--- | :--- | :--- |
| **Frontend (Compilateur)** | `include/compiler.h`<br>`src/compiler.c` | Analyse lexicale (*Lexer*), analyse syntaxique récursive descendante (*Parser*) et émission directe du bytecode avec résolution des sauts (*backpatching*). |
| **Machine Virtuelle** | `include/vm.h`<br>`src/vm.c` | Interpréteur à pile réentrant, gestion des cadres d'appel (*call frames*), profilage des invocations et commutation vers le code natif. |
| **Bytecode & Données** | `include/bytecode.h`<br>`src/bytecode.c` | Définition des 13 opcodes, structures `Instruction`, `BytecodeFunction`, gestion dynamique des pools de constantes et des tables de fonctions. |
| **Backend JIT** | `include/jit.h`<br>`src/jit.c` | Découpage en blocs de base (*Basic Blocks*), pile virtuelle symbolique, génération de l'IR `libgccjit` (`-O2`), pont d'appel et export assembleur (`--dump-asm`). |
| **Pilote & CLI** | `src/main.c`<br>`build.ps1` | Interface en ligne de commande, chargement de fichiers `.tiny`, bancs d'essais et mesure haute précision des temps CPU (`QueryPerformanceCounter`). |

```mermaid
flowchart LR
    Source[".tiny Script"] --> Frontend["Compiler (1-pass)"]
    Frontend --> Bytecode["BytecodeFunction"]
    Bytecode --> VM["VM (Fetch-Decode-Execute)"]
    VM --> Check{"call_count >= 1000 ?"}
    Check -- Non --> VM
    Check -- Oui --> JIT["libgccjit (-O2)"]
    JIT --> Native["Code machine x86-64"]
    Native --> CPU["CPU direct"]
```

Le cycle de vie d'une fonction `BytecodeFunction` est simple : tant que son compteur `call_count` est strictement inférieur à 1000, `vm_run()` interprète ses opcodes. Au 1000ᵉ appel, `jit_compile()` génère le code machine et renseigne le pointeur de fonction `fn->native_fn`. Tous les appels ultérieurs court-circuitent alors immédiatement l'interpréteur :
```c
if (fn->native_fn != NULL) {
    JitNativeFn native = (JitNativeFn)fn->native_fn;
    return native(args);
}
```

---

### 3. Le Langage Tiny et le Compilateur Frontend

Le langage source `.tiny` propose une syntaxe épurée manipulant exclusivement des entiers signés 64 bits (`int64_t`). Le compilateur adopte une architecture **en une passe** : chaque règle de grammaire émet immédiatement les instructions bytecode correspondantes dans le vecteur d'instructions, sans requérir l'allocation d'un arbre syntaxique abstrait (AST) intermédiaire lourd.

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

#### Résolution des sauts (Backpatching) :
Pour les structures de contrôle (`if/else` et `while`), l'adresse cible de sortie n'est pas connue au moment où le saut conditionnel `OP_JUMP_IF_FALSE` est émis. Le compilateur émet donc un saut provisoire vers l'adresse `0`, mémorise l'index de cette instruction, compile le corps du bloc, puis met à jour rétroactivement l'adresse cible (*backpatching*) :
```c
size_t jump_exit = emit_instruction(fn, OP_JUMP_IF_FALSE, 0);
// Compilation du corps du bloc...
fn->code[jump_exit].arg = (int32_t)fn->code_count; // Adresse de fin résolue
```

---

### 4. Jeu d'Instructions (ISA) et Machine Virtuelle

L'interpréteur repose sur une pile d'évaluation de capacité généreuse (`VM_STACK_CAPACITY = 1024`) et un espace de variables locales indexé (`VM_LOCALS_CAPACITY = 256`), où les paramètres de la fonction occupent les premiers slots `0 .. num_args - 1`.

| Opcode | Opérande | Sémantique sur la pile d'évaluation |
| :--- | :---: | :--- |
| `OP_CONST` | `idx` | Empile la constante entière située à l'index `idx` du pool. |
| `OP_LOAD` | `slot` | Charge la variable locale `slot` sur la pile. |
| `OP_STORE` | `slot` | Dépile le sommet et le stocke dans la variable locale `slot`. |
| `OP_ADD, SUB, MUL, DIV` | — | Dépile `b` puis `a`, effectue l'opération arithmétique et empile le résultat. |
| `OP_LT, EQ` | — | Compare `a < b` ou `a == b` et empile `1` (vrai) ou `0` (faux). |
| `OP_JUMP` | `target` | Saut inconditionnel : affecte `target` au pointeur d'instruction `ip`. |
| `OP_JUMP_IF_FALSE` | `target` | Dépile la condition ; si elle vaut `0`, saute à l'instruction `target`. |
| `OP_CALL` | `fn_idx` | Dépile les arguments, invoque `vm_run()` sur la fonction cible et empile le retour. |
| `OP_RET` | — | Dépile le résultat final et termine le cadre d'exécution courant. |

#### Réentrance et appels imbriqués :
Pour permettre la récursion (comme dans `fib`), la fonction `vm_run` est strictement **réentrante** : ses variables d'état (`stack` et `locals`) sont allouées localement sur la pile d'exécution C. Chaque appel de fonction imbriqué ou récursif dispose ainsi de son propre cadre d'évaluation isolé sans écraser les registres de l'appelant.

---

### 5. Conception du Compilateur JIT avec libgccjit

La traduction du bytecode vers du code machine x86-64 nécessite de transformer un flot linéaire d'instructions sur pile en un graphe de flot de contrôle (*Control Flow Graph* ou CFG) composé d'instructions en forme SSA exploitables par l'optimiseur de GCC.

#### 5.1 Découpage en Blocs de Base (Basic Blocks)
Avant d'émettre le code intermédiaire, une passe préliminaire analyse le bytecode pour identifier les cibles de sauts (`OP_JUMP`, `OP_JUMP_IF_FALSE`). Un bloc `gcc_jit_block` est instancié au début de la fonction et à chaque cible de branchement. Un bloc d'entrée dédié (`entry`) initialise les variables locales natives à partir des arguments avant de brancher vers le premier bloc de code (`bb_0`).

#### 5.2 Élimination de la pile à la compilation
Au lieu d'émuler une pile en mémoire vive pendant l'exécution native, le JIT maintient une **pile virtuelle symbolique** (`gcc_jit_rvalue *vstack[256]`) pendant la phase de compilation. Lors de la rencontre d'un `OP_ADD`, deux nœuds d'expressions `rvalue` sont dépilés et combinés via `gcc_jit_context_new_binary_op()`. L'optimiseur `-O2` de GCC alloue ensuite directement ces calculs dans les registres physiques du CPU (`%rax`, `%rdx`, `%rsi`), éliminant tout accès superflu à la mémoire.

#### 5.3 Résolution des appels (`OP_CALL`) sous Windows (PE/COFF)
Sous l'environnement Windows, une bibliothèque dynamique compilée en mémoire par `libgccjit` ne peut pas résoudre dynamiquement les symboles externes d'un `.exe` sans passer par une bibliothèque d'importation `.a`. 

Pour surmonter cette contrainte avec une portabilité totale, TinyJIT injecte directement l'adresse mémoire absolue de `vm_run` sous la forme d'un pointeur de fonction typé via **`gcc_jit_context_new_call_through_ptr`** :
```c
// Définition du prototype : int64_t (*)(void *, const int64_t *)
gcc_jit_type *param_types[2] = { t_void_ptr, t_args_ptr };
gcc_jit_type *t_vm_run_ptr = gcc_jit_context_new_function_ptr_type(
    ctxt, NULL, t_int64, 2, param_types, 0
);
// Injection de l'adresse mémoire directe de vm_run
gcc_jit_rvalue *r_vm_run_ptr = gcc_jit_context_new_rvalue_from_ptr(
    ctxt, t_vm_run_ptr, (void *)vm_run
);
// Émission de l'instruction d'appel direct
gcc_jit_rvalue *res = gcc_jit_context_new_call_through_ptr(
    ctxt, NULL, r_vm_run_ptr, 2, call_params
);
```
Grâce à ce mécanisme, le code natif peut appeler l'interpréteur sans passer par `ld.exe`, assurant une bascule fluide vers le code natif même au milieu d'un arbre d'appels récursifs profonds.

---

### 6. Inspection du Code Machine Généré (`--dump-asm`)

En fournissant l'option en ligne de commande `--dump-asm`, le moteur active `GCC_JIT_BOOL_OPTION_DUMP_GENERATED_CODE` et affiche l'assembleur x86-64 produit à la volée. 

Extrait de l'assembleur réel généré pour la fonction `fib(n)` sous la convention d'appel Microsoft x64 :
```x86asm
fib:
.L2:
    pushq   %rsi
    pushq   %rbx
    subq    $56, %rsp
    movq    (%rcx), %rax          # Charge n (args[0]) directement dans %rax
    cmpq    $1, %rax              # L'optimiseur transforme (n < 2) en (n <= 1)
    jle     .L1                   # Retour direct si condition vérifiée
.L4:
    movabsq $140701962018782, %rbx# Adresse absolue de vm_run injectée en dur
    leaq    -1(%rax), %rdx        # Calcule (n - 1) dans le registre %rdx
    subq    $2, %rax              # Calcule (n - 2) dans le registre %rax
    movabsq $2375107032080, %rcx  # Pointeur vers la structure de la fonction
    movq    %rdx, 32(%rsp)
    leaq    32(%rsp), %rdx
    movq    %rax, 40(%rsp)
    call    *%rbx                 # Appel direct de vm_run(fib, &(n-1))
    leaq    40(%rsp), %rdx
    movabsq $2375107032080, %rcx
    movq    %rax, %rsi            # Mémorise le premier résultat dans %rsi
    call    *%rbx                 # Appel direct de vm_run(fib, &(n-2))
    addq    %rsi, %rax            # Additionne les deux résultats dans %rax
.L1:
    addq    $56, %rsp
    popq    %rbx
    popq    %rsi
    ret                           # Valeur de retour transmise dans %rax
```

Cette trace confirme que la pile logicielle de la VM a été complètement éliminée : toutes les variables et opérations résident exclusivement dans les registres matériels du processeur.

---

### 7. Évaluation Expérimentale des Performances

Le banc de test intégré mesure le temps d'exécution de la boucle `sum_to(10000)` répétée 5 000 fois (soit **50 millions d'itérations**) à l'aide de l'API matérielle `QueryPerformanceCounter` (précision inférieure à la microseconde). L'interpréteur et le compilateur étant tous deux construits avec `-O2`, l'évaluation est strictement équitable :

| Mode d'exécution | Temps mesuré | Accélération | Caractéristiques du mode |
| :--- | :---: | :---: | :--- |
| **100 % Interpréteur pur** | `5 480,3 ms` | $1,0\times$ *(référence)* | Décodage logiciel de 17 instructions bytecode par tour de boucle. |
| **Mode Hybride (Seuil = 1000)** | `1 814,8 ms` | $\mathbf{3,0\times}$ | Phase d'interprétation + compilation `libgccjit` + fin en natif. |
| **Code Natif JIT pur** | `23,6 ms` | $\mathbf{232,2\times}$ | Code machine x86-64 direct avec registres CPU sans surcoût VM. |

#### Analyse des résultats :
1. **Amortissement immédiat** : Le coût de compilation en mémoire de `libgccjit` (environ 25 ms) est amorti dès les premières centaines d'itérations natives.
2. **Gain d'échelle** : Une fois la fonction compilée, le code tourne **232 fois plus vite** que la boucle d'interprétation, ce qui valide parfaitement l'intérêt de la compilation JIT adaptative.

---

### 8. Environnement d'Analyse et Visualisation : TinyJIT Studio

Afin de faciliter l'exploration pédagogique des différentes étapes de la compilation et d'offrir une démonstration visuelle vivante, le projet intègre **TinyJIT Studio**, un environnement interactif complet articulé autour de deux couches :

1. **Backend FastAPI (`studio/server.py`)** :
   - Orchestre l'exécution de `my_jit_vm.exe` via sous-processus sécurisés.
   - Fournit une API REST pour l'analyse lexicale, l'extraction du bytecode (`--disasm`), la capture de l'assembleur x86-64 (`--dump-asm`), le calcul de la topologie des blocs de base (CFG), et l'exécution du banc de benchmark (`--bench`).
   - Assure la gestion transparente des fichiers temporaires et des scripts d'exemples pré-enregistrés (`examples/`).

2. **Interface Utilisateur Moderne (`studio/static/index.html`)** :
   - **Éditeur Source réactif** avec numérotation de ligne et raccourcis clavier (`Ctrl+Enter`).
   - **Tableau d'Inspection du Bytecode** détaillant chaque offset, opcode, argument et effet opérationnel.
   - **Inspecteur Assembleur x86-64 (style *Godbolt*)** colorisant la syntaxe machine, les registres et les étiquettes de saut.
   - **Graphe de Flot de Contrôle (CFG)** représentant graphiquement les *Basic Blocks* et leurs transitions (vrai, faux, inconditionnel).
   - **Tableau de Bord de Benchmark** propulsé par `Chart.js` comparant visuellement l'interpréteur, le mode hybride JIT et le code natif.
   - **Console de Télémétrie** avec badges dynamiques signalant en temps réel la détection des points chauds et la bascule vers le code machine natif.

---

### 9. Conclusion et Perspectives

Le projet **TinyJIT** démontre concrètement les principes de fonctionnement d'un moteur d'exécution adaptatif moderne. En combinant un front-end compact en C11 et la puissance de génération de code de `libgccjit`, le système parvient à allier la simplicité d'un interpréteur à pile aux performances brutes du langage C compilé.

#### Pistes d'évolution futures :
1. **Compilation asynchrone** : Déporter l'appel à `jit_compile()` dans un thread séparé (`pthread` / `CreateThread`) afin que l'interpréteur ne subisse aucune pause lors de la compilation.
2. **Typage dynamique et NaN-boxing** : Supporter les flottants et les chaînes de caractères au sein d'une valeur de 64 bits compacte.
3. **Inlining des fonctions courtes** : Intégrer directement le corps des petites fonctions appelées fréquemment afin d'éliminer le saut indirect `call *%rbx`.
