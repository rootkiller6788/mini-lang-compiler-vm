#include "layout_opt.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

const char *data_layout_name(DataLayout layout)
{
    switch (layout) {
    case Layout_NCHW:  return "NCHW";
    case Layout_NHWC:  return "NHWC";
    case Layout_CHWN:  return "CHWN";
    case Layout_NHWCc: return "NHWCc";
    case Layout_NCHWc: return "NCHWc";
    default: return "unknown";
    }
}

DataLayout data_layout_from_string(const char *s)
{
    if (strcmp(s, "NCHW") == 0)  return Layout_NCHW;
    if (strcmp(s, "NHWC") == 0)  return Layout_NHWC;
    if (strcmp(s, "CHWN") == 0)  return Layout_CHWN;
    if (strcmp(s, "NHWCc") == 0) return Layout_NHWCc;
    if (strcmp(s, "NCHWc") == 0) return Layout_NCHWc;
    return Layout_NCHW;
}

LayoutPreference layout_pref_create(GraphOpType op, DataLayout cpu_pref,
                                    DataLayout gpu_pref, double fpb)
{
    LayoutPreference lp;
    lp.op_type = op;
    lp.preferred_cpu = cpu_pref;
    lp.preferred_gpu = gpu_pref;
    lp.flops_per_byte = fpb;
    return lp;
}

LayoutOptimizer layout_optimizer_create(bool target_gpu)
{
    LayoutOptimizer opt;
    memset(&opt, 0, sizeof(opt));
    opt.pref_count = 0;
    opt.transform_count = 0;
    opt.target_gpu = target_gpu;
    opt.total_cost = 0.0;

    layout_add_preference(&opt, layout_pref_create(GOp_CONV2D,
                          Layout_NCHW, Layout_NHWC, 200.0));
    layout_add_preference(&opt, layout_pref_create(GOp_RELU,
                          Layout_NHWC, Layout_NHWC, 5.0));
    layout_add_preference(&opt, layout_pref_create(GOp_BATCH_NORM,
                          Layout_NHWC, Layout_NHWC, 10.0));
    layout_add_preference(&opt, layout_pref_create(GOp_ADD,
                          Layout_NHWC, Layout_NHWC, 3.0));
    layout_add_preference(&opt, layout_pref_create(GOp_MUL,
                          Layout_NHWC, Layout_NHWC, 3.0));
    layout_add_preference(&opt, layout_pref_create(GOp_MATMUL,
                          Layout_NCHW, Layout_NHWC, 100.0));
    layout_add_preference(&opt, layout_pref_create(GOp_MAXPOOL2D,
                          Layout_NCHW, Layout_NHWC, 2.0));
    layout_add_preference(&opt, layout_pref_create(GOp_SOFTMAX,
                          Layout_NCHW, Layout_NHWC, 50.0));
    layout_add_preference(&opt, layout_pref_create(GOp_FUSED_CONV_BN_RELU,
                          Layout_NCHW, Layout_NHWC, 100.0));
    layout_add_preference(&opt, layout_pref_create(GOp_FUSED_MATMUL_BIAS_RELU,
                          Layout_NCHW, Layout_NHWC, 80.0));
    return opt;
}

void layout_add_preference(LayoutOptimizer *opt, LayoutPreference pref)
{
    if (opt->pref_count >= GOp_COUNT) return;
    opt->preferences[opt->pref_count] = pref;
    opt->pref_count++;
}

DataLayout layout_get_preferred(DataLayout current, GraphOpType op_type,
                                bool gpu)
{
    (void)current;
    switch (op_type) {
    case GOp_CONV2D:
    case GOp_FUSED_CONV_BN_RELU:
        return gpu ? Layout_NHWC : Layout_NCHW;
    case GOp_RELU:
    case GOp_BATCH_NORM:
    case GOp_ADD:
    case GOp_MUL:
    case GOp_SOFTMAX:
        return gpu ? Layout_NHWC : Layout_NCHW;
    case GOp_MATMUL:
    case GOp_FUSED_MATMUL_BIAS_RELU:
        return Layout_NCHW;
    case GOp_MAXPOOL2D:
    case GOp_AVGPOOL2D:
        return gpu ? Layout_NHWC : Layout_NCHW;
    default:
        return current;
    }
}

