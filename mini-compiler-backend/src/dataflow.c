#include "dataflow.h"

void df_init(DataFlowContext *ctx) {
    memset(ctx, 0, sizeof(DataFlowContext));
    ctx->var_capacity = 32;
    ctx->vars = (DFVar *)calloc(ctx->var_capacity, sizeof(DFVar));
    ctx->stmt_capacity = 64;
    ctx->stmts = (DFStatement *)calloc(ctx->stmt_capacity, sizeof(DFStatement));
    ctx->block_count = 0;
}

void df_free(DataFlowContext *ctx) {
    if (!ctx) return;
    free(ctx->vars);
    free(ctx->stmts);
    if (ctx->gen_set) {
        for (int32_t i = 0; i < ctx->block_count; i++) {
            free(ctx->gen_set[i]);
            free(ctx->kill_set[i]);
            free(ctx->in_set[i]);
            free(ctx->out_set[i]);
        }
        free(ctx->gen_set);
        free(ctx->kill_set);
        free(ctx->in_set);
        free(ctx->out_set);
    }
    memset(ctx, 0, sizeof(DataFlowContext));
}

int32_t df_add_var(DataFlowContext *ctx, const char *name) {
    if (ctx->var_count >= ctx->var_capacity) {
        ctx->var_capacity *= 2;
        ctx->vars = (DFVar *)realloc(ctx->vars,
            ctx->var_capacity * sizeof(DFVar));
    }
    DFVar *v = &ctx->vars[ctx->var_count];
    v->var_id = ctx->var_count;
    snprintf(v->name, sizeof(v->name), "%s", name);
    return ctx->var_count++;
}

int32_t df_add_stmt(DataFlowContext *ctx, DFStmtType type, int32_t var_id,
                    int32_t line, const char *text) {
    if (ctx->stmt_count >= ctx->stmt_capacity) {
        ctx->stmt_capacity *= 2;
        ctx->stmts = (DFStatement *)realloc(ctx->stmts,
            ctx->stmt_capacity * sizeof(DFStatement));
    }
    DFStatement *s = &ctx->stmts[ctx->stmt_count];
    s->type = type;
    s->var_id = var_id;
    s->line = line;
    snprintf(s->text, sizeof(s->text), "%s", text ? text : "");
    return ctx->stmt_count++;
}

int32_t df_add_block(DataFlowContext *ctx) {
    return ctx->block_count++;
}

void df_add_cfg_edge(DataFlowContext *ctx, int32_t from, int32_t to) {
    if (from < 0 || from >= 256 || to < 0 || to >= 256) return;
    int32_t c = ctx->pred_counts[to];
    if (c < 64) ctx->preds[to][c] = from;
    ctx->pred_counts[to] = c + 1;
}

size_t df_bitvec_words(DataFlowContext *ctx) {
    return ((size_t)ctx->var_count + 63) / 64;
}

void df_set_bit(bool *vec, int32_t idx, bool val) {
    if (idx >= 0) vec[idx] = val;
}

bool df_get_bit(const bool *vec, int32_t idx) {
    if (idx >= 0) return vec[idx];
    return false;
}

void df_or_vec(bool *dst, const bool *src, size_t words) {
    for (size_t i = 0; i < words; i++) dst[i] = dst[i] || src[i];
}

void df_and_vec(bool *dst, const bool *src, size_t words) {
    for (size_t i = 0; i < words; i++) dst[i] = dst[i] && src[i];
}

void df_copy_vec(bool *dst, const bool *src, size_t words) {
    memcpy(dst, src, words * sizeof(bool));
}

bool df_vec_equal(const bool *a, const bool *b, size_t words) {
    return memcmp(a, b, words * sizeof(bool)) == 0;
}

void df_vec_clear(bool *vec, size_t words) {
    memset(vec, 0, words * sizeof(bool));
}

/*
 * Generic iterative dataflow solver.
 * Forward: OUT[b] = transfer_b(IN[b])
 *          IN[b]  = meet_{p in pred(b)} OUT[p]
 * Iterate until fixed point (IN/OUT sets stabilize).
 *
 * Complexity: O(k * V * B^2) where k is the number of iterations,
 * typically O(V * B) in practice for structured programs.
 *
 * Reference: Kildall (1973) "A Unified Approach to Global Program
 *   Optimization", POPL.
 */
