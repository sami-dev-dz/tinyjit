#ifndef JIT_H
#define JIT_H

#include <stdint.h>
#include <stdbool.h>
#include "bytecode.h"
#include "vm.h"

#define HOTSPOT_THRESHOLD 1000

typedef int64_t (*JitNativeFn)(const int64_t *args);

void jit_init(bool dump_asm);
void jit_compile(BytecodeFunction *fn);
void jit_cleanup(void);

#endif // JIT_H