void layout_insert_transpose(LayoutOptimizer *opt, ComputeGraph *g,
                             int node_id, DataLayout from_layout,
                             DataLayout to_layout)
{
    int inputs[1];
    char name[GRAPH_MAX_STR_LEN];
    int id;

    if (opt->transform_count >= LAYOUT_MAX_TRANSFORMERS) return;

    inputs[0] = node_id;
    snprintf(name, GRAPH_MAX_STR_LEN, "transpose_%s_to_%s",
             data_layout_name(from_layout), data_layout_name(to_layout));

    id = graph_add_node(g, GOp_TRANSPOSE, inputs, 1, name);
    opt->transforms[opt->transform_count].input_layout = from_layout;
    opt->transforms[opt->transform_count].output_layout = to_layout;
    opt->transforms[opt->transform_count].node_id = id;
    opt->transform_count++;
}

double layout_cost_estimate(DataLayout producer, DataLayout consumer,
                            GraphOpType op_type, bool gpu)
{
    double base_cost = 1.0;
    double layout_mismatch_penalty = 0.0;
    double transpose_cost = 5.0;

    if (producer != consumer && producer != Layout_NCHW) {
        layout_mismatch_penalty += transpose_cost;
    }

    switch (op_type) {
    case GOp_CONV2D:
        base_cost = 10.0;
        if (gpu && consumer == Layout_NHWC) base_cost *= 0.6;
        if (!gpu && consumer == Layout_NCHW) base_cost *= 0.7;
        break;
    case GOp_RELU:
        base_cost = 1.0;
        break;
    case GOp_BATCH_NORM:
        base_cost = 2.0;
        break;
    case GOp_MATMUL:
        base_cost = 8.0;
        break;
    case GOp_SOFTMAX:
        base_cost = 4.0;
        break;
    default:
        base_cost = 2.0;
        break;
    }

    return base_cost + layout_mismatch_penalty;
}

DataLayout layout_propagate(DataLayout input, GraphOpType op, bool gpu)
{
    (void)input;
    if (gpu) {
        switch (op) {
        case GOp_CONV2D:
            return Layout_NHWC;
        case GOp_MATMUL:
            return Layout_NCHW;
        default:
            return Layout_NHWC;
        }
    } else {
        switch (op) {
        case GOp_CONV2D:
            return Layout_NCHW;
        case GOp_MATMUL:
            return Layout_NCHW;
        default:
            return Layout_NCHW;
        }
    }
}

void layout_analyze_graph(LayoutOptimizer *opt, ComputeGraph *g)
{
    int i;
    DataLayout current_layout = Layout_NCHW;

    printf("Layout Analysis:\n");
    printf("  Target: %s\n", opt->target_gpu ? "GPU" : "CPU");

    for (i = 0; i < g->node_count; i++) {
        GraphNode *node = &g->nodes[i];
        DataLayout preferred = layout_get_preferred(current_layout,
                                                     node->op_type,
                                                     opt->target_gpu);
        double cost = layout_cost_estimate(current_layout, preferred,
                                           node->op_type, opt->target_gpu);
        opt->total_cost += cost;

        printf("  Node[%d] %s: current=%s, preferred=%s, cost=%.2f\n",
               node->id, graph_op_type_name(node->op_type),
               data_layout_name(current_layout),
               data_layout_name(preferred), cost);

        if (current_layout != preferred) {
            layout_insert_transpose(opt, g, node->id,
                                   current_layout, preferred);
            current_layout = preferred;
        }
    }

    printf("  Total layout optimization cost: %.2f\n", opt->total_cost);
}

void layout_optimize(LayoutOptimizer *opt, ComputeGraph *g)
{
    layout_analyze_graph(opt, g);

    if (opt->transform_count > 0) {
        printf("Inserted %d transpose nodes to optimize layouts.\n",
               opt->transform_count);
    } else {
        printf("Layouts are already optimal. No transposes needed.\n");
    }
}

void layout_print_plan(LayoutOptimizer *opt)
{
    int i;
    printf("Layout Optimization Plan (GPU=%s):\n",
           opt->target_gpu ? "true" : "false");
    for (i = 0; i < opt->transform_count; i++) {
        LayoutTransform *t = &opt->transforms[i];
        printf("  Transpose node %d: %s -> %s\n",
               t->node_id, data_layout_name(t->input_layout),
               data_layout_name(t->output_layout));
    }
}

void layout_print_preferences(LayoutOptimizer *opt)
{
    int i;
    printf("Layout Preferences:\n");
    for (i = 0; i < opt->pref_count; i++) {
        LayoutPreference *lp = &opt->preferences[i];
        printf("  %s: CPU=%s GPU=%s FLOPS/byte=%.1f\n",
               graph_op_type_name(lp->op_type),
               data_layout_name(lp->preferred_cpu),
               data_layout_name(lp->preferred_gpu),
               lp->flops_per_byte);
    }
}
