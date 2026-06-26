#ifndef DEP_GRAPH_H
#define DEP_GRAPH_H

#include <stdbool.h>

#define DEP_MAX_DEPS   64
#define DEP_MAX_NODES 256

typedef struct {
    char  name[128];
    int   deps[DEP_MAX_DEPS];
    int   num_deps;
    bool  visited;
    bool  in_stack;
    int   level;
    int   indegree;
    int   topological_order;
    int   critical_length;
} DepNode;

typedef struct {
    DepNode nodes[DEP_MAX_NODES];
    int     num_nodes;
} DepGraph;

int  dep_add_node(DepGraph *dg, const char *name);
bool dep_add_edge(DepGraph *dg, int from_idx, int to_idx);
bool dep_add_edge_by_name(DepGraph *dg, const char *from, const char *to);
bool dep_topological_sort(DepGraph *dg, int *order, int *order_len);
bool dep_detect_cycle(const DepGraph *dg);
bool dep_parallel_schedule(const DepGraph *dg, int *schedule_order, int *num_levels);
void dep_print_order(const DepGraph *dg, const int *order, int order_len);
void dep_print_schedule(const DepGraph *dg, const int *schedule_order, int num_levels);
void dep_compute_critical_path(DepGraph *dg);
int  dep_find_node(const DepGraph *dg, const char *name);
void dep_reset_visited(DepGraph *dg);
bool dep_dfs_topo(DepGraph *dg, int node_idx, int *order, int *order_len);
bool dep_dfs_cycle(const DepGraph *dg, int node_idx);


/* L5: Tarjan's SCC Algorithm */
int  dep_find_sccs(DepGraph *dg, int scc_map[DEP_MAX_NODES]);
void dep_print_sccs(const DepGraph *dg, const int *scc_map, int num_sccs);

/* L8: Dominator Tree */
void dep_compute_dominators(DepGraph *dg, int dominators[DEP_MAX_NODES]);
void dep_print_dominators(const DepGraph *dg, const int *dominators);

#endif
