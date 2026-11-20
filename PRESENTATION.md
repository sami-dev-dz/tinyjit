# Soutenance de Projet — TinyJIT
## Guide de Présentation & Aide-Mémoire pour l'Oral

| Projet | TinyJIT — JIT Compiler & Stack VM |
| :--- | :--- |
| **Auteur** | Sami Ghoul |
| **Durée visée** | 5 à 10 minutes de présentation + démonstration + questions |
| **Stack technique** | C11, GCC 16.2, libgccjit, x86-64 (Windows UCRT64 / MSYS2), FastAPI / HTML5 Studio |

---

## 1. Déroulé de la Présentation (5 à 7 minutes)

### Étape 1 : Le Problème & Objectif (1 minute)
- **Le constat :** Les langages interprétés (Python, PHP standard) démarrent vite mais sont lents en exécution prolongée à cause de la boucle d'interprétation (*fetch-decode-execute*) et de la manipulation logicielle de la pile en RAM.
- **La solution JIT :** Le juste milieu entre interpréteur et compilateur statique (AOT). On commence par interpréter pour démarrer instantanément, on profile la fréquence d'exécution des fonctions, et dès qu'une fonction devient « chaude » (*hotspot*), on la compile en code machine natif pour le reste de l'exécution.
- **Notre réalisation :** TinyJIT, une machine virtuelle complète avec son compilateur front-end, son compilateur JIT utilisant **`libgccjit`** (`-O2`), et un **Studio Graphique interactif** de visualisation en direct.

### Étape 2 : L'Architecture en 4 couches (2 minutes)
1. **Front-End (`compiler.c`) :**
   - Parseur récursif descendant en **une passe**.
   - Pas besoin d'allouer un AST lourd : émission directe du bytecode.
   - Gestion des sauts avec **backpatching** pour les `if/else` et `while`.
2. **Machine Virtuelle réentrante (`vm.c`) :**
   - Interpréteur à pile avec un jeu de 15 opcodes.
   - Chaque appel dispose de son propre cadre d'évaluation (`stack` et `locals` sur la pile C).
   - Profilage dynamique : incrément de `call_count` à chaque appel.
3. **Moteur JIT (`jit.c`) :**
   - Découpage en blocs de base (*Basic Blocks*).
   - Simulation de la pile à la compilation (`vstack`) $\rightarrow$ transformation en registres x86-64 réels.
   - Pont d'appel `call_through_ptr` pour exécuter `OP_CALL` sans dépendance externe sous Windows PE/COFF.
4. **TinyJIT Studio (`studio/`) :**
   - Environnement de développement interactif avec désassembleur de bytecode, inspecteur assembleur x86-64 (Godbolt style), graphe de flot de contrôle (CFG) et tableau de bord de benchmark.

### Étape 3 : La Démonstration en Direct (2 minutes)
Vous avez le choix entre le terminal ou le Studio graphique :

#### Option A : Démonstration via TinyJIT Studio (Recommandé pour l'impact visuel)
```powershell
# Lance le Studio complet et ouvre l'interface dans le navigateur :
.\gui.ps1
```
1. Choisir l'exemple **Fibonacci** dans le menu déroulant.
2. Cliquer sur **▶ Exécuter (JIT)** : montrer le badge de bascule JIT vert et le résultat exact (6765).
3. Ouvrir l'onglet **📜 Bytecode ISA** : montrer les instructions générées et les constantes.
4. Ouvrir l'onglet **⚙️ x86-64 ASM** : montrer l'assembleur généré en direct avec les registres CPU.
5. Ouvrir l'onglet **🗺️ Basic Blocks (CFG)** : montrer le découpage en blocs et transitions.
6. Cliquer sur **📊 Benchmark** : admirer le graphique en barres illustrant le gain de $\mathbf{232\times}$.

#### Option B : Démonstration en Ligne de Commande
```powershell
# 1. Compilation instantanée
.\build.ps1

# 2. Exécution d'un script récursif Fibonacci
.\my_jit_vm.exe examples/fibonacci.tiny

# 3. L'effet "Wouah" : inspecter l'assembleur x86-64 généré à la volée !
.\my_jit_vm.exe examples/fibonacci.tiny --dump-asm

# 4. Le Benchmark officiel (gain x230)
.\my_jit_vm.exe --bench
```

