#include "dataflow.h"
#include <stdlib.h>
#include <string.h>

void bv_init(BitVector* bv) {
    if (!bv) return;
    memset(bv->bits, 0, sizeof(bv->bits));
}

void bv_set(BitVector* bv, int idx) {
    if (!bv || idx < 0) return;
    int word = idx / 32;
    int bit = idx % 32;
    bv->bits[word] |= (1u << bit);
}

void bv_clear(BitVector* bv, int idx) {
    if (!bv || idx < 0) return;
    int word = idx / 32;
    int bit = idx % 32;
    bv->bits[word] &= ~(1u << bit);
}

bool bv_test(const BitVector* bv, int idx) {
    if (!bv || idx < 0) return false;
    int word = idx / 32;
    int bit = idx % 32;
    return (bv->bits[word] & (1u << bit)) != 0;
}

void bv_union(BitVector* dst, const BitVector* src) {
    if (!dst || !src) return;
    for (int i = 0; i < BITVECTOR_WORDS; i++) {
        dst->bits[i] |= src->bits[i];
    }
}

void bv_intersect(BitVector* dst, const BitVector* src) {
    if (!dst || !src) return;
    for (int i = 0; i < BITVECTOR_WORDS; i++) {
        dst->bits[i] &= src->bits[i];
    }
}

void bv_copy(BitVector* dst, const BitVector* src) {
    if (!dst || !src) return;
    memcpy(dst->bits, src->bits, sizeof(dst->bits));
}

bool bv_equals(const BitVector* a, const BitVector* b) {
    if (!a || !b) return false;
    return memcmp(a->bits, b->bits, sizeof(a->bits)) == 0;
}

void bv_print(const BitVector* bv, int max_bits, FILE* out) {
    if (!bv || !out) return;
    fprintf(out, "{");
    bool first = true;
    for (int i = 0; i < max_bits; i++) {
        if (bv_test(bv, i)) {
            if (!first) fprintf(out, ", ");
            fprintf(out, "%d", i);
            first = false;
        }
    }
    fprintf(out, "}");
}

void df_analyze(const IRFunction* func, IRBasicBlock blocks[], int num_blocks,
                DataflowAnalysis type, DataflowResult* result) {
    if (!func || !blocks || !result) return;
    memset(result, 0, sizeof(DataflowResult));

    bool is_forward = (type != DF_LIVE_VARIABLES);
    bool use_intersect = (type == DF_AVAILABLE_EXPRS || type == DF_CONSTANT_PROP);

    if (use_intersect) {
        for (int i = 0; i < num_blocks; i++) {
            for (int w = 0; w < BITVECTOR_WORDS; w++) {
                result->IN[i].bits[w] = ~0u;
            }
        }
    }

    bool changed = true;
    int iter_count = 0;
    while (changed && iter_count < 1000) {
        changed = false;
        iter_count++;

        for (int i = 0; i < num_blocks; i++) {
            BitVector meet_val;
            bv_init(&meet_val);

            bool first_pred = true;
            if (is_forward) {
                for (int p = 0; p < blocks[i].num_pred; p++) {
                    int pred = blocks[i].predecessors[p];
                    if (first_pred) {
                        bv_copy(&meet_val, &result->OUT[pred]);
                        first_pred = false;
                    } else if (use_intersect) {
                        bv_intersect(&meet_val, &result->OUT[pred]);
                    } else {
                        bv_union(&meet_val, &result->OUT[pred]);
                    }
                }
                if (i == 0 && first_pred) bv_init(&meet_val);
            } else {
                for (int s = 0; s < blocks[i].num_succ; s++) {
                    int succ = blocks[i].successors[s];
                    if (first_pred) {
                        bv_copy(&meet_val, &result->IN[succ]);
                        first_pred = false;
                    } else {
                        bv_union(&meet_val, &result->IN[succ]);
                    }
                }
                if (!blocks[i].num_succ) bv_init(&meet_val);
            }

            BitVector old_in;
            bv_copy(&old_in, &result->IN[i]);
            bv_copy(&result->IN[i], &meet_val);

            bv_copy(&result->OUT[i], &result->IN[i]);

            if (!bv_equals(&old_in, &result->IN[i])) changed = true;
        }
    }
}

void df_reaching_defs(const IRFunction* func, IRBasicBlock blocks[], int num_blocks,
                      DataflowResult* result) {
    df_analyze(func, blocks, num_blocks, DF_REACHING_DEFS, result);
}

void df_live_variables(const IRFunction* func, IRBasicBlock blocks[], int num_blocks,
                       DataflowResult* result) {
    df_analyze(func, blocks, num_blocks, DF_LIVE_VARIABLES, result);
}

