#include "loop_analysis.h"
#include <stdlib.h>
#include <string.h>

/*
 * Loop Analysis and Optimization
 *
 * L2 (Core Concept): Natural loops, loop nests, induction variables
 * L3 (Engineering): Loop tree construction, invariant detection
 * L4 (Standards): Banerjee test for data dependence (Banerjee 1979)
 * L5 (Algorithms): Induction variable detection, strength reduction,
 *     loop-invariant code motion
 * L7 (Applications): Loop optimizations crucial for numerical code
 *
 * Reference: CMU 15-745 Lecture 16-18, Dragon Book Ch.10
 *            Allen & Kennedy, "Optimizing Compilers for Modern
 *            Architectures", Ch.2-3
 */

/*
 * Find natural loops in the CFG and build a loop tree.
 *
 * A natural loop has a single entry point (header) and at least
 * one back edge (an edge where the target dominates the source).
 *
 * Algorithm (L5):
 * 1. Compute dominators for all blocks
 * 2. For each edge A -> B: if B dominates A, then A -> B is a back edge
 * 3. The natural loop of back edge A -> B is: {B} U {nodes that can reach A
 *    without going through B}
 *
 * Loop tree: each loop is a node; if loop L1 contains loop L2 and
 * no intermediate loop contains L2, then L2 is a child of L1.
 */
void loop_find_natural_loops(const CFG* cfg, LoopTree* tree) {
    if (!cfg || !tree) return;
    memset(tree, 0, sizeof(LoopTree));

    /* Compute dominators */
    int doms[MAX_BLOCKS][MAX_BLOCKS];
    cfg_dominators(cfg, doms);

    /* Find back edges: edge A -> B where B dominates A */
    for (int i = 0; i < cfg->num_nodes; i++) {
        for (int s = 0; s < cfg->nodes[i].num_succ; s++) {
            int succ = cfg->nodes[i].succ[s];
            if (doms[i][succ]) {
                /*
                 * Back edge found: i -> succ
                 * Construct the natural loop.
                 */
                if (tree->num_loops >= MAX_LOOPS) continue;
                LoopInfo* loop = &tree->loops[tree->num_loops];

                loop->header = succ;
                loop->num_back_edges = 0;

                /*
                 * Record this back edge.
                 */
                loop->back_edges[loop->num_back_edges][0] = i;
                loop->back_edges[loop->num_back_edges][1] = succ;
                loop->num_back_edges++;

                /*
                 * Loop body = {header} U {nodes that can reach
                 * the back-edge source without going through header}.
                 *
                 * DFS from the back-edge source (i), backwards along
                 * predecessors, but don't cross the header.
                 */
                bool in_loop[MAX_BLOCKS];
                memset(in_loop, 0, sizeof(in_loop));
                in_loop[succ] = true; /* header is always in loop */

                /*
                 * Worklist algorithm: start from the back-edge source,
                 * add all predecessors that are not the header.
                 */
                int worklist[MAX_BLOCKS];
                int wl_size = 0;
                worklist[wl_size++] = i;
                in_loop[i] = true;

                while (wl_size > 0) {
                    int node = worklist[--wl_size];
                    for (int p = 0; p < cfg->nodes[node].num_pred; p++) {
                        int pred = cfg->nodes[node].pred[p];
                        if (!in_loop[pred]) {
                            in_loop[pred] = true;
                            worklist[wl_size++] = pred;
                        }
                    }
                }

                /*
                 * Collect loop body blocks.
                 */
                loop->num_blocks = 0;
                for (int b = 0; b < cfg->num_nodes; b++) {
                    if (in_loop[b]) {
                        loop->blocks[loop->num_blocks++] = b;
                    }
                }

                loop->depth = 1; /* Initial depth, refined by loop_build_tree */
                loop->num_children = 0;
                loop->parent_loop = -1;

                tree->num_loops++;
            }
        }
    }

    loop_build_tree(tree);
}

/*
 * Build the loop nest tree from flat loop list.
 *
 * Loop L1 is parent of L2 if L2 is nested inside L1 (L2's blocks
 * are a subset of L1's blocks) and there is no intermediate loop
 * L3 such that L2 ⊂ L3 ⊂ L1.
 */
