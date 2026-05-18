#define _CRT_SECURE_NO_WARNINGS
#include "dep_graph.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int dep_add_node(DepGraph *dg, const char *name) {
    if (dg->num_nodes >= DEP_MAX_NODES) return -1;

    int idx = dep_find_node(dg, name);
    if (idx >= 0) return idx;

    idx = dg->num_nodes++;
    strncpy(dg->nodes[idx].name, name, sizeof(dg->nodes[idx].name) - 1);
    dg->nodes[idx].num_deps = 0;
    dg->nodes[idx].visited = false;
    dg->nodes[idx].in_stack = false;
    dg->nodes[idx].level = 0;
    dg->nodes[idx].indegree = 0;
    dg->nodes[idx].topological_order = -1;
    dg->nodes[idx].critical_length = 0;
    return idx;
}

bool dep_add_edge(DepGraph *dg, int from_idx, int to_idx) {
    if (from_idx < 0 || from_idx >= dg->num_nodes) return false;
    if (to_idx < 0 || to_idx >= dg->num_nodes) return false;

    DepNode *node = &dg->nodes[from_idx];
    if (node->num_deps >= DEP_MAX_DEPS) return false;

    for (int i = 0; i < node->num_deps; i++) {
        if (node->deps[i] == to_idx) return true;
    }

    node->deps[node->num_deps++] = to_idx;
    dg->nodes[to_idx].indegree++;
    return true;
}

bool dep_add_edge_by_name(DepGraph *dg, const char *from, const char *to) {
    int fi = dep_add_node(dg, from);
    int ti = dep_add_node(dg, to);
    return dep_add_edge(dg, fi, ti);
}

int dep_find_node(const DepGraph *dg, const char *name) {
    for (int i = 0; i < dg->num_nodes; i++) {
        if (strcmp(dg->nodes[i].name, name) == 0) return i;
    }
    return -1;
}

void dep_reset_visited(DepGraph *dg) {
    for (int i = 0; i < dg->num_nodes; i++) {
        dg->nodes[i].visited = false;
        dg->nodes[i].in_stack = false;
    }
}

bool dep_dfs_topo(DepGraph *dg, int node_idx, int *order, int *order_len) {
    if (dg->nodes[node_idx].visited) return true;
    if (dg->nodes[node_idx].in_stack) return false;

    dg->nodes[node_idx].in_stack = true;

    DepNode *node = &dg->nodes[node_idx];
    for (int i = 0; i < node->num_deps; i++) {
        if (!dep_dfs_topo(dg, node->deps[i], order, order_len))
            return false;
    }

    dg->nodes[node_idx].in_stack = false;
    dg->nodes[node_idx].visited = true;

    if (*order_len < DEP_MAX_NODES) {
        order[*order_len] = node_idx;
        (*order_len)++;
    }
    return true;
}

bool dep_topological_sort(DepGraph *dg, int *order, int *order_len) {
    *order_len = 0;
    dep_reset_visited(dg);

    for (int i = 0; i < dg->num_nodes; i++) {
        dg->nodes[i].visited = false;
        dg->nodes[i].in_stack = false;
    }

    for (int i = 0; i < dg->num_nodes; i++) {
        if (!dg->nodes[i].visited) {
            if (!dep_dfs_topo(dg, i, order, order_len))
                return false;
        }
    }

    for (int i = 0; i < *order_len; i++) {
        dg->nodes[order[i]].topological_order = i;
    }

    return true;
}

bool dep_dfs_cycle(const DepGraph *dg, int node_idx) {
    DepGraph *mut = (DepGraph *)dg;
    if (mut->nodes[node_idx].in_stack) return true;
    if (mut->nodes[node_idx].visited) return false;

    mut->nodes[node_idx].visited = true;
    mut->nodes[node_idx].in_stack = true;

    const DepNode *node = &dg->nodes[node_idx];
    for (int i = 0; i < node->num_deps; i++) {
        if (dep_dfs_cycle(dg, node->deps[i])) return true;
    }

    mut->nodes[node_idx].in_stack = false;
    return false;
}

bool dep_detect_cycle(const DepGraph *dg) {
    DepGraph *mut = (DepGraph *)dg;
    for (int i = 0; i < dg->num_nodes; i++) {
        mut->nodes[i].visited = false;
        mut->nodes[i].in_stack = false;
    }

    for (int i = 0; i < dg->num_nodes; i++) {
        if (!dg->nodes[i].visited) {
            if (dep_dfs_cycle(dg, i)) return true;
        }
    }
    return false;
}

