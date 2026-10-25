# Soutenance TinyJIT — notes pour demain

**Soutenance :** 26 octobre 2026
**Durée visée :** 5 minutes de présentation + questions
**Dépôt :** <https://github.com/sami-dev-dz/tinyjit>

> **Message principal à faire passer :** le socle bytecode + VM fonctionne
> (`sum_to(10) = 55`) ; le front-end et le JIT sont les prochaines étapes, et
> leur conception est déjà préparée. Ne pas prétendre que le JIT fonctionne.

---

## 1. Checklist avant de passer

- [ ] Ouvrir le dépôt dans un terminal : `cd tinyjit`
- [ ] Vérifier Python : `python3 --version` (3.10+)
- [ ] Lancer la démo : `python3 src/main.py`
- [ ] Vérifier la dernière ligne : `>> Résultat de sum_to(10) sur la VM : 55`
- [ ] Avoir `README.md`, `RAPPORT.md` et les fichiers `src/*.py` ouverts
- [ ] Connaître le schéma de l'architecture (§ 3)
- [ ] Relire les 5 questions pièges (§ 6)
- [ ] Préparer une capture d'écran de la sortie, au cas où la démo échoue
- [ ] Arriver avec le dépôt déjà cloné (ne pas dépendre du Wi-Fi)

---

## 2. Pitch de 30 secondes (à mémoriser)

> « TinyJIT est un prototype de langage dynamique. Le principe est de compiler
> le programme en bytecode, de l'exécuter sur une machine virtuelle à pile, puis
> de compiler les fonctions les plus appelées en code natif avec libgccjit.
> Aujourd'hui, le socle est fonctionnel : le bytecode, la VM et une
> démonstration qui calcule la somme des entiers jusqu'à n. Le front-end et le
> JIT sont les prochaines étapes ; nous avons déjà défini leur architecture. »

Version courte si le professeur est pressé :

> « C'est un interpréteur de bytecode avec une architecture de JIT prévue :
> la VM marche, la compilation native reste à faire. »

---

## 3. Plan de présentation (5 minutes)

### 3.1 Le problème (30 s)

Un langage interprété est facile à porter, mais lent. Un compilateur classique
produit du code natif, mais compile tout, même le code exécuté une seule fois.
Le JIT est le compromis : on interprète au début, puis on compile ce qui est
réellement utilisé.

### 3.2 L'architecture (1 min)

Dessiner au tableau :

```text
source .src
   │
   ▼
lexer ──► parser ──► AST ──► compilateur ──► BytecodeFunction
                                                    │
                                    ┌───────────────┴───────────────┐
                                    ▼                               ▼
                               VM (fait)                       JIT (à faire)
                            interprétation                  code natif libgccjit
```

Insister sur la séparation :

- le **front-end** comprend le texte et produit une structure ;
- le **bytecode** est le contrat commun entre l'interpréteur et le JIT ;
- la **VM** exécute le bytecode ;
- le **JIT** compile le bytecode en code machine.

### 3.3 Ce qui marche (2 min)

Lancer la démo :

```bash
python3 src/main.py
```

Montrer :

1. le dump du bytecode : 2 constantes (`0`, `1`), 17 instructions ;
2. la boucle : instructions `0002` à `0014` ;
3. le saut de sortie `0005` vers `0015` (backpatch) ;
4. le résultat `55` pour `sum_to(10)`.

Expliquer la pile :

- `OP_CONST` empile une constante ;
- `OP_LOAD` empile une variable locale ;
- `OP_ADD` dépile deux valeurs et empile la somme ;
- `OP_STORE` dépile vers une variable locale ;
- `OP_JUMP_IF_FALSE` sort de la boucle quand `0 < n` devient faux.

### 3.4 Ce qui reste (1 min)

- `lexer.py` et `parser.py` : lire un vrai fichier `.src`.
- Le compilateur AST → bytecode.
- `OP_CALL` et les frames d'appel dans la VM.
- `jit.py` : compilation native avec libgccjit.
- Les tests (`add`, `fibonacci`, `loop`) et les benchmarks.

### 3.5 Conclusion (30 s)

> « Le projet est aujourd'hui un interpréteur de bytecode fonctionnel, avec une
> architecture prête pour le JIT. La prochaine étape est de remplacer
> l'exécution interprétée des fonctions chaudes par du code natif libgccjit,
> puis de valider par des tests différentiels VM / JIT. »

---

## 4. Démonstration : ce qu'il faut dire pendant la sortie

Commande :

```bash
python3 src/main.py
```

Sortie attendue (extrait) :

```text
=== Bytecode pour la fonction: sum_to ===
Arguments: 1, Variables locales: 2
Constantes (2) : [0, 1]
Instructions (17) :
  0000: OP_CONST           0
  0001: OP_STORE           1
  0002: OP_CONST           0
  ...
  0014: OP_JUMP            2
  0015: OP_LOAD            1
  0016: OP_RET             0
========================================

>> Résultat de sum_to(10) sur la VM : 55
```

