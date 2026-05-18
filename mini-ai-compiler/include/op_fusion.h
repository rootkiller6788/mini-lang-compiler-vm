#ifndef OP_FUSION_H
#define OP_FUSION_H

#include <stdbool.h>
#include "graph_ir.h"

#define FUSION_MAX_PATTERN_OPS  8
#define FUSION_MAX_PATTERNS     16
#define FUSION_MAX_MATCHES      32

typedef struct {
    GraphOpType sequence[FUSION_MAX_PATTERN_OPS];
    int op_count;
    GraphOpType fused_op;
    char name[GRAPH_MAX_STR_LEN];
    double memory_savings_ratio;
} FusionPattern;

typedef struct {
    int matched_nodes[FUSION_MAX_PATTERN_OPS];
    int match_count;
    int pattern_idx;
    int start_node_id;
} FusionMatch;

typedef struct {
    FusionPattern patterns[FUSION_MAX_PATTERNS];
    int pattern_count;
    FusionMatch matches[FUSION_MAX_MATCHES];
    int match_count;
    ComputeGraph *graph;
} FusionContext;

FusionPattern fusion_pattern_conv_bn_relu(void);
FusionPattern fusion_pattern_matmul_bias_relu(void);
FusionPattern fusion_pattern_elemwise_chain(void);

FusionContext fusion_context_create(ComputeGraph *g);
void fusion_register_pattern(FusionContext *ctx, FusionPattern pattern);

int fusion_find_patterns(FusionContext *ctx);
bool fusion_match_pattern_at(ComputeGraph *g, int start_id, FusionPattern *pattern,
                             int matched_out[FUSION_MAX_PATTERN_OPS]);
bool fusion_replace_with_fused(FusionContext *ctx, FusionMatch *match);
int fusion_apply_greedy(ComputeGraph *g);

double fusion_estimate_savings(FusionMatch *match, ComputeGraph *g);
double fusion_memory_bandwidth_reduction(int original_count, int fused_count);

void fusion_context_print(FusionContext *ctx);
void fusion_print_pattern(FusionPattern *p);
void fusion_print_match(FusionMatch *m, ComputeGraph *g);

#endif
