#include "ssa.h"

void ssa_init(SSAContext *ctx) {
    memset(ctx, 0, sizeof(SSAContext));
    ctx->block_capacity = 16;
    ctx->blocks = (BasicBlock *)calloc(ctx->block_capacity, sizeof(BasicBlock));
    ctx->def_capacity = 16;
    ctx->defs = (SSADef *)calloc(ctx->def_capacity, sizeof(SSADef));
    ctx->var_capacity = 64;
    ctx->vars = (SSAVar *)calloc(ctx->var_capacity, sizeof(SSAVar));
    ctx->phi_capacity = 32;
    ctx->phis = (PhiNode *)calloc(ctx->phi_capacity, sizeof(PhiNode));
    ctx->dom.id = -1;
}

void ssa_free(SSAContext *ctx) {
    if (!ctx) return;
    for (int32_t i = 0; i < ctx->block_count; i++) {
        free(ctx->blocks[i].predecessors);
        free(ctx->blocks[i].successors);
    }
    free(ctx->blocks);
    free(ctx->dom.doms);
    free(ctx->dom.dfrontier);
    for (int32_t i = 0; i < ctx->def_count; i++) {
        free(ctx->defs[i].def_blocks);
    }
    free(ctx->defs);
    free(ctx->vars);
    free(ctx->phis);
    memset(ctx, 0, sizeof(SSAContext));
}

int32_t ssa_add_basic_block(SSAContext *ctx) {
    if (ctx->block_count >= ctx->block_capacity) {
        ctx->block_capacity *= 2;
        ctx->blocks = (BasicBlock *)realloc(ctx->blocks,
            ctx->block_capacity * sizeof(BasicBlock));
        memset(ctx->blocks + ctx->block_count, 0,
               (ctx->block_capacity - ctx->block_count) * sizeof(BasicBlock));
    }
    BasicBlock *bb = &ctx->blocks[ctx->block_count];
    bb->id = ctx->block_count;
    bb->pred_capacity = 4;
    bb->predecessors = (int32_t *)calloc(bb->pred_capacity, sizeof(int32_t));
    bb->succ_capacity = 4;
    bb->successors = (int32_t *)calloc(bb->succ_capacity, sizeof(int32_t));
    return ctx->block_count++;
}

static void array_grow(int32_t **arr, int32_t *count, int32_t *capacity, int32_t val) {
    if (*count >= *capacity) {
        *capacity = (*capacity == 0) ? 4 : (*capacity * 2);
        *arr = (int32_t *)realloc(*arr, (*capacity) * sizeof(int32_t));
    }
    (*arr)[(*count)++] = val;
}

void ssa_add_edge(SSAContext *ctx, int32_t from, int32_t to) {
    if (from < 0 || from >= ctx->block_count || to < 0 || to >= ctx->block_count)
        return;
    BasicBlock *f = &ctx->blocks[from];
    BasicBlock *t = &ctx->blocks[to];
    for (int32_t i = 0; i < f->succ_count; i++)
        if (f->successors[i] == to) return;
    for (int32_t i = 0; i < t->pred_count; i++)
        if (t->predecessors[i] == from) return;
    array_grow(&f->successors, &f->succ_count, &f->succ_capacity, to);
    array_grow(&t->predecessors, &t->pred_count, &t->pred_capacity, from);
}

