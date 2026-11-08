#include <stdio.h>
#include "bytecode.h"
#include "vm.h"

// Crée une fonction: add(a, b) -> a + b
static BytecodeFunction *build_add_function(void)
{
    BytecodeFunction *fn = create_function("add", 2, 2);

    emit_instruction(fn, OP_LOAD, 0); // Charge a (slot 0)
    emit_instruction(fn, OP_LOAD, 1); // Charge b (slot 1)
    emit_instruction(fn, OP_ADD, 0);  // a + b
    emit_instruction(fn, OP_RET, 0);  // Retourne le résultat

    return fn;
}

// Crée une fonction: sum_to(n) -> 1 + 2 + ... + n
static BytecodeFunction *build_sum_to_function(void)
{
    // Slot 0 = n (argument)
    // Slot 1 = acc (accumulateur initialisé à 0)
    BytecodeFunction *fn = create_function("sum_to", 1, 2);

    int32_t c0 = (int32_t)add_constant(fn, 0);
    int32_t c1 = (int32_t)add_constant(fn, 1);

    // 0: acc = 0
    emit_instruction(fn, OP_CONST, c0);
    emit_instruction(fn, OP_STORE, 1);

    // 2: DEBUT BOUCLE : tester si 0 < n (c'est-à-dire n > 0)
    int32_t loop_start = (int32_t)fn->code_count;
    emit_instruction(fn, OP_CONST, c0);
    emit_instruction(fn, OP_LOAD, 0);
    emit_instruction(fn, OP_LT, 0);

    // 5: Si faux, sauter vers la fin (offset backpatché ensuite)
    size_t jump_exit = emit_instruction(fn, OP_JUMP_IF_FALSE, 0);

    // 6: acc = acc + n
    emit_instruction(fn, OP_LOAD, 1);
    emit_instruction(fn, OP_LOAD, 0);
    emit_instruction(fn, OP_ADD, 0);
    emit_instruction(fn, OP_STORE, 1);

    // 10: n = n - 1
    emit_instruction(fn, OP_LOAD, 0);
    emit_instruction(fn, OP_CONST, c1);
    emit_instruction(fn, OP_SUB, 0);
    emit_instruction(fn, OP_STORE, 0);

    // 14: Reboucler
    emit_instruction(fn, OP_JUMP, loop_start);

    // Backpatch de l'adresse de sortie
    fn->code[jump_exit].arg = (int32_t)fn->code_count;

    // 15: return acc
    emit_instruction(fn, OP_LOAD, 1);
    emit_instruction(fn, OP_RET, 0);

    return fn;
}

int main(void)
{
    VM vm;
    vm_init(&vm);

    printf("=== Test 1 : add(10, 32) ===\n");
    BytecodeFunction *fn_add = build_add_function();
    print_bytecode(fn_add);

    int64_t add_args[] = {10, 32};
    int64_t add_res = vm_run(&vm, fn_add, add_args, 2);
    printf(">> Resultat VM add(10, 32) = %lld\n\n", (long long)add_res);
    free_function(fn_add);

    printf("=== Test 2 : sum_to(10) ===\n");
    BytecodeFunction *fn_sum = build_sum_to_function();
    print_bytecode(fn_sum);

    int64_t sum_args[] = {10};
    int64_t sum_res = vm_run(&vm, fn_sum, sum_args, 1);
    printf(">> Resultat VM sum_to(10) = %lld (attendu: 55)\n", (long long)sum_res);
    free_function(fn_sum);

    return 0;
}
