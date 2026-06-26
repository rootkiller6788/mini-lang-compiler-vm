#ifndef BACKEND_H
#define BACKEND_H

#include <stdbool.h>
#include <stdio.h>
#include "ir.h"

#define MAX_TARGET_INST 2048
#define MAX_OPERANDS 4
#define STACK_SLOTS 128

typedef enum {
    TGT_MOV,
    TGT_ADD,
    TGT_SUB,
    TGT_IMUL,
    TGT_IDIV,
    TGT_LOAD,
    TGT_STORE,
    TGT_JMP,
    TGT_JZ,
    TGT_JNZ,
    TGT_CALL,
    TGT_RET,
    TGT_PUSH,
    TGT_POP,
    TGT_CMP,
    TGT_LEA,
    TGT_AND,
    TGT_OR,
    TGT_XOR,
    TGT_SHL,
    TGT_SHR,
    TGT_LABEL,
    TGT_COMMENT
} TargetOp;

typedef struct {
    TargetOp op;
    int operands[MAX_OPERANDS];
    int num_operands;
    char label[MAX_LABEL_LEN];
    char comment[128];
} TargetInst;

typedef struct {
    int offset;
    int size;
    bool used;
    int temp_id;
} StackSlot;

typedef struct {
    StackSlot slots[STACK_SLOTS];
    int num_slots;
    int total_size;
} StackFrame;

typedef struct {
    TargetInst instructions[MAX_TARGET_INST];
    int num_inst;
    StackFrame frame;
    int next_label;
    int reg_map[MAX_TEMP_REGS];
    const char* func_name;
} CodeGen;

typedef struct {
    int irmov_count;
    int arith_count;
    int mem_count;
    int branch_count;
    int total_count;
} CGStats;

CodeGen* cg_create(const char* func_name);
void cg_destroy(CodeGen* cg);
int  cg_emit(CodeGen* cg, TargetOp op, int o0, int o1, int o2, const char* label);
int  cg_new_label(CodeGen* cg);
void cg_generate(CodeGen* cg, const IRFunction* func, const int reg_assignments[]);
void cg_print_asm(const CodeGen* cg, FILE* out);
int  cg_allocate_stack_slot(CodeGen* cg, int temp_id, int size);
void cg_peephole_optimize(CodeGen* cg);
CGStats cg_get_stats(const CodeGen* cg);

const char* tgt_op_name(TargetOp op);

#endif
