#include "ir.h"
#include "cfg.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    IRFunction* func = ir_create_function("fibonacci");

    int a  = ir_new_temp(func);
    ir_emit(func, IR_MOV, a, 0, -1, NULL);

    int b  = ir_new_temp(func);
    ir_emit(func, IR_MOV, b, 1, -1, NULL);

    int i  = ir_new_temp(func);
    ir_emit(func, IR_MOV, i, 0, -1, NULL);

    int loop_lbl = ir_new_label(func);
    int end_lbl  = ir_new_label(func);
    char loop_str[8], end_str[8];
    snprintf(loop_str, sizeof(loop_str), "%d", loop_lbl);
    snprintf(end_str, sizeof(end_str), "%d", end_lbl);

    int t10 = ir_new_temp(func);
    ir_emit(func, IR_MOV, t10, 10, -1, NULL);

    int tcmp = ir_new_temp(func);
    ir_emit(func, IR_SUB, tcmp, i, t10, NULL);

    ir_emit(func, IR_BRCOND, -1, tcmp, -1, end_str);
    snprintf(func->instructions[func->num_inst - 1].src1_label,
             MAX_LABEL_LEN, "%s", loop_str);

    int sum = ir_new_temp(func);
    ir_emit(func, IR_ADD, sum, a, b, NULL);
    ir_emit(func, IR_MOV, a, b, -1, NULL);
    ir_emit(func, IR_MOV, b, sum, -1, NULL);

    int one = ir_new_temp(func);
    ir_emit(func, IR_MOV, one, 1, -1, NULL);
    int i2  = ir_new_temp(func);
    ir_emit(func, IR_ADD, i2, i, one, NULL);
    ir_emit(func, IR_MOV, i, i2, -1, NULL);

    ir_emit(func, IR_BR, -1, -1, loop_str);

    ir_emit(func, IR_RET, b, -1, -1, NULL);

    printf("=== IR Demo: Fibonacci ===\n\n");
    ir_print_function(func, stdout);

    printf("\n=== Control Flow Graph ===\n\n");
    CFG cfg;
    cfg_build(func, &cfg);
    cfg_print_graph(&cfg, stdout);

    int rpo[MAX_BLOCKS], num_rpo;
    cfg_reverse_postorder(&cfg, rpo, &num_rpo);
    printf("\nReverse Postorder: ");
    for (int i = 0; i < num_rpo; i++) {
        printf("BB%d ", cfg.nodes[rpo[i]].bb_id);
    }
    printf("\n");

    int doms[MAX_BLOCKS][MAX_BLOCKS];
    cfg_dominators(&cfg, doms);

    int loops[MAX_BLOCKS];
    int back_edges[MAX_BLOCKS][2];
    int num_be;
    cfg_find_loops(&cfg, loops, back_edges, &num_be);
    printf("Back edges found: %d\n", num_be);

    ir_destroy_function(func);
    return 0;
}
