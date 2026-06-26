#include "cfg.h"

void cfg_init(CFG *cfg) {
    memset(cfg, 0, sizeof(CFG));
    cfg->node_capacity = 16;
    cfg->nodes = (CFGNode *)calloc(cfg->node_capacity, sizeof(CFGNode));
    cfg->edge_capacity = 64;
    cfg->edges = (CFGEdge *)calloc(cfg->edge_capacity, sizeof(CFGEdge));
    cfg->entry_id = -1;
    cfg->exit_id = -1;
}

void cfg_free(CFG *cfg) {
    if (!cfg) return;
    for (int32_t i = 0; i < cfg->node_count; i++) {
        free(cfg->nodes[i].pred);
        free(cfg->nodes[i].succ);
    }
    free(cfg->nodes);
    free(cfg->edges);
    memset(cfg, 0, sizeof(CFG));
}

static void node_add_edge(CFGNode *n, int32_t target, int32_t *count, int32_t *cap, int32_t **arr) {
    (void)n;
    for (int32_t i = 0; i < *count; i++)
        if ((*arr)[i] == target) return;
    if (*count >= *cap) {
        *cap = (*cap == 0) ? 4 : (*cap * 2);
        *arr = (int32_t *)realloc(*arr, (*cap) * sizeof(int32_t));
    }
    (*arr)[(*count)++] = target;
}

int32_t cfg_add_node(CFG *cfg, const char *label) {
    if (cfg->node_count >= cfg->node_capacity) {
        cfg->node_capacity *= 2;
        cfg->nodes = (CFGNode *)realloc(cfg->nodes,
            cfg->node_capacity * sizeof(CFGNode));
        memset(cfg->nodes + cfg->node_count, 0,
               (cfg->node_capacity - cfg->node_count) * sizeof(CFGNode));
    }
    CFGNode *n = &cfg->nodes[cfg->node_count];
    memset(n, 0, sizeof(CFGNode));
    n->id = cfg->node_count;
    if (label) snprintf(n->label, sizeof(n->label), "%s", label);
    n->dfs_num = -1;
    n->low_link = -1;
    n->scc_id = -1;
    n->loop_depth = 0;
    return cfg->node_count++;
}

void cfg_add_edge(CFG *cfg, int32_t from, int32_t to, EdgeType type) {
    if (from < 0 || from >= cfg->node_count || to < 0 || to >= cfg->node_count)
        return;
    if (cfg->edge_count >= cfg->edge_capacity) {
        cfg->edge_capacity *= 2;
        cfg->edges = (CFGEdge *)realloc(cfg->edges,
            cfg->edge_capacity * sizeof(CFGEdge));
    }
    CFGEdge *e = &cfg->edges[cfg->edge_count++];
    e->from = from;
    e->to = to;
    e->type = type;
    node_add_edge(&cfg->nodes[from], to, &cfg->nodes[from].succ_count,
                  &cfg->nodes[from].succ_cap, &cfg->nodes[from].succ);
    node_add_edge(&cfg->nodes[to], from, &cfg->nodes[to].pred_count,
                  &cfg->nodes[to].pred_cap, &cfg->nodes[to].pred);
}

void cfg_set_entry(CFG *cfg, int32_t node_id) {
    if (node_id >= 0 && node_id < cfg->node_count) {
        cfg->entry_id = node_id;
        cfg->nodes[node_id].is_entry = true;
    }
}

void cfg_set_exit(CFG *cfg, int32_t node_id) {
    if (node_id >= 0 && node_id < cfg->node_count) {
        cfg->exit_id = node_id;
        cfg->nodes[node_id].is_exit = true;
    }
}

static int32_t dfs_time;
static bool *dfs_visited;