void df_constant_propagation(const IRFunction* func, IRBasicBlock blocks[], int num_blocks,
                             int* value_table) {
    if (!value_table) return;
    for (int i = 0; i < MAX_TEMP_REGS; i++) value_table[i] = INT32_MIN;

    bool changed = true;
    while (changed) {
        changed = false;
        for (int b = 0; b < num_blocks && b < MAX_BLOCKS; b++) {
            for (int i = 0; i < blocks[b].num_inst; i++) {
                const IRInst* inst = &func->instructions[blocks[b].inst_indices[i]];
                int v1 = (inst->src1 >= 0) ? value_table[inst->src1] : INT32_MIN;
                int v2 = (inst->src2 >= 0) ? value_table[inst->src2] : INT32_MIN;

                int result_val = INT32_MIN;
                switch (inst->op) {
                    case IR_MOV:
                        if (v1 != INT32_MIN) result_val = v1;
                        break;
                    case IR_ADD:
                        if (v1 != INT32_MIN && v2 != INT32_MIN) result_val = v1 + v2;
                        break;
                    case IR_SUB:
                        if (v1 != INT32_MIN && v2 != INT32_MIN) result_val = v1 - v2;
                        break;
                    case IR_MUL:
                        if (v1 != INT32_MIN && v2 != INT32_MIN) result_val = v1 * v2;
                        break;
                    case IR_DIV:
                        if (v1 != INT32_MIN && v2 != INT32_MIN && v2 != 0)
                            result_val = v1 / v2;
                        break;
                    default: break;
                }

                if (inst->dest >= 0 && result_val != value_table[inst->dest]) {
                    value_table[inst->dest] = result_val;
                    changed = true;
                }
            }
        }
    }
}

void df_print_result(const DataflowResult* result, int num_blocks, FILE* out) {
    if (!result || !out) return;
    for (int i = 0; i < num_blocks; i++) {
        fprintf(out, "Block %d:\n", i);
        fprintf(out, "  IN:  "); bv_print(&result->IN[i], MAX_TEMP_REGS, out);
        fprintf(out, "\n");
        fprintf(out, "  OUT: "); bv_print(&result->OUT[i], MAX_TEMP_REGS, out);
        fprintf(out, "\n");
    }
}

/*
 * Available Expressions Analysis (Forward, Must).
 *
 * Determines which expressions have been computed and their
 * operands have not been modified since.
 *
 * Lattice: Power set of expressions (bitvector).
 * Meet: Intersection (expression is available only if
 *       available on ALL incoming paths).
 * Direction: Forward.
 *
 * Transfer function:
 *   OUT[B] = GEN[B] U (IN[B] - KILL[B])
 *
 * GEN[B]: Expressions computed in B whose operands are not
 *         killed later in B.
 * KILL[B]: Expressions whose operands are redefined in B.
 *
 * L5 (Algorithm): Available expressions analysis enables
 * Common Subexpression Elimination (CSE) and redundant
 * expression elimination.
 *
 * Reference: Dragon Book §9.2.4, Cooper & Torczon §9.2
 */
void df_available_exprs(const IRFunction* func, IRBasicBlock blocks[],
                         int num_blocks, DataflowResult* result) {
    if (!func || !blocks || !result) return;
    memset(result, 0, sizeof(DataflowResult));

    /*
     * For available expressions (must-forward), initialize
     * OUT[b] = TOP (all 1's) for all blocks except entry,
     * and IN[entry] = BOTTOM (all 0's).
     *
     * We represent expressions as bit i where instruction i
     * defines a new expression.
     */
    for (int b = 0; b < num_blocks; b++) {
        for (int w = 0; w < BITVECTOR_WORDS; w++) {
            result->OUT[b].bits[w] = ~0u; /* TOP = all expressions available */
        }
    }

    /*
     * IN[entry] = BOTTOM = empty set (no expressions available at entry)
     */
    bv_init(&result->IN[0]);

    bool changed = true;
    int iter = 0;
    while (changed && iter < 1000) {
        changed = false;
        iter++;

        for (int b = 0; b < num_blocks; b++) {
            /*
             * IN[B] = ∩ OUT[P] for P in pred(B)
             */
            if (b == 0) {
                bv_init(&result->IN[b]); /* Entry has empty IN */
            } else if (blocks[b].num_pred > 0) {
                bv_copy(&result->IN[b], &result->OUT[blocks[b].predecessors[0]]);
                for (int p = 1; p < blocks[b].num_pred; p++) {
                    bv_intersect(&result->IN[b],
                                 &result->OUT[blocks[b].predecessors[p]]);
                }
            } else {
                bv_init(&result->IN[b]); /* Unreachable */
            }

            /*
             * Compute GEN[B] and KILL[B].
             * GEN: expressions computed in B (instruction op + operands)
             * KILL: expressions whose operands are redefined in B
             */
            BitVector GEN, KILL;
            bv_init(&GEN);
            bv_init(&KILL);

            /*
             * Build expression hash mapping:
             * expression_hash[i] = unique ID for the expression
             * computed by instruction i (based on op, src1, src2).
             */
            for (int i = 0; i < blocks[b].num_inst; i++) {
                int inst_idx = blocks[b].inst_indices[i];
                const IRInst* inst = &func->instructions[inst_idx];

                /*
                 * Only arithmetic and move instructions produce
                 * available expressions.
                 */
                if (inst->op == IR_ADD || inst->op == IR_SUB ||
                    inst->op == IR_MUL || inst->op == IR_DIV ||
                    inst->op == IR_MOV) {
                    if (inst->dest >= 0) {
                        bv_set(&GEN, inst_idx);
                    }
                }

                /*
                 * KILL: any expression whose operands include
                 * inst->dest is killed by this redefinition.
                 */
                if (inst->dest >= 0) {
                    for (int j = 0; j < func->num_inst; j++) {
                        const IRInst* other = &func->instructions[j];
                        if (other->src1 == inst->dest ||
                            other->src2 == inst->dest) {
                            bv_set(&KILL, j);
                        }
                    }
                }
            }

            /*
             * OUT[B] = GEN[B] U (IN[B] - KILL[B])
             */
            BitVector old_out;
            bv_copy(&old_out, &result->OUT[b]);

            bv_copy(&result->OUT[b], &result->IN[b]);
            /* Subtract KILL: clear bits that are killed */
            for (int k = 0; k < BITVECTOR_WORDS; k++) {
                result->OUT[b].bits[k] &= ~KILL.bits[k];
            }
            bv_union(&result->OUT[b], &GEN);

            if (!bv_equals(&old_out, &result->OUT[b])) {
                changed = true;
            }
        }
    }
}

