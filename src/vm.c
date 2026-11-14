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

int64_t vm_run(VM *vm, BytecodeFunction *fn, const int64_t *args, size_t num_args) {
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

    // 3. Réinitialiser la pile et configurer les variables locales pour cet appel
    vm->sp = 0;
    vm->num_locals = (size_t)fn->num_locals;

    size_t max_locals = sizeof(vm->locals) / sizeof(vm->locals[0]);
    if (vm->num_locals > max_locals) {
        fprintf(stderr, "Erreur VM: trop de variables locales (%zu)\n", vm->num_locals);
        return 0;
    }

    // Remettre à 0 les locales puis copier les arguments passés
    memset(vm->locals, 0, vm->num_locals * sizeof(int64_t));
    for (size_t i = 0; i < num_args && i < (size_t)fn->num_args; i++) {
        vm->locals[i] = args[i];
    }

    size_t ip = 0;
    while (ip < fn->code_count) {
        Instruction inst = fn->code[ip++];

        switch (inst.op) {
        case OP_CONST: {
            if (vm->sp >= VM_STACK_CAPACITY) {
                fprintf(stderr, "Erreur VM: débordement de pile\n");
                return 0;
            }
            if (inst.arg < 0 || (size_t)inst.arg >= fn->const_count) {
                fprintf(stderr, "Erreur VM: constante hors limites (%d)\n", inst.arg);
                return 0;
            }
            vm->stack[vm->sp++] = fn->constants[inst.arg];
            break;
        }

        case OP_LOAD: {
            if (vm->sp >= VM_STACK_CAPACITY) {
                fprintf(stderr, "Erreur VM: débordement de pile\n");
                return 0;
            }
            if (inst.arg < 0 || (size_t)inst.arg >= vm->num_locals) {
                fprintf(stderr, "Erreur VM: variable locale hors limites (%d)\n", inst.arg);
                return 0;
            }
            vm->stack[vm->sp++] = vm->locals[inst.arg];
            break;
        }

        case OP_STORE: {
            if (vm->sp == 0) {
                fprintf(stderr, "Erreur VM: pile vide lors de OP_STORE\n");
                return 0;
            }
            if (inst.arg < 0 || (size_t)inst.arg >= vm->num_locals) {
                fprintf(stderr, "Erreur VM: variable locale hors limites (%d)\n", inst.arg);
                return 0;
            }
            vm->locals[inst.arg] = vm->stack[--vm->sp];
            break;
        }

        case OP_ADD: {
            if (vm->sp < 2) {
                fprintf(stderr, "Erreur VM: pile insuffisante pour OP_ADD\n");
                return 0;
            }
            int64_t b = vm->stack[--vm->sp];
            int64_t a = vm->stack[--vm->sp];
            vm->stack[vm->sp++] = a + b;
            break;
        }

        case OP_SUB: {
            if (vm->sp < 2) {
                fprintf(stderr, "Erreur VM: pile insuffisante pour OP_SUB\n");
                return 0;
            }
            int64_t b = vm->stack[--vm->sp];
            int64_t a = vm->stack[--vm->sp];
            vm->stack[vm->sp++] = a - b;
            break;
        }

        case OP_MUL: {
            if (vm->sp < 2) {
                fprintf(stderr, "Erreur VM: pile insuffisante pour OP_MUL\n");
                return 0;
            }
            int64_t b = vm->stack[--vm->sp];
            int64_t a = vm->stack[--vm->sp];
            vm->stack[vm->sp++] = a * b;
            break;
        }

        case OP_DIV: {
            if (vm->sp < 2) {
                fprintf(stderr, "Erreur VM: pile insuffisante pour OP_DIV\n");
                return 0;
            }
            int64_t b = vm->stack[--vm->sp];
            int64_t a = vm->stack[--vm->sp];
            if (b == 0) {
                fprintf(stderr, "Erreur VM: division par zéro\n");
                return 0;
            }
            vm->stack[vm->sp++] = a / b;
            break;
        }

        case OP_LT: {
            if (vm->sp < 2) {
                fprintf(stderr, "Erreur VM: pile insuffisante pour OP_LT\n");
                return 0;
            }
            int64_t b = vm->stack[--vm->sp];
            int64_t a = vm->stack[--vm->sp];
            vm->stack[vm->sp++] = (a < b) ? 1 : 0;
            break;
        }

        case OP_EQ: {
            if (vm->sp < 2) {
                fprintf(stderr, "Erreur VM: pile insuffisante pour OP_EQ\n");
                return 0;
            }
            int64_t b = vm->stack[--vm->sp];
            int64_t a = vm->stack[--vm->sp];
            vm->stack[vm->sp++] = (a == b) ? 1 : 0;
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
            if (vm->sp == 0) {
                fprintf(stderr, "Erreur VM: pile vide lors de OP_JUMP_IF_FALSE\n");
                return 0;
            }
            int64_t cond = vm->stack[--vm->sp];
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
            // Nécessitera la gestion des frames d'appel dans les prochaines étapes
            fprintf(stderr, "Erreur VM: OP_CALL non implémenté\n");
            return 0;
        }

        case OP_RET: {
            if (vm->sp > 0) {
                return vm->stack[--vm->sp];
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

