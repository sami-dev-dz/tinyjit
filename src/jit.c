#include <stdio.h>
#include "jit.h"

void jit_init(void) {
    // Sera utilisé pour initialiser libgccjit
}

void jit_compile(BytecodeFunction* fn) {
    if (fn->native_fn != NULL) {
        return; 
    }

    printf("[JIT] Point chaud détecté pour '%s' (%ld appels) -> Compilation en cours...\n",
           fn->name, (long)fn->call_count);

    // À l'étape suivante, libgccjit générera ici le vrai pointeur machine
    // et l'assignera à fn->native_fn
}

void jit_cleanup(void) {
    // Libération des contextes libgccjit
}