#include <stdio.h>
#include <stdlib.h>
#include "bytecode.h"

static int32_t emit_push_int(ByteCode* bc, int64_t value) {
    Constant c;
    c.type        = CONST_INT;
    c.data.int_val = value;
    int32_t idx = bc_add_constant(bc, c);
    return bc_emit(bc, (idx << 8) | OP_PUSH);
}

int main(void) {
    ByteCode bc;
    memset(&bc, 0, sizeof(bc));

    printf("=== Bytecode VM Demo ===\n");
    printf("Expression: (+ 3 (* 4 5))\n\n");

    emit_push_int(&bc, 4);
    emit_push_int(&bc, 5);
    bc_emit(&bc, (0 << 8) | OP_MUL);
    emit_push_int(&bc, 3);
    bc_emit(&bc, (0 << 8) | OP_ADD);
    bc_emit(&bc, (0 << 8) | OP_PRINT);
    bc_emit(&bc, (0 << 8) | OP_HALT);

    printf("Bytecode disassembly (%d instructions):\n", bc.num_inst);
    for (int32_t i = 0; i < bc.num_inst; i++) {
        int32_t instr = bc.instructions[i];
        OpCode op = (OpCode)(instr & 0xFF);
        int32_t arg = (instr >> 8) & 0xFFFFFF;
        printf("  %04d  %-14s", i, opcode_name(op));
        if (op == OP_PUSH) {
            Constant c = bc.const_pool[arg];
            if (c.type == CONST_INT) printf(" %lld", (long long)c.data.int_val);
        }
        printf("\n");
    }

    StackVM vm;
    vm_init(&vm, &bc);

    printf("\nExecuting...\n");
    printf("Result: ");
    bool ok = vm_execute(&vm);

    if (ok) {
        vm_print_stack(&vm);
        printf("Execution successful.\n");
    } else {
        printf("Execution failed.\n");
    }

    printf("\n=== Demo Complete ===\n");
    return 0;
}
