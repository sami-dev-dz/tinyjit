# TinyJIT

TinyJIT est un prototype de langage dynamique : le code source est compilé en
bytecode, exécuté par une machine virtuelle à pile, puis les fonctions les plus
appelées (« fonctions chaudes ») sont destinées à être compilées en code natif
par un JIT basé sur **libgccjit**.

> **État au 25 octobre 2026**
>
> Le socle est fonctionnel : définition du bytecode, VM à pile et démonstration
> `sum_to`. Le front-end (lexer, parser, compilateur) et le JIT ne sont pas
> encore implémentés. Voir [RAPPORT.md](RAPPORT.md) pour le détail technique et
> [PRESENTATION.md](PRESENTATION.md) pour les notes de soutenance.

## Démonstration rapide

```bash
python3 src/main.py
```

Le programme construit à la main le bytecode de `sum_to(n)` (somme des entiers
de `1` à `n`), affiche le bytecode puis l'exécute sur la VM :

```text
=== Bytecode pour la fonction: sum_to ===
Arguments: 1, Variables locales: 2
Constantes (2) : [0, 1]
Instructions (17) :
  0000: OP_CONST           0
  ...
  0016: OP_RET             0
========================================

>> Résultat de sum_to(10) sur la VM : 55
```

## Structure du dépôt

| Fichier | État | Rôle |
| --- | --- | --- |
| `src/bytecode.py` | ✅ implémenté | Opcodes, instructions, `BytecodeFunction`, pool de constantes, émission et dump |
| `src/vm.py` | ✅ implémenté | Interpréteur à pile : arithmétique, comparaisons, sauts, variables locales, retour |
| `src/main.py` | ✅ implémenté | Construit le bytecode de `sum_to` et le fait tourner sur la VM |
| `src/lexer.py` | ⬜ vide | Analyse lexicale du langage source (à faire) |
| `src/parser.py` | ⬜ vide | Analyse syntaxique et AST (à faire) |
| `src/jit.py` | ⬜ vide | Compilation native des fonctions chaudes avec libgccjit (à faire) |
| `tests/*.src` | ⬜ vides | Programmes de test `add`, `fibonacci`, `loop` (à écrire) |

## Pipeline visé

```text
source .src
    │
    ▼
 lexer.py ──► parser.py ──► AST ──► compilateur ──► BytecodeFunction
                                                          │
                                          ┌───────────────┴───────────────┐
                                          ▼                               ▼
                                     vm.py (fait)                    jit.py (à faire)
                                  interprétation                  code natif (libgccjit)
```

Aujourd'hui, la flèche « source → bytecode » n'existe pas encore : `main.py`
construit le bytecode directement avec `emit()`.

## Prérequis

- Python 3.10 ou plus récent (testé avec Python 3.14.8) ;
- aucune dépendance Python pour la VM : `requirements.txt` est vide ;
- **libgccjit** sera nécessaire pour la phase JIT (testé avec la version
  16.2.1 installée sur la machine de développement).

## Limites connues

- Aucun langage source n'est encore reconnu : pas de lexer, pas de parser, pas
  de compilateur.
- Le bytecode ne manipule que des entiers ; les booléens sont représentés par
  `0` et `1`.
- `OP_CALL` est déclaré dans `bytecode.py` mais n'est **pas** exécuté par la VM.
- La VM utilise une pile et un tableau de variables locaux globaux à
  l'instance : la récursion n'est donc pas supportée en l'état.
- `src/jit.py` est vide : **le JIT n'est pas encore fonctionnel.**
- Les fichiers `tests/*.src` sont vides : il n'y a pas encore de tests
  automatiques.

## Prochaines étapes

1. Écrire le lexer et le parser pour le petit langage (`.src`).
2. Ajouter un compilateur AST → bytecode.
3. Implémenter `OP_CALL` et les frames d'appel dans la VM.
4. Implémenter le JIT libgccjit : seuil de chauffe, blocs de base, sauts,
   appels, génération du code natif.
5. Écrire les tests (`add`, `fibonacci`, `loop`) et comparer VM / JIT.
6. Mesurer les performances et documenter les résultats.
