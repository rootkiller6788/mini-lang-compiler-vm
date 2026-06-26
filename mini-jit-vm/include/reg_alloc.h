#ifndef REG_ALLOC_H
#define REG_ALLOC_H

/* ==========================================================================
 * reg_alloc.h — Linear Scan Register Allocator
 *
 * L1: LiveInterval, RegAllocState structs
 * L2: Liveness analysis, interference graph
 * L3: Register allocation pipeline: liveness → intervals → assign
 * L4: NP-completeness of optimal graph-coloring allocation (Chaitin 1982)
 * L5: Linear scan algorithm (Poletto & Sarkar 1999) — O(n log n)
 * L8: Register allocation is a core JIT optimization
 *
 * Refs:
 *   - Chaitin et al. (1981) "Register allocation via coloring"
 *   - Poletto & Sarkar (1999) "Linear scan register allocation"
 *   - Wimmer & Franz (2010) "Linear scan on SSA form"
 * ========================================================================== */

#include <stdbool.h>
#include <stdint.h>
#include "bytecode.h"

#define RA_MAX_REGISTERS   16
#define RA_MAX_INTERVALS   256
#define RA_NO_REGISTER     (-1)

/* L1: Live interval for a virtual register (stack slot) */
typedef struct {
    int32_t vreg;            /* virtual register id (stack slot index) */
    int32_t start_ip;        /* first instruction where value is live */
    int32_t end_ip;          /* last instruction where value is live */
    int32_t phys_reg;        /* assigned physical register, RA_NO_REGISTER if spilled */
    bool    is_spilled;      /* true if value must live in memory */
    int32_t spill_slot;      /* spill slot index if spilled */
} LiveInterval;

/* L1: Physical register descriptor */
typedef struct {
    int32_t id;              /* register number (0..RA_MAX_REGISTERS-1) */
    bool    in_use;          /* currently holding a live value */
    int32_t assigned_vreg;   /* which vreg it currently holds */
    int32_t free_at_ip;      /* instruction index when it becomes free */
} PhysReg;

/* L3: Register allocator state */
typedef struct {
    LiveInterval intervals[RA_MAX_INTERVALS];
    int32_t       interval_count;
    PhysReg       registers[RA_MAX_REGISTERS];
    int32_t       num_registers;
    int32_t       next_spill_slot;
    /* Statistics */
    int32_t       total_spills;
    int32_t       total_allocations;
} RegAlloc;

/* --- L5: Liveness Analysis --- */

/**
 * Compute live intervals from bytecode.
 * A value (stack slot) is live from its first definition (STORE/PUSH)
 * to its last use (LOAD/arithmetic consumer).
 *
 * Complexity: O(n * v) where n = num_instructions, v = num_vregs.
 * Space: O(v) for interval storage.
 */
void     ra_compute_live_intervals(const ByteCode* bc, RegAlloc* ra);

/* --- L5: Linear Scan Allocation --- */

/**
 * Linear scan register allocation (Poletto & Sarkar 1999).
 *
 * Algorithm:
 *   1. Sort intervals by start_ip
 *   2. For each interval:
 *      a. Expire any physical registers whose end_ip < interval.start_ip
 *      b. If free register exists, assign it
 *      c. Else, spill the interval with farthest end_ip
 *   3. Generate spill code (store/load around uses)
 *
 * Complexity: O(n log n) for sort + O(n * r) for scan (r = num_regs).
 * Space: O(n + r).
 */
void     ra_init(RegAlloc* ra, int32_t num_regs);
int32_t  ra_linear_scan_allocate(RegAlloc* ra);
void     ra_print_intervals(const RegAlloc* ra);
void     ra_print_stats(const RegAlloc* ra);

#endif /* REG_ALLOC_H */