void df_solve_forward(DataFlowContext *ctx, TransferFn transfer, MeetFn meet) {
    int32_t n = ctx->block_count;
    size_t w = df_bitvec_words(ctx);

    bool *temp_in = (bool *)calloc(w, sizeof(bool));
    bool *temp_out = (bool *)calloc(w, sizeof(bool));

    for (int32_t i = 0; i < n; i++) {
        df_vec_clear(ctx->in_set[i], w);
        df_vec_clear(ctx->out_set[i], w);
    }

    bool changed = true;
    int32_t max_iter = n * 10;
    int32_t iter = 0;
    while (changed && iter < max_iter) {
        changed = false;
        iter++;
        for (int32_t b = 0; b < n; b++) {
            if (meet) meet(ctx, b, temp_in);
            else df_vec_clear(temp_in, w);

            df_copy_vec(temp_out, ctx->in_set[b], w);
            if (transfer) transfer(ctx, b, temp_in, temp_out);

            if (!df_vec_equal(ctx->out_set[b], temp_out, w)) {
                df_copy_vec(ctx->out_set[b], temp_out, w);
                changed = true;
            }
            if (!df_vec_equal(ctx->in_set[b], temp_in, w)) {
                df_copy_vec(ctx->in_set[b], temp_in, w);
                changed = true;
            }
        }
    }
    free(temp_in);
    free(temp_out);
}

void df_solve_backward(DataFlowContext *ctx, TransferFn transfer, MeetFn meet) {
    int32_t n = ctx->block_count;
    size_t w = df_bitvec_words(ctx);

    bool *temp_in = (bool *)calloc(w, sizeof(bool));
    bool *temp_out = (bool *)calloc(w, sizeof(bool));

    for (int32_t i = 0; i < n; i++) {
        for (size_t j = 0; j < w; j++) {
            ctx->in_set[i][j] = true;
            ctx->out_set[i][j] = true;
        }
    }

    bool changed = true;
    int32_t max_iter = n * 10;
    int32_t iter = 0;
    while (changed && iter < max_iter) {
        changed = false;
        iter++;
        for (int32_t b = n - 1; b >= 0; b--) {
            if (meet) meet(ctx, b, temp_out);
            else df_vec_clear(temp_out, w);
            df_copy_vec(temp_in, ctx->out_set[b], w);
            if (transfer) transfer(ctx, b, temp_in, temp_out);
            if (!df_vec_equal(ctx->in_set[b], temp_in, w)) {
                df_copy_vec(ctx->in_set[b], temp_in, w);
                changed = true;
            }
        }
    }
    free(temp_in);
    free(temp_out);
}

static void alloc_sets(DataFlowContext *ctx) {
    size_t w = df_bitvec_words(ctx);
    int32_t n = ctx->block_count;
    ctx->gen_set = (bool **)calloc((size_t)n, sizeof(bool *));
    ctx->kill_set = (bool **)calloc((size_t)n, sizeof(bool *));
    ctx->in_set = (bool **)calloc((size_t)n, sizeof(bool *));
    ctx->out_set = (bool **)calloc((size_t)n, sizeof(bool *));
    for (int32_t i = 0; i < n; i++) {
        ctx->gen_set[i] = (bool *)calloc(w, sizeof(bool));
        ctx->kill_set[i] = (bool *)calloc(w, sizeof(bool));
        ctx->in_set[i] = (bool *)calloc(w, sizeof(bool));
        ctx->out_set[i] = (bool *)calloc(w, sizeof(bool));
    }
}

/*
 * Liveness analysis (backward, any-path).
 *   USE[b]: variables used before definition in block b
 *   DEF[b]: variables defined in block b
 *   LIVE-IN[b] = USE[b] U (LIVE-OUT[b] - DEF[b])
 *   LIVE-OUT[b] = U_{s in succ(b)} LIVE-IN[s]
 *
 * Theorem: A variable v is live at point p iff there exists a path from p
 * to a use of v along which v is not redefined.
 *
 * Used for register allocation: two variables can share a register iff
 * their live ranges do not overlap.
 */
static void liveness_gen_kill(DataFlowContext *ctx) {
    alloc_sets(ctx);
    (void)df_bitvec_words(ctx);

    for (int32_t i = 0; i < ctx->stmt_count; i++) {
        DFStatement *s = &ctx->stmts[i];
        int32_t bb = 0;
        if (s->type == DF_DEFINE) {
            df_set_bit(ctx->kill_set[bb], s->var_id, true);
        } else if (s->type == DF_USE) {
            if (!df_get_bit(ctx->kill_set[bb], s->var_id)) {
                df_set_bit(ctx->gen_set[bb], s->var_id, true);
            }
        }
    }
}

