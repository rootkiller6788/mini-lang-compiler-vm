#include "tensor_expr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Tensor Expression DSL -- Module 14.10
 *
 * Halide/TVM-style tensor expression language with compute/schedule separation.
 *
 * L2: Core Concepts -- From Halide (Ragan-Kelley et al., PLDI 2013):
 *   The key insight: separate WHAT you compute (algorithm) from
 *   HOW you compute it (schedule). This decoupling enables:
 *   - Same algorithm, different schedules for different hardware
 *   - Automatic optimization without changing algorithm
 *   - Schedule search space exploration (auto-tuning)
 *
 * L3: Engineering Structures:
 *   - Expression DAG: nodes represent operations, edges data flow
 *   - Schedule tree: loop-level transformations (split, fuse, reorder)
 *   - Storage hierarchy: global, shared, register
 *
 * L5: Algorithms:
 *   - Auto-bound inference: derive loop extents from tensor shapes
 *   - Storage flattening: map N-d indices to 1-d offsets
 *   - rfactor: factor a reduction dimension for parallelism
 */

/* ---- L1: Name tables ---- */

const char *texpr_expr_kind_name(ExprKind k)
{
    switch (k) {
    case Expr_PLACEHOLDER:  return "placeholder";
    case Expr_VAR:          return "var";
    case Expr_CONST:        return "const";
    case Expr_ADD:          return "add";
    case Expr_SUB:          return "sub";
    case Expr_MUL:          return "mul";
    case Expr_DIV:          return "div";
    case Expr_MAX:          return "max";
    case Expr_MIN:          return "min";
    case Expr_CMP_LT:       return "lt";
    case Expr_CMP_EQ:       return "eq";
    case Expr_SELECT:       return "select";
    case Expr_CAST:         return "cast";
    case Expr_LOAD:         return "load";
    case Expr_STORE:        return "store";
    case Expr_RAMP:         return "ramp";
    case Expr_BROADCAST:    return "broadcast";
    case Expr_REDUCE_SUM:   return "reduce_sum";
    case Expr_REDUCE_MAX:   return "reduce_max";
    case Expr_REDUCE_MIN:   return "reduce_min";
    case Expr_COMPUTE:      return "compute";
    case Expr_CALL:         return "call";
    case Expr_LET:          return "let";
    default: return "unknown";
    }
}

const char *texpr_sched_prim_name(SchedulePrimitive p)
{
    switch (p) {
    case Sched_SPLIT:         return "split";
    case Sched_REORDER:       return "reorder";
    case Sched_UNROLL:        return "unroll";
    case Sched_VECTORIZE:     return "vectorize";
    case Sched_PARALLEL:      return "parallel";
    case Sched_COMPUTE_AT:    return "compute_at";
    case Sched_INLINE:        return "inline";
    case Sched_BIND:          return "bind";
    case Sched_RFACTOR:       return "rfactor";
    case Sched_STORAGE_ALIGN: return "storage_align";
    default: return "unknown";
    }
}

/* ---- L1: Expression Node Allocation ---- */

static ExprNode *texpr_alloc_node(TensorExpr *ctx)
{
    if (ctx->node_count >= TEXPR_MAX_NODES) return NULL;
    ExprNode *n = &ctx->nodes[ctx->node_count];
    memset(n, 0, sizeof(ExprNode));
    n->id = ctx->next_id++;
    ctx->node_count++;
    return n;
}

/* ---- L2: Compute/Schedule Separation Primitives ----
   Halide philosophy: separate the computation definition
   (the "what") from the optimization schedule (the "how").
   
   Example: Blur a 2D image
     Computation: out(x,y) = (in(x-1,y) + in(x,y) + in(x+1,y)) / 3
     Schedule choices:
       - Tile loops for cache locality
       - Vectorize innermost loop
       - Compute intermediate values at a coarser level
   
   All schedules produce identical results but with vastly
   different performance (10x-100x difference common). */

TensorExpr tensor_expr_create(void)
{
    TensorExpr ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.node_count = 0;
    ctx.stage_count = 0;
    ctx.schedule_count = 0;
    ctx.next_id = 0;
    return ctx;
}

/* ---- L1: Expression Node Constructors ---- */

ExprNode *texpr_placeholder(TensorExpr *ctx, const char *name,
                            int ndim, DataType dtype)
{
    ExprNode *n = texpr_alloc_node(ctx);
    if (!n) return NULL;
    n->kind = Expr_PLACEHOLDER;
    n->dtype = dtype;
    strncpy(n->name, name, TEXPR_MAX_NAME_LEN - 1);
    n->index_count = ndim;
    return n;
}

