#ifndef JIT_METHOD_H
#define JIT_METHOD_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "bytecode.h"

#define JIT_CODE_BUFFER_SIZE 4096
#define JIT_DEFAULT_THRESHOLD 10

typedef int64_t (*NativeFnPtr)(void);

typedef struct {
    NativeFnPtr compiled_fn_ptr;
    int32_t     size;
    uint8_t     native_code[JIT_CODE_BUFFER_SIZE];
} JITCodeBlock;

typedef struct {
    int32_t     compilation_threshold;
    int32_t     call_count;
    bool        is_compiled;
    JITCodeBlock code_block;
} JITCompiler;

void    jit_compiler_init(JITCompiler* jc, int32_t threshold);
bool    jit_compile_function(JITCompiler* jc, const ByteCode* bc);
bool    jit_install_code(JITCompiler* jc);
int64_t jit_execute_native(const JITCompiler* jc);
bool    jit_should_compile(const JITCompiler* jc);
void    jit_record_call(JITCompiler* jc);

int64_t jit_interp_fallback(const ByteCode* bc);

#endif
