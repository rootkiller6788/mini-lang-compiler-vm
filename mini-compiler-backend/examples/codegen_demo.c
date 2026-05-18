#include <stdio.h>
#include <stdlib.h>

#include "codegen.h"

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Code Generation Demo - End-to-End ===\n\n");

    CodeGen *cg = (CodeGen *)calloc(1, sizeof(CodeGen));
    if (!cg) { printf("FATAL: calloc failed\n"); return 1; }
    codegen_init(cg, ARCH_X86);

    IRFunction func;
    func.name = "example_func";
    func.arg_count = 2;
    func.local_count = 3;

    IRNode *base_rbp = ir_node_create(IRO_BASE, 0);
    snprintf(base_rbp->label, sizeof(base_rbp->label), "rbp");

    IRNode *imm_16 = ir_node_create(IRO_CONST, -16);
    IRNode *param_offset = ir_node_create(IRO_ADD, 0);
    param_offset->left = base_rbp;
    param_offset->right = imm_16;

    IRNode *mem_param = ir_node_create(IRO_MEM, 0);
    mem_param->left = param_offset;

    IRNode *load_param = ir_node_create(IRO_LOAD, 0);
    load_param->left = mem_param;

    IRNode *imm_2 = ir_node_create(IRO_CONST, 2);
    IRNode *result = ir_node_create(IRO_ADD, 0);
    result->left = load_param;
    result->right = imm_2;

    printf("IR function: %s(args=%d, locals=%d)\n",
           func.name, func.arg_count, func.local_count);
    printf("IR tree: add(load(mem(add(base(rbp), const(-16)))), const(2))\n");
    printf("(Loads parameter from rbp-16, adds 2, returns result)\n\n");

    codegen_run(cg, &func, result);

    printf("=== After Instruction Selection & Peephole ===\n");
    isel_print_mapping(&cg->ilist, stdout);

    printf("\n=== Register Allocation ===\n");
    ra_print_assignment(&cg->ra_ctx, stdout);

    printf("\n=== Peephole Report ===\n");
    peephole_print_replacements(&cg->peep_ctx, stdout);

    printf("\n=== Final x86-64 Assembly ===\n\n");
    codegen_emit_asm(cg, &func, stdout);

    ir_tree_free(result);
    codegen_free(cg);
    free(cg);

    printf("\n=== ARM64 Target Demo ===\n\n");

    CodeGen *cg_arm = (CodeGen *)calloc(1, sizeof(CodeGen));
    if (!cg_arm) { printf("FATAL: calloc arm failed\n"); return 1; }
    codegen_init(cg_arm, ARCH_ARM);

    IRFunction func_arm;
    func_arm.name = "arm_add";
    func_arm.arg_count = 1;
    func_arm.local_count = 0;

    IRNode *base_rbp2 = ir_node_create(IRO_BASE, 0);
    snprintf(base_rbp2->label, sizeof(base_rbp2->label), "x0");
    IRNode *imm_3 = ir_node_create(IRO_CONST, 3);
    IRNode *result_arm = ir_node_create(IRO_ADD, 0);
    result_arm->left = base_rbp2;
    result_arm->right = imm_3;

    codegen_run(cg_arm, &func_arm, result_arm);

    codegen_emit_asm(cg_arm, &func_arm, stdout);

    ir_tree_free(result_arm);
    codegen_free(cg_arm);
    free(cg_arm);

    printf("\n=== RISC-V64 Target Demo ===\n\n");

    CodeGen *cg_riscv = (CodeGen *)calloc(1, sizeof(CodeGen));
    if (!cg_riscv) { printf("FATAL: calloc riscv failed\n"); return 1; }
    codegen_init(cg_riscv, ARCH_RISCV);

    IRFunction func_riscv;
    func_riscv.name = "riscv_mul";
    func_riscv.arg_count = 2;
    func_riscv.local_count = 1;

    IRNode *base_a0 = ir_node_create(IRO_BASE, 0);
    snprintf(base_a0->label, sizeof(base_a0->label), "a0");
    IRNode *base_a1 = ir_node_create(IRO_BASE, 0);
    snprintf(base_a1->label, sizeof(base_a1->label), "a1");
    IRNode *result_riscv = ir_node_create(IRO_MUL, 0);
    result_riscv->left = base_a0;
    result_riscv->right = base_a1;

    codegen_run(cg_riscv, &func_riscv, result_riscv);

    codegen_emit_asm(cg_riscv, &func_riscv, stdout);

    ir_tree_free(result_riscv);
    codegen_free(cg_riscv);
    free(cg_riscv);

    printf("\n=== All targets completed. ===\n");
    return 0;
}
