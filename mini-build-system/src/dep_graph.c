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

/* ========================================================================
 * L5: Tarjan's Strongly Connected Components (SCC) Algorithm
 *
 * Tarjan, R.E. "Depth-First Search and Linear Graph Algorithms"
 * SIAM Journal on Computing, Vol.1 No.2, 1972.
 *
 * O(V+E) algorithm to find all maximal strongly connected subgraphs.
 * In build systems: identifies irreducible circular dependencies.
 *
 * Course: MIT 6.006, CMU 15-451
 * ======================================================================== */

typedef struct {
    int  index;
    int  lowlink;
    bool on_stack;
} TarjanState;

static void tarjan_dfs(DepGraph *dg, int v_idx, int *index_counter,
                        int *stack, int *stack_top,
                        TarjanState *state,
                        int scc_map[DEP_MAX_NODES],
                        int *scc_id) {
    state[v_idx].index = *index_counter;
    state[v_idx].lowlink = *index_counter;
    (*index_counter)++;
    stack[(*stack_top)++] = v_idx;
    state[v_idx].on_stack = true;

    DepNode *node = &dg->nodes[v_idx];
    for (int i = 0; i < node->num_deps; i++) {
        int w_idx = node->deps[i];
        if (state[w_idx].index < 0) {
            tarjan_dfs(dg, w_idx, index_counter, stack, stack_top,
                       state, scc_map, scc_id);
            if (state[w_idx].lowlink < state[v_idx].lowlink)
                state[v_idx].lowlink = state[w_idx].lowlink;
        } else if (state[w_idx].on_stack) {
            if (state[w_idx].index < state[v_idx].lowlink)
                state[v_idx].lowlink = state[w_idx].index;
        }
    }

    if (state[v_idx].lowlink == state[v_idx].index) {
        int w;
        do {
            w = stack[--(*stack_top)];
            state[w].on_stack = false;
            scc_map[w] = *scc_id;
        } while (w != v_idx);
        (*scc_id)++;
    }
}

int dep_find_sccs(DepGraph *dg, int scc_map[DEP_MAX_NODES]) {
    TarjanState state[DEP_MAX_NODES];
    for (int i = 0; i < dg->num_nodes; i++) {
        state[i].index = -1;
        state[i].lowlink = -1;
        state[i].on_stack = false;
    }

    int stack[DEP_MAX_NODES];
    int stack_top = 0;
    int index_counter = 0;
    int scc_id = 0;

    for (int i = 0; i < dg->num_nodes; i++) {
        if (state[i].index < 0) {
            tarjan_dfs(dg, i, &index_counter, stack, &stack_top,
                       state, scc_map, &scc_id);
        }
    }
    return scc_id;
}

void dep_print_sccs(const DepGraph *dg, const int *scc_map, int num_sccs) {
    printf("\n=== Strongly Connected Components (%d) ===\n", num_sccs);
    for (int s = 0; s < num_sccs; s++) {
        printf("  SCC %d: [", s);
        bool first = true;
        for (int i = 0; i < dg->num_nodes; i++) {
            if (scc_map[i] == s) {
                printf("%s%s", first ? "" : ", ", dg->nodes[i].name);
                first = false;
            }
        }
        printf("]\n");
    }
    printf("==========================================\n");
}

/* ========================================================================
 * L8: Dominator Tree Construction (Cooper-Harvey-Kennedy, 2001)
 *
 * "A Simple, Fast Dominance Algorithm" - SPE 2001
 *
 * In a DAG, node d dominates node v if every path from root to v
 * passes through d. The dominator tree reveals which build steps
 * force downstream rebuilds - essential for change impact analysis.
 * ======================================================================== */

void dep_compute_dominators(DepGraph *dg, int dominators[DEP_MAX_NODES]) {
    int n = dg->num_nodes;
    if (n == 0) return;

    for (int i = 0; i < n; i++)
        dominators[i] = (i == 0) ? 0 : -1;

    int predecessors[DEP_MAX_NODES][DEP_MAX_NODES];
    int num_pred[DEP_MAX_NODES];
    memset(num_pred, 0, sizeof(int) * n);

    for (int i = 0; i < n; i++) {
        for (int j = 0; j < dg->nodes[i].num_deps; j++) {
            int succ = dg->nodes[i].deps[j];
            if (num_pred[succ] < DEP_MAX_NODES)
                predecessors[succ][num_pred[succ]++] = i;
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (int v = 1; v < n; v++) {
            if (num_pred[v] == 0) continue;
            int new_idom = -1;
            for (int p = 0; p < num_pred[v]; p++) {
                int pred = predecessors[v][p];
                if (dominators[pred] != -1) {
                    if (new_idom < 0) {
                        new_idom = pred;
                    } else {
                        int a = new_idom, b = pred;
                        bool visited_a[DEP_MAX_NODES] = {false};
                        while (a >= 0 && a < n) {
                            visited_a[a] = true;
                            if (dominators[a] == a || dominators[a] < 0) break;
                            a = dominators[a];
                        }
                        while (b >= 0 && b < n && !visited_a[b]) {
                            if (dominators[b] == b || dominators[b] < 0) { b = 0; break; }
                            b = dominators[b];
                        }
                        if (b >= 0 && b < n && visited_a[b]) new_idom = b;
                    }
                }
            }
            if (new_idom >= 0 && dominators[v] != new_idom) {
                dominators[v] = new_idom;
                changed = true;
            }
        }
    }
}

void dep_print_dominators(const DepGraph *dg, const int *dominators) {
    printf("\n=== Dominator Tree ===\n");
    for (int i = 0; i < dg->num_nodes; i++) {
        const char *dom_name = (dominators[i] >= 0 && dominators[i] < dg->num_nodes)
                               ? dg->nodes[dominators[i]].name : "?";
        printf("  idom(%s) = %s\n", dg->nodes[i].name, dom_name);
    }
    printf("========================\n");
}
