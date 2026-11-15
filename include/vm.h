#ifndef VM_H
#define VM_H

#include <stdint.h>
#include <stddef.h>
#include "bytecode.h"

#define VM_STACK_CAPACITY 1024
#define VM_LOCALS_CAPACITY 256

#ifdef _WIN32
#define JIT_EXPORT __declspec(dllexport)
#else
#define JIT_EXPORT
#endif

typedef struct {
    int64_t stack[VM_STACK_CAPACITY];
    size_t sp;
    int64_t locals[VM_LOCALS_CAPACITY];
    size_t num_locals;
} VM;

void vm_init(VM *vm);

// Exécute une fonction de bytecode avec ses arguments
// Exportée pour que le compilateur JIT puisse lier nativement cette fonction
JIT_EXPORT int64_t vm_run(BytecodeFunction *fn, const int64_t *args);

#endif