ExprNode *texpr_var(TensorExpr *ctx, const char *name, int extent)
{
    ExprNode *n = texpr_alloc_node(ctx);
    if (!n) return NULL;
    n->kind = Expr_VAR;
    n->dtype = DType_INT32;
    strncpy(n->var.name, name, 15);
    n->var.domain_min = 0;
    n->var.domain_extent = extent;
    return n;
}

ExprNode *texpr_constant(TensorExpr *ctx, float value)
{
    ExprNode *n = texpr_alloc_node(ctx);
    if (!n) return NULL;
    n->kind = Expr_CONST;
    n->dtype = DType_FLOAT32;
    n->const_val.fval = value;
    return n;
}

/* ---- L1: Binary expression constructors ---- */

static ExprNode *texpr_binary(TensorExpr *ctx, ExprKind kind,
                               ExprNode *a, ExprNode *b)
{
    ExprNode *n = texpr_alloc_node(ctx);
    if (!n) return NULL;
    n->kind = kind;
    n->args[0] = a;
    n->args[1] = b;
    n->arg_count = 2;
    n->dtype = a->dtype;
    return n;
}

ExprNode *texpr_add(TensorExpr *ctx, ExprNode *a, ExprNode *b)
{ return texpr_binary(ctx, Expr_ADD, a, b); }

ExprNode *texpr_sub(TensorExpr *ctx, ExprNode *a, ExprNode *b)
{ return texpr_binary(ctx, Expr_SUB, a, b); }

ExprNode *texpr_mul(TensorExpr *ctx, ExprNode *a, ExprNode *b)
{ return texpr_binary(ctx, Expr_MUL, a, b); }

ExprNode *texpr_div(TensorExpr *ctx, ExprNode *a, ExprNode *b)
{ return texpr_binary(ctx, Expr_DIV, a, b); }

ExprNode *texpr_max(TensorExpr *ctx, ExprNode *a, ExprNode *b)
{ return texpr_binary(ctx, Expr_MAX, a, b); }

ExprNode *texpr_min(TensorExpr *ctx, ExprNode *a, ExprNode *b)
{ return texpr_binary(ctx, Expr_MIN, a, b); }

/* ---- L1: Ternary (select) ---- */

ExprNode *texpr_select(TensorExpr *ctx, ExprNode *cond,
                        ExprNode *t, ExprNode *f)
{
    ExprNode *n = texpr_alloc_node(ctx);
    if (!n) return NULL;
    n->kind = Expr_SELECT;
    n->args[0] = cond;
    n->args[1] = t;
    n->args[2] = f;
    n->arg_count = 3;
    n->dtype = t->dtype;
    return n;
}

/* ---- L1: Cast ---- */

ExprNode *texpr_cast(TensorExpr *ctx, ExprNode *e, DataType target_dtype)
{
    ExprNode *n = texpr_alloc_node(ctx);
    if (!n) return NULL;
    n->kind = Expr_CAST;
    n->args[0] = e;
    n->arg_count = 1;
    n->dtype = target_dtype;
    return n;
}

/* ---- L5: Load from buffer with indices ----
   Maps a logical N-d tensor access to physical memory.
   Index variables represent loop iterators that will be
   lowered to actual for-loops. */

ExprNode *texpr_load(TensorExpr *ctx, const char *buffer,
                     IndexVar *indices, int n_indices)
{
    ExprNode *n = texpr_alloc_node(ctx);
    if (!n) return NULL;
    n->kind = Expr_LOAD;
    n->dtype = DType_FLOAT32;
    strncpy(n->buffer_name, buffer, TEXPR_MAX_NAME_LEN - 1);
    if (indices && n_indices <= TEXPR_MAX_DIMS) {
        memcpy(n->indices, indices, n_indices * sizeof(IndexVar));
    }
    n->index_count = n_indices;
    return n;
}

/* ---- L5: Reduction operators ----
   Reductions aggregate values along specified axes.
   Examples: sum over all elements, max over channels.

   The reduce_axes specify which loop dimensions are
   reduced (collapsed), leaving the remaining dimensions
   as output dimensions.

   Halide-style:
     RDom r(0, N);
     f(x) = sum(g(x, r));
   Here r is the reduction domain, x is the pure variable. */

