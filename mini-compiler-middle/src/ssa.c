#include "ssa.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

void dom_compute_dominators(const IRBasicBlock blocks[], int num_blocks,
                            int entry, int doms_out[MAX_BLOCKS][MAX_BLOCKS]) {
    int i, j, k;

    for (i = 0; i < num_blocks; i++) {
        for (j = 0; j < num_blocks; j++) {
            doms_out[i][j] = (i == entry) ? 1 : 0;
        }
    }
    for (j = 0; j < num_blocks; j++) {
        doms_out[entry][j] = (j == entry) ? 1 : 0;
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (i = 0; i < num_blocks; i++) {
            if (i == entry) continue;

            int temp[MAX_BLOCKS];
            for (j = 0; j < num_blocks; j++) temp[j] = 1;

            bool first_pred = true;
            for (int p = 0; p < blocks[i].num_pred; p++) {
                int pred = blocks[i].predecessors[p];
                if (first_pred) {
                    for (j = 0; j < num_blocks; j++) temp[j] = doms_out[pred][j];
                    first_pred = false;
                } else {
                    for (j = 0; j < num_blocks; j++) temp[j] = temp[j] && doms_out[pred][j];
                }
            }

            temp[i] = 1;
            for (j = 0; j < num_blocks; j++) {
                if (temp[j] != doms_out[i][j]) {
                    doms_out[i][j] = temp[j];
                    changed = true;
                }
            }
        }
    }
}

void dom_compute_dominance_frontier(const IRBasicBlock blocks[], int num_blocks,
                                     const int doms[MAX_BLOCKS][MAX_BLOCKS],
                                     int df_out[MAX_BLOCKS][MAX_BLOCKS]) {
    int i, j;

    for (i = 0; i < num_blocks; i++) {
        for (j = 0; j < num_blocks; j++) {
            df_out[i][j] = 0;
        }
    }

    for (i = 0; i < num_blocks; i++) {
        if (blocks[i].num_pred < 2) continue;
        for (int p = 0; p < blocks[i].num_pred; p++) {
            int runner = blocks[i].predecessors[p];
            while (runner != -1 && !df_out[runner][i]) {
                df_out[runner][i] = 1;
                int strict_dom = -1;
                for (j = 0; j < num_blocks; j++) {
                    if (doms[runner][j] && j != runner) {
                        strict_dom = j;
                        break;
                    }
                }
                runner = strict_dom;
            }
        }
    }
}

static bool var_is_defined_at(const IRFunction* func, int var_idx, int bb_idx,
                              const IRBasicBlock blocks[]) {
    const IRBasicBlock* bb = &blocks[bb_idx];
    for (int i = 0; i < bb->num_inst; i++) {
        const IRInst* inst = &func->instructions[bb->inst_indices[i]];
        if (inst->dest == var_idx && inst->op != IR_PHI) return true;
    }
    return false;
}

static bool bb_has_phi_for_var(const IRFunction* func, int bb_idx, int var_idx,
                               const IRBasicBlock blocks[]) {
    const IRBasicBlock* bb = &blocks[bb_idx];
    for (int i = 0; i < bb->num_inst; i++) {
        const IRInst* inst = &func->instructions[bb->inst_indices[i]];
        if (inst->op == IR_PHI && inst->dest == var_idx) return true;
    }
    return false;
}

int ssa_place_phi(IRFunction* func, IRBasicBlock blocks[], int num_blocks) {
    int doms[MAX_BLOCKS][MAX_BLOCKS];
    int df[MAX_BLOCKS][MAX_BLOCKS];
    int entry = 0;
    int phi_count = 0;

    dom_compute_dominators(blocks, num_blocks, entry, doms);
    dom_compute_dominance_frontier(blocks, num_blocks, doms, df);

    bool has_phi[MAX_BLOCKS][MAX_VARS];
    memset(has_phi, 0, sizeof(has_phi));

    for (int var = 0; var < func->next_temp; var++) {
        for (int b = 0; b < num_blocks; b++) {
            if (!var_is_defined_at(func, var, b, blocks)) continue;
            for (int w = 0; w < num_blocks; w++) {
                if (!df[b][w] || has_phi[w][var]) continue;
                if (func->num_inst >= MAX_INSTRUCTIONS) continue;
                IRInst* phi = &func->instructions[func->num_inst];
                phi->op = IR_PHI;
                phi->dest = var;
                phi->src1 = var;
                phi->src2 = var;
                snprintf(phi->label, MAX_LABEL_LEN, "%d", blocks[0].label);
                snprintf(phi->src1_label, MAX_LABEL_LEN, "%d", blocks[0].label);
                func->num_inst++;
                phi_count++;
                has_phi[w][var] = true;
            }
        }
    }
    return phi_count;
}

void ssa_rename(IRFunction* func, IRBasicBlock blocks[], int num_blocks) {
    SSABuilder builder;
    memset(&builder, 0, sizeof(builder));
    for (int i = 0; i < MAX_VARS; i++) {
        builder.current_def[i] = -1;
        builder.stack_top[i] = 0;
    }

    for (int b = 0; b < num_blocks; b++) {
        for (int i = 0; i < blocks[b].num_inst; i++) {
            int idx = blocks[b].inst_indices[i];
            IRInst* inst = &func->instructions[idx];

            if (inst->op != IR_PHI) {
                if (inst->src1 >= 0) {
                    if (builder.current_def[inst->src1] != -1) {
                        inst->src1 = builder.current_def[inst->src1];
                    }
                }
                if (inst->src2 >= 0) {
                    if (builder.current_def[inst->src2] != -1) {
                        inst->src2 = builder.current_def[inst->src2];
                    }
                }
            }

            if (inst->dest >= 0) {
                int new_def = func->next_temp++;
                builder.current_def[inst->dest] = new_def;
                inst->dest = new_def;
            }
        }

        for (int s = 0; s < blocks[b].num_succ; s++) {
            int succ = blocks[b].successors[s];
            for (int i = 0; i < blocks[succ].num_inst; i++) {
                int idx = blocks[succ].inst_indices[i];
                IRInst* inst = &func->instructions[idx];
                if (inst->op == IR_PHI) {
                    for (int p = 0; p < blocks[succ].num_pred; p++) {
                        if (blocks[succ].predecessors[p] == b) {
                            if (builder.current_def[inst->dest] != -1) {
                                if (p == 0) inst->src1 = builder.current_def[inst->dest];
                                else inst->src2 = builder.current_def[inst->dest];
                            }
                        }
                    }
                }
            }
        }
    }
}

void ssa_build(IRFunction* func) {
    IRBasicBlock blocks[MAX_BLOCKS];
    int num_blocks = ir_build_cfg(func, blocks, MAX_BLOCKS);
    if (num_blocks == 0) return;

    ssa_place_phi(func, blocks, num_blocks);
    ssa_rename(func, blocks, num_blocks);
}

void ssa_print(const IRFunction* func, FILE* out) {
    if (!func || !out) return;
    fprintf(out, "=== SSA Form ===\n");
    ir_print_function(func, out);
}
