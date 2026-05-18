#include "abi_target.h"

void abi_init(ABIInfo *abi, ABIType type) {
    memset(abi, 0, sizeof(ABIInfo));
    abi->abi_type = type;

    switch (type) {
        case ABI_X86_64_SYSV:
            abi->num_arg_regs = 6;
            abi->arg_regs[0] = "rdi";
            abi->arg_regs[1] = "rsi";
            abi->arg_regs[2] = "rdx";
            abi->arg_regs[3] = "rcx";
            abi->arg_regs[4] = "r8";
            abi->arg_regs[5] = "r9";
            abi->return_reg = "rax";
            abi->num_caller_saved = 9;
            abi->caller_saved[0] = "rax";
            abi->caller_saved[1] = "rcx";
            abi->caller_saved[2] = "rdx";
            abi->caller_saved[3] = "rsi";
            abi->caller_saved[4] = "rdi";
            abi->caller_saved[5] = "r8";
            abi->caller_saved[6] = "r9";
            abi->caller_saved[7] = "r10";
            abi->caller_saved[8] = "r11";
            abi->num_callee_saved = 6;
            abi->callee_saved[0] = "rbx";
            abi->callee_saved[1] = "rbp";
            abi->callee_saved[2] = "r12";
            abi->callee_saved[3] = "r13";
            abi->callee_saved[4] = "r14";
            abi->callee_saved[5] = "r15";
            abi->stack_alignment = 16;
            abi->red_zone = 128;
            abi->has_red_zone = false;
            abi->arg_slot_offset = 16;
            abi->local_var_offset = -8;
            break;

        case ABI_ARM64_AAPCS:
            abi->num_arg_regs = 8;
            abi->arg_regs[0] = "x0";
            abi->arg_regs[1] = "x1";
            abi->arg_regs[2] = "x2";
            abi->arg_regs[3] = "x3";
            abi->arg_regs[4] = "x4";
            abi->arg_regs[5] = "x5";
            abi->return_reg = "x0";
            abi->num_caller_saved = 9;
            abi->caller_saved[0] = "x0";
            abi->caller_saved[1] = "x1";
            abi->caller_saved[2] = "x2";
            abi->caller_saved[3] = "x3";
            abi->caller_saved[4] = "x4";
            abi->caller_saved[5] = "x5";
            abi->caller_saved[6] = "x6";
            abi->caller_saved[7] = "x7";
            abi->caller_saved[8] = "x8";
            abi->num_callee_saved = 6;
            abi->callee_saved[0] = "x19";
            abi->callee_saved[1] = "x20";
            abi->callee_saved[2] = "x21";
            abi->callee_saved[3] = "x22";
            abi->callee_saved[4] = "x23";
            abi->callee_saved[5] = "x24";
            abi->stack_alignment = 16;
            abi->red_zone = 0;
            abi->has_red_zone = false;
            abi->arg_slot_offset = 0;
            abi->local_var_offset = 0;
            break;

        case ABI_RISCV64_LP64:
            abi->num_arg_regs = 8;
            abi->arg_regs[0] = "a0";
            abi->arg_regs[1] = "a1";
            abi->arg_regs[2] = "a2";
            abi->arg_regs[3] = "a3";
            abi->arg_regs[4] = "a4";
            abi->arg_regs[5] = "a5";
            abi->arg_regs[6] = "a6";
            abi->arg_regs[7] = "a7";
            abi->return_reg = "a0";
            abi->num_caller_saved = 8;
            abi->caller_saved[0] = "ra";
            abi->caller_saved[1] = "t0";
            abi->caller_saved[2] = "t1";
            abi->caller_saved[3] = "t2";
            abi->caller_saved[4] = "t3";
            abi->caller_saved[5] = "t4";
            abi->caller_saved[6] = "t5";
            abi->caller_saved[7] = "t6";
            abi->num_callee_saved = 6;
            abi->callee_saved[0] = "s0";
            abi->callee_saved[1] = "s1";
            abi->callee_saved[2] = "s2";
            abi->callee_saved[3] = "s3";
            abi->callee_saved[4] = "s4";
            abi->callee_saved[5] = "s5";
            abi->stack_alignment = 16;
            abi->red_zone = 0;
            abi->has_red_zone = false;
            abi->arg_slot_offset = 0;
            abi->local_var_offset = 0;
            break;
    }
}

