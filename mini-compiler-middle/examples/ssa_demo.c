#include "ir.h"
#include "ssa.h"
#include "cfg.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    IRFunction* func = ir_create_function("ssa_demo");

    int x = ir_new_temp(func);
    ir_emit(func, IR_MOV, x, 1, -1, NULL);

    int y = ir_new_temp(func);
    ir_emit(func, IR_MOV, y, 2, -1, NULL);

    int true_lbl   = ir_new_label(func);
    int false_lbl  = ir_new_label(func);
    int merge_lbl  = ir_new_label(func);
    char true_str[8], false_str[8], merge_str[8];
    snprintf(true_str, sizeof(true_str), "%d", true_lbl);
    snprintf(false_str, sizeof(false_str), "%d", false_lbl);
    snprintf(merge_str, sizeof(merge_str), "%d", merge_lbl);

    ir_emit(func, IR_BRCOND, -1, x, -1, true_str);
    snprintf(func->instructions[func->num_inst - 1].src1_label,
             MAX_LABEL_LEN, "%s", false_str);

    int z1 = ir_new_temp(func);
    ir_emit(func, IR_ADD, z1, y, x, NULL);
    ir_emit(func, IR_MOV, y, z1, -1, NULL);
    ir_emit(func, IR_BR, -1, -1, -1, merge_str);

    int c3 = ir_new_temp(func);
    ir_emit(func, IR_MOV, c3, 3, -1, NULL);
    ir_emit(func, IR_ADD, y, x, c3, NULL);
    ir_emit(func, IR_BR, -1, -1, -1, merge_str);

    int result = ir_new_temp(func);
    ir_emit(func, IR_ADD, result, x, y, NULL);
    ir_emit(func, IR_RET, result, -1, -1, NULL);

    printf("=== Before SSA ===\n\n");
    ir_print_function(func, stdout);

    IRBasicBlock blocks[MAX_BLOCKS];
    int num_blocks = ir_build_cfg(func, blocks, MAX_BLOCKS);
    printf("\nBasic blocks: %d\n", num_blocks);
    for (int i = 0; i < num_blocks; i++) {
        printf("  BB%d: %d insts\n", blocks[i].label, blocks[i].num_inst);
    }

    ssa_build(func);

    printf("\n=== After SSA ===\n\n");
    ir_print_function(func, stdout);

    ir_destroy_function(func);
    return 0;
}
