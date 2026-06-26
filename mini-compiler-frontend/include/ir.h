#ifndef IR_H
#define IR_H

#include <stdbool.h>
#include <stddef.h>
#include "ast.h"

/*
 * Intermediate Representation (IR) — Three-Address Code
 *
 * L4: Static Single Assignment (SSA) form — each variable assigned exactly once,
 *     enabling sparse dataflow analysis (Cytron et al., 1991).
 * L5: Three-Address Code — at most one operator per instruction, making
 *     optimization and code generation tractable.
 * L6: Compiler Pipeline — AST → IR → Optimization → Code Generation.
 * L8: Advanced dataflow — liveness analysis, reaching definitions,
 *     dominator tree construction for SSA placement.
 *
 * Reference:
 *   - Aho, Lam, Sethi, Ullman "Compilers: Principles, Techniques, and Tools" Ch.6,8,9
 *   - Cytron et al. "Efficiently Computing Static Single Assignment Form" (1991)
 */

/* ─── L1: Core Definitions ──────────────────────────────────────────── */

/* Three-address code opcodes */
typedef enum {
    IR_LABEL,         /* label:                                     */
    IR_BINARY,        /* dst = src1 op src2     (arithmetic/logic)  */
    IR_UNARY,         /* dst = op src                               */
    IR_COPY,          /* dst = src                                  */
    IR_LOAD_IMM,      /* dst = imm                                  */
    IR_BR,            /* goto label                                 */
    IR_BR_COND,       /* if cond goto label                         */
    IR_BR_NOT_COND,   /* if !cond goto label                        */
    IR_CALL,          /* dst = call func(args...)                   */
    IR_RET,           /* return val                                 */
    IR_PARAM,         /* param val  (pass argument)                 */
    IR_ALLOCA,        /* dst = alloca size                          */
    IR_STORE,         /* *addr = val                                */
    IR_LOAD,          /* dst = *addr                                */
    IR_PHI,           /* dst = phi(src1:block1, src2:block2, ...)   */
    IR_NOP,           /* no operation                               */
    IR_COUNT
} IROpcode;

/* Maximum operands for PHI nodes */
#define IR_PHI_MAX_SOURCES 8

/* A single three-address code instruction.
 * For IR_BINARY, the operator character is stored in op field.
 * For IR_PHI, source operands are stored as space-separated names in src1,
 * and source block labels in src2 (parallel arrays encoded as strings). */
typedef struct IRInstr {
    IROpcode opcode;
    int id;                       /* unique sequential identifier */
    char dest[64];                /* destination operand */
    char src1[64];                /* source operand 1 (or operator char) */
    char src2[64];                /* source operand 2 */
    int imm;                      /* immediate integer value */
    char op;                      /* operator character for binary/unary */
    struct IRInstr *prev;
    struct IRInstr *next;
    int line;                     /* original source line (0 = synthetic) */
    bool dead;                    /* DCE: marked for removal */
} IRInstr;

/* ─── Basic Block & Control Flow Graph ──────────────────────────────── */

/* A basic block is a maximal sequence of instructions with a single entry
 * (the first instruction) and a single exit (the last instruction, which
 * must be a terminator: branch, return, or fall-through). */
typedef struct BasicBlock {
    int id;
    char label[64];              /* label name for this block (e.g., "L0") */
    IRInstr *first;              /* first instruction in block */
    IRInstr *last;               /* last instruction (terminator) */
    int pred_count;
    int pred_cap;
    struct BasicBlock **preds;   /* predecessor blocks */
    int succ_count;
    int succ_cap;
    struct BasicBlock **succs;   /* successor blocks */
    bool visited;                /* DFS/analysis marker */
    int dom_depth;               /* depth in dominator tree */
    struct BasicBlock *idom;     /* immediate dominator */
} BasicBlock;

/* Control Flow Graph: collection of basic blocks connected by branches.
 * Represents the program dynamic flow structure (L3: Engineering Structure). */
