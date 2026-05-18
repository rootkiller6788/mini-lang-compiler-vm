#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "instruction_selection.h"
#include "reg_alloc.h"
#include "peephole.h"
#include "abi_target.h"

typedef enum {
    ARCH_X86,
    ARCH_ARM,
    ARCH_RISCV
} TargetArch;

typedef struct {
    TargetArch target;
    InstructionList ilist;
    RegAllocContext ra_ctx;
    ABIInfo abi;
    PeepholeContext peep_ctx;
} CodeGen;

typedef struct {
    const char *name;
    int32_t arg_count;
    int32_t local_count;
} IRFunction;

void codegen_init(CodeGen *cg, TargetArch arch);
void codegen_run(CodeGen *cg, IRFunction *func, IRNode *ir_root);
void codegen_prologue(CodeGen *cg, FILE *out);
void codegen_epilogue(CodeGen *cg, FILE *out);
void codegen_emit_asm(CodeGen *cg, IRFunction *func, FILE *out);
void codegen_free(CodeGen *cg);

#endif
