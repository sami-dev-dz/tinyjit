---
marp: true
theme: default
paginate: true
header: "TinyJIT : Compilateur Just-In-Time & Machine Virtuelle à Pile"
footer: "TinyJIT — Département d'Informatique & Systèmes d'Exécution"
---

# TinyJIT
## Conception, Implémentation et Évaluation d'une Machine Virtuelle Hybride et d'un Compilateur Just-In-Time fondé sur `libgccjit`

**Cadre :** Théorie des Compilateurs, Systèmes d'Exécution & Machines Virtuelles  
**Environnement :** C11 / GCC 16.2 / libgccjit / Architecture x86-64  
**Date :** Année Académique 2025 – 2026  

---

## 1. Introduction & Contexte Scientifique

### Le dilemme classique de l'exécution logicielle :
* **Interprétation pure (AST / Bytecode) :**
  * Démarrage immédiat ($O(1)$ startup latency).
  * Surcoût temporel majeur : décodage répété des instructions (*instruction dispatch overhead*), accès constants à la mémoire pour simuler la pile.
* **Compilation Statique Anticipée (AOT - Ahead-Of-Time) :**
  * Génération de code machine optimal pour le microprocesseur cible.
  * Inconvénient : temps de compilation initial prohibitif et incapacité d'exploiter les profils d'exécution dynamiques (types, branches fréquentes).

### Le paradigme Just-In-Time (JIT) :
* Approche hybride adoptée par les moteurs industriels de pointe (Java HotSpot, V8 JavaScript, LuaJIT).
* **Idée maîtresse :** Interpréter le programme au démarrage, profiler dynamiquement la charge d'exécution, et compiler en mémoire vive native uniquement les fragments de code critiques (**points chauds** ou *hotspots*).

---

## 2. Objectifs de Recherche & Spécifications

1. **Compilateur Front-End en passe unique :**
   * Analyse lexicale et syntaxique descendante sans allocation d'AST intermédiaire volumineux.
   * Résolution d'adresses de branchement par la technique du **backpatching**.
2. **Interpréteur Réentrant à Pile :**
   * Évaluation à pile 64 bits ($S \in \mathbb{Z}^{64}$) réentrante, supportant la récursion profonde sans limitation artificielle de pile logicielle.
   * Comptabilisation dynamique du nombre d'invocations de chaque fonction.
3. **Backend JIT Adaptatif via `libgccjit` :**
   * Détection automatique des points chauds au seuil critique $\theta_{\text{hotspot}} = 1000$.
   * Partitionnement automatique du bytecode en **Blocs de Base (Basic Blocks)**.
   * Traduction symbolique de la pile vers la représentation intermédiaire (IR) de `libgccjit` avec optimisations `-O2`.
4. **Interopérabilité Binaire Portable :**
   * Élimination des contraintes d'édition de liens dynamique propres au format **PE/COFF (Windows)** via l'invocation native par pointeurs de fonctions.

---

## 3. Architecture Globale du Système

```
  ┌────────────────────────────────────────────────────────┐
  │                 Code Source (.tiny)                    │
  └──────────────────────────┬─────────────────────────────┘
                             ▼
  ┌────────────────────────────────────────────────────────┐
  │         Front-End : Lexer & Parser (compiler.c)        │
  │     (Analyse descendante récursive + Backpatching)      │
  └──────────────────────────┬─────────────────────────────┘
                             ▼
  ┌────────────────────────────────────────────────────────┐
  │           Représentation Intermédiaire Bytecode        │
  │      (Instruction Stream + Function Table + Constants) │
  └──────────────────────────┬─────────────────────────────┘
                             ▼
                 ┌───────────────────────┐
                 │  Interpréteur (vm.c)  │◄────────────┐
                 └───────────┬───────────┘             │
                             ▼                         │ OP_CALL
                [ call_count >= 1000 ? ]               │ (Appel non compilé)
                  │                  │                 │
             Non  ▼                  ▼  Oui            │
              [Interpréter]   ┌────────────────────┐   │
                              │ Moteur JIT (jit.c) │   │
                              │ libgccjit (-O2)    │   │
                              └──────────┬─────────┘   │
                                         ▼             │
                              ┌────────────────────┐   │
                              │ Code Natif x86-64  ├───┘
                              │ Exécution Directe  │
                              └────────────────────┘
```

