#include "codegen.h"

void codegen_init(CodeGen *cg, TargetArch arch) {
    memset(cg, 0, sizeof(CodeGen));
    cg->target = arch;
    instruction_list_init(&cg->ilist);
    ra_init_context(&cg->ra_ctx, 8);
    abi_init(&cg->abi, ABI_X86_64_SYSV);
    peephole_init_rules(&cg->peep_ctx);
}

void codegen_run(CodeGen *cg, IRFunction *func, IRNode *ir_root) {
    (void)func;
    TileSet ts;
    isel_init(&ts);

    isel_tile_tree(ir_root, &ts, &cg->ilist);

    peephole_optimize(&cg->peep_ctx, &cg->ilist);

    for (size_t i = 0; i < cg->ilist.count; i++) {
        ra_add_interval(&cg->ra_ctx, (int32_t)i, (int32_t)i, (int32_t)(i + 5));
    }
    ra_linear_scan(&cg->ra_ctx);
}

void codegen_prologue(CodeGen *cg, FILE *out) {
    if (!cg || !out) return;
    if (cg->target == ARCH_X86) {
        fprintf(out, "  push rbp\n");
        fprintf(out, "  mov  rbp, rsp\n");
        fprintf(out, "  sub  rsp, %d\n", cg->abi.red_zone > 0 ? cg->abi.red_zone : 32);
    } else if (cg->target == ARCH_ARM) {
        fprintf(out, "  stp fp, lr, [sp, #-16]!\n");
        fprintf(out, "  mov fp, sp\n");
    } else if (cg->target == ARCH_RISCV) {
        fprintf(out, "  addi sp, sp, -32\n");
        fprintf(out, "  sd   ra, 24(sp)\n");
        fprintf(out, "  sd   fp, 16(sp)\n");
        fprintf(out, "  addi fp, sp, 32\n");
    }
}

void codegen_epilogue(CodeGen *cg, FILE *out) {
    if (!cg || !out) return;
    if (cg->target == ARCH_X86) {
        fprintf(out, "  leave\n");
        fprintf(out, "  ret\n");
    } else if (cg->target == ARCH_ARM) {
        fprintf(out, "  ldp fp, lr, [sp], #16\n");
        fprintf(out, "  ret\n");
    } else if (cg->target == ARCH_RISCV) {
        fprintf(out, "  ld   ra, 24(sp)\n");
        fprintf(out, "  ld   fp, 16(sp)\n");
        fprintf(out, "  addi sp, sp, 32\n");
        fprintf(out, "  ret\n");
    }
}

static const char *arch_name(TargetArch arch) {
    switch (arch) {
        case ARCH_X86:   return "x86-64";
        case ARCH_ARM:   return "ARM64";
        case ARCH_RISCV: return "RISC-V64";
        default:         return "unknown";
    }
}

static void emit_single_instruction(InstructionNode *in, FILE *out) {
    const char *opn = isel_op_name(in->op);
    if (in->op == ISEL_NOP) {
        fprintf(out, "  nop\n");
        return;
    }
    if (in->op == ISEL_RET) {
        fprintf(out, "  ret\n");
        return;
    }
    if (in->has_label) {
        fprintf(out, "%s:\n", in->label);
    }
    if (in->op == ISEL_LOAD) {
        fprintf(out, "  %-6s %s, %s\n", opn, in->dst, in->src1);
    } else if (in->op == ISEL_STORE) {
        fprintf(out, "  %-6s %s, %s\n", opn, in->dst, in->src1);
    } else if (in->op == ISEL_PUSH || in->op == ISEL_POP) {
        fprintf(out, "  %-6s %s\n", opn, in->src1[0] ? in->src1 : in->dst);
    } else if (in->op == ISEL_CALL) {
        fprintf(out, "  %-6s %s\n", opn, in->dst[0] ? in->dst : in->src1);
    } else if (in->op == ISEL_JMP || in->op == ISEL_JE ||
               in->op == ISEL_JNE || in->op == ISEL_JL) {
        fprintf(out, "  %-6s %s\n", opn, in->label[0] ? in->label : in->dst);
    } else if (in->src2[0] != '\0') {
        fprintf(out, "  %-6s %s, %s, %s\n", opn, in->dst, in->src1, in->src2);
    } else if (in->src1[0] != '\0') {
        fprintf(out, "  %-6s %s, %s\n", opn, in->dst, in->src1);
    } else if (in->dst[0] != '\0') {
        fprintf(out, "  %-6s %s\n", opn, in->dst);
    } else {
        fprintf(out, "  %-6s\n", opn);
    }
}

void codegen_emit_asm(CodeGen *cg, IRFunction *func, FILE *out) {
    if (!cg || !func || !out) return;

    fprintf(out, "  .intel_syntax noprefix\n");
    fprintf(out, "  .globl %s\n", func->name);
    fprintf(out, "  .type %s, @function\n", func->name);
    fprintf(out, "%s:\n", func->name);
    fprintf(out, ";;; Target: %s\n", arch_name(cg->target));

    codegen_prologue(cg, out);

    fprintf(out, "\n;;; function body\n");
    for (size_t i = 0; i < cg->ilist.count; i++) {
        emit_single_instruction(&cg->ilist.instructions[i], out);
    }

    fprintf(out, "\n;;; epilogue\n");
    codegen_epilogue(cg, out);

    if (cg->ilist.count > 0) {
        fprintf(out, "\n;;; size %s-end\n", func->name);
    }
}

void codegen_free(CodeGen *cg) {
    if (!cg) return;
    instruction_list_free(&cg->ilist);
}