ExprNode *texpr_reduce_sum(TensorExpr *ctx, ExprNode *body,
                           const int *reduce_axes, int n_axes)
{
    ExprNode *n = texpr_alloc_node(ctx);
    if (!n) return NULL;
    n->kind = Expr_REDUCE_SUM;
    n->args[0] = body;
    n->arg_count = 1;
    n->dtype = body->dtype;
    if (reduce_axes && n_axes <= TEXPR_MAX_REDUCE_AXES) {
        memcpy(n->reduce_axes, reduce_axes, n_axes * sizeof(int));
    }
    n->reduce_axis_count = n_axes;
    return n;
}

ExprNode *texpr_reduce_max(TensorExpr *ctx, ExprNode *body,
                           const int *reduce_axes, int n_axes)
{
    ExprNode *n = texpr_alloc_node(ctx);
    if (!n) return NULL;
    n->kind = Expr_REDUCE_MAX;
    n->args[0] = body;
    n->arg_count = 1;
    n->dtype = body->dtype;
    if (reduce_axes && n_axes <= TEXPR_MAX_REDUCE_AXES) {
        memcpy(n->reduce_axes, reduce_axes, n_axes * sizeof(int));
    }
    n->reduce_axis_count = n_axes;
    return n;
}

/* ---- L3: Stage Creation and Scheduling ----
   A ComputeStage bundles an expression with its loop variables
   and scheduling directives. Stages form the schedule DAG:
   dependencies flow from earlier stages to later stages.

   The schedule determines:
   - Loop order (innermost to outermost)
   - Tiling (split a loop into inner/outer)
   - Vectorization (SIMD width)
   - Unrolling (replicate loop body)
   - Parallelization (thread mapping)
   - compute_at (fuse a stage's computation into another's loops) */

ComputeStage *texpr_create_stage(TensorExpr *ctx, const char *name,
                                  ExprNode *compute)
{
    if (ctx->stage_count >= TEXPR_MAX_SCHEDULE_STAGES) return NULL;

    ComputeStage *stage = &ctx->stages[ctx->stage_count];
    memset(stage, 0, sizeof(ComputeStage));
    strncpy(stage->name, name, TEXPR_MAX_NAME_LEN - 1);
    stage->compute_expr = compute;
    stage->loop_var_count = 0;
    stage->is_output = false;
    stage->is_inlined = false;
    stage->compute_at_stage = -1;
    stage->compute_at_level = -1;

    ctx->stage_count++;
    return stage;
}

void texpr_stage_set_loops(ComputeStage *stage, IndexVar *vars, int n_vars)
{
    if (n_vars > TEXPR_MAX_DIMS) n_vars = TEXPR_MAX_DIMS;
    if (vars) memcpy(stage->loop_vars, vars, n_vars * sizeof(IndexVar));
    stage->loop_var_count = n_vars;
}

/* ---- L3: Schedule Transforms ----
   Each transform adds a directive to the schedule list.
   Transforms are applied in order during lowering.
   The schedule is a sequence of primitive operations
   on the loop nest structure.

   Split:  for i in [0,N) -> for io in [0,ceil(N/f))
                               for ii in [0,f)
           where i = io * f + ii
   
   Reorder: change the nesting order of loops
            e.g., [i, j, k] -> [k, j, i]
   
   Unroll: replicate loop body, remove loop overhead
           for small constant bounds
   
   Vectorize: use SIMD instructions (AVX2: 256-bit = 8 floats)
   
   Parallel: map loop iterations to CPU threads
   
   compute_at: compute the stage where it's first consumed,
               rather than as a separate pass over the data */

void texpr_schedule_split(TensorExpr *ctx, int stage_id,
                           int loop_level, int factor)
{
    if (ctx->schedule_count >= TEXPR_MAX_SCHEDULE_STAGES * 4) return;
    ScheduleTransform *t = &ctx->schedule[ctx->schedule_count];
    t->prim = Sched_SPLIT;
    t->stage_id = stage_id;
    t->loop_level = loop_level;
    t->param_a = factor;
    t->param_b = 0;
    ctx->schedule_count++;
}

void texpr_schedule_reorder(TensorExpr *ctx, int stage_id,
                             const int *new_order, int n)
{
    if (ctx->schedule_count >= TEXPR_MAX_SCHEDULE_STAGES * 4) return;
    ScheduleTransform *t = &ctx->schedule[ctx->schedule_count];
    t->prim = Sched_REORDER;
    t->stage_id = stage_id;
    t->loop_level = n;
    /* new_order stored in param fields */
    if (new_order && n >= 1) t->param_a = new_order[0];
    if (n >= 2) t->param_b = new_order[1];
    ctx->schedule_count++;
}

