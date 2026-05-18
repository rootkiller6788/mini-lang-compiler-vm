#ifndef SSA_H
#define SSA_H

#include <stdbool.h>
#include <stdio.h>
#include "ir.h"

#define MAX_VARS 256

typedef struct {
    int current_def[MAX_VARS];
    bool sealed_blocks[MAX_BLOCKS];
    int phi_placed[MAX_BLOCKS][MAX_VARS];
    int phi_count[MAX_BLOCKS];
    int var_stack[MAX_VARS][MAX_INSTRUCTIONS];
    int stack_top[MAX_VARS];
} SSABuilder;

void     ssa_build(IRFunction* func);
int      ssa_place_phi(IRFunction* func, IRBasicBlock blocks[], int num_blocks);
void     ssa_rename(IRFunction* func, IRBasicBlock blocks[], int num_blocks);
void     ssa_print(const IRFunction* func, FILE* out);

void     dom_compute_dominators(const IRBasicBlock blocks[], int num_blocks,
                                int entry, int doms_out[MAX_BLOCKS][MAX_BLOCKS]);
void     dom_compute_dominance_frontier(const IRBasicBlock blocks[], int num_blocks,
                                         const int doms[MAX_BLOCKS][MAX_BLOCKS],
                                         int df_out[MAX_BLOCKS][MAX_BLOCKS]);

#endif
