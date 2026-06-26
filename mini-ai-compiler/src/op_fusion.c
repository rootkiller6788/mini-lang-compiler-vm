#include "op_fusion.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

FusionPattern fusion_pattern_conv_bn_relu(void)
{
    FusionPattern p;
    memset(&p, 0, sizeof(p));
    p.sequence[0] = GOp_CONV2D;
    p.sequence[1] = GOp_BATCH_NORM;
    p.sequence[2] = GOp_RELU;
    p.op_count = 3;
    p.fused_op = GOp_FUSED_CONV_BN_RELU;
    strncpy(p.name, "conv_bn_relu", GRAPH_MAX_STR_LEN - 1);
    p.memory_savings_ratio = 0.67;
    return p;
}

FusionPattern fusion_pattern_matmul_bias_relu(void)
{
    FusionPattern p;
    memset(&p, 0, sizeof(p));
    p.sequence[0] = GOp_MATMUL;
    p.sequence[1] = GOp_ADD;
    p.sequence[2] = GOp_RELU;
    p.op_count = 3;
    p.fused_op = GOp_FUSED_MATMUL_BIAS_RELU;
    strncpy(p.name, "matmul_bias_relu", GRAPH_MAX_STR_LEN - 1);
    p.memory_savings_ratio = 0.60;
    return p;
}

FusionPattern fusion_pattern_elemwise_chain(void)
{
    FusionPattern p;
    memset(&p, 0, sizeof(p));
    p.sequence[0] = GOp_ADD;
    p.sequence[1] = GOp_MUL;
    p.sequence[2] = GOp_ADD;
    p.op_count = 3;
    p.fused_op = GOp_FUSED_ELEMWISE_CHAIN;
    strncpy(p.name, "elemwise_chain", GRAPH_MAX_STR_LEN - 1);
    p.memory_savings_ratio = 0.66;
    return p;
}

FusionContext fusion_context_create(ComputeGraph *g)
{
    FusionContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.pattern_count = 0;
    ctx.match_count = 0;
    ctx.graph = g;
    return ctx;
}

void fusion_register_pattern(FusionContext *ctx, FusionPattern pattern)
{
    if (ctx->pattern_count >= FUSION_MAX_PATTERNS) return;
    ctx->patterns[ctx->pattern_count] = pattern;
    ctx->pattern_count++;
}

bool fusion_match_pattern_at(ComputeGraph *g, int start_id,
                             FusionPattern *pattern,
                             int matched_out[FUSION_MAX_PATTERN_OPS])
{
    int p_idx = 0;
    int node_idx;
    int i;

    (void)i;
    for (node_idx = 0; node_idx < g->node_count; node_idx++) {
        if (g->nodes[node_idx].id == start_id) {
            break;
        }
    }
    if (node_idx >= g->node_count) return false;

    while (p_idx < pattern->op_count) {
        if (node_idx >= g->node_count) return false;

        if (g->nodes[node_idx].op_type != pattern->sequence[p_idx]) {
            if (p_idx == 0) return false;
            break;
        }

        matched_out[p_idx] = g->nodes[node_idx].id;
        p_idx++;
        node_idx++;
    }

    return p_idx == pattern->op_count;
}

bool fusion_replace_with_fused(FusionContext *ctx, FusionMatch *match)
{
    ComputeGraph *g = ctx->graph;
    FusionPattern *pattern = &ctx->patterns[match->pattern_idx];
    int first_node = match->matched_nodes[0];
    int i, idx;
    GraphNode fused_node;

    memset(&fused_node, 0, sizeof(fused_node));
    fused_node.op_type = pattern->fused_op;
    strncpy(fused_node.name, pattern->name, GRAPH_MAX_STR_LEN - 1);

    for (idx = 0; idx < g->node_count; idx++) {
        if (g->nodes[idx].id == first_node) break;
    }
    if (idx >= g->node_count) return false;

    fused_node.id = first_node;
    fused_node.input_count = g->nodes[idx].input_count;
    for (i = 0; i < fused_node.input_count; i++) {
        fused_node.inputs[i] = g->nodes[idx].inputs[i];
    }

    if (g->nodes[idx].op_type == GOp_CONV2D) {
        memcpy(&fused_node.attrs, &g->nodes[idx].attrs,
               sizeof(GraphAttrs));
    }

    fused_node.output_count = 1;
    fused_node.outputs[0] = -1;

    return graph_replace_node(g, first_node, fused_node);
}

