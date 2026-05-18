#include <stdio.h>
#include <stdlib.h>

#include "reg_alloc.h"

int main(void) {
    printf("=== Register Allocation Demo - Linear Scan ===\n\n");

    RegAllocContext ctx;
    ra_init_context(&ctx, 8);

    ra_add_interval(&ctx, 0,  0,  4);
    ra_add_interval(&ctx, 1,  2,  8);
    ra_add_interval(&ctx, 2,  4,  10);
    ra_add_interval(&ctx, 3,  6,  12);
    ra_add_interval(&ctx, 4,  8,  14);
    ra_add_interval(&ctx, 5,  10, 16);
    ra_add_interval(&ctx, 6,  1,  5);
    ra_add_interval(&ctx, 7,  7,  11);
    ra_add_interval(&ctx, 8,  0,  3);
    ra_add_interval(&ctx, 9,  5,  15);

    for (int32_t i = 0; i < ctx.interval_count; i++) {
        ra_add_use(&ctx, i, ctx.intervals[i].start);
        ra_add_use(&ctx, i, (ctx.intervals[i].start + ctx.intervals[i].end) / 2);
        ra_add_use(&ctx, i, ctx.intervals[i].end);
    }

    printf("Input: 10 virtual registers, 8 physical registers (R0-R7)\n");
    printf("Live intervals:\n");
    printf("%-8s %-8s %-8s %-10s\n", "VirtID", "Start", "End", "Uses");
    for (int32_t i = 0; i < ctx.interval_count; i++) {
        printf("%-8d %-8d %-8d %-10d\n",
               ctx.intervals[i].virt_reg_id,
               ctx.intervals[i].start,
               ctx.intervals[i].end,
               ctx.intervals[i].num_uses);
    }
    printf("\n");

    ra_linear_scan(&ctx);

    ra_print_assignment(&ctx, stdout);

    printf("\n=== Assignment Summary ===\n");
    int32_t spilled_count = 0;
    for (int32_t i = 0; i < ctx.interval_count; i++) {
        if (ctx.intervals[i].spilled) spilled_count++;
    }
    printf("Registers allocated: %d\n", ctx.interval_count - spilled_count);
    printf("Registers spilled:  %d\n", spilled_count);
    printf("Spill slots used:   %d\n", ctx.next_spill_slot);

    printf("\n=== Graph Coloring Allocator ===\n\n");

    RegAllocContext ctx2;
    ra_init_context(&ctx2, 8);

    ra_add_interval(&ctx2, 0, 0, 4);
    ra_add_interval(&ctx2, 1, 2, 8);
    ra_add_interval(&ctx2, 2, 4, 10);
    ra_add_interval(&ctx2, 3, 6, 12);
    ra_add_interval(&ctx2, 4, 8, 14);
    ra_add_interval(&ctx2, 5, 10, 16);
    ra_add_interval(&ctx2, 6, 1, 5);
    ra_add_interval(&ctx2, 7, 7, 11);
    ra_add_interval(&ctx2, 8, 0, 3);
    ra_add_interval(&ctx2, 9, 5, 15);

    ra_graph_coloring(&ctx2);

    ra_print_assignment(&ctx2, stdout);

    spilled_count = 0;
    for (int32_t i = 0; i < ctx2.interval_count; i++) {
        if (ctx2.intervals[i].spilled) spilled_count++;
    }
    printf("\nGraph Coloring Summary:\n");
    printf("Registers allocated: %d\n", ctx2.interval_count - spilled_count);
    printf("Registers spilled:  %d\n", spilled_count);

    return 0;
}
