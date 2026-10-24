#ifndef BYTECODE_H
#define BYTECODE_H

#include <stdint.h>
#include <stddef.h>

// Liste des instructions élémentaires de la machine virtuelle
typedef enum {
    OP_CONST,         // Charge une constante
    OP_LOAD,          // Charge une variable locale
    OP_STORE,         // Stocke le sommet de pile dans une variable
    OP_ADD,           // Additionne les 2 valeurs au sommet de pile
    OP_SUB,           // Soustrait (a - b)
    OP_MUL,           // Multiplie (a * b)
    OP_DIV,           // Division entière (a / b)
    OP_LT,            // Test d'infériorité (a < b)
    OP_EQ,            // Test d'égalité (a == b)
    OP_JUMP,          // Saut inconditionnel : arg = offset d'instruction cible
    OP_JUMP_IF_FALSE, // Saut si faux (0) : arg = offset d'instruction cible
    OP_CALL,          // Appel de fonction : arg = nombre d'arguments
    OP_RET            // Retourne la valeur au sommet
} OpCode;

// code opération + argument éventuel
typedef struct {
    OpCode op;
    int32_t arg;
} Instruction;

// Structure représentant une fonction compilée
typedef struct {
    char* name;
    int num_args;
    int num_locals;

    // Tableau dynamique des constantes numériques
    int64_t* constants;
    size_t const_count;
    size_t const_capacity;

    // Tableau dynamique des instructions de bytecode
    Instruction* code;
    size_t code_count;
    size_t code_capacity;

    // Données pour le JIT (détection des points chauds)
    int64_t call_count;
    void* native_fn; // Pointeur vers le code compilé à chaud (NULL par défaut)
} BytecodeFunction;

// Fonctions de gestion du bytecode
BytecodeFunction* create_function(const char* name, int num_args, int num_locals);
size_t add_constant(BytecodeFunction* fn, int64_t val);
size_t emit_instruction(BytecodeFunction* fn, OpCode op, int32_t arg);
void print_bytecode(const BytecodeFunction* fn);
void free_function(BytecodeFunction* fn);

#endif