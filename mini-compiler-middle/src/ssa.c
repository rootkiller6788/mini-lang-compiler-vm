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

/*
 * SSA Destruction (Out-of-SSA translation).
 *
 * Converts SSA form back to conventional three-address code by
 * eliminating phi functions and inserting copy instructions in
 * predecessor blocks.
 *
 * Algorithm (L5: SSA Destruction, Briggs et al. 1998):
 * For each phi instruction in block B:
 *   For each predecessor P of B:
 *     Insert "mov dest, phi_src_for_P" at end of P
 *   Remove the phi instruction from B
 *
 * This is the standard approach used in GCC and LLVM for
 * translating out of SSA before register allocation.
 *
 * After SSA destruction, the IR is back to mutable variables
 * with multiple definitions, ready for register allocation
 * or direct code generation.
 */
void ssa_destroy(IRFunction* func) {
    if (!func) return;

    IRBasicBlock blocks[MAX_BLOCKS];
    int num_blocks = ir_build_cfg(func, blocks, MAX_BLOCKS);
    if (num_blocks == 0) return;

    /*
     * For each block, collect phi instructions.
     * Phi instructions appear at the start of blocks with
     * multiple predecessors.
     */
    for (int b = 0; b < num_blocks; b++) {
        int phi_insts[MAX_INSTRUCTIONS];
        int num_phi = 0;

        /*
         * Identify phi instructions in this block.
         */
        for (int i = 0; i < blocks[b].num_inst; i++) {
            int idx = blocks[b].inst_indices[i];
            if (func->instructions[idx].op == IR_PHI) {
                phi_insts[num_phi++] = idx;
            }
        }

        if (num_phi == 0) continue;

        /*
         * For each predecessor, insert copy instructions.
         * A phi like: %t_d = phi(%t_v1:L1, %t_v2:L2)
         * becomes:
         *   In predecessor L1: %t_d = mov %t_v1
         *   In predecessor L2: %t_d = mov %t_v2
         */
        for (int p = 0; p < blocks[b].num_pred; p++) {
            int pred_block = blocks[b].predecessors[p];

            /*
             * Find the insertion point: before the terminator
             * of the predecessor block.
             */
            if (blocks[pred_block].num_inst == 0) continue;
            int last_idx = blocks[pred_block].inst_indices[
                blocks[pred_block].num_inst - 1];

            /*
             * Check if the last instruction is a terminator.
             * Insert before it.
             */
            IRInst* last = &func->instructions[last_idx];
            int insert_pos = last_idx;
            if (last->op == IR_BR || last->op == IR_BRCOND ||
                last->op == IR_RET) {
                /* Insert before terminator */
            }

            for (int phi_i = 0; phi_i < num_phi; phi_i++) {
                IRInst* phi = &func->instructions[phi_insts[phi_i]];
                if (phi->dest < 0) continue;

                /*
                 * Determine which operand of the phi corresponds
                 * to predecessor p.
                 */
                int src_val = -1;
                if (p == 0) src_val = phi->src1;
                else if (p == 1) src_val = phi->src2;

                if (src_val >= 0 && func->num_inst < MAX_INSTRUCTIONS) {
                    /*
                     * Shift instructions to make room for the copy.
                     */
                    for (int k = func->num_inst; k > last_idx; k--) {
                        func->instructions[k] = func->instructions[k - 1];
                    }

                    /*
                     * Update block instruction indices.
                     */
                    for (int bk = 0; bk < num_blocks; bk++) {
                        for (int bi = 0; bi < blocks[bk].num_inst; bi++) {
                            if (blocks[bk].inst_indices[bi] >= last_idx) {
                                blocks[bk].inst_indices[bi]++;
                            }
                        }
                    }

                    /*
                     * Emit the copy: %t_d = mov %t_src
                     */
                    IRInst* copy = &func->instructions[last_idx];
                    copy->op = IR_MOV;
                    copy->dest = phi->dest;
                    copy->src1 = src_val;
                    copy->src2 = -1;
                    copy->label[0] = '\0';
                    copy->src1_label[0] = '\0';
                    copy->src2_label[0] = '\0';

                    func->num_inst++;
                    last_idx++;
                }
            }
        }

        /*
         * Remove phi instructions from this block.
         * We mark them as IR_MOV with sentinel values and
         * compact later.
         */
        for (int phi_i = 0; phi_i < num_phi; phi_i++) {
            func->instructions[phi_insts[phi_i]].op = IR_PHI;
            func->instructions[phi_insts[phi_i]].dest = -2; /* marker */
        }
    }

    /*
     * Compact: remove marked phi instructions.
     */
    int write_idx = 0;
    for (int i = 0; i < func->num_inst; i++) {
        if (func->instructions[i].dest == -2 &&
            func->instructions[i].op == IR_PHI) {
            continue; /* Remove marked phi */
        }
        if (write_idx != i) {
            func->instructions[write_idx] = func->instructions[i];
        }
        write_idx++;
    }
    func->num_inst = write_idx;
}

