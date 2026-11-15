#include <stdio.h>
#include <stdint.h>
#include <windows.h>

#include "bytecode.h"
#include "vm.h"
#include "jit.h"

// Crée une fonction : add(a, b) -> a + b
static BytecodeFunction *build_add_function(void)
{
    BytecodeFunction *fn = create_function("add", 2, 2);

    emit_instruction(fn, OP_LOAD, 0);  // Charge a (slot 0)
    emit_instruction(fn, OP_LOAD, 1);  // Charge b (slot 1)
    emit_instruction(fn, OP_ADD, 0);   // a + b
    emit_instruction(fn, OP_RET, 0);   // Retourne le résultat

    return fn;
}

// Crée une fonction : sum_to(n) -> 1 + 2 + ... + n
static BytecodeFunction *build_sum_to_function(void)
{
    // Slot 0 = n (argument)
    // Slot 1 = acc (accumulateur)
    BytecodeFunction *fn = create_function("sum_to", 1, 2);

    int32_t c0 = (int32_t)add_constant(fn, 0);
    int32_t c1 = (int32_t)add_constant(fn, 1);

    // acc = 0
    emit_instruction(fn, OP_CONST, c0);
    emit_instruction(fn, OP_STORE, 1);

    // Début de la boucle : tester si 0 < n
    int32_t loop_start = (int32_t)fn->code_count;

    emit_instruction(fn, OP_CONST, c0);
    emit_instruction(fn, OP_LOAD, 0);
    emit_instruction(fn, OP_LT, 0);

    // Si faux, sauter vers la fin
    size_t jump_exit = emit_instruction(fn, OP_JUMP_IF_FALSE, 0);

    // acc = acc + n
    emit_instruction(fn, OP_LOAD, 1);
    emit_instruction(fn, OP_LOAD, 0);
    emit_instruction(fn, OP_ADD, 0);
    emit_instruction(fn, OP_STORE, 1);

    // n = n - 1
    emit_instruction(fn, OP_LOAD, 0);
    emit_instruction(fn, OP_CONST, c1);
    emit_instruction(fn, OP_SUB, 0);
    emit_instruction(fn, OP_STORE, 0);

    // Reboucler
    emit_instruction(fn, OP_JUMP, loop_start);

    // Adresse de sortie
    fn->code[jump_exit].arg = (int32_t)fn->code_count;

    // return acc
    emit_instruction(fn, OP_LOAD, 1);
    emit_instruction(fn, OP_RET, 0);

    return fn;
}

// Crée une fonction : square(x) -> x * x
static BytecodeFunction *build_square_function(void)
{
    BytecodeFunction *fn = create_function("square", 1, 1);

    emit_instruction(fn, OP_LOAD, 0); // x
    emit_instruction(fn, OP_LOAD, 0); // x
    emit_instruction(fn, OP_MUL, 0);  // x * x
    emit_instruction(fn, OP_RET, 0);

    return fn;
}

// Crée une fonction : sum_of_squares(a, b) -> square(a) + square(b) via OP_CALL
static BytecodeFunction *build_sum_of_squares_function(BytecodeFunction *fn_square)
{
    BytecodeFunction *fn = create_function("sum_of_squares", 2, 2);

    // Enregistrer square dans la func_table
    size_t square_idx = add_function(fn, fn_square);

    // square(a)
    emit_instruction(fn, OP_LOAD, 0);
    emit_instruction(fn, OP_CALL, (int32_t)square_idx);

    // square(b)
    emit_instruction(fn, OP_LOAD, 1);
    emit_instruction(fn, OP_CALL, (int32_t)square_idx);

    // square(a) + square(b)
    emit_instruction(fn, OP_ADD, 0);
    emit_instruction(fn, OP_RET, 0);

    return fn;
}

// Mesure du temps en millisecondes
static double get_time_ms(void)
{
    LARGE_INTEGER freq, counter;

    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&counter);

    return (double)counter.QuadPart * 1000.0 /
           (double)freq.QuadPart;
}

