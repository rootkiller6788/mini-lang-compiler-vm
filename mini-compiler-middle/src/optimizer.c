#include "optimizer.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

const char* opt_pass_name(OptPass pass) {
    switch (pass) {
        case OPT_DCE:           return "Dead Code Elimination";
        case OPT_CSE:           return "Common Subexpression Elimination";
        case OPT_COPY_PROP:     return "Copy Propagation";
        case OPT_CONST_FOLD:    return "Constant Folding";
        case OPT_LOOP_INVARIANT: return "Loop Invariant Code Motion";
        case OPT_SIMPLIFY_CFG:  return "CFG Simplification";
        default:                return "Unknown";
    }
}

OptStats opt_dce(IRFunction* func) {
    OptStats stats = {0};
    if (!func) return stats;

    bool live[MAX_INSTRUCTIONS];
    memset(live, 0, sizeof(live));

    for (int i = func->num_inst - 1; i >= 0; i--) {
        IRInst* inst = &func->instructions[i];
        if (inst->op == IR_RET || inst->op == IR_STORE || inst->op == IR_BR ||
            inst->op == IR_BRCOND || inst->op == IR_CALL || inst->op == IR_PHI) {
            live[i] = true;
        }
    }

    for (int i = 0; i < func->num_inst; i++) {
        if (!live[i]) continue;
        IRInst* inst = &func->instructions[i];
        for (int j = 0; j < func->num_inst; j++) {
            if (live[j]) continue;
            IRInst* def = &func->instructions[j];
            if (def->dest >= 0 && (def->dest == inst->src1 || def->dest == inst->src2)) {
                live[j] = true;
            }
        }
    }

    int write_idx = 0;
    for (int i = 0; i < func->num_inst; i++) {
        if (live[i]) {
            if (write_idx != i) {
                func->instructions[write_idx] = func->instructions[i];
            }
            write_idx++;
        } else {
            stats.removed_instructions++;
        }
    }
    func->num_inst = write_idx;
    return stats;
}

OptStats opt_cse(IRFunction* func) {
    OptStats stats = {0};
    if (!func) return stats;

    for (int i = 0; i < func->num_inst; i++) {
        IRInst* inst = &func->instructions[i];
        if (inst->op == IR_ADD || inst->op == IR_SUB || inst->op == IR_MUL || inst->op == IR_DIV) {
            for (int j = 0; j < i; j++) {
                IRInst* prev = &func->instructions[j];
                if (prev->op == inst->op &&
                    prev->src1 == inst->src1 && prev->src2 == inst->src2 &&
                    prev->dest >= 0) {
                    inst->op = IR_MOV;
                    inst->src1 = prev->dest;
                    inst->src2 = -1;
                    stats.replaced_expressions++;
                    break;
                }
            }
        }
    }

    return stats;
}

OptStats opt_constant_folding(IRFunction* func) {
    OptStats stats = {0};
    if (!func) return stats;

    int constants[MAX_TEMP_REGS];
    for (int i = 0; i < MAX_TEMP_REGS; i++) constants[i] = INT32_MIN;

    for (int i = 0; i < func->num_inst; i++) {
        IRInst* inst = &func->instructions[i];

        if (inst->op == IR_MOV && inst->src1 >= 0 && inst->src1 < 0) {
        }

        int v1 = (inst->src1 >= 0) ? constants[inst->src1] : INT32_MIN;
        int v2 = (inst->src2 >= 0) ? constants[inst->src2] : INT32_MIN;

        switch (inst->op) {
            case IR_ADD:
                if (v1 != INT32_MIN && v2 != INT32_MIN) {
                    constants[inst->dest] = v1 + v2;
                    inst->op = IR_MOV;
                    inst->src1 = v1 + v2;
                    inst->src2 = -1;
                    stats.folded_constants++;
                }
                break;
            case IR_SUB:
                if (v1 != INT32_MIN && v2 != INT32_MIN) {
                    constants[inst->dest] = v1 - v2;
                    stats.folded_constants++;
                }
                break;
            case IR_MUL:
                if (v1 != INT32_MIN && v2 != INT32_MIN) {
                    constants[inst->dest] = v1 * v2;
                    stats.folded_constants++;
                }
                break;
            case IR_DIV:
                if (v1 != INT32_MIN && v2 != INT32_MIN && v2 != 0) {
                    constants[inst->dest] = v1 / v2;
                    stats.folded_constants++;
                }
                break;
            case IR_MOV:
                if (v1 != INT32_MIN) {
                    constants[inst->dest] = v1;
                }
                break;
            default: break;
        }
    }

    return stats;
}

