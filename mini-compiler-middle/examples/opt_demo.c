#include "ir.h"
#include "optimizer.h"
#include "cfg.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    IRFunction* func = ir_create_function("opt_demo");

    int a = ir_new_temp(func);
    ir_emit(func, IR_MOV, a, 3, -1, NULL);

    int b = ir_new_temp(func);
    ir_emit(func, IR_MOV, b, 4, -1, NULL);

    int sum1 = ir_new_temp(func);
    ir_emit(func, IR_ADD, sum1, a, b, NULL);

    int zero = ir_new_temp(func);
    ir_emit(func, IR_MOV, zero, 0, -1, NULL);

    int dead = ir_new_temp(func);
    ir_emit(func, IR_MUL, dead, zero, sum1, NULL);

    int sum2 = ir_new_temp(func);
    ir_emit(func, IR_ADD, sum2, a, b, NULL);

    int c10 = ir_new_temp(func);
    ir_emit(func, IR_MOV, c10, 10, -1, NULL);

    int add10 = ir_new_temp(func);
    ir_emit(func, IR_ADD, add10, c10, 0, NULL);

    int mul2 = ir_new_temp(func);
    ir_emit(func, IR_MUL, mul2, add10, 2, NULL);

    ir_emit(func, IR_RET, mul2, -1, -1, NULL);

    printf("=== Before Optimization ===\n\n");
    ir_print_function(func, stdout);

    OptPass pipeline[] = {OPT_DCE, OPT_CSE, OPT_CONST_FOLD, OPT_COPY_PROP};
    OptStats total = opt_run_pipeline(func, pipeline, 4);

    printf("\n=== After Optimization ===\n\n");
    ir_print_function(func, stdout);

    printf("\n");
    opt_print_changes(total, stdout);

    ir_destroy_function(func);
    return 0;
}
