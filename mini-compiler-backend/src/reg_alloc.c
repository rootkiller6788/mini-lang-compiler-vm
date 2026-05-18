#include "reg_alloc.h"

static int compare_intervals(const void *a, const void *b) {
    const LiveInterval *ia = (const LiveInterval *)a;
    const LiveInterval *ib = (const LiveInterval *)b;
    if (ia->start != ib->start) return ia->start - ib->start;
    return ia->end - ib->end;
}

void ra_init_context(RegAllocContext *ctx, int32_t num_regs) {
    memset(ctx, 0, sizeof(RegAllocContext));
    ctx->num_regs = num_regs;
    ctx->interval_count = 0;
    ctx->next_spill_slot = 0;
    for (int32_t i = 0; i < num_regs && i < MAX_PHYSICAL_REGS; i++) {
        ctx->phys_reg_used[i] = -1;
        snprintf(ctx->phys_reg_names[i], sizeof(ctx->phys_reg_names[i]),
                 "R%d", i);
    }
}

void ra_add_interval(RegAllocContext *ctx, int32_t virt_id,
                     int32_t start, int32_t end) {
    if (ctx->interval_count >= MAX_INTERVALS) return;
    LiveInterval *li = &ctx->intervals[ctx->interval_count++];
    memset(li, 0, sizeof(LiveInterval));
    li->virt_reg_id = virt_id;
    li->start = start;
    li->end = end;
    li->num_uses = 0;
    li->assigned_reg = -1;
    li->spilled = false;
    li->spill_slot = -1;
}

void ra_add_use(RegAllocContext *ctx, int32_t virt_id, int32_t point) {
    for (int32_t i = 0; i < ctx->interval_count; i++) {
        if (ctx->intervals[i].virt_reg_id == virt_id) {
            LiveInterval *li = &ctx->intervals[i];
            if (li->num_uses < 16) {
                li->uses[li->num_uses++] = point;
            }
            return;
        }
    }
}

typedef struct {
    int32_t virt_reg_id;
    int32_t end;
    int32_t phys_reg;
} ActiveEntry;

static int find_free_reg(RegAllocContext *ctx, bool *used_regs) {
    for (int32_t i = 0; i < ctx->num_regs; i++) {
        if (!used_regs[i]) return i;
    }
    return -1;
}

static int expire_old_intervals(ActiveEntry *active, int32_t *active_count,
                                int32_t current_pos, RegAllocContext *ctx) {
    int32_t new_count = 0;
    for (int32_t i = 0; i < *active_count; i++) {
        LiveInterval *li = NULL;
        for (int32_t j = 0; j < ctx->interval_count; j++) {
            if (ctx->intervals[j].virt_reg_id == active[i].virt_reg_id) {
                li = &ctx->intervals[j];
                break;
            }
        }
        if (li && li->end >= current_pos) {
            active[new_count++] = active[i];
        }
    }
    *active_count = new_count;
    return new_count;
}

