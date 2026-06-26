#ifndef JIT_H
#define JIT_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "codegen.h"

#define JIT_CODE_SIZE     65536
#define JIT_MAX_FUNCTIONS 64

typedef int64_t (*JITFunc0)(void);
typedef int64_t (*JITFunc1)(int64_t);
typedef int64_t (*JITFunc2)(int64_t, int64_t);
typedef int64_t (*JITFunc3)(int64_t, int64_t, int64_t);

typedef struct {
    uint8_t *code_buffer;
    size_t code_offset;
    size_t code_size;
    bool is_executable;
} JITCodeBuffer;

typedef struct {
    JITCodeBuffer code;
    CodeGen cg;
    const char *func_names[JIT_MAX_FUNCTIONS];
    void *func_ptrs[JIT_MAX_FUNCTIONS];
    int32_t func_count;
    bool initialized;
} JITCompiler;

void jit_init(JITCompiler *jit);
void jit_free(JITCompiler *jit);
void *jit_compile_function(JITCompiler *jit, IRFunction *func,
                            IRNode *ir_root, TargetArch arch);
void *jit_get_function(JITCompiler *jit, const char *name);
int32_t jit_code_bytes_written(JITCompiler *jit);
void jit_dump_code(JITCompiler *jit, FILE *out);
bool jit_is_supported(void);

#endif