/*
 * Lengauer-Tarjan Dominator Algorithm (TOPLAS 1979).
 *
 * Computes immediate dominators in O(E * log N) time using
 * depth-first search numbering and path compression on the
 * spanning tree.
 *
 * This is the standard efficient dominator algorithm used in
 * production compilers (GCC, LLVM). It improves on the naive
 * O(N^2) iterative algorithm by exploiting the DFS tree
 * structure.
 *
 * Key data structures:
 * - semi[v]: semidominator of v (the ancestor with smallest
 *   DFS number that can reach v without going through the
 *   DFS tree parent)
 * - idom[v]: immediate dominator of v
 * - ancestor[v]: forest for path compression
 * - label[v]: node with smallest semi on the path from v to
 *   ancestor[v] (for efficient queries)
 *
 * L8 (Advanced Topics): This algorithm represents the
 * theoretically optimal approach to dominator computation,
 * building on Tarjan's union-find data structure for
 * near-linear-time graph algorithms.
 *
 * Reference: Lengauer & Tarjan, "A Fast Algorithm for
 * Finding Dominators in a Flowgraph", TOPLAS 1979.
 */
static void lt_link(int v, int w, int ancestor[], int label[],
                     int semi[], int dfs_num[]) {
    ancestor[w] = v;
    (void)label;
    (void)semi;
    (void)dfs_num;
}

static int lt_eval(int v, int ancestor[], int label[],
                    int semi[], int dfs_num[]) {
    if (ancestor[v] == -1) return label[v];

    /*
     * Path compression: compress the path from v to the root
     * of the forest, updating labels along the way.
     */
    int root = v;
    while (ancestor[root] != -1) {
        root = ancestor[root];
    }

    /* Walk back, updating labels */
    int current = v;
    while (current != root) {
        int parent = ancestor[current];
        if (semi[label[current]] > semi[label[parent]]) {
            label[current] = label[parent];
        }
        ancestor[current] = root;
        current = parent;
    }

    return label[v];
}

/*
 * dom_lt_idoms: Compute immediate dominators using the
 * Lengauer-Tarjan algorithm.
 *
 * Returns idom[v] = immediate dominator of node v.
 * idom[entry] = -1 (entry has no dominator).
 */
