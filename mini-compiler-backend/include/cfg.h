#ifndef CFG_H
#define CFG_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_CFG_NODES      256
#define MAX_EDGES          1024

typedef enum {
    EDGE_FALLTHROUGH,
    EDGE_BRANCH,
    EDGE_CALL,
    EDGE_RETURN
} EdgeType;

typedef struct {
    int32_t from;
    int32_t to;
    EdgeType type;
} CFGEdge;

typedef struct {
    int32_t id;
    int32_t start_instr;
    int32_t end_instr;
    int32_t *pred;
    int32_t *succ;
    int32_t pred_count;
    int32_t succ_count;
    int32_t pred_cap;
    int32_t succ_cap;
    char label[32];
    bool is_entry;
    bool is_exit;
    bool is_loop_header;
    int32_t loop_depth;
    int32_t dfs_num;
    int32_t low_link;
    int32_t scc_id;
} CFGNode;

typedef struct {
    CFGNode *nodes;
    int32_t node_count;
    int32_t node_capacity;
    CFGEdge *edges;
    int32_t edge_count;
    int32_t edge_capacity;
    int32_t entry_id;
    int32_t exit_id;
} CFG;

typedef struct {
    CFG *cfg;
    int32_t *order;
    int32_t count;
    bool is_postorder;
} TraversalOrder;

void cfg_init(CFG *cfg);
void cfg_free(CFG *cfg);
int32_t cfg_add_node(CFG *cfg, const char *label);
void cfg_add_edge(CFG *cfg, int32_t from, int32_t to, EdgeType type);
void cfg_set_entry(CFG *cfg, int32_t node_id);
void cfg_set_exit(CFG *cfg, int32_t node_id);
void cfg_compute_dfs(CFG *cfg, int32_t start, TraversalOrder *order);
void cfg_compute_reverse_postorder(CFG *cfg, TraversalOrder *order);
void cfg_find_loops(CFG *cfg);
void cfg_find_scc(CFG *cfg);
void cfg_compute_dominators(CFG *cfg, int32_t *idom);
void cfg_compute_dominance_frontiers(CFG *cfg, const int32_t *idom,
                                      int32_t **frontier, int32_t *frontier_counts);
void cfg_dump(CFG *cfg, FILE *out);
void cfg_dump_dot(CFG *cfg, const char *filename);
void traversal_order_free(TraversalOrder *t);

#endif
