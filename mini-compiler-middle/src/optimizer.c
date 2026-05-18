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

    for (int i = 0; i < func->num_inst - 1; i++) {
        IRInst* inst = &func->instructions[i];
        if (inst->op == IR_BR && func->instructions[i + 1].label[0] != '\0') {
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
        case OPT_LOOP_INVARIANT: /* simplified stub */
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