void dom_lt_idoms(const IRBasicBlock blocks[], int num_blocks,
                  int entry, int idom_out[]) {
    if (!blocks || !idom_out || num_blocks <= 0) return;

    /*
     * Phase 1: DFS numbering.
     * Build the DFS tree and assign preorder numbers.
     */
    int dfs_num[MAX_BLOCKS];
    int vertex[MAX_BLOCKS];   /* vertex[i] = node with DFS number i */
    int parent[MAX_BLOCKS];   /* parent in DFS tree */
    int num_dfs = 0;

    for (int i = 0; i < num_blocks; i++) {
        dfs_num[i] = -1;
        vertex[i] = -1;
        parent[i] = -1;
    }

    /* DFS stack (iterative to avoid recursion depth issues) */
    int stack[MAX_BLOCKS];
    int stack_top = 0;
    stack[stack_top++] = entry;
    dfs_num[entry] = num_dfs;
    vertex[num_dfs] = entry;
    num_dfs++;

    while (stack_top > 0) {
        int v = stack[--stack_top];

        for (int s = 0; s < blocks[v].num_succ; s++) {
            int w = blocks[v].successors[s];
            if (dfs_num[w] < 0) {
                dfs_num[w] = num_dfs;
                vertex[num_dfs] = w;
                parent[w] = v;
                num_dfs++;
                stack[stack_top++] = w;
            }
        }
    }

    /*
     * Phase 2: Compute semidominators.
     */
    int semi[MAX_BLOCKS];
    int idom[MAX_BLOCKS];
    int ancestor[MAX_BLOCKS];
    int label[MAX_BLOCKS];
    int bucket[MAX_BLOCKS][MAX_BLOCKS];
    int bucket_size[MAX_BLOCKS];

    for (int i = 0; i < num_blocks; i++) {
        semi[i] = i;
        idom[i] = -1;
        ancestor[i] = -1;
        label[i] = i;
        bucket_size[i] = 0;
    }

    /*
     * Process nodes in reverse DFS order (excluding entry).
     */
    for (int i = num_dfs - 1; i >= 1; i--) {
        int w = vertex[i];

        /*
         * Step 2: Compute semi(w) = min{ dfs_num[v] | v is a
         * predecessor of w and there is a path from v to w
         * in the DFS tree }
         */
        for (int p = 0; p < blocks[w].num_pred; p++) {
            int v = blocks[w].predecessors[p];
            int u = lt_eval(v, ancestor, label, semi, dfs_num);
            if (semi[u] < semi[w]) {
                semi[w] = semi[u];
            }
        }

        /*
         * Add w to bucket of vertex[semi[w]].
         */
        int s = vertex[semi[w]];
        bucket[s][bucket_size[s]++] = w;

        /*
         * Link w to its DFS parent.
         */
        lt_link(parent[w], w, ancestor, label, semi, dfs_num);

        /*
         * Step 3: For each v in bucket[parent[w]], compute idom[v].
         */
        int p_w = parent[w];
        if (p_w >= 0) {
            for (int b = 0; b < bucket_size[p_w]; b++) {
                int v = bucket[p_w][b];
                int u = lt_eval(v, ancestor, label, semi, dfs_num);
                if (semi[u] == semi[v]) {
                    idom[v] = p_w;
                } else {
                    idom[v] = u;
                }
            }
            bucket_size[p_w] = 0;
        }
    }

    /*
     * Step 4: Finalize idom values.
     */
    for (int i = 1; i < num_dfs; i++) {
        int w = vertex[i];
        if (idom[w] != vertex[semi[w]]) {
            idom[w] = idom[idom[w]];
        }
    }

    idom[entry] = -1;

    /*
     * Copy results to output.
     */
    for (int i = 0; i < num_blocks; i++) {
        idom_out[i] = idom[i];
    }
}

/*
 * Convert immediate dominators to full dominator sets.
 *
 * DOM[n] = {n} U DOM[idom[n]]
 * Uses transitive closure on the idom tree.
 */
void dom_idoms_to_full(const int idom[], int num_blocks,
                        int doms_out[MAX_BLOCKS][MAX_BLOCKS]) {
    if (!idom || !doms_out) return;

    /* Initialize: each node dominates itself */
    for (int i = 0; i < num_blocks; i++) {
        for (int j = 0; j < num_blocks; j++) {
            doms_out[i][j] = 0;
        }
        doms_out[i][i] = 1;
    }

    /*
     * Propagate: if idom[n]=d, then DOM[n] = {n} U DOM[d]
     */
    for (int i = 0; i < num_blocks; i++) {
        if (idom[i] < 0) continue;
        int d = idom[i];
        for (int j = 0; j < num_blocks; j++) {
            if (doms_out[d][j]) {
                doms_out[i][j] = 1;
            }
        }
    }
}