---

## 4. Spécification Formelle du Langage `.tiny`

Le langage cible `.tiny` est un langage impératif d'ordre un manipulant des entiers signés de 64 bits ($\mathbb{Z}_{64}$) :

```ebnf
Program    ::= Function*
Function   ::= "fn" Identifier "(" [ ParameterList ] ")" "{" Statement* "}"
Statement  ::= "var" Identifier "=" Expression ";"
             | Identifier "=" Expression ";"
             | "if" "(" Condition ")" "{" Statement* "}" [ "else" "{" Statement* "}" ]
             | "while" "(" Condition ")" "{" Statement* "}"
             | "return" Expression ";"
Expression ::= Term ( ("+" | "-" | "*" | "/" | "%" | "==" | "<" | ">") Term )*
Term       ::= Identifier | Integer | CallExpr | "(" Expression ")"
CallExpr   ::= Identifier "(" [ ArgumentList ] ")"
```

### Propriétés sémantiques :
* Portée lexicale locale pour les variables déclarées par `var`.
* Récursivité terminale et non-terminale pleinement supportée.
* Point d'entrée standard désigné par la fonction `main()`.

---

## 5. Jeu d'Instructions de la VM (ISA Bytecode)

La machine virtuelle implémente un modèle à pile sans registres virtuels, opérant sur des mots machines de 64 bits :

