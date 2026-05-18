#include "mlir_dialect.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    MLIRContext ctx;
    MLIRBlock *block;
    MLIRRegion region;
    MLIROp add_op, mul_op, add2_op, ret_op;
    int op_count;

    printf("===== mini-mlir-dialect Demo =====\n\n");

    ctx = mlir_context_create();
    mlir_context_register_arith_ops(&ctx);
    printf("MLIR Context created.\n");
    printf("  Dialects: %s, %s, %s, %s\n",
           ctx.builtin.name, ctx.arith.name,
           ctx.memref.name, ctx.func.name);
    printf("  Registered %d arith ops.\n\n", ctx.arith.op_count);

    block = mlir_create_block();
    mlir_block_add_arg(block, "arg0", MLIR_TYPE_I32);
    mlir_block_add_arg(block, "arg1", MLIR_TYPE_I32);
    mlir_block_add_arg(block, "arg2", MLIR_TYPE_I32);

    add_op = mlir_arith_addi("loc1:0:0");
    mlir_op_add_operand(&add_op, "%arg0", MLIR_TYPE_I32);
    mlir_op_add_operand(&add_op, "%arg1", MLIR_TYPE_I32);
    mlir_op_add_result(&add_op, "%0", MLIR_TYPE_I32);

    mul_op = mlir_arith_muli("loc2:1:0");
    mlir_op_add_operand(&mul_op, "%0", MLIR_TYPE_I32);
    mlir_op_add_operand(&mul_op, "%arg2", MLIR_TYPE_I32);
    mlir_op_add_result(&mul_op, "%1", MLIR_TYPE_I32);

    add2_op = mlir_arith_addi("loc3:2:0");
    mlir_op_add_operand(&add2_op, "%1", MLIR_TYPE_I32);
    mlir_op_add_operand(&add2_op, "%arg0", MLIR_TYPE_I32);
    mlir_op_add_result(&add2_op, "%2", MLIR_TYPE_I32);

    ret_op = mlir_func_return("loc4:3:0");
    mlir_op_add_operand(&ret_op, "%2", MLIR_TYPE_I32);

    mlir_block_add_op(block, add_op);
    mlir_block_add_op(block, mul_op);
    mlir_block_add_op(block, add2_op);
    mlir_block_add_op(block, ret_op);

    region = mlir_create_region();
    mlir_region_add_block(&region, block);

    printf("Verifying %d ops in region...\n", block->op_count);
    op_count = 0;
    while (op_count < block->op_count) {
        bool valid = mlir_verify(&block->operations[op_count]);
        printf("  Op %d (%s.%s): %s\n",
               op_count, block->operations[op_count].dialect,
               block->operations[op_count].name,
               valid ? "VALID" : "INVALID");
        op_count++;
    }

    printf("\n--- MLIR Text IR ---\n");
    mlir_print_ir(&region);

    printf("\nSummary:\n");
    printf("  Blocks:  %d\n", region.block_count);
    printf("  Ops:     %d\n", block->op_count);
    printf("  Args:    %d\n", block->arg_count);

    free(block);
    return 0;
}