Commentaire oral :

- « La fonction prend un argument `n` et utilise deux slots locaux : `n` en
  slot 0, `acc` en slot 1. »
- « Le bytecode est une machine à pile : les opérations consomment et
  produisent des valeurs au sommet de la pile. »
- « Le saut `0005` est d'abord émis avec une cible vide, puis retouché quand on
  connaît la fin de la boucle : c'est le *backpatch*. »
- « La VM initialise les variables locales avec les arguments, puis complète
  avec des zéros. »
- « Pour `n = 10`, la boucle fait 10 tours et `acc` vaut 55. »

Si le professeur demande de montrer le code :

1. `src/bytecode.py` : `OpCode`, `Instruction`, `BytecodeFunction`.
2. `src/vm.py` : la boucle `while ip < code_len` et le dispatch.
3. `src/main.py` : la construction de `sum_to` et le backpatch.

---

## 5. Explication du code en 3 fichiers

### `src/bytecode.py`

- `OpCode` : les 13 opérations.
- `Instruction(op, arg)` : une instruction + son argument.
- `BytecodeFunction` : nom, nombre d'arguments, nombre de variables locales,
  pool de constantes, code, `call_count`, `native_fn`.
- `add_constant()` : ajoute une constante et renvoie son index.
- `emit()` : ajoute une instruction et renvoie son index (utile pour le
  backpatch).
- `dump()` : affiche le bytecode.

### `src/vm.py`

- `stack` : pile d'opérandes.
- `locals` : variables locales.
- `ip` : index de l'instruction courante.
- La boucle lit `fn.code[ip]`, incrémente `ip`, puis exécute l'opcode.
- Les sauts modifient directement `ip`.
- `OP_RET` renvoie le sommet de pile.

### `src/main.py`

- Construit `sum_to` instruction par instruction.
- `loop_start` mémorise l'index de début de boucle.
- `jump_exit` mémorise l'instruction de saut dont la cible sera complétée.
- `vm.run(fn, [10])` exécute la fonction.
- Affiche `55`.

---

## 6. Questions probables et réponses

### Q1. « C'est quoi un JIT ? »

> « *Just-In-Time* : au lieu de compiler tout le programme avant l'exécution,
> on compile une fonction au moment où elle devient chaude, c'est-à-dire
> souvent appelée. On garde le code natif en cache pour les appels suivants.
> C'est un compromis entre interprétation et compilation ahead-of-time. »

### Q2. « Où est le JIT dans votre projet ? »

Réponse honnête :

> « Il n'est pas encore implémenté : `src/jit.py` est vide. Ce qui est déjà en
> place, c'est le socle : le bytecode, la VM, et les champs `call_count` et
> `native_fn` qui serviront à déclencher et à stocker la compilation. La
> prochaine étape est d'utiliser libgccjit pour compiler une `BytecodeFunction`
> en code natif. »

Ne pas dire « il marche » ou « il est presque fini ». Dire précisément ce qui
est fait et ce qui ne l'est pas.

### Q3. « Pourquoi passer par du bytecode ? »

> « Le bytecode est une représentation intermédiaire simple et portable. Il
> découple le front-end (analyse du langage) du back-end (VM ou JIT). La même
> fonction peut être interprétée au début, puis compilée plus tard sans
> réécrire le parser. »

### Q4. « Pourquoi Python et pas C ? »

> « Le projet a commencé en C : il y avait un `Makefile`, des en-têtes pour
> l'AST, le parser, le compilateur et le JIT. Le bytecode a d'abord été
> implémenté en C, puis nous sommes passés à Python pour prototyper plus vite
> la VM et les tests. La performance de la VM Python n'est pas critique, parce
> que l'objectif final est justement de générer du code natif avec libgccjit. »

Si on demande pourquoi ne pas revenir au C :

> « C'est possible pour le JIT : libgccjit est une bibliothèque C. On peut
> l'appeler depuis Python avec `ctypes`, ou écrire une petite passerelle en C.
> Le reste du projet reste en Python pour la lisibilité. »

### Q5. « Comment fonctionne la VM ? »

> « C'est une machine à pile : `ip` parcourt les instructions, `stack` stocke
> les opérandes, `locals` stocke les variables. Chaque opcode dépile ses
> opérandes et empile son résultat. Les sauts modifient `ip`. »

### Q6. « Comment allez-vous compiler les sauts ? »

> « Chaque index qui est la cible d'un saut devient un bloc de base dans
> libgccjit. Un saut inconditionnel devient un saut vers ce bloc ; un saut
> conditionnel devient un branchement vers le bloc cible ou vers le bloc
> suivant. Il faudra aussi gérer les valeurs de pile qui arrivent par plusieurs
> chemins. »

