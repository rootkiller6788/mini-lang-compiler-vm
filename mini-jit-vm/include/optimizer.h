#ifndef OPTIMIZER_H
#define OPTIMIZER_H
/* ==========================================================================
 * optimizer.h — Bytecode Peephole Optimizer & Dataflow Analysis
 *
 * L1: OptimizationPass, PeepholeRule, DataFlowState structs
 * L2: Constant folding, copy propagation, dead code elimination
 * L3: Multi-pass optimization pipeline
 * L4: Rice's Theorem — limits of static analysis (non-trivial semantic
 *     properties of programs are undecidable in general)
 * L5: Peephole pattern matching, reaching definitions dataflow
 * L6: Optimizer for hot loops (classic compiler problem)
 *
 * Refs: Muchnick (1997) "Advanced Compiler Design"; Aho et al. Ch 8-10
 * ========================================================================== */

#include <stdbool.h>
#include <stdint.h>
#include "bytecode.h"

/* L1: Optimization pass descriptor */
typedef enum {
    OPT_CONSTANT_FOLDING = 0,
    OPT_DEAD_CODE_ELIM,
    OPT_PEEPHOLE,
    OPT_COPY_PROPAGATION,
    OPT_STRENGTH_REDUCTION,
    OPT_PASS_COUNT
} OptimizationPassType;

typedef struct {
    OptimizationPassType type;
    int32_t              changes_made;
    int32_t              instructions_removed;
    bool                 enabled;
} OptimizationStats;

typedef struct {
    bool        fold_constants;
    bool        eliminate_dead_code;
    bool        peephole_enabled;
    bool        copy_propagation;
    int32_t     max_passes;        /* convergence limit */
} OptimizationConfig;

/* L3: Optimization pipeline context */
typedef struct {
    OptimizationConfig config;
    OptimizationStats  stats[OPT_PASS_COUNT];
    int32_t            total_changes;
    int32_t            total_passes_run;
} Optimizer;

/* --- API --- */
void     optimizer_init(Optimizer* opt);
void     optimizer_config_default(OptimizationConfig* cfg);

/** L5: Constant folding — evaluate constant sub-expressions at compile time.
 *  Pattern: PUSH a; PUSH b; ADD/SUB/MUL/DIV → PUSH (a op b)
 *  Theorem: constant expressions are trivially loop-invariant (Muchnick §12)
 *  Complexity: O(n) single pass over bytecode */
int32_t  opt_constant_folding(ByteCode* bc);

/** L5: Dead code elimination — remove instructions whose results are never
 *  used (e.g., PUSH followed by POP without intervening reads).
 *  Uses liveness analysis (backward dataflow).
 *  Complexity: O(n) with bounded stack-depth analysis */
int32_t  opt_dead_code_elimination(ByteCode* bc);

/** L5: Peephole optimization — sliding window pattern replacement.
 *  Patterns:
 *    PUSH 0; ADD → nop (identity)
 *    PUSH 1; MUL → nop
 *    PUSH 0; SUB → nop
 *    NEG; NEG → nop
 *    NOT; NOT → nop
 *  Complexity: O(n·w) where w = window_size (typically 2-3) */
int32_t  opt_peephole(ByteCode* bc);

/** L3: Full optimization pipeline — runs passes to convergence or max_passes.
 *  Returns total number of changes across all passes. */
int32_t  optimizer_run(Optimizer* opt, ByteCode* bc);

/** Print optimization statistics */
void     optimizer_print_stats(const Optimizer* opt);

#endif /* OPTIMIZER_H */