static void kahn_level_assign(DepGraph *dg, int *levels) {
    int indegree[DEP_MAX_NODES];
    for (int i = 0; i < dg->num_nodes; i++)
        indegree[i] = dg->nodes[i].indegree;

    int queue[DEP_MAX_NODES];
    int qhead = 0, qtail = 0;

    for (int i = 0; i < dg->num_nodes; i++) {
        if (indegree[i] == 0) {
            queue[qtail++] = i;
            levels[i] = 0;
        }
    }

    while (qhead < qtail) {
        int cur = queue[qhead++];
        DepNode *node = &dg->nodes[cur];

        for (int i = 0; i < node->num_deps; i++) {
            int dep = node->deps[i];
            indegree[dep]--;
            if (levels[dep] < levels[cur] + 1)
                levels[dep] = levels[cur] + 1;
            if (indegree[dep] == 0) {
                queue[qtail++] = dep;
            }
        }
    }
}

bool dep_parallel_schedule(const DepGraph *dg, int *schedule_order,
                           int *num_levels) {
    int levels[DEP_MAX_NODES];
    memset(levels, 0, sizeof(int) * dg->num_nodes);

    DepGraph *mut = (DepGraph *)dg;
    kahn_level_assign(mut, levels);

    *num_levels = 0;
    for (int i = 0; i < dg->num_nodes; i++) {
        if (levels[i] >= *num_levels) *num_levels = levels[i] + 1;
    }

    memset(schedule_order, 0, sizeof(int) * dg->num_nodes);
    int pos = 0;
    for (int level = 0; level < *num_levels; level++) {
        for (int i = 0; i < dg->num_nodes; i++) {
            if (levels[i] == level) {
                schedule_order[pos++] = i;
            }
        }
    }

    return true;
}

void dep_compute_critical_path(DepGraph *dg) {
    int order[DEP_MAX_NODES];
    int order_len = 0;

    if (!dep_topological_sort(dg, order, &order_len)) return;

    for (int i = 0; i < dg->num_nodes; i++)
        dg->nodes[i].critical_length = 1;

    for (int i = 0; i < order_len; i++) {
        int node_idx = order[i];
        DepNode *node = &dg->nodes[node_idx];

        for (int k = 0; k < node->num_deps; k++) {
            int dep = node->deps[k];
            int cand = dg->nodes[dep].critical_length + 1;
            if (cand > node->critical_length)
                node->critical_length = cand;
        }
    }
}

void dep_print_order(const DepGraph *dg, const int *order, int order_len) {
    printf("Topological order:\n");
    for (int i = 0; i < order_len; i++) {
        int idx = order[i];
        printf("  %d: %s (indeg=%d, cp=%d)\n",
               i + 1, dg->nodes[idx].name,
               dg->nodes[idx].indegree, dg->nodes[idx].critical_length);
    }
}

void dep_print_schedule(const DepGraph *dg, const int *schedule_order,
                        int num_levels) {
    printf("Parallel schedule (%d levels):\n", num_levels);
    (void)schedule_order;
    int levels[DEP_MAX_NODES];
    memset(levels, 0, sizeof(levels));

    DepGraph *mut = (DepGraph *)dg;
    int indegree[DEP_MAX_NODES];
    for (int i = 0; i < dg->num_nodes; i++)
        indegree[i] = dg->nodes[i].indegree;

    int queue[DEP_MAX_NODES];
    int qhead = 0, qtail = 0;
    for (int i = 0; i < dg->num_nodes; i++) {
        if (indegree[i] == 0) {
            queue[qtail++] = i;
            levels[i] = 0;
        }
    }
    while (qhead < qtail) {
        int cur = queue[qhead++];
        mut->nodes[cur].level = levels[cur];
        for (int i = 0; i < mut->nodes[cur].num_deps; i++) {
            int dep = mut->nodes[cur].deps[i];
            indegree[dep]--;
            if (levels[dep] < levels[cur] + 1)
                levels[dep] = levels[cur] + 1;
            if (indegree[dep] == 0)
                queue[qtail++] = dep;
        }
    }

    for (int level = 0; level < num_levels; level++) {
        printf("  Level %d:", level);
        for (int i = 0; i < dg->num_nodes; i++) {
            if (levels[i] == level) {
                printf(" %s", dg->nodes[i].name);
            }
        }
        printf("\n");
    }
}