static void dfs_walk(CFG *cfg, int32_t node, TraversalOrder *order) {
    if (!cfg || node < 0 || node >= cfg->node_count) return;
    if (dfs_visited[node]) return;
    dfs_visited[node] = true;
    CFGNode *n = &cfg->nodes[node];
    n->dfs_num = dfs_time++;
    for (int32_t i = 0; i < n->succ_count; i++) {
        dfs_walk(cfg, n->succ[i], order);
    }
    if (order && order->count < cfg->node_count) {
        order->order[order->count++] = node;
        order->is_postorder = true;
    }
}

void cfg_compute_dfs(CFG *cfg, int32_t start, TraversalOrder *order) {
    if (!cfg || cfg->node_count == 0) return;
    if (order) {
        order->cfg = cfg;
        order->count = 0;
        order->is_postorder = true;
        free(order->order);
        order->order = NULL;
        order->order = (int32_t *)calloc((size_t)cfg->node_count, sizeof(int32_t));
    }
    dfs_visited = (bool *)calloc((size_t)cfg->node_count, sizeof(bool));
    dfs_time = 0;
    if (start >= 0) dfs_walk(cfg, start, order);
    for (int32_t i = 0; i < cfg->node_count; i++) {
        if (!dfs_visited[i]) dfs_walk(cfg, i, order);
    }
    free(dfs_visited);
}

void cfg_compute_reverse_postorder(CFG *cfg, TraversalOrder *order) {
    cfg_compute_dfs(cfg, cfg->entry_id, order);
    for (int32_t i = 0; i < order->count / 2; i++) {
        int32_t tmp = order->order[i];
        order->order[i] = order->order[order->count - 1 - i];
        order->order[order->count - 1 - i] = tmp;
    }
    order->is_postorder = false;
}

/*
 * Loop detection via backedges.
 * Edge n -> h is a backedge if h dominates n.
 * Reference: Aho, Sethi, Ullman "Compilers: Principles, Techniques, and Tools"
 *   (Dragon Book), Ch 10.4 - Natural Loops.
 */
void cfg_find_loops(CFG *cfg) {
    if (!cfg || cfg->node_count == 0) return;
    int32_t n = cfg->node_count;
    int32_t *idom = (int32_t *)calloc((size_t)n, sizeof(int32_t));
    for (int32_t i = 0; i < n; i++) idom[i] = -1;
    cfg_compute_dominators(cfg, idom);

    for (int32_t i = 0; i < n; i++) {
        CFGNode *node = &cfg->nodes[i];
        for (int32_t s = 0; s < node->succ_count; s++) {
            int32_t h = node->succ[s];
            int32_t runner = i;
            bool is_backedge = false;
            while (runner >= 0 && runner != h) {
                runner = idom[runner];
            }
            if (runner == h) is_backedge = true;
            if (is_backedge) {
                cfg->nodes[h].is_loop_header = true;
                cfg->nodes[h].loop_depth++;
            }
        }
    }

    for (int32_t i = 0; i < n; i++) {
        if (cfg->nodes[i].is_loop_header && i > 0 && idom[i] >= 0) {
            cfg->nodes[i].loop_depth += cfg->nodes[idom[i]].loop_depth;
        }
    }
    free(idom);
}

/*
 * Tarjan's SCC algorithm (1972).
 * Finds strongly connected components in O(V+E) time.
 * Used for irreducible loop detection and control dependence analysis.
 */
static int32_t scc_dfs_num;
static int32_t *scc_stack;
static int32_t scc_top;
static bool *scc_on_stack;
static int32_t scc_id_cnt;