void ra_linear_scan(RegAllocContext *ctx) {
    LiveInterval *sorted = (LiveInterval *)calloc(ctx->interval_count,
                                                   sizeof(LiveInterval));
    if (!sorted) return;
    memcpy(sorted, ctx->intervals, ctx->interval_count * sizeof(LiveInterval));
    qsort(sorted, ctx->interval_count, sizeof(LiveInterval), compare_intervals);

    ActiveEntry active[MAX_INTERVALS];
    int32_t active_count = 0;

    for (int32_t i = 0; i < ctx->interval_count; i++) {
        LiveInterval *cur_sorted = &sorted[i];
        LiveInterval *cur = NULL;
        for (int32_t j = 0; j < ctx->interval_count; j++) {
            if (ctx->intervals[j].virt_reg_id == cur_sorted->virt_reg_id) {
                cur = &ctx->intervals[j];
                break;
            }
        }
        if (!cur) continue;

        expire_old_intervals(active, &active_count, cur->start, ctx);

        if (active_count < ctx->num_regs) {
            bool used[MAX_PHYSICAL_REGS] = {0};
            for (int32_t j = 0; j < active_count; j++) {
                used[active[j].phys_reg] = true;
            }
            int32_t reg = find_free_reg(ctx, used);
            cur->assigned_reg = (reg >= 0) ? reg : 0;
            cur->spilled = false;
            active[active_count].virt_reg_id = cur->virt_reg_id;
            active[active_count].end = cur->end;
            active[active_count].phys_reg = cur->assigned_reg;
            active_count++;
        } else {
            int32_t furthest_idx = -1;
            int32_t furthest_end = -1;
            for (int32_t j = 0; j < active_count; j++) {
                LiveInterval *ali = NULL;
                for (int32_t k = 0; k < ctx->interval_count; k++) {
                    if (ctx->intervals[k].virt_reg_id == active[j].virt_reg_id) {
                        ali = &ctx->intervals[k];
                        break;
                    }
                }
                if (ali && ali->end > furthest_end) {
                    furthest_end = ali->end;
                    furthest_idx = j;
                }
            }

            if (furthest_idx >= 0 && furthest_end > cur->end) {
                LiveInterval *spill_li = NULL;
                for (int32_t k = 0; k < ctx->interval_count; k++) {
                    if (ctx->intervals[k].virt_reg_id == active[furthest_idx].virt_reg_id) {
                        spill_li = &ctx->intervals[k];
                        break;
                    }
                }
                if (spill_li) {
                    spill_li->spilled = true;
                    spill_li->spill_slot = ctx->next_spill_slot++;
                    spill_li->assigned_reg = -1;
                }
                cur->assigned_reg = active[furthest_idx].phys_reg;
                cur->spilled = false;
                active[furthest_idx].virt_reg_id = cur->virt_reg_id;
                active[furthest_idx].end = cur->end;
            } else {
                cur->spilled = true;
                cur->spill_slot = ctx->next_spill_slot++;
                cur->assigned_reg = -1;
            }
        }
    }

    free(sorted);
}

static void build_interference(int32_t num_nodes, bool **ig, RegAllocContext *ctx) {
    for (int32_t i = 0; i < num_nodes; i++) {
        for (int32_t j = i + 1; j < num_nodes; j++) {
            LiveInterval *a = &ctx->intervals[i];
            LiveInterval *b = &ctx->intervals[j];
            if (!(a->end <= b->start || b->end <= a->start)) {
                ig[i][j] = true;
                ig[j][i] = true;
            }
        }
    }
}

static int32_t find_node_to_spill(int32_t num_nodes, bool **ig,
                                   int32_t *degree, bool *removed) {
    (void)ig;
    int32_t best = -1;
    int32_t best_deg = 0;
    for (int32_t i = 0; i < num_nodes; i++) {
        if (!removed[i] && degree[i] > best_deg) {
            best_deg = degree[i];
            best = i;
        }
    }
    return best;
}

