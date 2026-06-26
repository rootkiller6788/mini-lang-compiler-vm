/* ==========================================================================
 * optimizer.c — Bytecode Optimizer: Constant Folding, DCE, Peephole
 *
 * L2: Dataflow analysis (reaching defs, liveness)
 * L4: Rice's Theorem — no perfect optimizer (undecidable semantic properties)
 * L5: Peephole matching (McKeeman 1965), constant folding, DCE
 * L6: Hot-loop optimization via fixed-point iteration
 *
 * Refs: McKeeman CACM 1965, Kildall POPL 1973, Muchnick 1997
 * ========================================================================== */

#include "optimizer.h"
#include <string.h>
#include <stdio.h>

void optimizer_init(Optimizer* opt) {
    memset(opt, 0, sizeof(Optimizer));
    optimizer_config_default(&opt->config);
}

void optimizer_config_default(OptimizationConfig* cfg) {
    cfg->fold_constants      = true;
    cfg->eliminate_dead_code = true;
    cfg->peephole_enabled    = true;
    cfg->copy_propagation    = false;
    cfg->max_passes          = 10;
}

/* =========================================================================
 * L5: Constant Folding — O(n) forward pass
 *
 * Identifies compile-time-evaluable subexpressions and replaces them
 * with their computed values.
 *
 * Theorem: Constant subexpressions are loop-invariant (Muchnick S12.1).
 * This enables hoisting and further simplification.
 * ========================================================================= */

int32_t opt_constant_folding(ByteCode* bc) {
    int32_t changes = 0;
    int32_t out[BC_MAX_INSTRUCTIONS];
    int32_t out_count = 0;
    Constant new_pool[BC_MAX_CONSTANTS];
    int32_t new_pool_count = bc->const_count;
    memcpy(new_pool, bc->const_pool, bc->const_count * sizeof(Constant));

    int32_t i = 0;
    while (i < bc->num_inst) {
        int32_t instr = bc->instructions[i];
        OpCode op = (OpCode)(instr & 0xFF);
        int32_t arg = (instr >> 8) & 0xFFFFFF;

        if (op == OP_PUSH && i + 2 < bc->num_inst) {
            int32_t i1 = bc->instructions[i + 1];
            int32_t i2 = bc->instructions[i + 2];
            OpCode op1 = (OpCode)(i1 & 0xFF);
            OpCode op2 = (OpCode)(i2 & 0xFF);

            /* Pattern: PUSH a; PUSH b; BINOP → PUSH (a binop b) */
            if (op1 == OP_PUSH && (op2 == OP_ADD || op2 == OP_SUB ||
                op2 == OP_MUL || op2 == OP_DIV)) {
                Constant c1 = bc->const_pool[arg];
                Constant c2 = bc->const_pool[(i1 >> 8) & 0xFFFFFF];
                if (c1.type == CONST_INT && c2.type == CONST_INT) {
                    int64_t r = 0; bool ok = true;
                    switch (op2) {
                    case OP_ADD: r = c1.data.int_val + c2.data.int_val; break;
                    case OP_SUB: r = c1.data.int_val - c2.data.int_val; break;
                    case OP_MUL: r = c1.data.int_val * c2.data.int_val; break;
                    case OP_DIV:
                        if (c2.data.int_val == 0) ok = false;
                        else r = c1.data.int_val / c2.data.int_val; break;
                    default: ok = false; break;
                    }
                    if (ok) {
                        Constant fc = { CONST_INT, { .int_val = r } };
                        int32_t idx = -1;
                        for (int32_t j = 0; j < new_pool_count; j++)
                            if (new_pool[j].type == CONST_INT &&
                                new_pool[j].data.int_val == r) { idx = j; break; }
                        if (idx < 0 && new_pool_count < BC_MAX_CONSTANTS) {
                            idx = new_pool_count;
                            new_pool[new_pool_count++] = fc;
                        }
                        if (idx >= 0) {
                            out[out_count++] = (idx << 8) | OP_PUSH;
                            i += 3; changes++; continue;
                        }
                    }
                }
            }
            /* PUSH x; NEG → PUSH (-x) */
            if (op1 == OP_NEG && bc->const_pool[arg].type == CONST_INT) {
                Constant fc = { CONST_INT,
                    { .int_val = -bc->const_pool[arg].data.int_val } };
                int32_t idx = -1;
                for (int32_t j = 0; j < new_pool_count; j++)
                    if (new_pool[j].type == CONST_INT &&
                        new_pool[j].data.int_val == fc.data.int_val)
                    { idx = j; break; }
                if (idx < 0 && new_pool_count < BC_MAX_CONSTANTS) {
                    idx = new_pool_count;
                    new_pool[new_pool_count++] = fc;
                }
                if (idx >= 0) {
                    out[out_count++] = (idx << 8) | OP_PUSH;
                    i += 2; changes++; continue;
                }
            }
            /* PUSH x; NOT → PUSH (!x) */
            if (op1 == OP_NOT && bc->const_pool[arg].type == CONST_INT) {
                int64_t v = bc->const_pool[arg].data.int_val ? 0 : 1;
                Constant fc = { CONST_INT, { .int_val = v } };
                int32_t idx = -1;
                for (int32_t j = 0; j < new_pool_count; j++)
                    if (new_pool[j].type == CONST_INT &&
                        new_pool[j].data.int_val == v) { idx = j; break; }
                if (idx < 0 && new_pool_count < BC_MAX_CONSTANTS) {
                    idx = new_pool_count;
                    new_pool[new_pool_count++] = fc;
                }
                if (idx >= 0) {
                    out[out_count++] = (idx << 8) | OP_PUSH;
                    i += 2; changes++; continue;
                }
            }
        }
        if (out_count < BC_MAX_INSTRUCTIONS)
            out[out_count++] = bc->instructions[i];
        i++;
    }

    bc->num_inst = out_count;
    bc->const_count = new_pool_count;
    memcpy(bc->instructions, out, out_count * sizeof(int32_t));
    memcpy(bc->const_pool, new_pool, new_pool_count * sizeof(Constant));
    return changes;
}