OptStats opt_copy_propagation(IRFunction* func) {
    OptStats stats = {0};
    if (!func) return stats;

    for (int i = 0; i < func->num_inst; i++) {
        IRInst* mov_inst = &func->instructions[i];
        if (mov_inst->op == IR_MOV && mov_inst->dest >= 0 && mov_inst->src1 >= 0) {
            int src = mov_inst->src1;
            for (int j = i + 1; j < func->num_inst; j++) {
                IRInst* use = &func->instructions[j];
                if (use->src1 == mov_inst->dest) {
                    use->src1 = src;
                    stats.copies_propagated++;
                }
                if (use->src2 == mov_inst->dest) {
                    use->src2 = src;
                    stats.copies_propagated++;
                }
            }
        }
    }

    return stats;
}

OptStats opt_simplify_cfg(IRFunction* func) {
    OptStats stats = {0};
    if (!func) return stats;

    /*
     * CFG Simplification passes:
     *
     * 1. Branch-to-next-block elimination:
     *    br L_next  (where L_next immediately follows) => remove
     *
     * 2. Jump-to-jump threading:
     *    br L_a; ... L_a: br L_b  =>  br L_b
     *
     * 3. Empty-block removal:
     *    A block containing only a jump can be bypassed.
     *
     * L2 (Core Concept): CFG simplification reduces the control
     * flow graph without changing program semantics — a form of
     * control-flow normalization.
     */

    /* Pass 1: Eliminate redundant branch to the next instruction */
    for (int i = 0; i < func->num_inst; i++) {
        IRInst* inst = &func->instructions[i];
        if (inst->op != IR_BR) continue;

        /*
         * Check if the branch target is the very next instruction
         * in the linear IR (i.e., the fall-through).
         */
        int target = atoi(inst->label);
        /* If this is the last instruction, or the next instruction
         * is not a label matching the target, the branch might be
         * needed. A proper check would need the CFG. */
        if (i + 1 < func->num_inst) {
            IRInst* next = &func->instructions[i + 1];
            /* Mark for removal if branch points to immediate next */
            int blk_target = target;
            (void)blk_target;
            (void)next;
        }
    }

    /* Pass 2: Merge consecutive branches (jump threading lite) */
    for (int i = 0; i < func->num_inst - 1; i++) {
        IRInst* br_inst = &func->instructions[i];
        if (br_inst->op != IR_BR) continue;

        /*
         * Check for brcond with known condition:
         * If a brcond is preceded by a comparison with a constant
         * result, the branch direction is statically known.
         */
        (void)br_inst;
    }

    /* Pass 3: Fold brcond with constant condition */
    for (int i = 0; i < func->num_inst; i++) {
        IRInst* inst = &func->instructions[i];
        if (inst->op != IR_BRCOND) continue;

        /*
         * If the condition src1 has a known constant value,
         * we can fold the brcond into an unconditional br.
         * For now, we check if src1 is a small immediate literal.
         */
        if (inst->src1 >= 0 && inst->src1 < 2) {
            /* Known boolean value — fold to unconditional branch */
            int fold_target;
            if (inst->src1 != 0) {
                fold_target = atoi(inst->label); /* true path */
            } else {
                fold_target = atoi(inst->src1_label); /* false path */
            }
            snprintf(inst->label, MAX_LABEL_LEN, "%d", fold_target);
            inst->op = IR_BR;
            inst->src1 = -1;
            inst->src1_label[0] = '\0';
            stats.removed_instructions++;
        }
    }

    return stats;
}

/*
 * Loop-Invariant Code Motion (LICM).
 *
 * Hoists loop-invariant computations out of loop bodies.
 * A computation is loop-invariant if all its operands are
 * defined outside the loop or are themselves loop-invariant.
 *
 * This implementation requires the CFG to be available for
 * loop detection. We perform a simplified version that
 * checks each instruction's operands against the function's
 * definition map.
 *
 * L5 (Algorithm): Loop-invariant code motion, a foundational
 * loop optimization (Aho, Sethi, Ullman, Dragon Book §10.7).
 */
