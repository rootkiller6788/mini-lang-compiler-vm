#ifndef REG_ALLOC_H
#define REG_ALLOC_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_VIRTUAL_REGS   128
#define MAX_PHYSICAL_REGS  8
#define MAX_INTERVALS      256

typedef struct {
    int32_t virt_reg_id;
    int32_t start;
    int32_t end;
    bool spilled;
    int32_t spill_slot;
} LiveRange;

typedef struct {
    int32_t virt_reg_id;
    int32_t start;
    int32_t end;
    int32_t uses[16];
    int32_t num_uses;
    int32_t assigned_reg;
    bool spilled;
    int32_t spill_slot;
} LiveInterval;

typedef struct {
    int32_t num_regs;
    LiveInterval intervals[MAX_INTERVALS];
    int32_t interval_count;
    int32_t phys_reg_used[MAX_PHYSICAL_REGS];
    char phys_reg_names[MAX_PHYSICAL_REGS][4];
    int32_t next_spill_slot;
} RegAllocContext;

void ra_linear_scan(RegAllocContext *ctx);
void ra_graph_coloring(RegAllocContext *ctx);
void ra_print_assignment(RegAllocContext *ctx, FILE *out);
void ra_init_context(RegAllocContext *ctx, int32_t num_regs);
void ra_add_interval(RegAllocContext *ctx, int32_t virt_id, int32_t start, int32_t end);
void ra_add_use(RegAllocContext *ctx, int32_t virt_id, int32_t point);
int32_t ra_get_assignment(RegAllocContext *ctx, int32_t virt_id);
bool ra_is_spilled(RegAllocContext *ctx, int32_t virt_id);
int32_t ra_get_spill_slot(RegAllocContext *ctx, int32_t virt_id);

#endif