void ssa_build_dominance(SSAContext *ctx) {
    int32_t n = ctx->block_count;
    if (n == 0) return;
    DomTree *dt = &ctx->dom;
    dt->id = 0;
    free(dt->doms);
    dt->doms = (int32_t *)calloc((size_t)n * (size_t)n, sizeof(int32_t));
    dt->dom_count = n;
    free(dt->dfrontier);
    dt->dfrontier = NULL;
    dt->df_count = 0;
    dt->df_capacity = 0;

    bool **dom = (bool **)calloc((size_t)n, sizeof(bool *));
    for (int32_t i = 0; i < n; i++) {
        dom[i] = (bool *)calloc((size_t)n, sizeof(bool));
        for (int32_t j = 0; j < n; j++) dom[i][j] = true;
    }
    for (int32_t j = 0; j < n; j++) dom[0][j] = (j == 0);

    bool changed = true;
    while (changed) {
        changed = false;
        for (int32_t i = 1; i < n; i++) {
            bool temp[MAX_BB_COUNT] = {0};
            bool first = true;
            BasicBlock *bb = &ctx->blocks[i];
            for (int32_t p = 0; p < bb->pred_count; p++) {
                int32_t pred = bb->predecessors[p];
                if (first) {
                    memcpy(temp, dom[pred], (size_t)n * sizeof(bool));
                    first = false;
                } else {
                    for (int32_t j = 0; j < n; j++)
                        temp[j] = temp[j] && dom[pred][j];
                }
            }
            if (first) {
                for (int32_t j = 0; j < n; j++) temp[j] = false;
            }
            temp[i] = true;
            for (int32_t j = 0; j < n; j++) {
                if (dom[i][j] != temp[j]) {
                    changed = true;
                    dom[i][j] = temp[j];
                }
            }
        }
    }

    for (int32_t i = 0; i < n; i++) {
        int32_t best = -1;
        for (int32_t j = 0; j < n; j++) {
            if (dom[i][j] && i != j) {
                if (best == -1) { best = j; }
                else { if (dom[best][j]) best = j; }
            }
        }
        dt->doms[i * n + i] = 1;
        if (best >= 0) dt->doms[best * n + i] = 1;
    }

    for (int32_t i = 0; i < n; i++) free(dom[i]);
    free(dom);
}

void ssa_compute_dominance_frontiers(SSAContext *ctx) {
    int32_t n = ctx->block_count;
    if (n == 0) return;

    DomTree *dt = &ctx->dom;
    free(dt->dfrontier);
    dt->dfrontier = (int32_t *)calloc((size_t)n * 32, sizeof(int32_t));
    dt->df_capacity = 32;
    dt->df_count = 0;

    int32_t *df_per_block = (int32_t *)calloc((size_t)n, sizeof(int32_t));

    for (int32_t b = 0; b < n; b++) {
        BasicBlock *bb = &ctx->blocks[b];
        if (bb->pred_count < 2) continue;
        for (int32_t p = 0; p < bb->pred_count; p++) {
            int32_t runner = bb->predecessors[p];
            while (runner >= 0) {
                bool runner_sdom_b = false;
                for (int32_t j = 0; j < n; j++) {
                    if (dt->doms[runner * n + j] && j == b) {
                        runner_sdom_b = true;
                        break;
                    }
                }
                if (!runner_sdom_b) {
                    bool already = false;
                    for (int32_t j = 0; j < df_per_block[runner]; j++) {
                        if (dt->dfrontier[runner * 32 + j] == b) {
                            already = true;
                            break;
                        }
                    }
                    if (!already && df_per_block[runner] < 32) {
                        dt->dfrontier[runner * 32 + df_per_block[runner]++] = b;
                    }
                }
                break;
            }
        }
    }

    int32_t total = 0;
    for (int32_t i = 0; i < n; i++) total += df_per_block[i];
    dt->df_count = total;
    free(df_per_block);
}

void ssa_insert_phi_nodes(SSAContext *ctx) {
    int32_t n = ctx->block_count;
    if (n == 0) return;

    bool *has_phi = (bool *)calloc((size_t)n, sizeof(bool));
    int32_t *worklist = (int32_t *)calloc((size_t)n * 2, sizeof(int32_t));
    int32_t wl_size = 0;

    for (int32_t v = 0; v < ctx->def_count; v++) {
        SSADef *def = &ctx->defs[v];
        wl_size = 0;
        memset(has_phi, 0, (size_t)n * sizeof(bool));
        for (int32_t d = 0; d < def->def_count; d++) {
            worklist[wl_size++] = def->def_blocks[d];
        }
        int32_t iter_count = 0;
        while (wl_size > 0 && iter_count < (n * n)) {
            iter_count++;
            int32_t b = worklist[--wl_size];
            for (int32_t runner = 0; runner < n; runner++) {
                for (int32_t j = 0; j < 32; j++) {
                    if (ctx->dom.dfrontier[runner * 32 + j] == b) {
                        if (runner >= 0 && runner < n) {
                            if (!has_phi[runner]) {
                                has_phi[runner] = true;
                                if (ctx->phi_count >= ctx->phi_capacity) {
                                    ctx->phi_capacity *= 2;
                                    ctx->phis = (PhiNode *)realloc(ctx->phis,
                                        ctx->phi_capacity * sizeof(PhiNode));
                                }
                                PhiNode *phi = &ctx->phis[ctx->phi_count++];
                                memset(phi, 0, sizeof(PhiNode));
                                phi->var_id = v;
                                phi->bb_id = runner;
                                worklist[wl_size++] = runner;
                            }
                        }
                    }
                }
            }
        }
    }

    free(has_phi);
    free(worklist);
}