/* =========================================================================
 * L5: Dead Code Elimination - Backward Liveness Dataflow (Kildall 1973)
 * ========================================================================= */

int32_t opt_dead_code_elimination(ByteCode* bc) {
    if (bc->num_inst == 0) return 0;

    bool live[BC_MAX_INSTRUCTIONS];
    memset(live, 0, sizeof(live));

    /* Mark side-effecting instructions as inherently live */
    for (int32_t i = 0; i < bc->num_inst; i++) {
        OpCode op = (OpCode)(bc->instructions[i] & 0xFF);
        if (op == OP_HALT || op == OP_PRINT || op == OP_STORE ||
            op == OP_JMP || op == OP_JMP_IF_FALSE || op == OP_CALL ||
            op == OP_RET || op == OP_POP)
            live[i] = true;
    }

    /* Mark jump targets as live */
    for (int32_t i = 0; i < bc->num_inst; i++) {
        OpCode op = (OpCode)(bc->instructions[i] & 0xFF);
        int32_t arg = (bc->instructions[i] >> 8) & 0xFFFFFF;
        if ((op == OP_JMP || op == OP_JMP_IF_FALSE) &&
            arg >= 0 && arg < bc->num_inst)
            live[arg] = true;
    }

    /* Fixed-point backward liveness propagation */
    bool changed = true;
    int32_t max_iter = bc->num_inst;
    while (changed && max_iter-- > 0) {
        changed = false;
        for (int32_t i = 0; i < bc->num_inst; i++) {
            if (!live[i]) continue;
            OpCode op = (OpCode)(bc->instructions[i] & 0xFF);
            if (op == OP_ADD || op == OP_SUB || op == OP_MUL || op == OP_DIV ||
                op == OP_AND || op == OP_OR) {
                int found = 0;
                for (int32_t j = i - 1; j >= 0 && found < 2; j--) {
                    OpCode po = (OpCode)(bc->instructions[j] & 0xFF);
                    if (po == OP_PUSH || po == OP_LOAD || po == OP_ADD ||
                        po == OP_SUB || po == OP_MUL || po == OP_DIV ||
                        po == OP_NEG || po == OP_NOT) {
                        if (!live[j]) { live[j] = true; changed = true; }
                        found++;
                    }
                }
            }
            if (op == OP_NEG || op == OP_NOT) {
                for (int32_t j = i - 1; j >= 0; j--) {
                    OpCode po = (OpCode)(bc->instructions[j] & 0xFF);
                    if (po == OP_PUSH || po == OP_LOAD || po == OP_ADD ||
                        po == OP_SUB || po == OP_MUL || po == OP_DIV ||
                        po == OP_NEG || po == OP_NOT) {
                        if (!live[j]) { live[j] = true; changed = true; }
                        break;
                    }
                }
            }
        }
    }

    /* Compact */
    int32_t out[BC_MAX_INSTRUCTIONS];
    int32_t out_count = 0, removed = 0;
    for (int32_t i = 0; i < bc->num_inst; i++) {
        if (live[i]) out[out_count++] = bc->instructions[i];
        else removed++;
    }
    memcpy(bc->instructions, out, out_count * sizeof(int32_t));
    bc->num_inst = out_count;
    return removed;
}