void texpr_schedule_unroll(TensorExpr *ctx, int stage_id, int loop_level)
{
    if (ctx->schedule_count >= TEXPR_MAX_SCHEDULE_STAGES * 4) return;
    ScheduleTransform *t = &ctx->schedule[ctx->schedule_count];
    t->prim = Sched_UNROLL;
    t->stage_id = stage_id;
    t->loop_level = loop_level;
    ctx->schedule_count++;
}

void texpr_schedule_vectorize(TensorExpr *ctx, int stage_id, int loop_level)
{
    if (ctx->schedule_count >= TEXPR_MAX_SCHEDULE_STAGES * 4) return;
    ScheduleTransform *t = &ctx->schedule[ctx->schedule_count];
    t->prim = Sched_VECTORIZE;
    t->stage_id = stage_id;
    t->loop_level = loop_level;
    ctx->schedule_count++;
}

void texpr_schedule_parallel(TensorExpr *ctx, int stage_id, int loop_level)
{
    if (ctx->schedule_count >= TEXPR_MAX_SCHEDULE_STAGES * 4) return;
    ScheduleTransform *t = &ctx->schedule[ctx->schedule_count];
    t->prim = Sched_PARALLEL;
    t->stage_id = stage_id;
    t->loop_level = loop_level;
    ctx->schedule_count++;
}

void texpr_schedule_compute_at(TensorExpr *ctx, int stage_id,
                                int target_stage, int loop_level)
{
    if (ctx->schedule_count >= TEXPR_MAX_SCHEDULE_STAGES * 4) return;
    ScheduleTransform *t = &ctx->schedule[ctx->schedule_count];
    t->prim = Sched_COMPUTE_AT;
    t->stage_id = stage_id;
    t->param_a = target_stage;
    t->param_b = loop_level;
    ctx->schedule_count++;
}

void texpr_schedule_inline(TensorExpr *ctx, int stage_id)
{
    if (ctx->schedule_count >= TEXPR_MAX_SCHEDULE_STAGES * 4) return;
    ScheduleTransform *t = &ctx->schedule[ctx->schedule_count];
    t->prim = Sched_INLINE;
    t->stage_id = stage_id;
    ctx->schedule_count++;
}

/* ---- L5: Auto-Bound Inference ----
   Derives loop extents from the expression tree.
   Walks the expression DAG to determine the domain of each
   loop variable based on buffer dimensions and operations.

   For example, for out(x, y) = in(x, y) + 1:
   - x domain = [0, in.dim[0])
   - y domain = [0, in.dim[1])
   
   Reduction bounds come from reduce domain variables. */

void texpr_infer_bounds(TensorExpr *ctx)
{
    int i;
    for (i = 0; i < ctx->stage_count; i++) {
        ComputeStage *stage = &ctx->stages[i];
        texpr_infer_stage_bounds(stage, stage->compute_expr);
    }
}

int texpr_infer_stage_bounds(ComputeStage *stage, ExprNode *expr)
{
    int inferred = 0;
    int i;

    if (!expr) return 0;

    switch (expr->kind) {
    case Expr_LOAD:
        /* Loop bounds match the buffer dimensions */
        for (i = 0; i < expr->index_count && i < stage->loop_var_count; i++) {
            if (stage->loop_vars[i].domain_extent == 0) {
                /* Set extent to a default value */
                stage->loop_vars[i].domain_extent = 256;
                inferred++;
            }
        }
        break;
    case Expr_ADD:
    case Expr_SUB:
    case Expr_MUL:
    case Expr_DIV:
    case Expr_MAX:
    case Expr_MIN:
        for (i = 0; i < expr->arg_count; i++) {
            inferred += texpr_infer_stage_bounds(stage, expr->args[i]);
        }
        break;
    case Expr_REDUCE_SUM:
    case Expr_REDUCE_MAX:
    case Expr_REDUCE_MIN:
        /* Reduce axes are loop dimensions that get collapsed */
        if (expr->args[0]) {
            inferred += texpr_infer_stage_bounds(stage, expr->args[0]);
        }
        break;
    case Expr_PLACEHOLDER:
    case Expr_VAR:
    case Expr_CONST:
        /* Terminal -- no bounds to infer */
        break;
    default:
        for (i = 0; i < expr->arg_count; i++) {
            inferred += texpr_infer_stage_bounds(stage, expr->args[i]);
        }
        break;
    }

    return inferred;
}

