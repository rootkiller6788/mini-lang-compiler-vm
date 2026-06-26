#ifndef TENSOR_EXPR_H
#define TENSOR_EXPR_H

#include <stdbool.h>
#include <stddef.h>
#include "graph_ir.h"

/*
 * Tensor Expression DSL — Module 14.10
 *
 * Halide/TVM-style tensor expression language with compute/schedule separation.
 * References:
 *   - Ragan-Kelley et al., "Halide: A Language and Compiler for Optimizing
 *     Parallelism, Locality, and Recomputation" (PLDI 2013)
 *   - Chen et al., "TVM: An Automated End-to-End Optimizing Compiler
 *     for Deep Learning" (OSDI 2018)
 *
 * L2: Core Concepts — compute/schedule separation, reduce axis
 * L3: Engineering Structures — expression nodes, schedule tree
 * L5: Algorithms — auto-bound inference, storage flattening
 * L8: Advanced Topics — compute_at, rfactor, storage folding
 */

#define TEXPR_MAX_NODES          128
#define TEXPR_MAX_ARGS             4
#define TEXPR_MAX_DIMS             6
#define TEXPR_MAX_REDUCE_AXES      3
#define TEXPR_MAX_SCHEDULE_STAGES  16
#define TEXPR_MAX_NAME_LEN         64

/* ---- L1: Definitions — Expression Node Types ---- */
typedef enum {
    Expr_PLACEHOLDER,   /* Input tensor */
    Expr_VAR,           /* Index variable (loop iterator) */
    Expr_CONST,         /* Constant scalar */
    Expr_ADD,
    Expr_SUB,
    Expr_MUL,
    Expr_DIV,
    Expr_MAX,
    Expr_MIN,
    Expr_CMP_LT,        /* less-than */
    Expr_CMP_EQ,        /* equality */
    Expr_SELECT,        /* ternary: cond ? a : b */
    Expr_CAST,          /* type cast */
    Expr_LOAD,          /* load from buffer */
    Expr_STORE,         /* store to buffer */
    Expr_RAMP,          /* vector ramp: [base, base+stride, ...] */
    Expr_BROADCAST,     /* broadcast scalar to shape */
    Expr_REDUCE_SUM,
    Expr_REDUCE_MAX,
    Expr_REDUCE_MIN,
    Expr_COMPUTE,       /* general compute node */
    Expr_CALL,          /* external function call */
    Expr_LET,           /* let-binding */
    Expr_COUNT
} ExprKind;

/* ---- L1: Definitions — Index Variable ---- */
typedef struct {
    char name[16];
    int  domain_min;
    int  domain_extent;
} IndexVar;

/* ---- L1: Definitions — Expression Node ---- */
typedef struct ExprNode {
    ExprKind kind;
    struct ExprNode *args[TEXPR_MAX_ARGS];
    int arg_count;
    DataType dtype;

    /* For loads/stores: buffer reference */
    char buffer_name[TEXPR_MAX_NAME_LEN];
    IndexVar indices[TEXPR_MAX_DIMS];
    int index_count;

    /* For constants */
    union {
        int ival;
        float fval;
    } const_val;

    /* For reduce: axes being reduced */
    int reduce_axes[TEXPR_MAX_REDUCE_AXES];
    int reduce_axis_count;

    /* For var: the variable itself */
    IndexVar var;

    int id;
    char name[TEXPR_MAX_NAME_LEN];
} ExprNode;

/* ---- L1: Definitions — Compute Stage ---- */
typedef struct {
    char name[TEXPR_MAX_NAME_LEN];
    ExprNode *compute_expr;
    IndexVar loop_vars[TEXPR_MAX_DIMS];
    int loop_var_count;
    bool is_output;
    bool is_inlined;
    int compute_at_stage;  /* Stage to compute_at (Halide-style) */
    int compute_at_level;  /* Loop level */
} ComputeStage;

/* ---- L1: Definitions — Schedule ---- */
typedef enum {
    Sched_SPLIT,      /* Split a loop into inner/outer tiles */
    Sched_REORDER,    /* Reorder loop nest */
    Sched_UNROLL,     /* Unroll innermost loop */
    Sched_VECTORIZE,  /* Vectorize innermost loop */
    Sched_PARALLEL,   /* Parallelize a loop */
    Sched_COMPUTE_AT, /* Compute a stage at a loop level of another */
    Sched_INLINE,     /* Inline a computation */
    Sched_BIND,       /* Bind a loop to thread/block */
    Sched_RFACTOR,    /* Factor reduction for parallelism */
    Sched_STORAGE_ALIGN, /* Align storage for vectorization */
    Sched_COUNT
} SchedulePrimitive;