/* =========================================================================
 * L5: Peephole Optimization - Sliding Window (McKeeman 1965)
 *
 * Patterns: identity elimination (a+0, a*1), double negation,
 * dead push-pop, redundant load-after-store.
 * ========================================================================= */

int32_t opt_peephole(ByteCode* bc) {
    int32_t changes = 0;

    for (int32_t pass = 0; pass < 3; pass++) {
        int32_t out[BC_MAX_INSTRUCTIONS];
        int32_t out_count = 0, i = 0;
        bool pass_changed = false;

        while (i < bc->num_inst) {
            if (i + 1 >= bc->num_inst) {
                if (out_count < BC_MAX_INSTRUCTIONS)
                    out[out_count++] = bc->instructions[i];
                i++; continue;
            }
            OpCode op0 = (OpCode)(bc->instructions[i] & 0xFF);
            OpCode op1 = (OpCode)(bc->instructions[i + 1] & 0xFF);
            int32_t arg0 = (bc->instructions[i] >> 8) & 0xFFFFFF;
            int32_t arg1 = (bc->instructions[i + 1] >> 8) & 0xFFFFFF;

            /* Identity patterns */
            bool skip = false;
            if (op0 == OP_PUSH && (op1 == OP_ADD || op1 == OP_SUB) &&
                arg0 < bc->const_count &&
                bc->const_pool[arg0].type == CONST_INT &&
                bc->const_pool[arg0].data.int_val == 0)
                skip = true;
            if (op0 == OP_PUSH && (op1 == OP_MUL || op1 == OP_DIV) &&
                arg0 < bc->const_count &&
                bc->const_pool[arg0].type == CONST_INT &&
                bc->const_pool[arg0].data.int_val == 1)
                skip = true;
            if ((op0 == OP_NEG && op1 == OP_NEG) ||
                (op0 == OP_NOT && op1 == OP_NOT))
                skip = true;
            if (op0 == OP_PUSH && op1 == OP_POP)
                skip = true;

            if (skip) {
                i += 2; changes++; pass_changed = true; continue;
            }

            /* No-op jump: JMP next -> skip */
            if (op0 == OP_JMP && arg0 == i + 1) {
                i += 1; changes++; pass_changed = true; continue;
            }

            /* Redundant LOAD after STORE */
            if (i + 2 < bc->num_inst && op1 == OP_STORE &&
                (OpCode)(bc->instructions[i + 2] & 0xFF) == OP_LOAD &&
                ((bc->instructions[i + 2] >> 8) & 0xFFFFFF) == arg1) {
                if (out_count < BC_MAX_INSTRUCTIONS) {
                    out[out_count++] = bc->instructions[i];
                    out[out_count++] = bc->instructions[i + 1];
                }
                i += 3; changes++; pass_changed = true; continue;
            }

            if (out_count < BC_MAX_INSTRUCTIONS)
                out[out_count++] = bc->instructions[i];
            i++;
        }
        bc->num_inst = out_count;
        memcpy(bc->instructions, out, out_count * sizeof(int32_t));
        if (!pass_changed) break;
    }
    return changes;
}

/* =========================================================================
 * L3: Fixed-Point Optimization Pipeline
 * ========================================================================= */

int32_t optimizer_run(Optimizer* opt, ByteCode* bc) {
    opt->total_changes = 0;
    opt->total_passes_run = 0;
    for (int32_t iter = 0; iter < opt->config.max_passes; iter++) {
        int32_t ch = 0;
        if (opt->config.fold_constants) {
            int32_t c = opt_constant_folding(bc);
            opt->stats[OPT_CONSTANT_FOLDING].changes_made += c; ch += c;
        }
        if (opt->config.peephole_enabled) {
            int32_t c = opt_peephole(bc);
            opt->stats[OPT_PEEPHOLE].changes_made += c; ch += c;
        }
        if (opt->config.eliminate_dead_code) {
            int32_t c = opt_dead_code_elimination(bc);
            opt->stats[OPT_DEAD_CODE_ELIM].changes_made += c;
            opt->stats[OPT_DEAD_CODE_ELIM].instructions_removed += c; ch += c;
        }
        opt->total_changes += ch;
        if (ch == 0) break;
        opt->total_passes_run++;
    }
    return opt->total_changes;
}

void optimizer_print_stats(const Optimizer* opt) {
    printf("=== Optimization Stats ===\n");
    printf("Passes: %d, Changes: %d\n",
           opt->total_passes_run, opt->total_changes);
    printf("  Fold: %d  Peephole: %d  DCE: %d\n",
           opt->stats[OPT_CONSTANT_FOLDING].changes_made,
           opt->stats[OPT_PEEPHOLE].changes_made,
           opt->stats[OPT_DEAD_CODE_ELIM].instructions_removed);
}