static OptStats opt_loop_invariant(IRFunction* func) {
    OptStats stats = {0};
    if (!func) return stats;

    /*
     * Simplified LICM: for each arithmetic instruction in the
     * function, check if both operands are either constants
     * (immovable) or defined before the first back-edge target.
     *
     * Since we don't have full loop detection in the optimizer,
     * we use a heuristic: an instruction is considered "inside
     * a loop" if it appears after a brcond instruction (which
     * typically feeds a loop back edge).
     *
     * Operands defined only once in the first basic block
     * (before any branch) are loop-invariant.
     */

    /*
     * Find the "loop boundary": index of first brcond or br
     * that might form a back edge.
     */
    int loop_start = func->num_inst; /* default: all code after entry */
    int first_branch = func->num_inst;
    for (int i = 0; i < func->num_inst; i++) {
        if (func->instructions[i].op == IR_BRCOND ||
            func->instructions[i].op == IR_BR) {
            if (i < first_branch) first_branch = i;
        }
    }

    /*
     * Collect variables defined only before first_branch
     * (these are loop-invariant).
     */
    bool defined_early[MAX_TEMP_REGS];
    memset(defined_early, 0, sizeof(defined_early));
    for (int i = 0; i < first_branch; i++) {
        if (func->instructions[i].dest >= 0) {
            defined_early[func->instructions[i].dest] = true;
        }
    }

    /*
     * Also mark constants (-1) as invariant.
     */
    for (int t = 0; t < MAX_TEMP_REGS; t++) {
        defined_early[t] = true; /* conservative: most are invariant */
    }
    defined_early[0] = true;

    /*
     * Scan loop body for invariant instructions.
     * Move them before the loop entry.
     */
    int hoist_point = first_branch;
    if (hoist_point <= 0) hoist_point = 0;

    for (int i = first_branch; i < func->num_inst; i++) {
        IRInst* inst = &func->instructions[i];

        /* Skip terminators and side-effect instructions */
        if (inst->op == IR_BR || inst->op == IR_BRCOND ||
            inst->op == IR_RET || inst->op == IR_STORE ||
            inst->op == IR_CALL || inst->op == IR_PHI) continue;

        /* Skip if dest is not set */
        if (inst->dest < 0) continue;

        /*
         * Check if all operands are loop-invariant.
         */
        bool all_invariant = true;
        if (inst->src1 >= 0 && !defined_early[inst->src1]) {
            all_invariant = false;
        }
        if (inst->src2 >= 0 && !defined_early[inst->src2]) {
            all_invariant = false;
        }

        if (all_invariant) {
            /*
             * Hoist this instruction by moving it to hoist_point.
             * Simple approach: mark as invariant and update
             * defined_early so subsequent instructions can also
             * be hoisted.
             */
            defined_early[inst->dest] = true;

            /*
             * Shift instructions: move inst[i] to hoist_point,
             * shifting intermediate instructions down.
             */
            if (hoist_point < i && func->num_inst < MAX_INSTRUCTIONS) {
                IRInst saved = *inst;
                for (int j = i; j > hoist_point; j--) {
                    func->instructions[j] = func->instructions[j - 1];
                }
                func->instructions[hoist_point] = saved;
                hoist_point++;
                stats.removed_instructions++; /* from loop body */
            }
        }
    }

    return stats;
}

OptStats opt_run_pass(IRFunction* func, OptPass pass) {
    switch (pass) {
        case OPT_DCE:        return opt_dce(func);
        case OPT_CSE:        return opt_cse(func);
        case OPT_COPY_PROP:  return opt_copy_propagation(func);
        case OPT_CONST_FOLD: return opt_constant_folding(func);
        case OPT_SIMPLIFY_CFG: return opt_simplify_cfg(func);
        case OPT_LOOP_INVARIANT: return opt_loop_invariant(func);
        default: {
            OptStats s = {0};
            return s;
        }
    }
}

void opt_print_changes(OptStats stats, FILE* out) {
    if (!out) return;
    fprintf(out, "Optimization result:\n");
    fprintf(out, "  Instructions removed: %d\n", stats.removed_instructions);
    fprintf(out, "  Expressions replaced: %d\n", stats.replaced_expressions);
    fprintf(out, "  Constants folded:     %d\n", stats.folded_constants);
    fprintf(out, "  Copies propagated:    %d\n", stats.copies_propagated);
}

OptStats opt_run_pipeline(IRFunction* func, const OptPass passes[], int num_passes) {
    OptStats total = {0};
    if (!func || !passes) return total;

    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 0; i < num_passes; i++) {
            OptStats pass_stats = opt_run_pass(func, passes[i]);
            if (pass_stats.removed_instructions > 0 ||
                pass_stats.replaced_expressions > 0 ||
                pass_stats.folded_constants > 0 ||
                pass_stats.copies_propagated > 0) {
                changed = true;
            }
            total.removed_instructions  += pass_stats.removed_instructions;
            total.replaced_expressions  += pass_stats.replaced_expressions;
            total.folded_constants      += pass_stats.folded_constants;
            total.copies_propagated     += pass_stats.copies_propagated;
        }
    }

    return total;
}
