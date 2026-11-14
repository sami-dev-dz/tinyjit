#ifndef JIT_H
#define JIT_H

#include <stdint.h>
#include "bytecode.h"

// Seuil d'appels avant de déclencher la compilation à la volée
#define HOTSPOT_THRESHOLD 1000

// Prototype générique d'une fonction compilée en code machine natif :
// Prend un tableau d'arguments int64_t et renvoie un résultat int64_t
typedef int64_t (*JitNativeFn)(const int64_t* args);

// Initialise le moteur JIT (contexte global si nécessaire)
void jit_init(void);

// Compile une BytecodeFunction en code machine natif et remplit fn->native_fn
void jit_compile(BytecodeFunction* fn);

// Libère les ressources du compilateur JIT à la fin du programme
void jit_cleanup(void);

#endif