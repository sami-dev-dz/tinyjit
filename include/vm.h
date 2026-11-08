#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stddef.h>

#include "bytecode.h"

#define VM_STACK_CAPACITY 1024
#define VM_LOCALS_CAPACITY 256

// Structure représentant l'état de la machine virtuelle à pile
typedef struct {
    int64_t stack[VM_STACK_CAPACITY];
    size_t sp; // Pointeur du sommet de la pile

    int64_t locals[VM_LOCALS_CAPACITY];
    size_t num_locals;
} VM;

// Initialise l'état de la machine virtuelle
void vm_init(VM *vm);

// Exécute une fonction de bytecode avec les arguments donnés
// Retourne la valeur retournée au sommet de la pile
int64_t vm_run(VM *vm, BytecodeFunction *fn, const int64_t *args, size_t num_args);

#endif
