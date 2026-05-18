#include "cfg.h"
#include <stdlib.h>
#include <string.h>

void cfg_build(const IRFunction* func, CFG* cfg) {
    if (!func || !cfg) return;
    memset(cfg, 0, sizeof(CFG));

    int current_bb = 0;
    cfg->nodes[0].bb_id = 0;
    cfg->nodes[0].num_inst = 0;
    cfg->num_nodes = 1;

    for (int i = 0; i < func->num_inst; i++) {
        const IRInst* inst = &func->instructions[i];
        CFGNode* node = &cfg->nodes[current_bb];
        node->inst_indices[node->num_inst++] = i;

        bool is_term = (inst->op == IR_BR || inst->op == IR_BRCOND || inst->op == IR_RET);
        bool last_inst = (i == func->num_inst - 1);

        if (is_term || last_inst) {
            if (!last_inst || is_term) {
                if (i < func->num_inst - 1 && cfg->num_nodes < MAX_BLOCKS) {
                    current_bb = cfg->num_nodes;
                    cfg->nodes[current_bb].bb_id = current_bb;
                    cfg->nodes[current_bb].num_inst = 0;
                    cfg->num_nodes++;
                }
            }
        }
    }

    for (int i = 0; i < cfg->num_nodes; i++) {
        CFGNode* node = &cfg->nodes[i];
        if (node->num_inst == 0) continue;
        int last_idx = node->inst_indices[node->num_inst - 1];
        const IRInst* last = &func->instructions[last_idx];

        if (last->op == IR_BR) {
            int target = atoi(last->label);
            int found = -1;
            for (int j = 0; j < cfg->num_nodes; j++) {
                if (cfg->nodes[j].bb_id == target) { found = j; break; }
            }
            if (found >= 0 && node->num_succ < 4) {
                node->succ[node->num_succ] = found;
                cfg->nodes[found].pred[cfg->nodes[found].num_pred] = i;
                node->num_succ++;
                cfg->nodes[found].num_pred++;
            }
        } else if (last->op == IR_BRCOND) {
            int t1 = atoi(last->label);
            int t2 = atoi(last->src1_label);
            int found1 = -1, found2 = -1;
            for (int j = 0; j < cfg->num_nodes; j++) {
                if (cfg->nodes[j].bb_id == t1) found1 = j;
                if (cfg->nodes[j].bb_id == t2) found2 = j;
            }
            if (found1 >= 0 && node->num_succ < 4) {
                node->succ[node->num_succ++] = found1;
                cfg->nodes[found1].pred[cfg->nodes[found1].num_pred++] = i;
            }
            if (found2 >= 0 && node->num_succ < 4) {
                node->succ[node->num_succ++] = found2;
                cfg->nodes[found2].pred[cfg->nodes[found2].num_pred++] = i;
            }
        } else if (last->op != IR_RET && i + 1 < cfg->num_nodes && node->num_succ < 4) {
            node->succ[node->num_succ++] = i + 1;
            cfg->nodes[i + 1].pred[cfg->nodes[i + 1].num_pred++] = i;
        }
    }

    cfg->entry = 0;
    cfg->exit = cfg->num_nodes - 1;
}

void cfg_print_graph(const CFG* cfg, FILE* out) {
    if (!cfg || !out) return;
    fprintf(out, "digraph CFG {\n");
    fprintf(out, "  node [shape=box];\n");
    for (int i = 0; i < cfg->num_nodes; i++) {
        fprintf(out, "  BB%d [label=\"BB%d\\l", cfg->nodes[i].bb_id, cfg->nodes[i].bb_id);
        for (int j = 0; j < cfg->nodes[i].num_inst; j++) {
            fprintf(out, "%d\\l", cfg->nodes[i].inst_indices[j]);
        }
        fprintf(out, "\"];\n");
    }
    for (int i = 0; i < cfg->num_nodes; i++) {
        for (int s = 0; s < cfg->nodes[i].num_succ; s++) {
            fprintf(out, "  BB%d -> BB%d;\n", cfg->nodes[i].bb_id,
                    cfg->nodes[cfg->nodes[i].succ[s]].bb_id);
        }
    }
    fprintf(out, "}\n");
}

static void rpo_dfs(const CFG* cfg, int node, bool* visited, int* order, int* idx) {
    visited[node] = true;
    for (int s = 0; s < cfg->nodes[node].num_succ; s++) {
        int succ = cfg->nodes[node].succ[s];
        if (!visited[succ]) rpo_dfs(cfg, succ, visited, order, idx);
    }
    order[*idx] = node;
    (*idx)++;
}

void cfg_reverse_postorder(const CFG* cfg, int order[MAX_BLOCKS], int* num_order) {
    if (!cfg || !order || !num_order) return;
    bool visited[MAX_BLOCKS] = {false};
    *num_order = 0;
    for (int i = 0; i < cfg->num_nodes; i++) {
        if (!visited[i]) rpo_dfs(cfg, i, visited, order, num_order);
    }
    for (int i = 0; i < *num_order / 2; i++) {
        int tmp = order[i];
        order[i] = order[*num_order - 1 - i];
        order[*num_order - 1 - i] = tmp;
    }
}

void cfg_dominators(const CFG* cfg, int doms[MAX_BLOCKS][MAX_BLOCKS]) {
    if (!cfg || !doms) return;
    int n = cfg->num_nodes;
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) doms[i][j] = 1;
    }
    for (int j = 0; j < n; j++) doms[0][j] = (j == 0);

    bool changed = true;
    while (changed) {
        changed = false;
        for (int i = 1; i < n; i++) {
            int temp[MAX_BLOCKS];
            for (int j = 0; j < n; j++) temp[j] = 1;
            bool first = true;
            for (int p = 0; p < cfg->nodes[i].num_pred; p++) {
                int pred = cfg->nodes[i].pred[p];
                if (first) {
                    for (int j = 0; j < n; j++) temp[j] = doms[pred][j];
                    first = false;
                } else {
                    for (int j = 0; j < n; j++) temp[j] = temp[j] && doms[pred][j];
                }
            }
            temp[i] = 1;
            for (int j = 0; j < n; j++) {
                if (temp[j] != doms[i][j]) {
                    doms[i][j] = temp[j];
                    changed = true;
                }
            }
        }
    }
}

void cfg_find_loops(const CFG* cfg, int loop_headers[MAX_BLOCKS],
                    int back_edges[MAX_BLOCKS][2], int* num_back_edges) {
    if (!cfg || !loop_headers || !back_edges || !num_back_edges) return;
    *num_back_edges = 0;

    int doms[MAX_BLOCKS][MAX_BLOCKS];
    cfg_dominators(cfg, doms);

    for (int i = 0; i < cfg->num_nodes; i++) {
        for (int s = 0; s < cfg->nodes[i].num_succ; s++) {
            int succ = cfg->nodes[i].succ[s];
            if (doms[i][succ]) {
                loop_headers[succ] = 1;
                back_edges[*num_back_edges][0] = i;
                back_edges[*num_back_edges][1] = succ;
                (*num_back_edges)++;
            }
        }
    }
}
