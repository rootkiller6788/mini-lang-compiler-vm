#include "inst_sched.h"

void sched_init(SchedDAG *dag) {
    memset(dag, 0, sizeof(SchedDAG));
    dag->node_capacity = 32;
    dag->nodes = (SchedNode *)calloc(dag->node_capacity, sizeof(SchedNode));
    dag->ready_capacity = 32;
    dag->ready_list = (int32_t *)calloc(dag->ready_capacity, sizeof(int32_t));
}

void sched_free(SchedDAG *dag) {
    if (!dag) return;
    for (int32_t i = 0; i < dag->node_count; i++) {
        free(dag->nodes[i].deps);
        free(dag->nodes[i].rev_deps);
    }
    free(dag->nodes);
    free(dag->ready_list);
    memset(dag, 0, sizeof(SchedDAG));
}

static void dag_grow_array(int32_t **arr, int32_t *count, int32_t *capacity, int32_t val) {
    if (*count >= *capacity) {
        *capacity = (*capacity == 0) ? 4 : (*capacity * 2);
        *arr = (int32_t *)realloc(*arr, (*capacity) * sizeof(int32_t));
    }
    (*arr)[(*count)++] = val;
}

int32_t sched_add_node(SchedDAG *dag, InstructionNode *instr,
                        int32_t latency, const char *resource) {
    if (dag->node_count >= dag->node_capacity) {
        dag->node_capacity *= 2;
        dag->nodes = (SchedNode *)realloc(dag->nodes,
            dag->node_capacity * sizeof(SchedNode));
        memset(dag->nodes + dag->node_count, 0,
               (dag->node_capacity - dag->node_count) * sizeof(SchedNode));
    }
    SchedNode *n = &dag->nodes[dag->node_count];
    memset(n, 0, sizeof(SchedNode));
    n->node_id = dag->node_count;
    if (instr) memcpy(&n->instr, instr, sizeof(InstructionNode));
    n->sched_info.latency = latency;
    if (resource) snprintf(n->sched_info.resource_name,
                           sizeof(n->sched_info.resource_name), "%s", resource);
    n->sched_info.issue_cycle = -1;
    n->sched_info.finish_cycle = -1;
    n->scheduled = false;
    n->sched_cycle = -1;
    return dag->node_count++;
}

void sched_add_dep(SchedDAG *dag, int32_t from, int32_t to) {
    if (from < 0 || from >= dag->node_count || to < 0 || to >= dag->node_count)
        return;
    for (int32_t i = 0; i < dag->nodes[from].dep_count; i++)
        if (dag->nodes[from].deps[i] == to) return;

    dag_grow_array(&dag->nodes[from].deps, &dag->nodes[from].dep_count,
                   &dag->nodes[from].dep_capacity, to);
    dag_grow_array(&dag->nodes[to].rev_deps, &dag->nodes[to].rev_count,
                   &dag->nodes[to].rev_capacity, from);
}

void sched_list_schedule(SchedDAG *dag, SchedResult *result) {
    int32_t n = dag->node_count;
    if (n == 0) return;

    int32_t *priorities = (int32_t *)calloc((size_t)n, sizeof(int32_t));
    for (int32_t i = 0; i < n; i++) priorities[i] = 1;
    sched_priority_schedule(dag, result, priorities);
    free(priorities);
}