void df_compute_liveness(DataFlowContext *ctx) {
    liveness_gen_kill(ctx);
    int32_t n = ctx->block_count;
    size_t w = df_bitvec_words(ctx);

    bool *temp = (bool *)calloc(w, sizeof(bool));
    bool changed = true;
    while (changed) {
        changed = false;
        for (int32_t b = n - 1; b >= 0; b--) {
            df_vec_clear(temp, w);
            for (int32_t s = 0; s < n; s++) {
                for (int32_t p = 0; p < ctx->pred_counts[s]; p++) {
                    if (ctx->preds[s][p] == b) {
                        df_or_vec(temp, ctx->in_set[s], w);
                    }
                }
            }
            bool old_out[256] = {0};
            memcpy(old_out, ctx->out_set[b], w * sizeof(bool));
            df_copy_vec(ctx->out_set[b], temp, w);
            df_copy_vec(temp, ctx->out_set[b], w);
            for (size_t j = 0; j < w; j++) {
                temp[j] = temp[j] && !ctx->kill_set[b][j];
            }
            df_or_vec(temp, ctx->gen_set[b], w);
            if (!df_vec_equal(ctx->in_set[b], temp, w)) {
                df_copy_vec(ctx->in_set[b], temp, w);
                changed = true;
            }
        }
    }
    free(temp);
}

/*
 * Reaching definitions analysis (forward, any-path).
 *   GEN[b]: definitions in b that reach the end of b
 *   KILL[b]: definitions in b of a variable that kill all prior defs of that var
 *   IN[b] = U_{p in pred(b)} OUT[p]
 *   OUT[b] = GEN[b] U (IN[b] - KILL[b])
 *
 * Used for constant propagation and copy propagation.
 */
void df_compute_reaching_defs(DataFlowContext *ctx) {
    if (!ctx->gen_set) alloc_sets(ctx);
    int32_t n = ctx->block_count;
    size_t w = df_bitvec_words(ctx);

    bool *temp = (bool *)calloc(w, sizeof(bool));
    bool changed = true;
    while (changed) {
        changed = false;
        for (int32_t b = 0; b < n; b++) {
            df_vec_clear(temp, w);
            for (int32_t p = 0; p < ctx->pred_counts[b]; p++) {
                int32_t pred = ctx->preds[b][p];
                df_or_vec(temp, ctx->out_set[pred], w);
            }
            if (!df_vec_equal(ctx->in_set[b], temp, w)) {
                df_copy_vec(ctx->in_set[b], temp, w);
                changed = true;
            }
            df_copy_vec(temp, ctx->in_set[b], w);
            for (size_t j = 0; j < w; j++) {
                temp[j] = temp[j] && !ctx->kill_set[b][j];
            }
            df_or_vec(temp, ctx->gen_set[b], w);
            if (!df_vec_equal(ctx->out_set[b], temp, w)) {
                df_copy_vec(ctx->out_set[b], temp, w);
                changed = true;
            }
        }
    }
    free(temp);
}

/*
 * Available expressions (forward, all-path).
 * An expression e is available at point p if every path from entry to p
 * evaluates e, and no operand of e is modified after the last evaluation.
 *
 * Used for common subexpression elimination (CSE).
 *
 * Reference: Cocke (1970), "Global Common Subexpression Elimination",
 *   ACM SIGPLAN Notices.
 */
