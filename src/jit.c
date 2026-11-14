#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <libgccjit.h>
#include "jit.h"

// Tableau pour conserver les résultats compilés et pouvoir les libérer à la fin
static gcc_jit_result **compiled_results = NULL;
static size_t results_count = 0;

void jit_init(void) {
    compiled_results = NULL;
    results_count = 0;
}

void jit_compile(BytecodeFunction *fn) {
    if (fn == NULL || fn->native_fn != NULL) {
        return;
    }

    printf("[JIT] Compilation native de '%s' (%ld appels)...\n",
           fn->name, (long)fn->call_count);

    // 1. Créer le contexte de compilation libgccjit
    gcc_jit_context *ctxt = gcc_jit_context_acquire();
    if (!ctxt) {
        fprintf(stderr, "[JIT] Erreur: impossible d'acquerir le contexte libgccjit\n");
        return;
    }

    // Niveau d'optimisation (O2 pour du code machine rapide)
    gcc_jit_context_set_int_option(ctxt, GCC_JIT_INT_OPTION_OPTIMIZATION_LEVEL, 2);

    // 2. Définir les types C équivalents : int64_t et const int64_t*
    gcc_jit_type *t_int64 = gcc_jit_context_get_type(ctxt, GCC_JIT_TYPE_LONG_LONG);
    gcc_jit_type *t_const_int64 = gcc_jit_type_get_const(t_int64);
    gcc_jit_type *t_args_ptr = gcc_jit_type_get_pointer(t_const_int64);

    // 3. Créer la signature de la fonction native : int64_t fn_jit(const int64_t *args)
    gcc_jit_param *param_args = gcc_jit_context_new_param(ctxt, NULL, t_args_ptr, "args");
    gcc_jit_function *jit_fn = gcc_jit_context_new_function(
        ctxt, NULL,
        GCC_JIT_FUNCTION_EXPORTED,
        t_int64,
        fn->name,
        1, &param_args,
        0
    );

    // 4. Allouer les variables locales natives (locals[0..num_locals-1])
    gcc_jit_lvalue **locals = calloc((size_t)fn->num_locals, sizeof(gcc_jit_lvalue *));
    for (int i = 0; i < fn->num_locals; i++) {
        char var_name[32];
        snprintf(var_name, sizeof(var_name), "local_%d", i);
        locals[i] = gcc_jit_function_new_local(jit_fn, NULL, t_int64, var_name);
    }

    // 5. Créer le bloc d'entrée EN PREMIER (libgccjit démarre toujours sur le 1er bloc créé)
    gcc_jit_block *entry_block = gcc_jit_function_new_block(jit_fn, "entry");

    // 6. Repérer les cibles de saut pour découper le code en Basic Blocks
    bool *is_block_start = calloc(fn->code_count + 1, sizeof(bool));
    is_block_start[0] = true;
    for (size_t i = 0; i < fn->code_count; i++) {
        Instruction inst = fn->code[i];
        if (inst.op == OP_JUMP || inst.op == OP_JUMP_IF_FALSE) {
            if (inst.arg >= 0 && (size_t)inst.arg < fn->code_count) {
                is_block_start[inst.arg] = true;
            }
            if (i + 1 < fn->code_count) {
                is_block_start[i + 1] = true;
            }
        }
    }

    gcc_jit_block **blocks = calloc(fn->code_count, sizeof(gcc_jit_block *));
    for (size_t i = 0; i < fn->code_count; i++) {
        if (is_block_start[i]) {
            char blk_name[32];
            snprintf(blk_name, sizeof(blk_name), "bb_%zu", i);
            blocks[i] = gcc_jit_function_new_block(jit_fn, blk_name);
        }
    }

    // Remplir le bloc d'entrée : copier args[i] dans local_i et initialiser les autres à 0
    gcc_jit_rvalue *r_args = gcc_jit_param_as_rvalue(param_args);

    for (int i = 0; i < fn->num_locals; i++) {
        if (i < fn->num_args) {
            gcc_jit_rvalue *idx = gcc_jit_context_new_rvalue_from_int(ctxt, t_int64, i);
            gcc_jit_lvalue *arg_elem = gcc_jit_context_new_array_access(ctxt, NULL, r_args, idx);
            gcc_jit_block_add_assignment(entry_block, NULL, locals[i], gcc_jit_lvalue_as_rvalue(arg_elem));
        } else {
            gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctxt, t_int64, 0);
            gcc_jit_block_add_assignment(entry_block, NULL, locals[i], zero);
        }
    }
    gcc_jit_block_end_with_jump(entry_block, NULL, blocks[0]);

    // 7. Traduction du bytecode vers l'IR libgccjit (pile virtuelle à la compilation)
    gcc_jit_rvalue *vstack[256];
    int vsp = 0;
    gcc_jit_block *curr_block = blocks[0];
    bool block_terminated = false;

    for (size_t i = 0; i < fn->code_count; i++) {
        // Si une nouvelle étiquette de bloc commence ici
        if (i > 0 && blocks[i] != NULL) {
            if (!block_terminated) {
                gcc_jit_block_end_with_jump(curr_block, NULL, blocks[i]);
            }
            curr_block = blocks[i];
            block_terminated = false;
        }

        Instruction inst = fn->code[i];

        switch (inst.op) {
        case OP_CONST: {
            int64_t val = fn->constants[inst.arg];
            vstack[vsp++] = gcc_jit_context_new_rvalue_from_long(ctxt, t_int64, (long)val);
            break;
        }

        case OP_LOAD: {
            vstack[vsp++] = gcc_jit_lvalue_as_rvalue(locals[inst.arg]);
            break;
        }

        case OP_STORE: {
            gcc_jit_rvalue *val = vstack[--vsp];
            gcc_jit_block_add_assignment(curr_block, NULL, locals[inst.arg], val);
            break;
        }

        case OP_ADD: {
            gcc_jit_rvalue *b = vstack[--vsp];
            gcc_jit_rvalue *a = vstack[--vsp];
            vstack[vsp++] = gcc_jit_context_new_binary_op(
                ctxt, NULL, GCC_JIT_BINARY_OP_PLUS, t_int64, a, b);
            break;
        }

        case OP_SUB: {
            gcc_jit_rvalue *b = vstack[--vsp];
            gcc_jit_rvalue *a = vstack[--vsp];
            vstack[vsp++] = gcc_jit_context_new_binary_op(
                ctxt, NULL, GCC_JIT_BINARY_OP_MINUS, t_int64, a, b);
            break;
        }

        case OP_MUL: {
            gcc_jit_rvalue *b = vstack[--vsp];
            gcc_jit_rvalue *a = vstack[--vsp];
            vstack[vsp++] = gcc_jit_context_new_binary_op(
                ctxt, NULL, GCC_JIT_BINARY_OP_MULT, t_int64, a, b);
            break;
        }

        case OP_DIV: {
            gcc_jit_rvalue *b = vstack[--vsp];
            gcc_jit_rvalue *a = vstack[--vsp];
            vstack[vsp++] = gcc_jit_context_new_binary_op(
                ctxt, NULL, GCC_JIT_BINARY_OP_DIVIDE, t_int64, a, b);
            break;
        }

        case OP_LT: {
            gcc_jit_rvalue *b = vstack[--vsp];
            gcc_jit_rvalue *a = vstack[--vsp];
            gcc_jit_rvalue *cmp = gcc_jit_context_new_comparison(
                ctxt, NULL, GCC_JIT_COMPARISON_LT, a, b);
            vstack[vsp++] = gcc_jit_context_new_cast(ctxt, NULL, cmp, t_int64);
            break;
        }

        case OP_EQ: {
            gcc_jit_rvalue *b = vstack[--vsp];
            gcc_jit_rvalue *a = vstack[--vsp];
            gcc_jit_rvalue *cmp = gcc_jit_context_new_comparison(
                ctxt, NULL, GCC_JIT_COMPARISON_EQ, a, b);
            vstack[vsp++] = gcc_jit_context_new_cast(ctxt, NULL, cmp, t_int64);
            break;
        }

        case OP_JUMP: {
            gcc_jit_block_end_with_jump(curr_block, NULL, blocks[inst.arg]);
            block_terminated = true;
            break;
        }

        case OP_JUMP_IF_FALSE: {
            gcc_jit_rvalue *cond_val = vstack[--vsp];
            gcc_jit_rvalue *zero = gcc_jit_context_new_rvalue_from_int(ctxt, t_int64, 0);
            gcc_jit_rvalue *is_false = gcc_jit_context_new_comparison(
                ctxt, NULL, GCC_JIT_COMPARISON_EQ, cond_val, zero);

            gcc_jit_block_end_with_conditional(
                curr_block, NULL,
                is_false,
                blocks[inst.arg],  // Si faux (== 0), sauter à la cible
                blocks[i + 1]      // Sinon continuer à l'instruction suivante
            );
            block_terminated = true;
            break;
        }

        case OP_RET: {
            gcc_jit_rvalue *ret_val = (vsp > 0)
                ? vstack[--vsp]
                : gcc_jit_context_new_rvalue_from_int(ctxt, t_int64, 0);
            gcc_jit_block_end_with_return(curr_block, NULL, ret_val);
            block_terminated = true;
            break;
        }

        default:
            fprintf(stderr, "[JIT] Opcode non supporte (%d)\n", inst.op);
            break;
        }
    }

    // 8. Compiler en mémoire et récupérer le pointeur de fonction natif
    gcc_jit_result *result = gcc_jit_context_compile(ctxt);
    if (result) {
        void *fn_ptr = gcc_jit_result_get_code(result, fn->name);
        if (fn_ptr) {
            fn->native_fn = fn_ptr;
            compiled_results = realloc(compiled_results, (results_count + 1) * sizeof(gcc_jit_result *));
            compiled_results[results_count++] = result;
            printf("[JIT] Compilation reussie -> bascule sur le code natif !\n");
        } else {
            gcc_jit_result_release(result);
        }
    } else {
        fprintf(stderr, "[JIT] Echec de la compilation libgccjit pour '%s'\n", fn->name);
    }

    gcc_jit_context_release(ctxt);
    free(locals);
    free(is_block_start);
    free(blocks);
}

void jit_cleanup(void) {
    for (size_t i = 0; i < results_count; i++) {
        if (compiled_results[i]) {
            gcc_jit_result_release(compiled_results[i]);
        }
    }
    free(compiled_results);
    compiled_results = NULL;
    results_count = 0;
}