void sched_priority_schedule(SchedDAG *dag, SchedResult *result,
                              int32_t *priorities) {
    int32_t n = dag->node_count;
    if (n == 0) return;

    result->count = 0;
    result->makespan = 0;
    free(result->schedule);
    result->schedule = (int32_t *)calloc((size_t)n, sizeof(int32_t));
    for (int32_t i = 0; i < n; i++) result->schedule[i] = -1;

    int32_t *ready = (int32_t *)calloc((size_t)n, sizeof(int32_t));
    int32_t ready_count = 0;
    int32_t *sched_cycle = (int32_t *)calloc((size_t)n, sizeof(int32_t));
    bool *scheduled = (bool *)calloc((size_t)n, sizeof(bool));

    int32_t *dep_remaining = (int32_t *)calloc((size_t)n, sizeof(int32_t));
    for (int32_t i = 0; i < n; i++) {
        dep_remaining[i] = dag->nodes[i].rev_count;
        if (dep_remaining[i] == 0) {
            ready[ready_count++] = i;
        }
    }

    int32_t cycle = 0;
    int32_t scheduled_count = 0;
    while (scheduled_count < n) {
        int32_t issued_this_cycle = 0;

        while (ready_count > 0 && issued_this_cycle < 1) {
            int32_t best_idx = -1;
            int32_t best_prio = -1;
            for (int32_t r = 0; r < ready_count; r++) {
                int32_t node = ready[r];
                if (scheduled[node]) continue;
                if (priorities[node] > best_prio) {
                    best_prio = priorities[node];
                    best_idx = r;
                }
            }

            if (best_idx < 0) break;

            int32_t node = ready[best_idx];
            ready[best_idx] = ready[--ready_count];

            scheduled[node] = true;
            sched_cycle[node] = cycle;
            result->schedule[scheduled_count++] = node;
            result->makespan = cycle + dag->nodes[node].sched_info.latency;
            issued_this_cycle++;

            SchedNode *sn = &dag->nodes[node];
            for (int32_t d = 0; d < sn->dep_count; d++) {
                int32_t succ = sn->deps[d];
                dep_remaining[succ]--;
                if (dep_remaining[succ] == 0 && !scheduled[succ]) {
                    ready[ready_count++] = succ;
                }
            }
        }
        cycle++;
    }

    for (int32_t i = 0; i < n; i++) {
        dag->nodes[i].sched_cycle = sched_cycle[i];
        dag->nodes[i].scheduled = scheduled[i];
    }

    free(ready);
    free(sched_cycle);
    free(scheduled);
    free(dep_remaining);
}

void sched_compute_delay_slots(SchedDAG *dag, int32_t branch_node,
                                int32_t num_slots, int32_t *fill_nodes) {
    if (!dag || branch_node < 0 || branch_node >= dag->node_count) return;

    for (int32_t s = 0; s < num_slots; s++) fill_nodes[s] = -1;

    int32_t filled = 0;
    SchedNode *br = &dag->nodes[branch_node];

    for (int32_t i = 0; i < dag->node_count && filled < num_slots; i++) {
        if (i == branch_node) continue;
        if (dag->nodes[i].scheduled) continue;

        bool has_dep = false;
        for (int32_t d = 0; d < br->rev_count; d++) {
            if (br->rev_deps[d] == i) { has_dep = true; break; }
        }
        if (has_dep) continue;

        bool dep_on_br = false;
        SchedNode *candidate = &dag->nodes[i];
        for (int32_t d = 0; d < candidate->rev_count; d++) {
            if (candidate->rev_deps[d] == branch_node) {
                dep_on_br = true;
                break;
            }
        }

        if (!dep_on_br) {
            fill_nodes[filled++] = i;
        }
    }
}

void sched_print_schedule(SchedResult *result, SchedDAG *dag, FILE *out) {
    if (!result || !dag || !out) return;
    fprintf(out, "=== Instruction Schedule (makespan=%d cycles) ===\n", result->makespan);
    int32_t max_cycle = 0;
    for (int32_t i = 0; i < result->count; i++) {
        int32_t node = result->schedule[i];
        if (node >= 0 && dag->nodes[node].sched_cycle > max_cycle)
            max_cycle = dag->nodes[node].sched_cycle;
    }

    for (int32_t c = 0; c <= max_cycle; c++) {
        fprintf(out, "Cycle %2d: ", c);
        bool found = false;
        for (int32_t i = 0; i < result->count; i++) {
            int32_t node = result->schedule[i];
            if (node >= 0 && dag->nodes[node].sched_cycle == c) {
                const char *opn = isel_op_name(dag->nodes[node].instr.op);
                fprintf(out, "%s%s", opn, found ? ", " : "");
                found = true;
            }
        }
        if (!found) fprintf(out, "---");
        fprintf(out, "\n");
    }
}

void sched_result_free(SchedResult *r) {
    if (!r) return;
    free(r->schedule);
    r->schedule = NULL;
    r->count = 0;
    r->makespan = 0;
}
