#ifndef BUILD_THEOREM_H
#define BUILD_THEOREM_H

#include <stdbool.h>

/* ============================================================================
 * L4: Standards/Theorems - Build System Theory
 *
 * This module provides code-verified implementations of key theorems
 * governing parallel build systems: Amdahl's Law, Gustafson's Law,
 * makespan bounds, and deterministic build verification.
 *
 * Reference:
 *   Amdahl, G. "Validity of the Single Processor Approach to Achieving
 *   Large Scale Computing Capabilities" (AFIPS, 1967)
 *   Gustafson, J. "Reevaluating Amdahl's Law" (CACM, 1988)
 *   Graham, R.L. "Bounds on Multiprocessing Timing Anomalies" (SIAM, 1969)
 * Course: CMU 15-418 (Parallel), UC Berkeley CS 267 (HPC)
 * ========================================================================= */

#define THEOREM_MAX_SEGMENTS 64
#define THEOREM_MAX_LOG_ENTRIES 256

/* L1: CriticalPathSegment - a segment of the build critical path */
typedef struct {
    char name[128];
    int  duration_ms;
    bool is_parallelizable;
} CriticalPathSegment;

/* L1: BuildSpeedupResult - Amdahl/Gustafson analysis output */
typedef struct {
    double parallel_fraction;    /* fraction of work that is parallelizable */
    double serial_fraction;      /* fraction of work that is inherently serial */
    int    num_processors;
    double amdahl_speedup;       /* theoretical speedup (Amdahl) */
    double gustafson_speedup;    /* scaled speedup (Gustafson) */
    double efficiency;           /* speedup / processors */
    int    optimal_processors;   /* where diminishing returns start */
} BuildSpeedupResult;

/* L4: Amdahl's Law
 *
 * Speedup(P) = 1 / (S + (1-S)/P)
 * where S = serial fraction, P = number of processors.
 *
 * Corollary: As P -> infinity, Speedup -> 1/S.
 * Implication: The serial fraction imposes a hard limit on parallel speedup. */
double amdahl_speedup(double parallel_fraction, int num_processors);

/* L4: Gustafson's Law (Scaled Speedup)
 *
 * Speedup(P) = P - alpha * (P - 1)
 * where alpha = serial fraction, P = processors.
 *
 * Key insight: As problem size scales with processors, the serial
 * fraction becomes less significant. This better models real-world
 * build systems where more processors handle more source files. */
double gustafson_speedup(double parallel_fraction, int num_processors);

/* L4: Makespan Lower Bound Theorem
 *
 * For any valid schedule of tasks with precedence constraints on P processors:
 *   Makespan >= max(CPL, Sum(work_i) / P)
 * where CPL = critical path length, Sum(work_i) = total work.
 *
 * This is a fundamental lower bound - no scheduler can beat this.
 * Reference: Graham, R.L. "Bounds for Certain Multiprocessing Anomalies" */
double makespan_lower_bound(double critical_path, double total_work,
                             int num_processors);

/* L4: Graham's List Scheduling Bound
 *
 * For Graham's list scheduling (greedy, no preemption):
 *   Makespan <= (2 - 1/P) * Makespan_optimal
 * This is tight - worst-case 2x the optimal for P->infinity. */
double graham_list_bound(double optimal_makespan, int num_processors);

/* L4: Deterministic build verification.
 * Compares two build logs to verify that builds are reproducible.
 * Two builds are deterministic if they produce identical output ordering
 * and all steps complete in the same dependency-respecting order. */
bool verify_build_determinism(const char *log1, const char *log2);

/* L7: Application - Analyze build bottleneck.
 * Given critical path segments, identify which serialized portions
 * most constrain overall build parallelism. */
int  analyze_build_bottleneck(const CriticalPathSegment *segments,
                               int num_segments,
                               int bottleneck_ids[],
                               int *num_bottlenecks);

/* L4: Optimal parallelism calculation.
 * Finds the processor count where marginal speedup drops below threshold.
 * Uses the derivative of Amdahl's Law to find the knee point. */
int  compute_optimal_parallelism(double serial_fraction,
                                  double efficiency_threshold);

/* Full analysis */
void speedup_analyze(double parallel_fraction, int num_processors,
                     BuildSpeedupResult *result);
void speedup_print(const BuildSpeedupResult *result);

#endif
