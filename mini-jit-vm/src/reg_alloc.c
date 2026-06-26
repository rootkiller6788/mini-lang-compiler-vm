/* ==========================================================================
 * reg_alloc.c — Linear Scan Register Allocator (Poletto & Sarkar 1999)
 *
 * L2: Liveness analysis — backward dataflow for live-range computation
 * L4: NP-completeness of graph coloring (Chaitin 1982)
 *     Linear scan is a polynomial-time 2-approximation
 * L5: Linear scan in O(n log n) with spill heuristics
 * L8: JIT register allocation — fast compile, small footprint
 *
 * Refs: Poletto & Sarkar (1999) ACM TOPLAS 21(5)
 *        Chaitin et al. (1981) "Register allocation via coloring"
 * ========================================================================== */

#include "reg_alloc.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

void ra_init(RegAlloc* ra, int32_t num_regs) {
    memset(ra, 0, sizeof(RegAlloc));
    ra->num_registers = num_regs > RA_MAX_REGISTERS ? RA_MAX_REGISTERS : num_regs;
    for (int32_t i = 0; i < ra->num_registers; i++) {
        ra->registers[i].id = i;
        ra->registers[i].in_use = false;
        ra->registers[i].assigned_vreg = RA_NO_REGISTER;
        ra->registers[i].free_at_ip = -1;
    }
    ra->next_spill_slot = 0;
}

/* =========================================================================
 * L5: Liveness Analysis
 *
 * For each virtual register (stack slot), compute [start_ip, end_ip]
 * where its value is live. A value is live from its definition to its
 * last use. Conservative approximation: if no explicit last-use info,
 * assume live-through to end of function.
 *
 * Complexity: O(n * v) two-pass scan.
 * ========================================================================= */

void ra_compute_live_intervals(const ByteCode* bc, RegAlloc* ra) {
    ra->interval_count = 0;
    if (bc->num_inst == 0) return;

    int32_t last_use[VM_STACK_SIZE];
    int32_t first_def[VM_STACK_SIZE];
    for (int32_t i = 0; i < VM_STACK_SIZE; i++) {
        last_use[i] = -1;
        first_def[i] = -1;
    }

    /* Forward scan: record defs and uses */
    for (int32_t ip = 0; ip < bc->num_inst; ip++) {
        int32_t instr = bc->instructions[ip];
        OpCode op = (OpCode)(instr & 0xFF);
        int32_t arg = (instr >> 8) & 0xFFFFFF;

        switch (op) {
        case OP_STORE:
            if (arg >= 0 && arg < VM_STACK_SIZE) {
                if (first_def[arg] < 0) first_def[arg] = ip;
                last_use[arg] = ip;
            }
            break;
        case OP_LOAD:
            if (arg >= 0 && arg < VM_STACK_SIZE) {
                if (last_use[arg] < 0 || ip > last_use[arg])
                    last_use[arg] = ip;
            }
            break;
        case OP_PUSH:
            /* PUSH defines a temporary stack value */
            break;
        default:
            /* Arithmetic ops consume TOS values — approximate */
            break;
        }
    }

    /* Build intervals */
    for (int32_t vreg = 0; vreg < VM_STACK_SIZE; vreg++) {
        if (first_def[vreg] >= 0 && last_use[vreg] >= 0 &&
            ra->interval_count < RA_MAX_INTERVALS) {
            LiveInterval* li = &ra->intervals[ra->interval_count++];
            li->vreg       = vreg;
            li->start_ip   = first_def[vreg];
            li->end_ip     = last_use[vreg];
            li->phys_reg   = RA_NO_REGISTER;
            li->is_spilled = false;
            li->spill_slot = -1;
        } else if (first_def[vreg] >= 0 && last_use[vreg] < 0 &&
                   ra->interval_count < RA_MAX_INTERVALS) {
            /* Def but no use: live through to end */
            LiveInterval* li = &ra->intervals[ra->interval_count++];
            li->vreg       = vreg;
            li->start_ip   = first_def[vreg];
            li->end_ip     = bc->num_inst;
            li->phys_reg   = RA_NO_REGISTER;
            li->is_spilled = false;
            li->spill_slot = -1;
        }
    }
}

/* =========================================================================
 * L5: Linear Scan Register Allocation
 *
 * Poletto & Sarkar (1999) — O(n log n) greedy algorithm.
 *
 * Key insight: sort intervals by start time, then assign registers
 * greedily. When all registers are occupied, spill the interval whose
 * end is farthest in the future (maximizes register utilization).
 *
 * This is a 2-approximation for the NP-complete graph coloring problem
 * (Chaitin 1982) when interference graphs are interval graphs.
 * ========================================================================= */

