#include <stdio.h>
#include "bytecode.h"

int main(void) {
    // Création d'une fonction factice: add(a, b) -> a + b
    BytecodeFunction* fn = create_function("add", 2, 2);

    // Émission manuelle du bytecode :
    emit_instruction(fn, OP_LOAD, 0); // Charge a (slot 0)
    emit_instruction(fn, OP_LOAD, 1); // Charge b (slot 1)
    emit_instruction(fn, OP_ADD, 0);  // Additionne les deux
    emit_instruction(fn, OP_RET, 0);  // Retourne le résultat

    // Affichage 
    print_bytecode(fn);

    free_function(fn);
    return 0;
}