typedef struct CFG {
    BasicBlock **blocks;
    int block_count;
    int block_cap;
    BasicBlock *entry;           /* function entry block */
    BasicBlock *exit;            /* function exit block (synthetic, for analysis) */
} CFG;

/* The IR module holds the flat instruction list and CFG for a single function. */
typedef struct {
    IRInstr *head;
    IRInstr *tail;
    int next_id;
    int temp_counter;
    int label_counter;
    CFG *cfg;
} IRModule;

/* ─── API ───────────────────────────────────────────────────────────── */

/* Module lifecycle */
IRModule *ir_create(void);
void ir_destroy(IRModule *ir);

/* Instruction emitters (each creates and appends one IRInstr) */
IRInstr *ir_emit_label(IRModule *ir, const char *name);
IRInstr *ir_emit_binary(IRModule *ir, const char *dst, const char *src1,
                         char op, const char *src2);
IRInstr *ir_emit_unary(IRModule *ir, const char *dst, char op, const char *src);
IRInstr *ir_emit_copy(IRModule *ir, const char *dst, const char *src);
IRInstr *ir_emit_load_imm(IRModule *ir, const char *dst, int imm);
IRInstr *ir_emit_br(IRModule *ir, const char *label);
IRInstr *ir_emit_br_cond(IRModule *ir, const char *cond, const char *label);
IRInstr *ir_emit_br_not_cond(IRModule *ir, const char *cond, const char *label);
IRInstr *ir_emit_call(IRModule *ir, const char *dst, const char *func, int nargs);
IRInstr *ir_emit_ret(IRModule *ir, const char *val);
IRInstr *ir_emit_param(IRModule *ir, const char *val);
IRInstr *ir_emit_alloca(IRModule *ir, const char *dst, int size);
IRInstr *ir_emit_store(IRModule *ir, const char *addr, const char *val);
IRInstr *ir_emit_load(IRModule *ir, const char *dst, const char *addr);
IRInstr *ir_emit_phi(IRModule *ir, const char *dst);
IRInstr *ir_emit_nop(IRModule *ir);

/* Temporary / label name generation */
char *ir_new_temp(IRModule *ir, char *buf, size_t bufsz);
char *ir_new_label(IRModule *ir, char *buf, size_t bufsz);

/* Debug output */
void ir_print(const IRModule *ir);
void ir_print_instr(const IRInstr *instr);
const char *ir_opcode_name(IROpcode op);

/* ─── AST → IR Translation (L5: Algorithm) ─────────────────────────── */

/*
 * Translates an AST program into IR modules (one per function).
 * Returns a NULL-terminated array of IRModule* pointers.
 */
typedef struct IRProgram {
    IRModule **functions;
    char     **func_names;
    int        count;
} IRProgram;

IRProgram *ir_program_from_ast(ASTNode *program);
void ir_program_destroy(IRProgram *prog);

/* ─── Optimization Passes (L5: Algorithms, L8: Advanced) ───────────── */

/*
 * Constant Folding: evaluates constant sub-expressions at compile time.
 * Implements the classic sparse conditional constant propagation framework
 * (Wegman & Zadeck, 1991) simplified for our IR.
 * Returns: number of instructions folded.
 */
int ir_constant_folding(IRModule *ir);

/*
 * Dead Code Elimination (DCE): removes instructions whose results are
 * never used (no live downstream consumer).
 * Uses a mark-sweep approach: start from "critical" instructions
 * (stores, calls, returns, branches) and mark all reachable defs.
 * Returns: number of instructions eliminated.
 */
int ir_dead_code_elimination(IRModule *ir);

/*
 * Copy Propagation: replaces uses of a copy destination with the source.
 *   t1 = x;  t2 = t1 + 1  →  t1 = x;  t2 = x + 1
 * Then DCE can eliminate the dead copy.
 * Returns: number of copies propagated.
 */