void df_compute_available_exprs(DataFlowContext *ctx) {
    if (!ctx->gen_set) alloc_sets(ctx);
    int32_t n = ctx->block_count;
    size_t w = df_bitvec_words(ctx);

    for (size_t j = 0; j < w; j++) ctx->in_set[0][j] = false;
    for (size_t j = 0; j < w; j++) ctx->out_set[0][j] = false;

    bool *temp = (bool *)calloc(w, sizeof(bool));
    bool changed = true;
    while (changed) {
        changed = false;
        for (int32_t b = 1; b < n; b++) {
            bool first = true;
            df_vec_clear(temp, w);
            for (int32_t p = 0; p < ctx->pred_counts[b]; p++) {
                int32_t pred = ctx->preds[b][p];
                if (first) {
                    df_copy_vec(temp, ctx->out_set[pred], w);
                    first = false;
                } else {
                    df_and_vec(temp, ctx->out_set[pred], w);
                }
            }
            if (first) df_vec_clear(temp, w);
            if (!df_vec_equal(ctx->in_set[b], temp, w)) {
                df_copy_vec(ctx->in_set[b], temp, w);
                changed = true;
            }
            df_copy_vec(temp, ctx->in_set[b], w);
            for (size_t j = 0; j < w; j++) {
                temp[j] = temp[j] && !ctx->kill_set[b][j];
            }
            df_or_vec(temp, ctx->gen_set[b], w);
            if (!df_vec_equal(ctx->out_set[b], temp, w)) {
                df_copy_vec(ctx->out_set[b], temp, w);
                changed = true;
            }
        }
    }
    free(temp);
}

/*
 * Very busy expressions (backward, all-path).
 * An expression e is very busy at point p if on every path from p, e is
 * evaluated before any of its operands are modified.
 *
 * Used for code hoisting (moving expressions earlier to reduce code size).
 */
void df_compute_very_busy(DataFlowContext *ctx) {
    if (!ctx->gen_set) alloc_sets(ctx);
    int32_t n = ctx->block_count;
    size_t w = df_bitvec_words(ctx);

    bool *temp = (bool *)calloc(w, sizeof(bool));
    bool changed = true;
    while (changed) {
        changed = false;
        for (int32_t b = n - 1; b >= 0; b--) {
            bool first = true;
            df_vec_clear(temp, w);
            for (int32_t s = 0; s < n; s++) {
                for (int32_t p = 0; p < ctx->pred_counts[s]; p++) {
                    if (ctx->preds[s][p] == b) {
                        if (first) {
                            df_copy_vec(temp, ctx->in_set[s], w);
                            first = false;
                        } else {
                            df_and_vec(temp, ctx->in_set[s], w);
                        }
                    }
                }
            }
            if (first) df_vec_clear(temp, w);
            if (!df_vec_equal(ctx->out_set[b], temp, w)) {
                df_copy_vec(ctx->out_set[b], temp, w);
                changed = true;
            }
            df_copy_vec(temp, ctx->out_set[b], w);
            for (size_t j = 0; j < w; j++) {
                temp[j] = temp[j] && !ctx->kill_set[b][j];
            }
            df_or_vec(temp, ctx->gen_set[b], w);
            if (!df_vec_equal(ctx->in_set[b], temp, w)) {
                df_copy_vec(ctx->in_set[b], temp, w);
                changed = true;
            }
        }
    }
    free(temp);
}

void df_dump_sets(DataFlowContext *ctx, FILE *out) {
    if (!ctx || !out) return;
    size_t nv = (size_t)ctx->var_count;
    int32_t nb = ctx->block_count;

    fprintf(out, ";;; Dataflow Analysis Results (%d vars, %d blocks):\n", (int)nv, nb);
    fprintf(out, ";;; Variables: ");
    for (size_t i = 0; i < nv; i++) fprintf(out, "%s ", ctx->vars[i].name);
    fprintf(out, "\n");

    for (int32_t b = 0; b < nb; b++) {
        fprintf(out, ";;; Block %d:\n", b);
        fprintf(out, ";;;   GEN:  [");
        for (size_t j = 0; j < nv; j++)
            fprintf(out, "%d", df_get_bit(ctx->gen_set[b], (int32_t)j) ? 1 : 0);
        fprintf(out, "]\n");
        fprintf(out, ";;;   KILL: [");
        for (size_t j = 0; j < nv; j++)
            fprintf(out, "%d", df_get_bit(ctx->kill_set[b], (int32_t)j) ? 1 : 0);
        fprintf(out, "]\n");
        fprintf(out, ";;;   IN:   [");
        for (size_t j = 0; j < nv; j++)
            fprintf(out, "%d", df_get_bit(ctx->in_set[b], (int32_t)j) ? 1 : 0);
        fprintf(out, "]\n");
        fprintf(out, ";;;   OUT:  [");
        for (size_t j = 0; j < nv; j++)
            fprintf(out, "%d", df_get_bit(ctx->out_set[b], (int32_t)j) ? 1 : 0);
        fprintf(out, "]\n");
    }
}