---

## 2. Démonstrations & Résultats Clés à montrer au Jury

### Résultat 1 : La Bascule Automatique vers le Code Natif
Quand on lance `.\my_jit_vm.exe examples/fibonacci.tiny` :
```text
[JIT] Compilation native de 'fib' (1000 appels)...
[JIT] Compilation reussie -> bascule sur le code natif !
>> Resultat de examples/fibonacci.tiny : 6765
>> Temps d'execution : 0.243 ms
```
- Le programme commence en mode interprété.
- Au bout de 1 000 appels récursifs, le point chaud est détecté.
- `libgccjit` compile la fonction en tâche de fond en mémoire vive.
- L'exécution termine instantanément en code machine CPU direct.

### Résultat 2 : L'Assembleur x86-64 produit à la volée (`--dump-asm`)
Montrer au professeur que la pile virtuelle a disparu au profit des registres matériels :
```x86asm
fib:
    movq    (%rcx), %rax          # Charge n directement dans %rax
    cmpq    $1, %rax              # Optimise (n < 2) en comparaison matérielle
    jle     .L1                   # Saut conditionnel CPU direct
.L4:
    movabsq $140701962018782, %rbx# Adresse absolue de vm_run en mémoire
    ...
    call    *%rbx                 # Appel direct via pointeur
    addq    %rsi, %rax            # Addition matérielle dans %rax
```

### Résultat 3 : Le Benchmark d'Accélération ($232\times$ plus rapide)
Mesuré avec `QueryPerformanceCounter` sur 50 millions d'itérations :
- **Interpréteur pur :** `5 480 ms`
- **Code natif JIT pur :** `23,6 ms`
- **Gain :** **$232\times$ plus rapide !**

---

## 3. Foire Aux Questions (Questions Pièges du Professeur)

### Q1. Pourquoi avoir choisi une machine à pile plutôt qu'à registres ?
> **Réponse :** « Une machine à pile est beaucoup plus simple et élégante à cibler pour un compilateur en une passe, car l'évaluation des expressions se traduit naturellement par un parcours postfixé (empiler les opérandes, puis exécuter l'opérateur). C'est le modèle utilisé par la JVM (Java) et le bytecode Python. »

### Q2. Comment fonctionne le "Backpatching" dans votre compilateur ?
> **Réponse :** « Quand on compile un `if` ou un `while`, on émet l'instruction de saut conditionnel `OP_JUMP_IF_FALSE`, mais on ne sait pas encore à quelle adresse d'instruction se trouvera la fin du bloc. On émet donc un saut provisoire vers l'adresse 0, on mémorise l'index de cette instruction, et dès que le corps est entièrement compilé, on met à jour la valeur de l'argument avec l'adresse finale calculée. »

### Q3. Comment libgccjit sait-il quoi faire avec la pile d'évaluation de la VM ?
> **Réponse :** « Lors de la compilation JIT, nous n'émulons pas la pile en mémoire. Nous maintenons une pile virtuelle symbolique `vstack` qui accumule des nœuds d'expressions `gcc_jit_rvalue *`. Lorsque l'optimiseur `-O2` de libgccjit génère le code assembleur, il transforme ces expressions en registres processeur directs (`%rax`, `%rdx`, etc.), éliminant tous les accès mémoire inutiles. »

### Q4. Comment avez-vous géré les appels récursifs sous Windows (PE/COFF) ?
> **Réponse :** « Sous Windows, une bibliothèque dynamique en mémoire ne peut pas importer automatiquement des fonctions de l'exécutable hôte. Pour éviter les dépendances complexes d'édition de liens, nous utilisons `gcc_jit_context_new_call_through_ptr` : nous injectons directement l'adresse mémoire absolue de `vm_run` comme pointeur de fonction C. C'est ultra-rapide, direct et 100% portable. »

### Q5. Comment fonctionne l'interface TinyJIT Studio ?
> **Réponse :** « Le Studio est propulsé par une API locale en FastAPI qui communique par sous-processus et parsing de flux avec `my_jit_vm.exe`. Le frontend est un client HTML5/CSS3/JavaScript pur sans dépendance lourde qui offre un désassemblage complet du bytecode, une vue assembleur x86-64 typée Godbolt, un graphe de flot de contrôle et des graphiques de performance avec Chart.js. »
