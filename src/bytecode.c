#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bytecode.h"

static char *copy_string(const char *src)
{
    size_t len = strlen(src) + 1;
    char *copy = malloc(len);

    if (copy != NULL) {
        memcpy(copy, src, len);
    }

    return copy;
}

BytecodeFunction *create_function(
    const char *name,
    int num_args,
    int num_locals
)
{
    BytecodeFunction *fn = malloc(sizeof(BytecodeFunction));

    if (fn == NULL) {
        return NULL;
    }

    fn->name = copy_string(name);

    if (fn->name == NULL) {
        free(fn);
        return NULL;
    }

    fn->num_args = num_args;
    fn->num_locals = num_locals;

    fn->constants = NULL;
    fn->const_count = 0;
    fn->const_capacity = 0;

    fn->code = NULL;
    fn->code_count = 0;
    fn->code_capacity = 0;

    fn->call_count = 0;
    fn->native_fn = NULL;

    return fn;
}

size_t add_constant(BytecodeFunction *fn, int64_t val)
{
    if (fn == NULL) {
        return (size_t)-1;
    }

    if (fn->const_count >= fn->const_capacity) {
        size_t new_capacity =
            fn->const_capacity == 0 ? 8 : fn->const_capacity * 2;

        int64_t *new_constants = realloc(
            fn->constants,
            new_capacity * sizeof(int64_t)
        );

        if (new_constants == NULL) {
            return (size_t)-1;
        }

        fn->constants = new_constants;
        fn->const_capacity = new_capacity;
    }

    size_t index = fn->const_count;
    fn->constants[index] = val;
    fn->const_count++;

    return index;
}

size_t emit_instruction(BytecodeFunction *fn, OpCode op, int32_t arg)
{
    if (fn == NULL) {
        return (size_t)-1;
    }

    if (fn->code_count >= fn->code_capacity) {
        size_t new_capacity =
            fn->code_capacity == 0 ? 16 : fn->code_capacity * 2;

        Instruction *new_code = realloc(
            fn->code,
            new_capacity * sizeof(Instruction)
        );

        if (new_code == NULL) {
            return (size_t)-1;
        }

        fn->code = new_code;
        fn->code_capacity = new_capacity;
    }

    size_t index = fn->code_count;

    fn->code[index].op = op;
    fn->code[index].arg = arg;
    fn->code_count++;

    return index;
}

static const char *opcode_to_string(OpCode op)
{
    switch (op) {
        case OP_CONST:          return "CONST";
        case OP_LOAD:           return "LOAD";
        case OP_STORE:          return "STORE";
        case OP_ADD:            return "ADD";
        case OP_SUB:            return "SUB";
        case OP_MUL:            return "MUL";
        case OP_DIV:            return "DIV";
        case OP_LT:             return "LT";
        case OP_EQ:             return "EQ";
        case OP_JUMP:           return "JUMP";
        case OP_JUMP_IF_FALSE:  return "JUMP_IF_FALSE";
        case OP_CALL:           return "CALL";
        case OP_RET:            return "RET";
        default:                return "UNKNOWN";
    }
}

void print_bytecode(const BytecodeFunction *fn)
{
    if (fn == NULL) {
        fprintf(stderr, "Erreur : fonction NULL.\n");
        return;
    }

    printf("Function: %s\n", fn->name);
    printf("Arguments: %d, Locals: %d\n",
           fn->num_args, fn->num_locals);

    printf("\nConstants:\n");

    for (size_t i = 0; i < fn->const_count; i++) {
        printf("  [%zu] = %lld\n",
               i, (long long)fn->constants[i]);
    }

    printf("\nInstructions:\n");

    for (size_t i = 0; i < fn->code_count; i++) {
        printf("  %zu: %-14s %d\n",
               i,
               opcode_to_string(fn->code[i].op),
               fn->code[i].arg);
    }
}

void free_function(BytecodeFunction *fn)
{
    if (fn == NULL) {
        return;
    }

    free(fn->name);
    free(fn->constants);
    free(fn->code);
    free(fn);
}