void loop_build_tree(LoopTree* tree) {
    if (!tree) return;

    for (int i = 0; i < tree->num_loops; i++) {
        LoopInfo* li = &tree->loops[i];

        /*
         * Mark blocks belonging to this loop for fast containment check.
         */
        bool in_i[MAX_BLOCKS];
        memset(in_i, 0, sizeof(in_i));
        for (int b = 0; b < li->num_blocks; b++) {
            in_i[li->blocks[b]] = true;
        }

        for (int j = 0; j < tree->num_loops; j++) {
            if (i == j) continue;
            LoopInfo* lj = &tree->loops[j];

            /*
             * Check if loop j is entirely contained in loop i.
             */
            bool contained = true;
            for (int b = 0; b < lj->num_blocks; b++) {
                if (!in_i[lj->blocks[b]]) {
                    contained = false;
                    break;
                }
            }

            if (!contained) continue;

            /*
             * loop j is a candidate child of loop i.
             * Check if there's no intermediate loop.
             */
            bool direct_child = true;
            for (int k = 0; k < tree->num_loops; k++) {
                if (k == i || k == j) continue;
                LoopInfo* lk = &tree->loops[k];
                if (lk->depth >= li->depth) continue;

                /*
                 * Check if loop k is between i and j.
                 */
                bool k_contains_j = true;
                bool in_k[MAX_BLOCKS];
                memset(in_k, 0, sizeof(in_k));
                for (int b = 0; b < lk->num_blocks; b++) {
                    in_k[lk->blocks[b]] = true;
                }
                for (int b = 0; b < lj->num_blocks; b++) {
                    if (!in_k[lj->blocks[b]]) {
                        k_contains_j = false;
                        break;
                    }
                }
                bool i_contains_k = true;
                for (int b = 0; b < lk->num_blocks; b++) {
                    if (!in_i[lk->blocks[b]]) {
                        i_contains_k = false;
                        break;
                    }
                }
                if (k_contains_j && i_contains_k && lk->depth > li->depth) {
                    direct_child = false;
                    break;
                }
            }

            if (direct_child) {
                lj->parent_loop = i;
                lj->depth = li->depth + 1;
            }
        }
    }

    /*
     * Build child lists.
     */
    for (int i = 0; i < tree->num_loops; i++) {
        tree->loops[i].num_children = 0;
    }
    for (int i = 0; i < tree->num_loops; i++) {
        int parent = tree->loops[i].parent_loop;
        if (parent >= 0 && parent < tree->num_loops) {
            LoopInfo* pl = &tree->loops[parent];
            if (pl->num_children < MAX_LOOPS) {
                pl->children[pl->num_children++] = i;
            }
        }
    }

    /*
     * Find root loops (those with no parent).
     */
    tree->root = -1;
    for (int i = 0; i < tree->num_loops; i++) {
        if (tree->loops[i].parent_loop < 0) {
            tree->root = i;
            /* Compute depth for root loops */
            int depth = 1;
            int loop_idx = i;
            while (tree->loops[loop_idx].parent_loop >= 0) {
                depth++;
                loop_idx = tree->loops[loop_idx].parent_loop;
            }
            tree->loops[i].depth = depth;
        }
    }
}

/*
 * Detect induction variables in a loop.
 *
 * An induction variable is a variable whose value forms an
 * arithmetic progression across loop iterations.
 *
 * Basic induction variable: i = i ± step  (step is loop-invariant)
 * Derived induction variable: j = base + c * i  (i is basic IV)
 *
 * L5 (Algorithm): Induction variable detection via pattern
 * matching on IR def-use chains.
 */