static void scc_dfs(CFG *cfg, int32_t node) {
    cfg->nodes[node].dfs_num = scc_dfs_num;
    cfg->nodes[node].low_link = scc_dfs_num;
    scc_dfs_num++;
    scc_stack[scc_top++] = node;
    scc_on_stack[node] = true;

    CFGNode *n = &cfg->nodes[node];
    for (int32_t i = 0; i < n->succ_count; i++) {
        int32_t w = n->succ[i];
        if (cfg->nodes[w].dfs_num < 0) {
            scc_dfs(cfg, w);
            if (cfg->nodes[w].low_link < cfg->nodes[node].low_link)
                cfg->nodes[node].low_link = cfg->nodes[w].low_link;
        } else if (scc_on_stack[w]) {
            if (cfg->nodes[w].dfs_num < cfg->nodes[node].low_link)
                cfg->nodes[node].low_link = cfg->nodes[w].dfs_num;
        }
    }

    if (cfg->nodes[node].low_link == cfg->nodes[node].dfs_num) {
        int32_t w;
        do {
            w = scc_stack[--scc_top];
            scc_on_stack[w] = false;
            cfg->nodes[w].scc_id = scc_id_cnt;
        } while (w != node);
        scc_id_cnt++;
    }
}

void cfg_find_scc(CFG *cfg) {
    if (!cfg || cfg->node_count == 0) return;
    int32_t n = cfg->node_count;
    scc_stack = (int32_t *)calloc((size_t)n, sizeof(int32_t));
    scc_on_stack = (bool *)calloc((size_t)n, sizeof(bool));
    scc_dfs_num = 0;
    scc_top = 0;
    scc_id_cnt = 0;

    for (int32_t i = 0; i < n; i++) {
        cfg->nodes[i].dfs_num = -1;
        cfg->nodes[i].low_link = -1;
        cfg->nodes[i].scc_id = -1;
    }

    for (int32_t i = 0; i < n; i++) {
        if (cfg->nodes[i].dfs_num < 0) scc_dfs(cfg, i);
    }

    free(scc_stack);
    free(scc_on_stack);
}

/*
 * Iterative dominator algorithm (Cooper, Harvey, Kennedy 2001).
 *   idom[start] = start
 *   for all n != start: idom[n] = -1
 *   repeat until fixed point:
 *     for n in reverse postorder (except start):
 *       new_idom = first processed predecessor
 *       for each other predecessor p: new_idom = intersect(p, new_idom)
 *       if idom[n] != new_idom: idom[n] = new_idom; changed = true
 *
 * intersect(b1, b2): walk up dominator tree until b1 == b2
 */
static int32_t intersect_idom(const int32_t *idom, int32_t b1, int32_t b2, const int32_t *rpo) {
    while (b1 != b2) {
        while (b1 > b2) b1 = idom[b1];
        while (b2 > b1) b2 = idom[b2];
    }
    (void)rpo;
    return b1;
}

void cfg_compute_dominators(CFG *cfg, int32_t *idom) {
    int32_t n = cfg->node_count;
    if (n == 0) return;

    idom[0] = 0;
    for (int32_t i = 1; i < n; i++) idom[i] = -1;

    TraversalOrder rpo;
    memset(&rpo, 0, sizeof(rpo));
    cfg_compute_reverse_postorder(cfg, &rpo);

    bool changed = true;
    while (changed) {
        changed = false;
        for (int32_t i = 0; i < rpo.count; i++) {
            int32_t b = rpo.order[i];
            if (b == 0) continue;

            CFGNode *node = &cfg->nodes[b];
            int32_t new_idom = -1;
            for (int32_t p = 0; p < node->pred_count; p++) {
                int32_t pred = node->pred[p];
                if (idom[pred] != -1) {
                    new_idom = pred;
                    break;
                }
            }
            if (new_idom == -1) continue;

            for (int32_t p = 0; p < node->pred_count; p++) {
                int32_t pred = node->pred[p];
                if (pred == new_idom) continue;
                if (idom[pred] != -1) {
                    new_idom = intersect_idom(idom, pred, new_idom, rpo.order);
                }
            }

            if (idom[b] != new_idom) {
                idom[b] = new_idom;
                changed = true;
            }
        }
    }
    traversal_order_free(&rpo);
}