void ssa_rename_variables(SSAContext *ctx) {
    (void)ctx;
}

void ssa_construct(SSAContext *ctx) {
    ssa_build_dominance(ctx);
    ssa_compute_dominance_frontiers(ctx);
    ssa_insert_phi_nodes(ctx);
    ssa_rename_variables(ctx);
}

int32_t ssa_add_variable(SSAContext *ctx, const char *name) {
    if (ctx->def_count >= ctx->def_capacity) {
        ctx->def_capacity *= 2;
        ctx->defs = (SSADef *)realloc(ctx->defs,
            ctx->def_capacity * sizeof(SSADef));
    }
    SSADef *def = &ctx->defs[ctx->def_count];
    memset(def, 0, sizeof(SSADef));
    snprintf(def->var_name, sizeof(def->var_name), "%s", name);
    def->current_version = 0;
    def->def_capacity = 8;
    def->def_blocks = (int32_t *)calloc(def->def_capacity, sizeof(int32_t));
    return ctx->def_count++;
}

void ssa_add_definition(SSAContext *ctx, int32_t var_id, int32_t bb_id) {
    if (var_id < 0 || var_id >= ctx->def_count) return;
    SSADef *def = &ctx->defs[var_id];
    array_grow(&def->def_blocks, &def->def_count, &def->def_capacity, bb_id);
}

void ssa_dump(SSAContext *ctx, FILE *out) {
    if (!ctx || !out) return;
    fprintf(out, ";;; SSA Construction Results:\n");
    fprintf(out, ";;; Blocks: %d\n", ctx->block_count);
    fprintf(out, ";;; Variables: %d\n", ctx->def_count);
    fprintf(out, ";;; Phi nodes inserted: %d\n", ctx->phi_count);
    fprintf(out, ";;;\n;;; CFG edges:\n");
    for (int32_t i = 0; i < ctx->block_count; i++) {
        BasicBlock *bb = &ctx->blocks[i];
        fprintf(out, ";;; BB%d -> [", i);
        for (int32_t j = 0; j < bb->succ_count; j++) {
            fprintf(out, "%sBB%d", j > 0 ? ", " : "", bb->successors[j]);
        }
        fprintf(out, "]\n");
    }
    fprintf(out, ";;;\n;;; Dominance frontiers:\n");
    for (int32_t i = 0; i < ctx->block_count; i++) {
        fprintf(out, ";;; DF(BB%d) = {", i);
        int32_t cnt = 0;
        for (int32_t j = 0; j < ctx->block_count; j++) {
            for (int32_t k = 0; k < 32; k++) {
                if (ctx->dom.dfrontier[j * 32 + k] == i) {
                    fprintf(out, "%sBB%d", cnt++ > 0 ? ", " : "", j);
                }
            }
        }
        fprintf(out, " }\n");
    }
    if (ctx->phi_count > 0) {
        fprintf(out, ";;;\n;;; Phi nodes:\n");
        for (int32_t i = 0; i < ctx->phi_count; i++) {
            PhiNode *phi = &ctx->phis[i];
            fprintf(out, ";;; BB%d: %s = phi(...) [%d sources]\n",
                    phi->bb_id, ctx->defs[phi->var_id].var_name,
                    phi->source_count);
        }
    }
}