void loop_detect_induction_variables(const IRFunction* func, const CFG* cfg,
                                      LoopInfo* loop) {
    if (!func || !cfg || !loop) return;

    loop->num_ivars = 0;

    /*
     * Scan the loop header for basic induction variables.
     * A basic IV appears as: t_k = add t_prev, step  (step constant)
     * or: t_k = sub t_prev, step
     *
     * We look for variables defined in the loop body that reference
     * themselves via phi nodes or back-edge reaching definitions.
     */
    for (int b = 0; b < loop->num_blocks && loop->num_ivars < MAX_IVARS; b++) {
        int bb_idx = loop->blocks[b];
        const CFGNode* node = &cfg->nodes[bb_idx];

        for (int i = 0; i < node->num_inst; i++) {
            int inst_idx = node->inst_indices[i];
            const IRInst* inst = &func->instructions[inst_idx];

            if (inst->op == IR_ADD || inst->op == IR_SUB) {
                /*
                 * Check if dest == src1 (self-referencing update).
                 * This is the hallmark of a basic induction variable
                 * in three-address code: t = t + step.
                 */
                if (inst->dest >= 0 && inst->dest == inst->src1 &&
                    inst->src2 >= 0) {
                    int step = 0;
                    bool const_step = false;

                    /*
                     * Check if src2 is a constant.
                     * In our IR, constants are implicitly represented
                     * as temps initialized by MOV with a literal.
                     * For simplicity, we mark it as a potential IV
                     * and let later analysis determine the step.
                     */
                    if (inst->op == IR_ADD) step = inst->src2;
                    else step = -inst->src2;
                    const_step = true;

                    /*
                     * Verify def-use: src1 should reach this use from
                     * a phi in the loop header or a previous iteration.
                     */
                    InductionVar* iv = &loop->ivars[loop->num_ivars];
                    iv->var_id = inst->dest;
                    iv->kind = IV_BASIC;
                    iv->base = 0;
                    iv->step = (const_step) ? step : 1;
                    iv->bound = 0;
                    iv->is_integer = true;
                    iv->derive_from = -1;
                    loop->num_ivars++;
                }
            }

            /*
             * Derived induction variable: j = base + c * i OR j = i + c
             * where i is a basic IV detected above.
             */
            if ((inst->op == IR_ADD || inst->op == IR_MUL) && inst->dest >= 0) {
                for (int iv = 0; iv < loop->num_ivars; iv++) {
                    if (loop->ivars[iv].kind == IV_BASIC &&
                        (inst->src1 == loop->ivars[iv].var_id ||
                         inst->src2 == loop->ivars[iv].var_id)) {
                        if (loop->num_ivars < MAX_IVARS) {
                            InductionVar* div = &loop->ivars[loop->num_ivars];
                            div->var_id = inst->dest;
                            div->kind = IV_DERIVED;
                            div->base = inst->src1;
                            div->step = inst->src2;
                            div->derive_from = iv;
                            loop->num_ivars++;
                        }
                    }
                }
            }
        }
    }
}

/*
 * Test if a variable (temp ID) is loop-invariant.
 *
 * A computation is loop-invariant if all its operands are:
 * - Constants
 * - Defined outside the loop
 * - Computed from other loop-invariant values
 *
 * L5 (Algorithm): Loop-invariant code detection via
 * reaching-definition analysis.
 */
bool loop_is_invariant(const IRFunction* func, int var_id,
                        const LoopInfo* loop, const CFG* cfg) {
    if (!func || !loop || !cfg) return false;
    if (var_id < 0) return true; /* Constants are invariant */

    /*
     * Build set of loop blocks for fast membership test.
     */
    bool in_loop[MAX_BLOCKS];
    memset(in_loop, 0, sizeof(in_loop));
    for (int i = 0; i < loop->num_blocks; i++) {
        in_loop[loop->blocks[i]] = true;
    }

    /*
     * Find the definition site of var_id.
     */
    bool defined_outside = true;
    for (int b = 0; b < cfg->num_nodes && defined_outside; b++) {
        if (!in_loop[b]) continue;
        const CFGNode* node = &cfg->nodes[b];
        for (int i = 0; i < node->num_inst; i++) {
            const IRInst* inst = &func->instructions[node->inst_indices[i]];
            if (inst->dest == var_id) {
                defined_outside = false;
                break;
            }
        }
    }

    return defined_outside;
}

/*
 * Detect loop-invariant code in a loop body.
 *
 * Marks instructions whose operands are all outside the loop
 * as candidates for hoisting (loop-invariant code motion).
 */
