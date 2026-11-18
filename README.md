# TinyJIT

Une machine virtuelle à bytecode écrite en C11 dotée d'un compilateur **Just-In-Time (JIT)** natif propulsé par `libgccjit`.

## Architecture

1. **Frontend (`src/compiler.c`)** : Lexer et parseur récursif descendant compilant le langage `.tiny` (`fn`, `var`, `if/else`, `while`, récursivité) en bytecode à pile.
2. **Interpréteur & Profiler (`src/vm.c`)** : Exécute le bytecode et profile la fréquence d'appel de chaque fonction.
3. **Compilateur JIT (`src/jit.c`)** : Dès qu'une fonction atteint le seuil de point chaud (`1000` appels), son bytecode est découpé en *Basic Blocks*, traduit en représentation intermédiaire `libgccjit` (optimisation `-O2`) et compilé en code machine x86-64 exécuté directement par le CPU.

## Prérequis (Windows / MSYS2)

```bash
pacman -S mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-libgccjit
```

## Compilation et exécution

```powershell
# Compiler la VM
.\build.ps1

# Lancer la suite de tests et le benchmark intégré
.\my_jit_vm.exe

# Exécuter un script Tiny
.\my_jit_vm.exe script.tiny

# Inspecter l'assembleur x86-64 généré à la volée par le JIT
.\my_jit_vm.exe script.tiny --dump-asm
```