typedef struct {
    SchedulePrimitive prim;
    int stage_id;
    int loop_level;
    int param_a;
    int param_b;
    char param_name[32];
} ScheduleTransform;

/* ---- L1: Definitions — Tensor Expression DSL Context ---- */
typedef struct {
    ExprNode nodes[TEXPR_MAX_NODES];
    int node_count;
    ComputeStage stages[TEXPR_MAX_SCHEDULE_STAGES];
    int stage_count;
    ScheduleTransform schedule[TEXPR_MAX_SCHEDULE_STAGES * 4];
    int schedule_count;
    int next_id;
} TensorExpr;

/* ---- L1: API Declarations ---- */

/* L2: Compute/Schedule separation primitives */
TensorExpr tensor_expr_create(void);

/* Create expression nodes */
ExprNode *texpr_placeholder(TensorExpr *ctx, const char *name,
                            int ndim, DataType dtype);
ExprNode *texpr_var(TensorExpr *ctx, const char *name, int extent);
ExprNode *texpr_constant(TensorExpr *ctx, float value);
ExprNode *texpr_add(TensorExpr *ctx, ExprNode *a, ExprNode *b);
ExprNode *texpr_sub(TensorExpr *ctx, ExprNode *a, ExprNode *b);
ExprNode *texpr_mul(TensorExpr *ctx, ExprNode *a, ExprNode *b);
ExprNode *texpr_div(TensorExpr *ctx, ExprNode *a, ExprNode *b);
ExprNode *texpr_max(TensorExpr *ctx, ExprNode *a, ExprNode *b);
ExprNode *texpr_min(TensorExpr *ctx, ExprNode *a, ExprNode *b);
ExprNode *texpr_select(TensorExpr *ctx, ExprNode *cond, ExprNode *t, ExprNode *f);
ExprNode *texpr_cast(TensorExpr *ctx, ExprNode *e, DataType target_dtype);
ExprNode *texpr_load(TensorExpr *ctx, const char *buffer, IndexVar *indices,
                     int n_indices);
ExprNode *texpr_reduce_sum(TensorExpr *ctx, ExprNode *body,
                           const int *reduce_axes, int n_axes);
ExprNode *texpr_reduce_max(TensorExpr *ctx, ExprNode *body,
                           const int *reduce_axes, int n_axes);

/* L3: Stage creation and scheduling */
ComputeStage *texpr_create_stage(TensorExpr *ctx, const char *name,
                                  ExprNode *compute);
void texpr_stage_set_loops(ComputeStage *stage, IndexVar *vars, int n_vars);
void texpr_schedule_split(TensorExpr *ctx, int stage_id, int loop_level,
                           int factor);
void texpr_schedule_reorder(TensorExpr *ctx, int stage_id,
                             const int *new_order, int n);
void texpr_schedule_unroll(TensorExpr *ctx, int stage_id, int loop_level);
void texpr_schedule_vectorize(TensorExpr *ctx, int stage_id, int loop_level);
void texpr_schedule_parallel(TensorExpr *ctx, int stage_id, int loop_level);
void texpr_schedule_compute_at(TensorExpr *ctx, int stage_id,
                                int target_stage, int loop_level);
void texpr_schedule_inline(TensorExpr *ctx, int stage_id);

/* L5: Auto-bound inference */
void texpr_infer_bounds(TensorExpr *ctx);
int  texpr_infer_stage_bounds(ComputeStage *stage, ExprNode *expr);

/* L5: Storage flattening — N-d indices to 1-d offset */
int  texpr_flatten_indices(const int *indices, const int *strides, int ndim);
void texpr_compute_strides(const int *dims, int ndim, int *strides);

/* L7: Lower to ComputeGraph */
int texpr_lower_to_graph(TensorExpr *ctx, ComputeGraph *g);

/* L8: rfactor — factor a reduction for parallelism */
bool texpr_rfactor(TensorExpr *ctx, int stage_id, int reduce_axis);

/* L8: storage_align */
void texpr_storage_align(TensorExpr *ctx, int stage_id, int dim, int factor);

/* Print/debug */
void texpr_print_node(ExprNode *n);
void texpr_print_stage(ComputeStage *s);
void texpr_print_schedule(TensorExpr *ctx);
void texpr_print_full(TensorExpr *ctx);
const char *texpr_expr_kind_name(ExprKind k);
const char *texpr_sched_prim_name(SchedulePrimitive p);

#endif /* TENSOR_EXPR_H */