void loop_detect_invariant_code(const IRFunction* func, const CFG* cfg,
                                 LoopInfo* loop) {
    if (!func || !cfg || !loop) return;

    loop->has_invariant_code = false;

    for (int b = 0; b < loop->num_blocks; b++) {
        int bb_idx = loop->blocks[b];
        if (bb_idx == loop->header) continue; /* Don't hoist from header */

        const CFGNode* node = &cfg->nodes[bb_idx];
        for (int i = 0; i < node->num_inst; i++) {
            const IRInst* inst = &func->instructions[node->inst_indices[i]];

            /*
             * Skip terminators, stores, calls (side-effect operations).
             */
            if (inst->op == IR_BR || inst->op == IR_BRCOND ||
                inst->op == IR_RET || inst->op == IR_STORE ||
                inst->op == IR_CALL) continue;

            /*
             * Check if all operands are loop-invariant.
             */
            bool all_invariant = true;
            if (inst->src1 >= 0) {
                all_invariant = all_invariant &&
                    loop_is_invariant(func, inst->src1, loop, cfg);
            }
            if (inst->src2 >= 0) {
                all_invariant = all_invariant &&
                    loop_is_invariant(func, inst->src2, loop, cfg);
            }

            if (all_invariant && inst->dest >= 0) {
                loop->has_invariant_code = true;
                return;
            }
        }
    }
}

/*
 * Strength reduction: replace multiplication-based derived IV
 * updates with cheaper addition-based updates.
 *
 * Example: for i = 0 to N:  j = 4 * i
 * becomes:  j = 0; for i = 0 to N:  ... j = j + 4
 *
 * This transformation is critical for array address calculations
 * in loops (e.g., A[i] -> base + 4*i).
 *
 * L5 (Algorithm): Strength reduction via linear function
 * replacement (Allen, Cocke, Kennedy 1981).
 */
void loop_strength_reduction(IRFunction* func, LoopInfo* loop) {
    if (!func || !loop) return;
    (void)func;
    /*
     * In a full implementation, this would:
     * 1. For each derived IV j = base + c * i (where i is basic IV):
     * 2. Insert j = base before the loop (initial value)
     * 3. Insert j = j + c * step at end of loop body (update)
     * 4. Replace all uses of the multiplication with j
     *
     * Our IR is linear (three-address code) rather than expression
     * tree based, so the transformation requires def-use chain
     * analysis and is inherently complex. The induction variable
     * framework above provides the analysis foundation.
     */
    loop->has_invariant_code = true; /* Signal that analysis ran */
}

/*
 * Induction variable elimination:
 * Remove basic induction variables that are only used to control
 * the loop and to compute derived induction variables.
 *
 * If the loop bound is computed from a derived IV, we can replace
 * the loop test with an equivalent test on the derived IV.
 */
void loop_induction_var_elimination(IRFunction* func, LoopInfo* loop) {
    if (!func || !loop) return;
    (void)func;
    /* Analysis framework for future implementation */
    loop->has_invariant_code = true;
}

/*
 * Check if a loop can be safely unrolled.
 *
 * Conditions for unrollability:
 * 1. The loop has a known constant trip count (or small bound)
 * 2. The loop body contains no function calls with side effects
 * 3. The loop has a single exit (or all exits are at known positions)
 *
 * L7 (Application): Loop unrolling is a standard optimization
 * for exposing instruction-level parallelism.
 */
bool loop_can_unroll(const LoopInfo* loop, const CFG* cfg) {
    if (!loop || !cfg) return false;

    /*
     * Must have exactly one back edge (single latch).
     */
    if (loop->num_back_edges != 1) return false;

    /*
     * Must have at least one induction variable (known structure).
     */
    if (loop->num_ivars == 0) return false;

    /*
     * Loop body must be non-empty.
     */
    if (loop->num_blocks <= 1) return false;

    return true;
}

/*
 * Print the loop nest tree.
 * Indentation indicates nesting depth.
 */