int fusion_find_patterns(FusionContext *ctx)
{
    int p, i;
    ctx->match_count = 0;

    for (p = 0; p < ctx->pattern_count; p++) {
        FusionPattern *pattern = &ctx->patterns[p];

        for (i = 0; i < ctx->graph->node_count; i++) {
            int matched[FUSION_MAX_PATTERN_OPS] = {0};

            if (fusion_match_pattern_at(ctx->graph,
                                        ctx->graph->nodes[i].id,
                                        pattern, matched)) {
                if (ctx->match_count >= FUSION_MAX_MATCHES) return ctx->match_count;

                FusionMatch *match = &ctx->matches[ctx->match_count];
                memcpy(match->matched_nodes, matched,
                       pattern->op_count * sizeof(int));
                match->match_count = pattern->op_count;
                match->pattern_idx = p;
                match->start_node_id = ctx->graph->nodes[i].id;
                ctx->match_count++;
            }
        }
    }
    return ctx->match_count;
}

int fusion_apply_greedy(ComputeGraph *g)
{
    FusionContext ctx = fusion_context_create(g);
    int fusion_count = 0;
    int prev_match_count;
    int i;

    fusion_register_pattern(&ctx, fusion_pattern_conv_bn_relu());
    fusion_register_pattern(&ctx, fusion_pattern_matmul_bias_relu());
    fusion_register_pattern(&ctx, fusion_pattern_elemwise_chain());

    do {
        prev_match_count = ctx.match_count;
        ctx.match_count = 0;
        fusion_find_patterns(&ctx);

        if (ctx.match_count > 0) {
            for (i = 0; i < ctx.match_count; i++) {
                if (fusion_replace_with_fused(&ctx, &ctx.matches[i])) {
                    fusion_count++;
                }
            }
        }
    } while (ctx.match_count > 0 && fusion_count < 100);

    (void)prev_match_count;
    return fusion_count;
}

double fusion_estimate_savings(FusionMatch *match, ComputeGraph *g)
{
    (void)g;
    double base_bandwidth = 0.0;
    int i;

    for (i = 0; i < match->match_count; i++) {
        base_bandwidth += 4.0;
    }
    return fusion_memory_bandwidth_reduction((int)base_bandwidth, (int)(base_bandwidth * 0.33));
}

double fusion_memory_bandwidth_reduction(int original_count, int fused_count)
{
    if (original_count == 0) return 0.0;
    return 1.0 - (double)fused_count / (double)original_count;
}

void fusion_context_print(FusionContext *ctx)
{
    int i;
    printf("FusionContext: %d patterns, %d matches\n",
           ctx->pattern_count, ctx->match_count);
    for (i = 0; i < ctx->pattern_count; i++) {
        fusion_print_pattern(&ctx->patterns[i]);
    }
    for (i = 0; i < ctx->match_count; i++) {
        fusion_print_match(&ctx->matches[i], ctx->graph);
    }
}

void fusion_print_pattern(FusionPattern *p)
{
    int i;
    printf("  Pattern \"%s\" -> %s (savings=%.0f%%) [",
           p->name, graph_op_type_name(p->fused_op),
           p->memory_savings_ratio * 100.0);
    for (i = 0; i < p->op_count; i++) {
        if (i > 0) printf(", ");
        printf("%s", graph_op_type_name(p->sequence[i]));
    }
    printf("]\n");
}

void fusion_print_match(FusionMatch *m, ComputeGraph *g)
{
    int i;
    (void)g;
    printf("  Match @ node %d: [", m->start_node_id);
    for (i = 0; i < m->match_count; i++) {
        if (i > 0) printf(", ");
        printf("%d", m->matched_nodes[i]);
    }
    printf("] pattern=%d\n", m->pattern_idx);
}
