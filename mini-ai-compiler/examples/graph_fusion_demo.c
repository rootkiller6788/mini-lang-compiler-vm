#include "graph_ir.h"
#include "op_fusion.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    ComputeGraph g;
    FusionContext ctx;
    int inputs_empty[1] = {0};
    int i;
    int conv_id, bn_id, relu_id, pool_id;

    printf("===== Operator Fusion Demo =====\n\n");

    g = graph_create();

    conv_id = graph_add_node(&g, GOp_CONV2D, inputs_empty, 0, "conv1");
    graph_node_set_kernel(&g.nodes[g.node_count - 1], 3, 3, 1, 1, 1, 1);

    int bn_inputs[] = {conv_id};
    bn_id = graph_add_node(&g, GOp_BATCH_NORM, bn_inputs, 1, "bn1");
    graph_node_set_attr_float(&g.nodes[g.node_count - 1].attrs, 1e-5);

    int relu_inputs[] = {bn_id};
    relu_id = graph_add_node(&g, GOp_RELU, relu_inputs, 1, "relu1");

    int pool_inputs[] = {relu_id};
    pool_id = graph_add_node(&g, GOp_MAXPOOL2D, pool_inputs, 1, "pool1");
    graph_node_set_kernel(&g.nodes[g.node_count - 1], 2, 2, 2, 2, 0, 0);

    graph_set_output(&g, pool_id);

    printf("Original Compute Graph:\n");
    printf("========================\n");
    graph_print(&g);
    printf("\n");

    ctx = fusion_context_create(&g);
    fusion_register_pattern(&ctx, fusion_pattern_conv_bn_relu());
    fusion_register_pattern(&ctx, fusion_pattern_matmul_bias_relu());
    fusion_register_pattern(&ctx, fusion_pattern_elemwise_chain());

    printf("Registered Fusion Patterns:\n");
    printf("===========================\n");
    for (i = 0; i < ctx.pattern_count; i++) {
        fusion_print_pattern(&ctx.patterns[i]);
    }
    printf("\n");

    printf("Pattern Matching Results:\n");
    printf("=========================\n");
    int match_count = fusion_find_patterns(&ctx);
    printf("Found %d matches:\n", match_count);
    for (i = 0; i < ctx.match_count; i++) {
        fusion_print_match(&ctx.matches[i], ctx.graph);
        double savings = fusion_estimate_savings(&ctx.matches[i], &g);
        printf("    Memory bandwidth savings: %.0f%%\n", savings * 100.0);
    }
    printf("\n");

    printf("Applying Greedy Fusion...\n");
    printf("=========================\n");
    int fused = fusion_apply_greedy(&g);
    printf("Fused %d pattern(s).\n\n", fused);

    printf("Fused Compute Graph:\n");
    printf("====================\n");
    graph_print(&g);
    printf("\n");

    printf("Fusion Summary:\n");
    printf("  Original ops: Conv2D + BatchNorm + ReLU + MaxPool2D (4 ops)\n");
    printf("  After fusion: FusedConvBNReLU + MaxPool2D (%d ops)\n",
           g.node_count);
    printf("  Estimated ~67%% memory bandwidth reduction for Conv+BN+ReLU\n");

    return 0;
}