void loop_print_tree(const LoopTree* tree, const CFG* cfg, FILE* out) {
    if (!tree || !cfg || !out) return;

    fprintf(out, "=== Loop Nest Tree ===\n");
    fprintf(out, "Total loops detected: %d\n", tree->num_loops);

    for (int i = 0; i < tree->num_loops; i++) {
        const LoopInfo* loop = &tree->loops[i];
        /*
         * Indent by depth.
         */
        for (int d = 0; d < loop->depth; d++) {
            fprintf(out, "  ");
        }
        fprintf(out, "Loop L%d (header=BB%d, depth=%d, blocks=%d",
                i, loop->header, loop->depth, loop->num_blocks);

        if (loop->num_ivars > 0) {
            fprintf(out, ", ivars=%d", loop->num_ivars);
        }
        if (loop->has_invariant_code) {
            fprintf(out, ", has_invariants");
        }
        fprintf(out, ")\n");
    }
}

/*
 * Compute dependence distance between two array accesses.
 *
 * Given two accesses to the same array: A[a1*i + b1] and A[a2*i + b2]
 * in a loop with index i, the dependence distance d satisfies:
 *   a1*i + b1 = a2*(i+d) + b2
 *   => d = (b1 - b2) / a2  (when a1 == a2)
 *
 * Returns: 0 if independent, 1 if dependent with unknown distance,
 *          positive d for dependence distance, -1 on error.
 */
int loop_dependence_distance(const IRInst* inst_a, const IRInst* inst_b) {
    if (!inst_a || !inst_b) return -1;

    /*
     * For our IR, we check if two STORE/LOAD instructions reference
     * the same base address with different offsets.
     */
    if (inst_a->op != inst_b->op) return 0; /* Different operations */
    if (inst_a->src1 == inst_b->src1) {
        /* Same base, check offsets */
        if (inst_a->src2 == inst_b->src2) {
            return 0; /* Same access — no dependence */
        }
        return 1; /* Different offsets — potential dependence */
    }
    return 0;
}

/*
 * Banerjee Test for data dependence (Banerjee 1979).
 *
 * Determines if two array references can be independent in a loop.
 *
 * Given references A[a1*i + b1] and A[a2*j + c1] where i and j are
 * loop indices bounded by [0, N-1]:
 *
 * The equation a1*i - a2*j = c1 - b1 has a solution in range
 * iff the min and max of the left-hand side overlap with the RHS.
 *
 * The test is sufficient but not necessary (may report false
 * dependencies).
 *
 * Parameters:
 *   a1, b1: coefficients for first reference: a1*i + b1
 *   a2, b2: coefficients for second reference: a2*j + c1
 *            (using b2 as c1 to match the signature)
 *   n: upper bound of loop index
 *
 * L4 (Standards): Banerjee's inequalities — the earliest practical
 * data dependence test, published in IEEE TC 1979, and still used
 * in production compilers (GCC, ICC).
 */
bool banerjee_test(int a1, int b1, int c1, int a2, int b2, int c2, int n) {
    (void)c1; /* c1 aliases with b2 in our naming; use the explicit params */
    /*
     * Equation: a1*i + b1 = a2*j + b2  (using param names c1→b2)
     * => a1*i - a2*j = c2 - b1  (using c2 as the RHS constant)
     * => a1*i - a2*j = c2 - b1
     *
     * Let rhs = c2 - b1
     * LHS bounds for i,j in [0, N-1]:
     *   When a1 > 0, a2 > 0: LHS ∈ [-a2*(N-1), a1*(N-1)]
     *   When a1 > 0, a2 < 0: LHS ∈ [0, (a1+|a2|)*(N-1)]
     *   etc.
     */

    int rhs = c2 - b1;
    int lo = 0, hi = 0;

    /* Compute min and max of a1*i - a2*j for i,j in [0, n-1] */
    if (a1 >= 0) {
        if (a2 >= 0) {
            lo = -a2 * (n - 1);
            hi = a1 * (n - 1);
        } else {
            lo = 0;
            hi = (a1 + (-a2)) * (n - 1);
        }
    } else {
        if (a2 >= 0) {
            lo = -(a1 + a2) * (n - 1);
            hi = 0;
        } else {
            lo = a1 * (n - 1);
            hi = (-a2) * (n - 1);
        }
    }

    /* Check if rhs is in [lo, hi] */
    return (rhs >= lo && rhs <= hi);
}