### Q7. « Et la division par zéro ? »

> « La VM lève une exception. En code natif, une division par zéro provoque un
> `SIGFPE`. Le JIT devra donc tester le diviseur avant l'opération et appeler
> un chemin d'erreur, ou retomber sur l'interpréteur. »

### Q8. « Vous utilisez des entiers de quelle taille ? »

> « Pour l'instant, la VM Python utilise des entiers de taille arbitraire. Le
> code natif utiliserait `int64_t`. Il faut donc fixer la sémantique du
> langage : le plus simple est de définir les entiers comme signés 64 bits et
> de vérifier les débordements, ou de documenter la différence. »

### Q9. « Pourquoi libgccjit plutôt que LLVM ? »

> « libgccjit est plus léger et fournit une API C adaptée à un projet
> pédagogique. LLVM est plus puissant mais beaucoup plus lourd à intégrer. Pour
> un petit langage, libgccjit suffit largement. »

### Q10. « Est-ce que ça marche pour Fibonacci ? »

> « Pas encore. Les fichiers `tests/fibonacci.src` et `tests/loop.src` sont
> vides, et `OP_CALL` n'est pas implémenté dans la VM. La démonstration actuelle
> est `sum_to`, construite manuellement. Fibonacci est justement un bon test
> pour valider les appels et la récursion. »

### Q11. « Qui a fait quoi ? »

Répondre honnêtement, sans inventer. Exemple de trame à adapter :

> « Le socle bytecode et la VM ont été développés dans le dépôt ; je me suis
> joint au projet pour [ta contribution réelle : documentation, tests, relecture,
> préparation de la démo...]. Nous avons revu ensemble l'architecture et le plan
> du JIT. »

Si tu n'as pas encore écrit de code, ne prétends pas le contraire. Tu peux dire :

> « Pour l'instant j'ai documenté l'état du projet, vérifié la démonstration et
> préparé la partie JIT ; la prochaine étape est d'implémenter `jit.py`. »

### Q12. « C'est un vrai langage ? »

> « C'est un prototype : le bytecode et la VM sont réels, mais il n'y a pas
> encore de parser. Le langage source sera ajouté avec `lexer.py` et
> `parser.py`. Aujourd'hui, les fonctions sont construites directement en
> bytecode. »

---

## 7. Phrases à éviter

- ❌ « Le JIT est fini. » → faux, `jit.py` est vide.
- ❌ « C'est un compilateur complet. » → il n'y a pas encore de parser.
- ❌ « Ça marche pour tous les programmes. » → seulement `sum_to`, construit à
  la main.
- ❌ « Je n'ai rien fait. » → tu as au moins documenté et vérifié le projet.
- ❌ Blâmer un camarade devant le professeur. → parler de l'état du dépôt, pas
  des personnes.

---

## 8. Si le professeur demande une démo du JIT

Réponse possible :

> « La démo disponible aujourd'hui est celle de la VM. Le JIT n'est pas encore
> branché ; voici le point d'insertion prévu : dans la VM, après
> l'incrémentation de `call_count`, si la fonction dépasse le seuil, on appelle
> `jit.compile(fn)`, on stocke le pointeur dans `fn.native_fn`, et les appels
> suivants passent par `ctypes`. »

Puis montrer `src/jit.py` (vide) et expliquer que c'est la prochaine étape.
Mieux vaut être clair que bluffer.

---

## 9. Fiche mémo : vocabulaire

| Terme | Définition courte |
| --- | --- |
| **Bytecode** | Représentation intermédiaire d'un programme, exécutée par une VM |
| **VM** | Machine virtuelle qui interprète le bytecode |
| **Pile d'opérandes** | Structure LIFO utilisée pour évaluer les expressions |
| **Instruction pointer (`ip`)** | Index de l'instruction courante |
| **Backpatch** | Écrire la cible d'un saut après avoir émis la suite du code |
| **Fonction chaude** | Fonction appelée suffisamment souvent pour justifier une compilation |
| **JIT** | Compilation en code natif au moment de l'exécution |
| **libgccjit** | Bibliothèque de GCC pour compiler du code à la volée |
| **Bloc de base** | Suite d'instructions sans branchement interne |
| **SSA** | Forme où chaque variable n'est assignée qu'une fois |
| **ABI** | Convention d'appel entre fonctions (registres, pile, types) |
| **Test différentiel** | Comparer les résultats de deux implémentations (VM vs JIT) |

---

## 10. Conclusion en une phrase

> « TinyJIT est aujourd'hui un interpréteur de bytecode fonctionnel, avec une
> architecture et un plan précis pour ajouter la compilation JIT avec
> libgccjit ; la démonstration `sum_to(10) = 55` valide le socle. »