/*
 * Very Busy Expressions Analysis (Backward, Must).
 *
 * An expression is very busy at a point if it will be evaluated
 * along ALL paths from that point before any of its operands
 * are redefined.
 *
 * Direction: Backward
 * Meet: Intersection (must be evaluated on all paths)
 *
 * Transfer function:
 *   IN[B] = USE[B] U (OUT[B] - DEF[B])
 *
 * L5 (Algorithm): Very busy expressions analysis enables
 * code hoisting — moving computations earlier to reduce
 * code duplication.
 *
 * Reference: Dragon Book §9.2.3
 */
void df_very_busy_exprs(const IRFunction* func, IRBasicBlock blocks[],
                         int num_blocks, DataflowResult* result) {
    if (!func || !blocks || !result) return;
    memset(result, 0, sizeof(DataflowResult));

    /*
     * Initialize OUT[exit] = BOTTOM (empty set).
     * OUT[other blocks] = TOP (all expressions very busy).
     */
    for (int b = 0; b < num_blocks; b++) {
        for (int w = 0; w < BITVECTOR_WORDS; w++) {
            result->OUT[b].bits[w] = ~0u;
        }
    }
    if (num_blocks > 0) {
        bv_init(&result->OUT[num_blocks - 1]); /* Exit block */
    }

    bool changed = true;
    int iter = 0;
    while (changed && iter < 1000) {
        changed = false;
        iter++;

        for (int b = 0; b < num_blocks; b++) {
            /*
             * OUT[B] = ∩ IN[S] for S in succ(B)
             */
            if (blocks[b].num_succ > 0) {
                bv_copy(&result->OUT[b],
                        &result->IN[blocks[b].successors[0]]);
                for (int s = 1; s < blocks[b].num_succ; s++) {
                    bv_intersect(&result->OUT[b],
                                 &result->IN[blocks[b].successors[s]]);
                }
            } else {
                bv_init(&result->OUT[b]); /* No successors */
            }

            /*
             * USE[B]: expressions used in B before any definition
             * DEF[B]: operands defined in B before any use
             */
            BitVector USE, DEF;
            bv_init(&USE);
            bv_init(&DEF);

            bool def_seen[MAX_TEMP_REGS];
            memset(def_seen, 0, sizeof(def_seen));

            for (int i = 0; i < blocks[b].num_inst; i++) {
                int inst_idx = blocks[b].inst_indices[i];
                const IRInst* inst = &func->instructions[inst_idx];

                if (inst->op == IR_ADD || inst->op == IR_SUB ||
                    inst->op == IR_MUL || inst->op == IR_DIV) {
                    /*
                     * This expression's operands must not be
                     * redefined before this point for the
                     * expression to be used.
                     */
                    if (!def_seen[inst->src1] && !def_seen[inst->src2]
                        && inst->src1 >= 0 && inst->src2 >= 0) {
                        bv_set(&USE, inst_idx);
                    }
                }

                /*
                 * Track definitions.
                 */
                if (inst->dest >= 0 && inst->dest < MAX_TEMP_REGS) {
                    def_seen[inst->dest] = true;
                    bv_set(&DEF, inst->dest);
                }
            }

            /*
             * IN[B] = USE[B] U (OUT[B] - DEF[B])
             */
            BitVector old_in;
            bv_copy(&old_in, &result->IN[b]);

            bv_copy(&result->IN[b], &result->OUT[b]);
            for (int d = 0; d < BITVECTOR_WORDS; d++) {
                result->IN[b].bits[d] &= ~DEF.bits[d];
            }
            bv_union(&result->IN[b], &USE);

            if (!bv_equals(&old_in, &result->IN[b])) {
                changed = true;
            }
        }
    }
}
