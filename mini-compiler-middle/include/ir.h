#ifndef IR_H
#define IR_H

#include <stdbool.h>
#include <stdio.h>

#define MAX_INSTRUCTIONS 1024
#define MAX_BLOCKS 128
#define MAX_TEMP_REGS 256
#define MAX_LABEL_LEN 32

typedef enum {
    IR_ADD,
    IR_SUB,
    IR_MUL,
    IR_DIV,
    IR_LOAD,
    IR_STORE,
    IR_BR,
    IR_BRCOND,
    IR_CALL,
    IR_RET,
    IR_MOV,
    IR_PHI,
    IR_ALLOCA
} IROp;

typedef struct {
    IROp op;
    int dest;
    int src1;
    int src2;
    char label[MAX_LABEL_LEN];
    char src1_label[MAX_LABEL_LEN];
    char src2_label[MAX_LABEL_LEN];
} IRInst;

typedef struct {
    char name[64];
    IRInst instructions[MAX_INSTRUCTIONS];
    int num_inst;
    int next_temp;
} IRFunction;

typedef struct {
    int label;
    int inst_indices[MAX_INSTRUCTIONS];
    int num_inst;
    int predecessors[MAX_BLOCKS];
    int num_pred;
    int successors[MAX_BLOCKS];
    int num_succ;
} IRBasicBlock;

IRFunction* ir_create_function(const char* name);
int      ir_emit(IRFunction* func, IROp op, int dest, int src1, int src2, const char* label);
int      ir_new_label(IRFunction* func);
int      ir_new_temp(IRFunction* func);
void     ir_print_function(const IRFunction* func, FILE* out);
int      ir_build_cfg(const IRFunction* func, IRBasicBlock blocks[], int max_blocks);
void     ir_destroy_function(IRFunction* func);

const char* ir_op_name(IROp op);

#endif