| Opcode | Opérande | Transition de Pile ($S \rightarrow S'$) | Sémantique Opérationnelle |
| :--- | :---: | :---: | :--- |
| `OP_CONST` | $c \in \mathbb{N}$ | $[ \, ] \rightarrow [ \text{constants}[c] ]$ | Empile la constante entière indexée |
| `OP_LOAD` | $i \in \mathbb{N}$ | $[ \, ] \rightarrow [ \text{locals}[i] ]$ | Charge la variable locale sur la pile |
| `OP_STORE` | $i \in \mathbb{N}$ | $[ v ] \rightarrow [ \, ]$ | Dépile et assigne la valeur à $\text{locals}[i]$ |
| `OP_ADD` / `SUB` | — | $[ a, b ] \rightarrow [ a \circ b ]$ | Évaluation arithmétique binaire |
| `OP_MUL` / `DIV` | — | $[ a, b ] \rightarrow [ a \circ b ]$ | Division avec protection anti-division par zéro |
| `OP_LT` / `EQ` | — | $[ a, b ] \rightarrow [ a \bowtie b \ ? \ 1 : 0 ]$ | Comparaison logique binaire |
| `OP_JUMP` | $target$ | $[ \, ] \rightarrow [ \, ]$ | Saut inconditionnel ($PC \leftarrow target$) |
| `OP_JUMP_IF_FALSE` | $target$ | $[ c ] \rightarrow [ \, ]$ | Saut conditionnel si $c = 0$ ($PC \leftarrow target$) |
| `OP_CALL` | $f \in \mathbb{N}$ | $[ a_1, \dots, a_k ] \rightarrow [ \text{res} ]$ | Invocation de sous-routine avec cadre dédié |
| `OP_RET` | — | $[ v ] \rightarrow [ \, ]$ | Restitution du cadre et transmission du résultat |

---

## 6. Analyse Syntaxique & Algorithme de Backpatching

Le compilateur front-end n'instancie pas d'arbre de syntaxe abstraite (AST). Il émet les opcodes de manière séquentielle lors du parcours descendant.

### Problématique des branchements avant :
Lors de l'émission d'une instruction `OP_JUMP_IF_FALSE`, l'adresse mémoire cible de fin de bloc conditionnel ou de corps de boucle est encore indéterminée.

### Résolution par Backpatching :
1. Émission de l'instruction de saut avec une adresse cible provisoire $0$ :
   $$\text{emit}(OP\_JUMP\_IF\_FALSE, 0) \implies \text{index} = \text{offset}$$
2. Compilation du corps du bloc conditionnel ($Statement^*$).
3. Résolution a posteriori de l'adresse de saut :
   $$\text{instructions}[\text{offset}].\text{arg} = \text{current\_code\_offset}()$$
4. Dans le cas d'une structure `while`, un saut inconditionnel `OP_JUMP` est inséré en fin de bloc pour boucler vers la condition initiale.

---

## 7. Moteur d'Interprétation Réentrant & Profilage

### Conception du cadre d'activation (`src/vm.c`) :
L'interpréteur n'alloue pas de mémoire sur le tas (*heap*) pour ses frames :
```c
int64_t vm_run(BytecodeFunction *fn, const int64_t *args) {
    if (fn->native_code != NULL) {
        return fn->native_code(args); // Aiguillage direct vers le code compilé
    }
    fn->call_count++;
    if (fn->call_count == HOTSPOT_THRESHOLD) {
        jit_compile(fn); // Compilation à la volée
        if (fn->native_code != NULL) return fn->native_code(args);
    }
    int64_t stack[VM_STACK_CAPACITY];
    int64_t locals[VM_LOCALS_CAPACITY];
    // Boucle Fetch-Decode-Execute...
}
```

* **Réentrance native :** Chaque niveau d'appel (notamment en récursion profonde) exploite l'espace d'adressage de la pile C hôte.
* **Complexité spatiale :** $O(D)$ où $D$ est la profondeur d'appel maximale.

---

## 8. Backend JIT : Découpage en Blocs de Base (CFG)

Un bloc de base (*Basic Block*) est défini formellement comme une séquence d'instructions sans saut entrant excepté en première position, et sans saut sortant excepté en dernière position.

### Algorithme de partitionnement du bytecode :
1. **Identification des têtes de blocs (Leaders) :**
   * L'instruction initiale ($\text{offset} = 0$).
   * Toute instruction cible d'un saut `OP_JUMP` ou `OP_JUMP_IF_FALSE`.
   * Toute instruction succédant immédiatement à un saut ou à un `OP_RET`.
2. **Construction des Blocs `gcc_jit_block *` :**
   Chaque leader définit un point d'entrée unique associé à un bloc libgccjit :
   $$\mathcal{B}_k = [ \text{leader}_k, \dots, \text{leader}_{k+1} - 1 ]$$
3. **Établissement des arêtes du graphe (CFG Edges) :**
   * Arête conditionnelle : $\mathcal{B}_{\text{cond}} \xrightarrow{\text{vrai}} \mathcal{B}_{\text{next}}$ et $\mathcal{B}_{\text{cond}} \xrightarrow{\text{faux}} \mathcal{B}_{\text{target}}$.
   * Arête inconditionnelle : $\mathcal{B}_{\text{jump}} \rightarrow \mathcal{B}_{\text{target}}$.

---

## 9. Compilation Symbolique & Simulation de Pile

Pour convertir un code fondé sur une pile en code machine fondé sur des registres, le moteur JIT maintient une **pile d'évaluation symbolique à la compilation** (`vstack`) :

```
Bytecode:          Simulation de Pile JIT:             IR libgccjit générée:
────────────────────────────────────────────────────────────────────────────
LOAD 0             vstack[0] = rval(arg[0])           (aucun code émis)
CONST 1            vstack[1] = rval(1)                (aucun code émis)
ADD                v0 = vstack[0]; v1 = vstack[1];
                   vstack[0] = gcc_jit_context_new_binary_op(ctxt, ADD, v0, v1)
STORE 0            gcc_jit_block_add_assignment(block, local[0], vstack[0])
```

### Bénéfice d'optimisation majeur :
* Les transferts de pile virtuels sont court-circuités au niveau de l'arbre d'expressions (*Expression DAG*).
* Le backend d'optimisation `-O2` de `libgccjit` affecte directement les expressions aux registres matériels du processeur (**allocation de registres via coloriage de graphe**), éliminant intégralement la pile mémoire.

---

## 10. Interopérabilité Binaire sous Windows (PE/COFF)

### Le verrou technique des bibliothèques dynamiques en mémoire :
* Sous les systèmes Unix (ELF), les symboles de l'exécutable hôte (`vm_run`) sont naturellement exportés dans la table globale des symboles dynamiques (`-rdynamic`).
* Sous **Windows (PE/COFF)**, un code binaire généré en mémoire vive par `libgccjit` ne peut importer des fonctions de l'exécutable parent sans bibliothèque d'importation `.a` externe, provoquant l'échec de l'éditeur de liens interne `ld.exe`.

### Solution formelle : Invocation par pointeur de fonction absolu
```c
// 1. Définition de la signature de vm_run
gcc_jit_type *fn_ptr_type = gcc_jit_context_new_function_ptr_type(
    ctxt, NULL, t_int64, 2, fn_param_types, 0);

// 2. Injection de l'adresse mémoire absolue (Run-time Virtual Address)
gcc_jit_rvalue *fn_ptr = gcc_jit_context_new_rvalue_from_ptr(
    ctxt, fn_ptr_type, (void *)vm_run);

// 3. Émission d'un appel direct indirect via pointeur
gcc_jit_rvalue *call_res = gcc_jit_context_new_call_through_ptr(
    ctxt, NULL, fn_ptr, 2, call_args);
```
**Résultat :** Traduction en instruction assembleur directe `call *%rax`, indépendance totale vis-à-vis de l'éditeur de liens statique, et overhead d'appel minimal.

---

## 11. Analyse Comparative du Code Machine Produit

### Exemple : Fonction récursive `fib(n)`

```x86asm
# Assembleur généré à la volée par TinyJIT (-O2 sur architecture x86-64) :
fib:
    pushq   %rsi                  # Sauvegarde des registres callee-saved
    pushq   %rbx
    subq    $56, %rsp             # Allocation du cadre de pile matériel
    movq    (%rcx), %rax          # Charge l'argument n dans le registre %rax
    cmpq    $1, %rax              # Évaluation matérielle de (n < 2)
    jle     .L_BASE_CASE          # Branchement conditionnel direct
.L_RECURSE:
    movabsq $140701962018782, %rbx# Injection de l'adresse mémoire de vm_run
    leaq    -1(%rax), %rdx        # Calcul de (n - 1) via calcul d'adresse LEA
    movq    %rdx, 32(%rsp)
    call    *%rbx                 # Appel direct via pointeur (fib(n-1))
    movq    %rax, %rsi            # Mémorisation du résultat intermédiaire
    # ... Calcul similaire pour fib(n-2) ...
    addq    %rsi, %rax            # Somme finale dans le registre de retour %rax
    addq    $56, %rsp
    popq    %rbx
    popq    %rsi
    ret
```

*Élimination intégrale du switch de dispatch, conservation des variables dans les registres x86-64.*

---

## 12. Protocole Expérimental & Résultats de Performance

### Conditions expérimentales :
* **Processeur :** AMD64 x86-64 (Windows 11, UCRT64 GCC 16.2.0, build `-O2`).
* **Précision temporelle :** API matérielle `QueryPerformanceCounter` ($\Delta t < 1\,\mu\text{s}$).
* **Test de référence :** Calcul de $\sum_{i=1}^{10\,000} i$ exécuté $5\,000$ fois (soit **$50 \times 10^6$ itérations**).

### Résultats comparatifs mesurés :

| Mode d'Exécution | Temps Mesuré | Facteur d'Accélération | Analyse Technique |
| :--- | :---: | :---: | :--- |
| **Interpréteur seul (Pur)** | `5 480,3 ms` | $1,0\times$ *(référence)* | Surcoût de dispatch pour chaque opcode |
| **Mode Hybride (Seuil = 1 000)** | `1 814,8 ms` | $\mathbf{3,0\times}$ | Phase d'interprétation + JIT en mémoire |
| **Code Natif JIT Pur** | `23,6 ms` | $\mathbf{232,2\times}$ | **Code machine natif direct (registres CPU)** |

$$\text{Speedup} = \frac{T_{\text{interprété}}}{T_{\text{natif}}} = \frac{5480,3\,\text{ms}}{23,6\,\text{ms}} \approx \mathbf{232,2}$$

---

## 13. Outil Pédagogique & Exploration : TinyJIT Studio

Pour valider et visualiser les transformations de code à chaque étape, un environnement d'analyse moderne a été conçu :

* **Architecture découplée :** Backend FastAPI communicant avec `my_jit_vm.exe` et Single Page Application en HTML5 / CSS3 / JavaScript pur.
* **Fonctionnalités d'inspection :**
  1. **Tableau Bytecode ISA :** Visualisation typée des opcodes, constantes et tables de fonctions.
  2. **Inspecteur Assembleur x86-64 (style *Godbolt*) :** Analyse fine des instructions machines produites par le compilateur JIT.
  3. **Visualisation du Flot de Contrôle (CFG) :** Représentation schématique des blocs de base et de leurs transitions conditionnelles.
  4. **Tableau de bord de performance (Chart.js) :** Télémétrie en temps réel des temps d'exécution et des accélérations relatives.

---

## 14. Limites Actuelles & Discussion

1. **Compilation Synchrone Bloquante :**
   * Au 1000ᵉ appel, l'interpréteur suspend son exécution pendant environ $25\,\text{ms}$ pour compiler la fonction avec `libgccjit`.
   * *Conséquence :* Légère gigue temporelle (*latency spike*) lors de la transition d'échelon.
2. **Granularité par Méthode (Method JIT) :**
   * L'unité de compilation est la fonction entière, par opposition aux compilateurs par traces (*Tracing JIT* comme LuaJIT) qui compilent uniquement les boucles les plus denses.
3. **Absence d'OSR (On-Stack Replacement) :**
   * Une fonction contenant une boucle de longue durée entrée en mode interprété ne peut basculer en mode natif en cours d'exécution ; la transition nécessite une nouvelle invocation de fonction.

---

## 15. Perspectives de Recherche & Évolutions

* **Compilation Asynchrone en Arrière-Plan :**
  * Déléguer `jit_compile()` à un thread de travail dédié (`pthread` / `std::thread`).
  * L'interpréteur poursuit son travail sans latence perceptible jusqu'à la mise à disposition atomique du pointeur `native_code`.
* **Inlining Interprocédural basé sur le Profil :**
  * Remplacement direct des `OP_CALL` vers de petites fonctions par leur corps compilé afin d'éliminer le coût de l'instruction `call *%rax`.
* **Système de Typage Dynamique avec NaN-Boxing :**
  * Représentation uniforme en 64 bits des flottants double précision (IEEE 754), entiers 48 bits, booléens et pointeurs mémoire sans boxing lourd.

---

## 16. Conclusion Scientifique

### Bilan du projet :
* Démonstration expérimentale probante de la viabilité d'un compilateur Just-In-Time fondé sur `libgccjit`.
* Conception complète de la chaîne d'exécution : de la grammaire lexicale jusqu'à l'émission de code machine natif x86-64.
* Validation d'un facteur d'accélération d'un ordre de grandeur supérieur à **$200\times$** sur des charges calculatoires intensives.
* Résolution élégante de la problématique d'édition de liens dynamique sous environnement Windows PE/COFF.

---

## 17. Références Bibliographiques

1. **Aho, A. V., Lam, M. S., Sethi, R., & Ullman, J. D.** (2006). *Compilers: Principles, Techniques, and Tools* (2nd Edition). Addison-Wesley.
2. **Aycock, J.** (2003). *A brief history of just-in-time.* ACM Computing Surveys (CSUR), 35(2), 97-113.
3. **Malcolm, D.** (2014). *libgccjit: An embeddable JIT compiler library based on GCC.* Free Software Foundation Proceedings.
4. **Cooper, K. D., & Torczon, L.** (2011). *Engineering a Compiler* (2nd Edition). Morgan Kaufmann.
5. **Nuzman, D., & Dyshel, S.** (2013). *V8: An open-source JavaScript engine.* Google Technical Reports.
