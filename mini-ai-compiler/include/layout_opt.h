#ifndef LAYOUT_OPT_H
#define LAYOUT_OPT_H

#include <stdbool.h>
#include "graph_ir.h"

#define LAYOUT_MAX_TRANSFORMERS 16

typedef enum {
    Layout_NCHW,
    Layout_NHWC,
    Layout_CHWN,
    Layout_NHWCc,
    Layout_NCHWc,
    Layout_COUNT
} DataLayout;

typedef struct {
    GraphOpType op_type;
    DataLayout preferred_cpu;
    DataLayout preferred_gpu;
    double flops_per_byte;
} LayoutPreference;

typedef struct {
    DataLayout input_layout;
    DataLayout output_layout;
    int node_id;
} LayoutTransform;

typedef struct {
    LayoutPreference preferences[GOp_COUNT];
    int pref_count;
    LayoutTransform transforms[LAYOUT_MAX_TRANSFORMERS];
    int transform_count;
    bool target_gpu;
    double total_cost;
} LayoutOptimizer;

const char *data_layout_name(DataLayout layout);
DataLayout data_layout_from_string(const char *s);

LayoutPreference layout_pref_create(GraphOpType op, DataLayout cpu_pref,
                                    DataLayout gpu_pref, double fpb);
LayoutOptimizer layout_optimizer_create(bool target_gpu);
void layout_add_preference(LayoutOptimizer *opt, LayoutPreference pref);

void layout_analyze_graph(LayoutOptimizer *opt, ComputeGraph *g);
DataLayout layout_get_preferred(DataLayout current, GraphOpType op_type, bool gpu);
void layout_insert_transpose(LayoutOptimizer *opt, ComputeGraph *g,
                             int node_id, DataLayout from_layout,
                             DataLayout to_layout);

void layout_optimize(LayoutOptimizer *opt, ComputeGraph *g);
double layout_cost_estimate(DataLayout producer, DataLayout consumer,
                            GraphOpType op_type, bool gpu);
DataLayout layout_propagate(DataLayout input, GraphOpType op, bool gpu);

void layout_print_plan(LayoutOptimizer *opt);
void layout_print_preferences(LayoutOptimizer *opt);

#endif