void ra_graph_coloring(RegAllocContext *ctx) {
    int32_t n = ctx->interval_count;
    if (n == 0) return;

    bool **ig = (bool **)calloc(n, sizeof(bool *));
    int32_t *degree = (int32_t *)calloc(n, sizeof(int32_t));
    bool *removed = (bool *)calloc(n, sizeof(bool));
    int32_t *stack = (int32_t *)calloc(n, sizeof(int32_t));
    int32_t stack_top = 0;
    if (!ig || !degree || !removed || !stack) {
        free(ig); free(degree); free(removed); free(stack);
        return;
    }
    for (int32_t i = 0; i < n; i++) {
        ig[i] = (bool *)calloc(n, sizeof(bool));
        if (!ig[i]) {
            for (int32_t j = 0; j < i; j++) free(ig[j]);
            free(ig); free(degree); free(removed); free(stack);
            return;
        }
    }

    build_interference(n, ig, ctx);

    for (int32_t i = 0; i < n; i++) {
        for (int32_t j = 0; j < n; j++) {
            if (ig[i][j]) degree[i]++;
        }
    }

    bool progress = true;
    while (progress && stack_top < n) {
        progress = false;
        for (int32_t i = 0; i < n; i++) {
            if (!removed[i] && degree[i] < ctx->num_regs) {
                removed[i] = true;
                stack[stack_top++] = i;
                progress = true;
                for (int32_t j = 0; j < n; j++) {
                    if (ig[i][j] && !removed[j] && degree[j] > 0) {
                        degree[j]--;
                    }
                }
            }
        }
        if (!progress && stack_top < n) {
            int32_t spill = find_node_to_spill(n, ig, degree, removed);
            if (spill >= 0) {
                ctx->intervals[spill].spilled = true;
                ctx->intervals[spill].spill_slot = ctx->next_spill_slot++;
                ctx->intervals[spill].assigned_reg = -1;
                removed[spill] = true;
                stack[stack_top++] = spill;
                progress = true;
                for (int32_t j = 0; j < n; j++) {
                    if (ig[spill][j] && !removed[j] && degree[j] > 0) {
                        degree[j]--;
                    }
                }
            }
        }
    }

    for (int32_t i = 0; i < n; i++) {
        ctx->intervals[i].assigned_reg = -1;
        ctx->intervals[i].spilled = false;
        ctx->intervals[i].spill_slot = -1;
    }

    for (int32_t si = stack_top - 1; si >= 0; si--) {
        int32_t node = stack[si];
        if (ctx->intervals[node].spilled) continue;

        bool used[MAX_PHYSICAL_REGS] = {0};
        for (int32_t j = 0; j < n; j++) {
            if (ig[node][j] && ctx->intervals[j].assigned_reg >= 0) {
                int32_t cr = ctx->intervals[j].assigned_reg;
                if (cr < ctx->num_regs) used[cr] = true;
            }
        }
        int32_t reg = find_free_reg(ctx, used);
        if (reg >= 0) {
            ctx->intervals[node].assigned_reg = reg;
            ctx->intervals[node].spilled = false;
        } else {
            ctx->intervals[node].spilled = true;
            ctx->intervals[node].spill_slot = ctx->next_spill_slot++;
        }
    }

    for (int32_t i = 0; i < n; i++) free(ig[i]);
    free(ig);
    free(degree);
    free(removed);
    free(stack);
}

void ra_print_assignment(RegAllocContext *ctx, FILE *out) {
    if (!ctx || !out) return;
    fprintf(out, ";;; Register Allocation Results:\n");
    fprintf(out, ";;; Physical registers: %d (R0-R%d)\n", ctx->num_regs, ctx->num_regs - 1);
    fprintf(out, ";;; %-8s %-8s %-8s %-10s %s\n",
            "VirtID", "Start", "End", "PhysReg", "Status");
    fprintf(out, ";;; -------- -------- -------- ---------- ----------\n");
    for (int32_t i = 0; i < ctx->interval_count; i++) {
        LiveInterval *li = &ctx->intervals[i];
        const char *status;
        char phys_str[16];
        if (li->spilled) {
            status = "SPILLED";
            snprintf(phys_str, sizeof(phys_str), "slot%d", li->spill_slot);
        } else if (li->assigned_reg >= 0) {
            status = "OK";
            snprintf(phys_str, sizeof(phys_str), "%s", ctx->phys_reg_names[li->assigned_reg]);
        } else {
            status = "NONE";
            snprintf(phys_str, sizeof(phys_str), "---");
        }
        fprintf(out, ";;; %-8d %-8d %-8d %-10s %s\n",
                li->virt_reg_id, li->start, li->end, phys_str, status);
    }
}

int32_t ra_get_assignment(RegAllocContext *ctx, int32_t virt_id) {
    for (int32_t i = 0; i < ctx->interval_count; i++) {
        if (ctx->intervals[i].virt_reg_id == virt_id) {
            return ctx->intervals[i].assigned_reg;
        }
    }
    return -1;
}

bool ra_is_spilled(RegAllocContext *ctx, int32_t virt_id) {
    for (int32_t i = 0; i < ctx->interval_count; i++) {
        if (ctx->intervals[i].virt_reg_id == virt_id) {
            return ctx->intervals[i].spilled;
        }
    }
    return false;
}

int32_t ra_get_spill_slot(RegAllocContext *ctx, int32_t virt_id) {
    for (int32_t i = 0; i < ctx->interval_count; i++) {
        if (ctx->intervals[i].virt_reg_id == virt_id) {
            return ctx->intervals[i].spill_slot;
        }
    }
    return -1;
}