int main(void)
{
    jit_init();

    // ==========================================
    // TEST 1 : ADD
    // ==========================================

    printf("=== Test 1 : add(10, 32) ===\n");

    BytecodeFunction *fn_add = build_add_function();
    print_bytecode(fn_add);

    int64_t add_args[] = {10, 32};
    int64_t add_res = vm_run(fn_add, add_args);

    printf(">> Resultat VM add(10, 32) = %lld\n\n", (long long)add_res);
    free_function(fn_add);

    // ==========================================
    // TEST 2 : SUM_TO AVEC DETECTION HOTSPOT
    // ==========================================

    printf("=== Test 2 : sum_to(10) avec detection de point chaud ===\n");

    BytecodeFunction *fn_sum = build_sum_to_function();
    print_bytecode(fn_sum);

    int64_t sum_args[] = {10};
    int64_t sum_res = 0;

    // 1500 appels pour dépasser le seuil HOTSPOT_THRESHOLD
    for (int i = 0; i < 1500; i++)
    {
        sum_res = vm_run(fn_sum, sum_args);
    }

    printf(">> Resultat VM sum_to(10) apres 1500 appels = %lld (attendu : 55)\n\n",
           (long long)sum_res);

    free_function(fn_sum);

    // ==========================================
    // TEST 3 : OP_CALL (INTERPRETE ET JIT)
    // ==========================================

    printf("=== Test 3 : OP_CALL (sum_of_squares(3, 4) -> 3*3 + 4*4 = 25) ===\n");

    BytecodeFunction *fn_sq = build_square_function();
    BytecodeFunction *fn_sos = build_sum_of_squares_function(fn_sq);

    print_bytecode(fn_sos);

    int64_t sos_args[] = {3, 4};
    int64_t sos_res = 0;

    // 1500 appels pour tester à la fois l'interpréteur et la compilation native avec OP_CALL
    for (int i = 0; i < 1500; i++)
    {
        sos_res = vm_run(fn_sos, sos_args);
    }

    printf(">> Resultat sum_of_squares(3, 4) = %lld (attendu : 25)\n\n",
           (long long)sos_res);

    free_function(fn_sos);
    free_function(fn_sq);

    // ==========================================
    // TEST 4 : BENCHMARK INTERPRETEUR VS JIT
    // ==========================================

    printf("=== Test 4 : Benchmark Interpreteur vs JIT ===\n");
    printf("sum_to(10000) x 5000 appels\n\n");

    int64_t bench_args[] = {10000};
    int64_t bench_res = 0;

    // 1. INTERPRETEUR SEUL
    BytecodeFunction *fn_interp = build_sum_to_function();
    fn_interp->call_count = -1000000; // Empêcher le déclenchement du JIT

    double t0 = get_time_ms();
    for (int i = 0; i < 5000; i++)
    {
        bench_res = vm_run(fn_interp, bench_args);
    }
    double t_interp = get_time_ms() - t0;

    // 2. MODE HYBRIDE : INTERPRETEUR + JIT
    BytecodeFunction *fn_jit = build_sum_to_function();

    double t1 = get_time_ms();
    for (int i = 0; i < 5000; i++)
    {
        bench_res = vm_run(fn_jit, bench_args);
    }
    double t_jit = get_time_ms() - t1;

    // 3. JIT DEJA CHAUD (CODE NATIF PUR)
    double t2 = get_time_ms();
    for (int i = 0; i < 5000; i++)
    {
        bench_res = vm_run(fn_jit, bench_args);
    }
    double t_pure_native = get_time_ms() - t2;

    printf(">> Resultat final : %lld (attendu : 50005000)\n", (long long)bench_res);
    printf(">> Temps Interpreteur seul : %.3f ms\n", t_interp);
    printf(">> Temps Hybride (JIT)     : %.3f ms\n", t_jit);
    printf(">> Temps Natif pur         : %.3f ms\n", t_pure_native);
    printf(">> Acceleration            : %.1fx plus rapide !\n\n",
           t_interp / t_pure_native);

    free_function(fn_interp);
    free_function(fn_jit);

    jit_cleanup();
    return 0;
}