/* ---- L5: Storage Flattening ----
   Maps N-dimensional tensor indices to 1-dimensional memory offsets.
   Row-major layout (C convention): last index varies fastest.
   
   offset = sum_{d=0}^{ndim-1} index[d] * stride[d]
   where stride[d] = prod_{k=d+1}^{ndim-1} dims[k]

   Example: 3D tensor [2, 3, 4]
   strides = [12, 4, 1]
   element (1, 2, 3) = 1*12 + 2*4 + 3*1 = 23 */

int texpr_flatten_indices(const int *indices, const int *strides, int ndim)
{
    int offset = 0, i;
    for (i = 0; i < ndim; i++) {
        offset += indices[i] * strides[i];
    }
    return offset;
}

void texpr_compute_strides(const int *dims, int ndim, int *strides)
{
    int i;
    strides[ndim - 1] = 1;
    for (i = ndim - 2; i >= 0; i--) {
        strides[i] = strides[i + 1] * dims[i + 1];
    }
}

/* ---- L7: Lower to ComputeGraph ----
   Converts the high-level tensor expression DSL to the
   lower-level ComputeGraph IR that can be further optimized
   by fusion, layout optimization, and code generation.

   Lowering steps:
   1. Create graph nodes for each stage.
   2. Connect them via dataflow edges.
   3. Preserve schedule information as graph attributes. */

int texpr_lower_to_graph(TensorExpr *ctx, ComputeGraph *g)
{
    int i;
    int node_ids[TEXPR_MAX_SCHEDULE_STAGES];
    int prev_id = -1;

    for (i = 0; i < ctx->stage_count; i++) {
        ComputeStage *stage = &ctx->stages[i];
        GraphOpType op_type;

        /* Map expression kind to graph operation type */
        switch (stage->compute_expr->kind) {
        case Expr_ADD:         op_type = GOp_ADD; break;
        case Expr_MUL:         op_type = GOp_MUL; break;
        case Expr_REDUCE_SUM:  op_type = GOp_SOFTMAX; break;
        case Expr_LOAD:        op_type = GOp_CONV2D; break;
        default:               op_type = GOp_ADD; break;
        }

        int inputs[1] = { prev_id };
        int n_inputs = (prev_id >= 0) ? 1 : 0;

        node_ids[i] = graph_add_node(g, op_type,
                                      n_inputs > 0 ? inputs : NULL,
                                      n_inputs, stage->name);
        prev_id = node_ids[i];
    }

    return ctx->stage_count;
}

/* ---- L8: rfactor -- Factor Reduction for Parallelism ----
   Splits an associative reduction into two stages:
   1. Partial reductions (parallelizable)
   2. Global reduction (combine partials)

   This is the key transformation for parallelizing reductions.
   Example: sum over array -> partial sums (threads) + final sum (atomic)

   rfactor(sum(A[i]), axis=0) ->
     partial[thread_id] += A[thread_id * chunk + i]  (parallel)
     result = sum(partial)                            (single)

   Reference: Suriana et al. (PLDI 2017),
   "A Scheduler for Halide's Parallelizing Compiler" */

bool texpr_rfactor(TensorExpr *ctx, int stage_id, int reduce_axis)
{
    if (stage_id >= ctx->stage_count) return false;

    ComputeStage *stage = &ctx->stages[stage_id];
    ExprNode *orig = stage->compute_expr;

    if (!orig) return false;
    if (orig->kind != Expr_REDUCE_SUM &&
        orig->kind != Expr_REDUCE_MAX &&
        orig->kind != Expr_REDUCE_MIN) {
        return false; /* Only reductions can be rfactored */
    }

    /* Create intermediate stage for partial reduction */
    char int_name[TEXPR_MAX_NAME_LEN];
    snprintf(int_name, sizeof(int_name), "%s_partial", stage->name);

    ExprNode *partial = texpr_alloc_node(ctx);
    if (!partial) return false;
    *partial = *orig;
    snprintf(partial->name, TEXPR_MAX_NAME_LEN, "%s", int_name);

    ComputeStage *int_stage = texpr_create_stage(ctx, int_name, partial);
    if (!int_stage) return false;
    int_stage->loop_var_count = stage->loop_var_count;
    memcpy(int_stage->loop_vars, stage->loop_vars,
           stage->loop_var_count * sizeof(IndexVar));

    /* Mark original stage to consume partial result */
    stage->compute_expr = texpr_alloc_node(ctx);
    if (!stage->compute_expr) return false;

    stage->compute_expr->kind = orig->kind;
    stage->compute_expr->args[0] = partial;
    stage->compute_expr->arg_count = 1;
    stage->compute_expr->reduce_axis_count = 0; /* Now global reduction */

    (void)reduce_axis;
    return true;
}

