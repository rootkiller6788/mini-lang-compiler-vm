#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "bytecode.h"
#include "jit_method.h"

static int32_t emit_push_int(ByteCode* bc, int64_t value) {
    Constant c;
    c.type         = CONST_INT;
    c.data.int_val = value;
    int32_t idx = bc_add_constant(bc, c);
    return bc_emit(bc, (idx << 8) | OP_PUSH);
}

static int32_t emit_op(ByteCode* bc, OpCode op) {
    return bc_emit(bc, (0 << 8) | op);
}

static int32_t emit_jmp(ByteCode* bc, int32_t target) {
    return bc_emit(bc, (target << 8) | OP_JMP);
}

static int32_t emit_jmp_if_false(ByteCode* bc, int32_t target) {
    return bc_emit(bc, (target << 8) | OP_JMP_IF_FALSE);
}

static void build_fib_bytecode(ByteCode* bc, int64_t n) {
    memset(bc, 0, sizeof(ByteCode));

    emit_push_int(bc, n);

    int32_t loop_start = bc->num_inst;
    emit_push_int(bc, 1);
    emit_push_int(bc, 1);
    emit_push_int(bc, 1);
    emit_op(bc, OP_ADD);
    emit_push_int(bc, 1);
    emit_push_int(bc, 1);
    emit_op(bc, OP_ADD);
    emit_op(bc, OP_ADD);
    emit_push_int(bc, 1);
    emit_push_int(bc, 1);
    emit_op(bc, OP_ADD);
    emit_push_int(bc, 1);
    emit_op(bc, OP_ADD);
    emit_push_int(bc, 1);
    emit_op(bc, OP_ADD);
    emit_push_int(bc, 1);
    emit_push_int(bc, 1);
    emit_op(bc, OP_ADD);
    emit_op(bc, OP_ADD);
    emit_op(bc, OP_ADD);
    emit_op(bc, OP_ADD);

    emit_push_int(bc, 1);
    emit_op(bc, OP_SUB);
    emit_op(bc, OP_NOT);
    int32_t jmp_if_idx = bc->num_inst;
    emit_op(bc, OP_JMP_IF_FALSE);
    emit_push_int(bc, 1);
    emit_op(bc, OP_POP);
    emit_jmp(bc, loop_start);

    int32_t after_loop = bc->num_inst;
    bc->instructions[jmp_if_idx] = (after_loop << 8) | OP_JMP_IF_FALSE;
    emit_push_int(bc, 42);
    emit_op(bc, OP_HALT);
}

static double get_time_ms(void) {
    clock_t c = clock();
    return (double)c * 1000.0 / (double)CLOCKS_PER_SEC;
}

int main(void) {
    printf("=== JIT Compilation Demo ===\n\n");

    ByteCode fib_bc;
    build_fib_bytecode(&fib_bc, 1000);

    printf("Fibonacci bytecode: %d instructions\n", fib_bc.num_inst);
    printf("Constant pool: %d entries\n\n", fib_bc.const_count);

    JITCompiler jc;
    jit_compiler_init(&jc, JIT_DEFAULT_THRESHOLD);

    const int32_t iterations = 100000;

    printf("--- Phase 1: Interpreted (call count < threshold) ---\n");
    double interp_start = get_time_ms();
    int64_t result = 0;
    for (int32_t i = 0; i < iterations; i++) {
        jit_record_call(&jc);
        result = jit_interp_fallback(&fib_bc);

        if (jit_should_compile(&jc)) {
            printf("\n[Threshold reached at call #%d!]\n", jc.call_count);
            bool compiled = jit_compile_function(&jc, &fib_bc);
            printf("[JIT compilation: %s]\n", compiled ? "SUCCESS" : "FAILED");
            printf("[Native code size: %d bytes]\n", jc.code_block.size);

            jit_install_code(&jc);
            break;
        }
    }
    double interp_end = get_time_ms();
    printf("Interpreted result: %lld\n", (long long)result);
    printf("Interpreted time: %.3f ms\n\n", interp_end - interp_start);

    printf("--- Phase 2: JIT Compiled (native execution) ---\n");
    int32_t remaining = iterations - jc.call_count;
    double jit_start = get_time_ms();
    int64_t native_result = 0;
    for (int32_t i = 0; i < remaining; i++) {
        native_result = jit_execute_native(&jc);
    }
    double jit_end = get_time_ms();

    printf("JIT result: %lld\n", (long long)native_result);
    printf("JIT time: %.3f ms for %d calls\n", jit_end - jit_start, remaining);

    double interp_per_call = (interp_end - interp_start) / jc.call_count;
    double jit_per_call = (jit_end - jit_start) / (remaining > 0 ? remaining : 1);
    printf("\n--- Comparison ---\n");
    printf("Interpreted: %.6f ms/call\n", interp_per_call);
    printf("JIT compiled: %.6f ms/call\n", jit_per_call);
    printf("Speedup: %.2fx\n",
           interp_per_call / (jit_per_call > 0 ? jit_per_call : 0.000001));

    printf("\n=== Demo Complete ===\n");
    return 0;
}
