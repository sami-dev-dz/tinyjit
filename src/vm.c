#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "jit.h"
#include "vm.h"

void vm_init(VM *vm)
{
    if (vm == NULL) {
        return;
    }

    vm->sp = 0;
    vm->num_locals = 0;
    memset(vm->stack, 0, sizeof(vm->stack));
    memset(vm->locals, 0, sizeof(vm->locals));
}

int64_t vm_run(BytecodeFunction *fn, const int64_t *args)
{
    if (fn == NULL) {
        return 0;
    }

    // 1. Si la fonction a déjà été compilée en natif, exécution directe
    if (fn->native_fn != NULL) {
        JitNativeFn native = (JitNativeFn)fn->native_fn;
        return native(args);
    }

    // 2. Profilage : incrémenter le compteur d'appels
    fn->call_count++;
    if (fn->call_count == HOTSPOT_THRESHOLD) {
        jit_compile(fn);
        if (fn->native_fn != NULL) {
            JitNativeFn native = (JitNativeFn)fn->native_fn;
            return native(args);
        }
    }

    // 3. Pile virtuelle et variables locales propres au frame d'appel courant
    int64_t stack[VM_STACK_CAPACITY];
    size_t sp = 0;

    int64_t locals[VM_LOCALS_CAPACITY];
    memset(locals, 0, sizeof(locals));

    if (args != NULL && fn->num_args > 0) {
        for (int i = 0; i < fn->num_args && i < VM_LOCALS_CAPACITY; i++) {
            locals[i] = args[i];
        }
    }

    size_t ip = 0; // Instruction Pointer

    while (ip < fn->code_count) {
        Instruction inst = fn->code[ip++];

        switch (inst.op) {
        case OP_CONST: {
            if (sp >= VM_STACK_CAPACITY) {
                fprintf(stderr, "Erreur VM: debordement de pile\n");
                return 0;
            }
            if (inst.arg < 0 || (size_t)inst.arg >= fn->const_count) {
                fprintf(stderr, "Erreur VM: constante hors limites (%d)\n", inst.arg);
                return 0;
            }
            stack[sp++] = fn->constants[inst.arg];
            break;
        }

        case OP_LOAD: {
            if (sp >= VM_STACK_CAPACITY) {
                fprintf(stderr, "Erreur VM: debordement de pile\n");
                return 0;
            }
            if (inst.arg < 0 || inst.arg >= VM_LOCALS_CAPACITY) {
                fprintf(stderr, "Erreur VM: variable locale hors limites (%d)\n", inst.arg);
                return 0;
            }
            stack[sp++] = locals[inst.arg];
            break;
        }

        case OP_STORE: {
            if (sp == 0) {
                fprintf(stderr, "Erreur VM: pile vide lors de OP_STORE\n");
                return 0;
            }
            if (inst.arg < 0 || inst.arg >= VM_LOCALS_CAPACITY) {
                fprintf(stderr, "Erreur VM: variable locale hors limites (%d)\n", inst.arg);
                return 0;
            }
            locals[inst.arg] = stack[--sp];
            break;
        }

        case OP_ADD: {
            if (sp < 2) {
                fprintf(stderr, "Erreur VM: pile insuffisante pour OP_ADD\n");
                return 0;
            }
            int64_t b = stack[--sp];
            int64_t a = stack[--sp];
            stack[sp++] = a + b;
            break;
        }

        case OP_SUB: {
            if (sp < 2) {
                fprintf(stderr, "Erreur VM: pile insuffisante pour OP_SUB\n");
                return 0;
            }
            int64_t b = stack[--sp];
            int64_t a = stack[--sp];
            stack[sp++] = a - b;
            break;
        }

        case OP_MUL: {
            if (sp < 2) {
                fprintf(stderr, "Erreur VM: pile insuffisante pour OP_MUL\n");
                return 0;
            }
            int64_t b = stack[--sp];
            int64_t a = stack[--sp];
            stack[sp++] = a * b;
            break;
        }

        case OP_DIV: {
            if (sp < 2) {
                fprintf(stderr, "Erreur VM: pile insuffisante pour OP_DIV\n");
                return 0;
            }
            int64_t b = stack[--sp];
            int64_t a = stack[--sp];
            if (b == 0) {
                fprintf(stderr, "Erreur VM: division par zero\n");
                return 0;
            }
            stack[sp++] = a / b;
            break;
        }

        case OP_LT: {
            if (sp < 2) {
                fprintf(stderr, "Erreur VM: pile insuffisante pour OP_LT\n");
                return 0;
            }
            int64_t b = stack[--sp];
            int64_t a = stack[--sp];
            stack[sp++] = (a < b) ? 1 : 0;
            break;
        }

        case OP_EQ: {
            if (sp < 2) {
                fprintf(stderr, "Erreur VM: pile insuffisante pour OP_EQ\n");
                return 0;
            }
            int64_t b = stack[--sp];
            int64_t a = stack[--sp];
            stack[sp++] = (a == b) ? 1 : 0;
            break;
        }

        case OP_JUMP: {
            if (inst.arg < 0 || (size_t)inst.arg > fn->code_count) {
                fprintf(stderr, "Erreur VM: cible de saut invalide (%d)\n", inst.arg);
                return 0;
            }
            ip = (size_t)inst.arg;
            break;
        }

        case OP_JUMP_IF_FALSE: {
            if (sp == 0) {
                fprintf(stderr, "Erreur VM: pile vide lors de OP_JUMP_IF_FALSE\n");
                return 0;
            }
            int64_t cond = stack[--sp];
            if (cond == 0) {
                if (inst.arg < 0 || (size_t)inst.arg > fn->code_count) {
                    fprintf(stderr, "Erreur VM: cible de saut invalide (%d)\n", inst.arg);
                    return 0;
                }
                ip = (size_t)inst.arg;
            }
            break;
        }

        case OP_CALL: {
            if (fn->func_table == NULL || (size_t)inst.arg >= fn->func_table_count) {
                fprintf(stderr, "Erreur VM: index invalide dans func_table (%d)\n", inst.arg);
                return 0;
            }
            BytecodeFunction *callee = fn->func_table[inst.arg];
            int argc = callee->num_args;

            if (sp < (size_t)argc) {
                fprintf(stderr, "Erreur VM: pas assez d'arguments sur la pile pour OP_CALL\n");
                return 0;
            }

            // Dépiler les arguments dans le bon ordre (du dernier au premier)
            int64_t call_args[16];
            for (int i = argc - 1; i >= 0; i--) {
                call_args[i] = stack[--sp];
            }

            // Exécuter la fonction cible (interprétée ou JIT selon son compteur)
            int64_t result = vm_run(callee, call_args);

            // Empiler le résultat sur la pile de la fonction appelante
            stack[sp++] = result;
            break;
        }

        case OP_RET: {
            if (sp > 0) {
                return stack[--sp];
            }
            return 0;
        }

        default:
            fprintf(stderr, "Erreur VM: opcode non reconnu (%d)\n", inst.op);
            return 0;
        }
    }

    return 0;
}