ArgClass abi_classify_arg(const ABIInfo *abi, int32_t arg_index,
                          int32_t type_size) {
    (void)abi;
    if (type_size <= 8) {
        if (arg_index < abi->num_arg_regs) {
            return ARG_CLASS_INTEGER;
        }
        return ARG_CLASS_MEMORY;
    }
    if (type_size <= 16) {
        if (arg_index + 1 < abi->num_arg_regs) {
            return ARG_CLASS_INTEGER;
        }
        return ARG_CLASS_MEMORY;
    }
    return ARG_CLASS_MEMORY;
}

void abi_emit_call(ABIInfo *abi, const char *func_name, int32_t arg_count,
                   const char **arg_vregs, const char *result_reg,
                   FILE *out) {
    if (!abi || !out) return;

    for (int32_t i = 0; i < arg_count && i < MAX_ARG_REGS; i++) {
        if (arg_vregs[i]) {
            fprintf(out, "  mov  %s, %s\n",
                    abi->arg_regs[i], arg_vregs[i]);
        }
    }

    for (int32_t i = MAX_ARG_REGS; i < arg_count; i++) {
        if (arg_vregs[i]) {
            fprintf(out, "  push %s\n", arg_vregs[i]);
        }
    }

    fprintf(out, "  call %s\n", func_name);

    if (result_reg && result_reg[0]) {
        fprintf(out, "  mov  %s, %s\n", result_reg, abi->return_reg);
    }
}

void abi_emit_return(ABIInfo *abi, const char *ret_reg, FILE *out) {
    if (!abi || !out) return;
    if (ret_reg && ret_reg[0]) {
        fprintf(out, "  mov  %s, %s\n", abi->return_reg, ret_reg);
    }
    fprintf(out, "  ret\n");
}

void abi_emit_prologue(ABIInfo *abi, FILE *out) {
    if (!abi || !out) return;
    switch (abi->abi_type) {
        case ABI_X86_64_SYSV:
            fprintf(out, "  push rbp\n");
            fprintf(out, "  mov  rbp, rsp\n");
            if (abi->has_red_zone) {
                fprintf(out, "  sub  rsp, %d\n", abi->red_zone);
            }
            for (int32_t i = 0; i < abi->num_callee_saved; i++) {
                fprintf(out, "  push %s\n", abi->callee_saved[i]);
            }
            break;
        case ABI_ARM64_AAPCS:
            fprintf(out, "  stp fp, lr, [sp, #-16]!\n");
            fprintf(out, "  mov fp, sp\n");
            break;
        case ABI_RISCV64_LP64:
            fprintf(out, "  addi sp, sp, -32\n");
            fprintf(out, "  sd   ra, 24(sp)\n");
            fprintf(out, "  sd   fp, 16(sp)\n");
            fprintf(out, "  addi fp, sp, 32\n");
            break;
    }
}

void abi_emit_epilogue(ABIInfo *abi, FILE *out) {
    if (!abi || !out) return;
    switch (abi->abi_type) {
        case ABI_X86_64_SYSV:
            for (int32_t i = abi->num_callee_saved - 1; i >= 0; i--) {
                fprintf(out, "  pop  %s\n", abi->callee_saved[i]);
            }
            fprintf(out, "  leave\n");
            fprintf(out, "  ret\n");
            break;
        case ABI_ARM64_AAPCS:
            fprintf(out, "  ldp fp, lr, [sp], #16\n");
            fprintf(out, "  ret\n");
            break;
        case ABI_RISCV64_LP64:
            fprintf(out, "  ld   ra, 24(sp)\n");
            fprintf(out, "  ld   fp, 16(sp)\n");
            fprintf(out, "  addi sp, sp, 32\n");
            fprintf(out, "  ret\n");
            break;
    }
}

const char *abi_get_arg_reg(ABIInfo *abi, int32_t index) {
    if (!abi || index < 0 || index >= abi->num_arg_regs) return NULL;
    return abi->arg_regs[index];
}
