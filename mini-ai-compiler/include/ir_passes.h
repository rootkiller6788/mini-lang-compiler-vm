#ifndef IR_PASSES_H
#define IR_PASSES_H

#include <stdbool.h>
#include <stddef.h>
#include "graph_ir.h"

/*
 * IR Optimization Passes — Module 14.6
 *
 * Canonical compiler optimization passes for computation graphs.
 * References:
 *   - Cooper & Torczon, "Engineering a Compiler" (CSE/DCE/LVN algorithms)
 *   - Aho, Lam, Sethi, Ullman, "Compilers: Principles, Techniques, and Tools"
 *     (Dragon Book, Ch 8-10: Code Optimization)
 *   - Lattner & Adve, "LLVM: A Compilation Framework" (Pass infrastructure)
 *
 * L2: Core Concepts — scalar/loop optimization categories
 * L4: Standards/Theorems — algebraic identities, lattice theory for constant prop
 * L5: Algorithms — hash-based CSE, worklist DCE, GVN
 */

/* ---- Constants ---- */
#define PASS_MAX_OPS        128
#define PASS_HASH_TABLE_SIZE 64
#define PASS_MAX_WORKLIST   256

/* ---- L1: Definitions — Optimization Statistics ---- */
typedef struct {
    int ops_removed;
    int ops_added;
    int ops_modified;
    int constants_folded;
    int subexpressions_eliminated;
    int dead_ops_eliminated;
    int algebraic_simplifications;
    double time_ms;
} PassStatistics;

/* ---- L1: Definitions — Constant Value ---- */
typedef enum {
    Const_INT,
    Const_FLOAT,
    Const_BOOL,
    Const_NONE
} ConstKind;

typedef struct {
    ConstKind kind;
    union {
        long long ival;
        double   fval;
        bool     bval;
    } data;
} ConstantValue;

/* ---- L2/L4: Algebraic Identity Rules ---- */
typedef struct {
    GraphOpType op;
    ConstKind operand_kind;
    long long identity_val;    /* x+0=x, x*1=x, etc. */
    long long annihilator_val; /* x*0=0 */
    bool has_identity;
    bool has_annihilator;
    bool is_commutative;
    bool is_associative;
} AlgebraicRule;

/* ---- Pass Context ---- */
typedef struct {
    ComputeGraph *graph;
    PassStatistics stats;
    bool changed;
} PassContext;

/* ---- L1: API Declarations ---- */

/* Initialize pass infrastructure */
PassContext pass_context_create(ComputeGraph *g);
void pass_reset_stats(PassContext *ctx);

/* L5: Common Subexpression Elimination (CSE)
 * Hash-based approach: maps operation signatures → node IDs.
 * If two nodes compute the same operation on the same inputs,
 * replace all uses of the second with the first. */
int pass_run_cse(PassContext *ctx);

/* L5: Dead Code Elimination (DCE)
 * Mark-and-sweep: mark all nodes reachable from outputs,
 * then remove unmarked nodes. */
int pass_run_dce(PassContext *ctx);

/* L5: Constant Folding
 * Evaluate operations on constant inputs at compile time.
 * e.g., 2+3 → 5, reshape([3,4], [2,2]) → compute output shape */
int pass_run_constant_folding(PassContext *ctx);

/* L4: Algebraic Simplification
 * Apply mathematical identities:
 *   x+0 → x, x*1 → x, x*0 → 0
 *   x-x → 0, x/x → 1 (when x≠0)
 *   -(x-y) → y-x
 *   double negation → identity
 */
int pass_run_algebraic_simplify(PassContext *ctx);

/* L5: Full optimization pipeline — run all passes to fixed point */
int pass_run_pipeline(PassContext *ctx);

/* L5: Compute hash signature for an operation */
unsigned long pass_op_hash(ComputeGraph *g, int node_idx);
bool pass_op_equivalent(ComputeGraph *g, int a_idx, int b_idx);

/* L2/L4: Query algebraic properties */
const AlgebraicRule *pass_get_rule(GraphOpType op);
bool pass_is_identity_op(GraphOpType op, long long operand_val);
bool pass_is_annihilator_op(GraphOpType op, long long operand_val);

/* L7: Application — print optimization report */
void pass_print_stats(PassContext *ctx);
void pass_print_algebraic_rules(void);

/* L8: Advanced — Loop-Invariant Code Motion (LICM) for loop nests */
int pass_run_licm(PassContext *ctx);

/* L8: Advanced — Strength Reduction
 * Replace expensive ops with cheaper equivalents:
 *   x*2 → x+x, x*2^k → x<<k, x/2^k → x>>k */
int pass_run_strength_reduction(PassContext *ctx);

#endif /* IR_PASSES_H */