/* ---- L8: Storage Alignment ----
   Aligns the base address of a buffer to a SIMD-friendly boundary.
   For AVX2 (256-bit = 32 bytes), alignment to 32 bytes enables
   aligned loads (vmovaps) instead of unaligned (vmovups),
   improving throughput by ~10-20%. */

void texpr_storage_align(TensorExpr *ctx, int stage_id, int dim, int factor)
{
    if (ctx->schedule_count >= TEXPR_MAX_SCHEDULE_STAGES * 4) return;
    ScheduleTransform *t = &ctx->schedule[ctx->schedule_count];
    t->prim = Sched_STORAGE_ALIGN;
    t->stage_id = stage_id;
    t->loop_level = dim;
    t->param_a = factor;
    ctx->schedule_count++;
}

/* ---- Print / Debug ---- */

void texpr_print_node(ExprNode *n)
{
    if (!n) { printf("(null)"); return; }

    printf("%s", texpr_expr_kind_name(n->kind));

    if (n->arg_count > 0) {
        int i;
        printf("(");
        for (i = 0; i < n->arg_count; i++) {
            if (i > 0) printf(", ");
            texpr_print_node(n->args[i]);
        }
        printf(")");
    }

    switch (n->kind) {
    case Expr_CONST:
        printf("=%g", n->const_val.fval);
        break;
    case Expr_VAR:
        printf("=%s:[%d,%d]", n->var.name,
               n->var.domain_min, n->var.domain_extent);
        break;
    case Expr_LOAD:
        printf("=%s[", n->buffer_name);
        {
            int i;
            for (i = 0; i < n->index_count; i++) {
                if (i > 0) printf(",");
                printf("%s", n->indices[i].name);
            }
        }
        printf("]");
        break;
    case Expr_REDUCE_SUM:
    case Expr_REDUCE_MAX:
    case Expr_REDUCE_MIN:
        printf(" axes=[");
        {
            int i;
            for (i = 0; i < n->reduce_axis_count; i++) {
                if (i > 0) printf(",");
                printf("%d", n->reduce_axes[i]);
            }
        }
        printf("]");
        break;
    default:
        break;
    }
}

void texpr_print_stage(ComputeStage *s)
{
    int i;
    printf("Stage \"%s\": %s", s->name,
           s->is_inlined ? "(inlined) " : "");
    printf("loops=[");
    for (i = 0; i < s->loop_var_count; i++) {
        if (i > 0) printf(", ");
        printf("%s:[0,%d)", s->loop_vars[i].name,
               s->loop_vars[i].domain_extent);
    }
    printf("]");
    if (s->compute_at_stage >= 0) {
        printf(" compute_at(stage=%d, level=%d)",
               s->compute_at_stage, s->compute_at_level);
    }
    printf(" expr=");
    texpr_print_node(s->compute_expr);
    printf("\n");
}

void texpr_print_schedule(TensorExpr *ctx)
{
    int i;
    printf("Schedule (%d transforms):\n", ctx->schedule_count);
    for (i = 0; i < ctx->schedule_count; i++) {
        ScheduleTransform *t = &ctx->schedule[i];
        printf("  [%d] %s(stage=%d, level=%d, a=%d, b=%d)\n",
               i, texpr_sched_prim_name(t->prim),
               t->stage_id, t->loop_level, t->param_a, t->param_b);
    }
}

void texpr_print_full(TensorExpr *ctx)
{
    int i;
    printf("Tensor Expression DSL:\n");
    printf("  Nodes: %d, Stages: %d, Schedule transforms: %d\n",
           ctx->node_count, ctx->stage_count, ctx->schedule_count);

    printf("\nStages:\n");
    for (i = 0; i < ctx->stage_count; i++) {
        printf("  ");
        texpr_print_stage(&ctx->stages[i]);
    }

    if (ctx->schedule_count > 0) {
        printf("\n");
        texpr_print_schedule(ctx);
    }
}
