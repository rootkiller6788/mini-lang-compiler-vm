#ifndef CFG_H
#define CFG_H

#include <stdbool.h>
#include <stdio.h>
#include "ir.h"

typedef struct {
    int bb_id;
    int inst_indices[MAX_INSTRUCTIONS];
    int num_inst;
    int pred[4];
    int num_pred;
    int succ[4];
    int num_succ;
    bool visited;
} CFGNode;

typedef struct {
    CFGNode nodes[MAX_BLOCKS];
    int num_nodes;
    int entry;
    int exit;
} CFG;

void cfg_build(const IRFunction* func, CFG* cfg);
void cfg_print_graph(const CFG* cfg, FILE* out);
void cfg_reverse_postorder(const CFG* cfg, int order[MAX_BLOCKS], int* num_order);
void cfg_dominators(const CFG* cfg, int doms[MAX_BLOCKS][MAX_BLOCKS]);
void cfg_find_loops(const CFG* cfg, int loop_headers[MAX_BLOCKS],
                    int back_edges[MAX_BLOCKS][2], int* num_back_edges);

#endif