int ir_copy_propagation(IRModule *ir);

/* Run all optimization passes in order until convergence */
void ir_optimize(IRModule *ir);

/* ─── CFG Construction (L5: Algorithm) ────────────────────────────── */

/* Build CFG from flat IR instruction list.
 * Algorithm: identify leaders (first instr, branch targets, instr after branch),
 * partition into basic blocks, connect predecessors/successors. */
CFG *cfg_build(IRModule *ir);
void cfg_destroy(CFG *cfg);
void cfg_print(const CFG *cfg);

/* Compute dominator tree using Cooper-Harvey-Kennedy iterative algorithm.
 * Returns array where doms[i] = immediate dominator of block i
 * (-1 for entry, -2 for unreachable).  Caller must free. */
int *cfg_compute_dominators(const CFG *cfg);

/* Compute dominance frontiers (required for SSA phi-placement).
 * A block w is in the dominance frontier of block b if b dominates a
 * predecessor of w but does not strictly dominate w. */
typedef struct {
    int *frontier_sets;  /* flat array: for each block, a buffer of frontier block ids terminated by -1 */
    int *frontier_starts; /* offset into frontier_sets for each block */
    int *frontier_sizes;  /* number of frontier blocks for each block */
    int block_count;
} DomFrontier;

void dom_frontier_init(DomFrontier *df, int nblocks);
void dom_frontier_destroy(DomFrontier *df);
void dom_frontier_compute(DomFrontier *df, const CFG *cfg, const int *idom);
bool dom_frontier_contains(const DomFrontier *df, int block, int test_block);
void dom_frontier_print(const DomFrontier *df, const CFG *cfg);

/* ─── Liveness Analysis (Backward dataflow) ───────────────────────── */

/*
 * Liveness: a variable v is "live" at point p if there exists a path from p
 * to a use of v along which v is not redefined.
 *
 * For each basic block B, we compute:
 *   USE[B]  = variables used before any definition in B
 *   DEF[B]  = variables defined in B
 *   IN[B]   = variables live at entry to B
 *   OUT[B]  = variables live at exit from B
 *
 * Dataflow equations (backward):
 *   IN[B]  = USE[B] ∪ (OUT[B] − DEF[B])
 *   OUT[B] = ∪_{S ∈ succ[B]} IN[S]
 *
 * Algorithm: iterative fixed-point (round-robin), O(n^3) worst-case,
 * O(n^2) typical on structured CFGs.
 */

#define LV_MAX_VARS 256

typedef struct {
    bool def[LV_MAX_VARS];
    bool use[LV_MAX_VARS];
    bool live_in[LV_MAX_VARS];
    bool live_out[LV_MAX_VARS];
} BlockLiveness;

/* Compute liveness for all blocks.
 * block_lv: pre-allocated array of BlockLiveness[block_count]
 * varnames: parallel array mapping variable index to name */
void lv_compute(CFG *cfg, BlockLiveness *block_lv,
                int nblocks, char * const *varnames, int varcount);
void lv_print(const CFG *cfg, const BlockLiveness *block_lv, int nblocks,
              char * const *varnames, int nvars);

/* Build the interference graph for register allocation from liveness info.
 * Two variables interfere if they are simultaneously live at any program
 * point, meaning they cannot share the same register. */
typedef struct {
    bool **matrix;     /* nvars x nvars adjacency matrix */
    int nvars;
    int *degree;       /* degree of each node */
} InterferenceGraph;

InterferenceGraph *ig_build(CFG *cfg, const BlockLiveness *block_lv,
                             int nblocks, int nvars);
void ig_destroy(InterferenceGraph *ig);

/*
 * Chaitin-Briggs graph-coloring register allocation (L8: Advanced).
 * k = number of available registers
 * Returns: array of size nvars, allocation[i] = register number or -1 (spill)
 */
int *ig_alloc_registers(const InterferenceGraph *ig, int k);

#endif /* IR_H */