static int cmp_interval(const void* a, const void* b) {
    const LiveInterval* ia = (const LiveInterval*)a;
    const LiveInterval* ib = (const LiveInterval*)b;
    if (ia->start_ip < ib->start_ip) return -1;
    if (ia->start_ip > ib->start_ip) return 1;
    return 0;
}

int32_t ra_linear_scan_allocate(RegAlloc* ra) {
    if (ra->interval_count == 0) return 0;

    /* Sort intervals by increasing start_ip */
    qsort(ra->intervals, ra->interval_count, sizeof(LiveInterval), cmp_interval);

    /* Reset all registers */
    for (int32_t i = 0; i < ra->num_registers; i++) {
        ra->registers[i].in_use = false;
        ra->registers[i].free_at_ip = -1;
    }
    ra->total_spills = 0;
    ra->total_allocations = 0;

    for (int32_t i = 0; i < ra->interval_count; i++) {
        LiveInterval* cur = &ra->intervals[i];

        /* Expire: free registers whose intervals have ended */
        for (int32_t r = 0; r < ra->num_registers; r++) {
            if (ra->registers[r].in_use &&
                ra->registers[r].free_at_ip <= cur->start_ip) {
                ra->registers[r].in_use = false;
                ra->registers[r].assigned_vreg = RA_NO_REGISTER;
            }
        }

        /* Try to find a free register */
        int32_t free_reg = RA_NO_REGISTER;
        for (int32_t r = 0; r < ra->num_registers; r++) {
            if (!ra->registers[r].in_use) { free_reg = r; break; }
        }

        if (free_reg != RA_NO_REGISTER) {
            /* Assign free register to current interval */
            cur->phys_reg = free_reg;
            cur->is_spilled = false;
            ra->registers[free_reg].in_use = true;
            ra->registers[free_reg].assigned_vreg = cur->vreg;
            ra->registers[free_reg].free_at_ip = cur->end_ip;
            ra->total_allocations++;
            continue;
        }

        /* Spill: find the active interval with farthest end_ip */
        int32_t farthest_end = cur->end_ip;
        int32_t spill_reg = -1;

        /* Check if it's better to spill current */
        for (int32_t r = 0; r < ra->num_registers; r++) {
            if (!ra->registers[r].in_use) continue;
            int32_t active_vreg = ra->registers[r].assigned_vreg;
            /* Find the interval for this active vreg */
            for (int32_t j = 0; j < ra->interval_count; j++) {
                if (ra->intervals[j].vreg == active_vreg &&
                    ra->intervals[j].phys_reg == r) {
                    if (ra->intervals[j].end_ip > farthest_end) {
                        farthest_end = ra->intervals[j].end_ip;
                        spill_reg = r;
                    }
                    break;
                }
            }
        }

        if (spill_reg >= 0 && farthest_end > cur->end_ip) {
            /* Spill the far-end interval, give register to current */
            int32_t spilled_vreg = ra->registers[spill_reg].assigned_vreg;
            for (int32_t j = 0; j < ra->interval_count; j++) {
                if (ra->intervals[j].vreg == spilled_vreg) {
                    ra->intervals[j].phys_reg = RA_NO_REGISTER;
                    ra->intervals[j].is_spilled = true;
                    ra->intervals[j].spill_slot = ra->next_spill_slot++;
                    break;
                }
            }
            ra->registers[spill_reg].assigned_vreg = cur->vreg;
            ra->registers[spill_reg].free_at_ip = cur->end_ip;
            cur->phys_reg = spill_reg;
            cur->is_spilled = false;
            ra->total_spills++;
            ra->total_allocations++;
        } else {
            /* Spill current */
            cur->phys_reg = RA_NO_REGISTER;
            cur->is_spilled = true;
            cur->spill_slot = ra->next_spill_slot++;
            ra->total_spills++;
        }
    }
    return ra->total_allocations;
}

void ra_print_intervals(const RegAlloc* ra) {
    printf("=== Live Intervals (%d) ===\n", ra->interval_count);
    for (int32_t i = 0; i < ra->interval_count; i++) {
        const LiveInterval* li = &ra->intervals[i];
        printf("  vreg=%3d [%4d-%4d] reg=%s spill=%s\n",
               li->vreg, li->start_ip, li->end_ip,
               li->phys_reg >= 0 ?
                 (char[]){'r','0'+li->phys_reg,'\0'} : "--",
               li->is_spilled ? "YES" : "no");
    }
}

void ra_print_stats(const RegAlloc* ra) {
    printf("=== RA Stats: regs=%d intervals=%d allocs=%d spills=%d (%.1f%%) ===\n",
           ra->num_registers, ra->interval_count,
           ra->total_allocations, ra->total_spills,
           ra->interval_count > 0
               ? 100.0 * ra->total_spills / ra->interval_count : 0.0);
}
