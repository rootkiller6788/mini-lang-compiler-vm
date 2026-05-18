#ifndef ABI_TARGET_H
#define ABI_TARGET_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_ARG_REGS       8
#define MAX_SAVED_REGS     8
#define MAX_CALLEE_SAVED   6
#define MAX_CALLER_SAVED   9

typedef enum {
    ABI_X86_64_SYSV,
    ABI_ARM64_AAPCS,
    ABI_RISCV64_LP64
} ABIType;

typedef enum {
    ARG_CLASS_INTEGER,
    ARG_CLASS_SSE,
    ARG_CLASS_MEMORY,
    ARG_CLASS_NO_CLASS
} ArgClass;

typedef struct {
    ABIType abi_type;
    int32_t num_arg_regs;
    const char *arg_regs[MAX_ARG_REGS];
    const char *return_reg;
    int32_t num_caller_saved;
    const char *caller_saved[MAX_CALLER_SAVED];
    int32_t num_callee_saved;
    const char *callee_saved[MAX_CALLEE_SAVED];
    int32_t stack_alignment;
    int32_t red_zone;
    bool has_red_zone;
    int32_t arg_slot_offset;
    int32_t local_var_offset;
} ABIInfo;

typedef struct {
    ArgClass arg_classes[16];
    int32_t count;
} ArgClassList;

void abi_init(ABIInfo *abi, ABIType type);
ArgClass abi_classify_arg(const ABIInfo *abi, int32_t arg_index, int32_t type_size);
void abi_emit_call(ABIInfo *abi, const char *func_name, int32_t arg_count,
                   const char **arg_vregs, const char *result_reg, FILE *out);
void abi_emit_return(ABIInfo *abi, const char *ret_reg, FILE *out);
void abi_emit_prologue(ABIInfo *abi, FILE *out);
void abi_emit_epilogue(ABIInfo *abi, FILE *out);
const char *abi_get_arg_reg(ABIInfo *abi, int32_t index);

#endif