void cfg_compute_dominance_frontiers(CFG *cfg, const int32_t *idom,
                                      int32_t **frontier, int32_t *frontier_counts) {
    int32_t n = cfg->node_count;
    if (n == 0) return;

    int32_t *caps = (int32_t *)calloc((size_t)n, sizeof(int32_t));
    for (int32_t i = 0; i < n; i++) {
        frontier[i] = NULL;
        caps[i] = 8;
        frontier[i] = (int32_t *)calloc((size_t)caps[i], sizeof(int32_t));
        frontier_counts[i] = 0;
    }

    for (int32_t b = 0; b < n; b++) {
        CFGNode *node = &cfg->nodes[b];
        if (node->pred_count < 2) continue;
        for (int32_t p = 0; p < node->pred_count; p++) {
            int32_t runner = node->pred[p];
            while (runner != idom[b] && runner >= 0) {
                bool already = false;
                for (int32_t k = 0; k < frontier_counts[runner]; k++) {
                    if (frontier[runner][k] == b) { already = true; break; }
                }
                if (!already) {
                    if (frontier_counts[runner] >= caps[runner]) {
                        caps[runner] *= 2;
                        frontier[runner] = (int32_t *)realloc(
                            frontier[runner], (size_t)caps[runner] * sizeof(int32_t));
                    }
                    frontier[runner][frontier_counts[runner]++] = b;
                }
                runner = idom[runner];
            }
        }
    }
    for (int32_t i = 0; i < n; i++) free((void*)caps);
    free(caps);
}

void cfg_dump(CFG *cfg, FILE *out) {
    if (!cfg || !out) return;
    fprintf(out, ";;; CFG: %d nodes, %d edges\n", cfg->node_count, cfg->edge_count);
    fprintf(out, ";;; Entry: BB%d, Exit: BB%d\n", cfg->entry_id, cfg->exit_id);
    for (int32_t i = 0; i < cfg->node_count; i++) {
        CFGNode *n = &cfg->nodes[i];
        fprintf(out, ";;; BB%d", i);
        if (n->label[0]) fprintf(out, " [%s]", n->label);
        if (n->is_entry) fprintf(out, " (entry)");
        if (n->is_exit) fprintf(out, " (exit)");
        if (n->is_loop_header) fprintf(out, " (loop hdr, depth=%d)", n->loop_depth);
        fprintf(out, ": preds=[");
        for (int32_t j = 0; j < n->pred_count; j++)
            fprintf(out, "%s%d", j > 0 ? "," : "", n->pred[j]);
        fprintf(out, "] succs=[");
        for (int32_t j = 0; j < n->succ_count; j++)
            fprintf(out, "%s%d", j > 0 ? "," : "", n->succ[j]);
        fprintf(out, "] scc=%d\n", n->scc_id);
    }
}

void cfg_dump_dot(CFG *cfg, const char *filename) {
    if (!cfg || !filename) return;
    FILE *f = fopen(filename, "w");
    if (!f) return;
    fprintf(f, "digraph CFG {\n");
    fprintf(f, "  node [shape=record];\n");
    for (int32_t i = 0; i < cfg->node_count; i++) {
        CFGNode *n = &cfg->nodes[i];
        const char *extra = "";
        if (n->is_entry) extra = " (entry)";
        if (n->is_exit) extra = " (exit)";
        if (n->is_loop_header) extra = " (loop)";
        fprintf(f, "  BB%d [label=\"BB%d%s\"];\n", i, i, extra);
    }
    for (int32_t i = 0; i < cfg->edge_count; i++) {
        fprintf(f, "  BB%d -> BB%d [label=\"%d\"];\n",
                cfg->edges[i].from, cfg->edges[i].to, cfg->edges[i].type);
    }
    fprintf(f, "}\n");
    fclose(f);
}

void traversal_order_free(TraversalOrder *t) {
    if (!t) return;
    free(t->order);
    t->order = NULL;
    t->count = 0;
}
