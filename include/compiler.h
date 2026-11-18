#ifndef COMPILER_H
#define COMPILER_H

#include <stddef.h>
#include "vm.h" 
typedef struct {
    BytecodeFunction **functions;
    size_t count;
    BytecodeFunction *entry_fn; // Pointeur vers la fonction 'main'
} Program;

// Compile un code source Tiny en un ensemble de fonctions bytecode
Program *compile_source(const char *source);

// Libère toutes les fonctions compilées du programme
void free_program(Program *prog);

#endif // COMPILER_H