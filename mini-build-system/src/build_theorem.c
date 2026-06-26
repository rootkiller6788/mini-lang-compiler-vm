#define _CRT_SECURE_NO_WARNINGS
#include "build_theorem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*
 * ============================================================================
 * L4: Standards/Theorems - Build System Theory
 *
 * This module provides code-verified implementations of key theorems
 * that govern parallel build system performance:
 *
 *   1. Amdahl's Law (1967) - theoretical speedup limit
 *   2. Gustafson's Law (1988) - scaled speedup perspective
 *   3. Makespan Lower Bound (Graham, 1969)
 *   4. Graham's List Scheduling Bound
 *   5. Optimal Parallelism Calculation
 *   6. Deterministic Build Verification
 *   7. Build Bottleneck Analysis (L7: Application)
 *
 * Reference:
 *   Amdahl, G. "Validity of the Single Processor Approach to Achieving
 *     Large Scale Computing Capabilities" (AFIPS Spring Joint Computer
 *     Conference, 1967) - pp. 483-485
 *   Gustafson, J.L. "Reevaluating Amdahl's Law" (Communications of the ACM,
 *     Vol.31 No.5, 1988) - pp. 532-533
 *   Graham, R.L. "Bounds on Multiprocessing Timing Anomalies"
 *     (SIAM Journal on Applied Mathematics, Vol.17 No.2, 1969) - pp. 416-429
 *
 * Course: CMU 15-418 (Parallel Computer Architecture and Programming)
 *         UC Berkeley CS 267 (Applications of Parallel Computers)
 *         Georgia Tech CS 6290 (High Performance Computer Architecture)
 * ============================================================================
 */

/* ========================================================================
 * L4: Amdahl's Law
 *
 * Formula: Speedup(P) = 1 / (S + (1-S)/P)
 * Where:   S = serial fraction (0 <= S <= 1)
 *          P = number of processors
 *
 * Key implications:
 *   - As P -> infinity, Speedup -> 1/S
 *   - Even a 5% serial portion (S = 0.05) limits speedup to 20x MAX
 *   - For build systems, serial portions include:
 *     * Makefile parsing (cannot be parallelized)
 *     * Link step (typically single-threaded)
 *     * I/O serialization (disk/network bottlenecks)
 *
 * This is the fundamental reason why adding more CPU cores
 * eventually stops helping build times.
 *
 * Theorem: lim(P->inf) Speedup = 1/S
 * Proof:   Speedup = 1 / (S + (1-S)/P)
 *          As P -> infinity, (1-S)/P -> 0
 *          Therefore Speedup -> 1/S
 *
 * Corollary: If S > 0, perfect linear speedup is impossible.
 * ======================================================================== */

double amdahl_speedup(double parallel_fraction, int num_processors) {
    if (num_processors <= 0) return 0.0;
    if (parallel_fraction < 0.0) parallel_fraction = 0.0;
    if (parallel_fraction > 1.0) parallel_fraction = 1.0;

    double serial_fraction = 1.0 - parallel_fraction;
    if (serial_fraction <= 0.0) return (double)num_processors;

    return 1.0 / (serial_fraction + parallel_fraction / num_processors);
}

/* ========================================================================
 * L4: Gustafson's Law (Scaled Speedup)
 *
 * Formula: Speedup(P) = P - alpha * (P - 1)
 * Where:   alpha = serial fraction
 *
 * Gustafson's key insight: Amdahl assumed fixed problem size.
 * In reality, we scale the problem with more processors.
 * For build systems, more CPUs means we compile more files
 * simultaneously, and the parallel portion grows.
 *
 * This better models real-world build scaling:
 *   - With 4 cores: compile 4 files at once -> ~4x speedup
 *   - With 16 cores: compile 16 files at once -> ~16x speedup
 *   - The serial fraction (linking) stays constant while
 *     the parallel fraction (compilation) scales linearly
 *
 * Reference: Gustafson, J.L. "Reevaluating Amdahl's Law" (CACM, 1988)
 * ======================================================================== */

double gustafson_speedup(double parallel_fraction, int num_processors) {
    if (num_processors <= 0) return 0.0;
    if (parallel_fraction < 0.0) parallel_fraction = 0.0;
    if (parallel_fraction > 1.0) parallel_fraction = 1.0;

    double alpha = 1.0 - parallel_fraction;  /* serial fraction */
    return num_processors - alpha * (num_processors - 1);
}

/* ========================================================================
 * L4: Makespan Lower Bound Theorem
 *
 * For a set of tasks with precedence constraints running on P processors:
 *
 *   Makespan >= max(CPL, W_total / P)
 *
 * Where:
 *   CPL = Critical Path Length (longest dependency chain)
 *   W_total = Sum of all task durations
 *
 * Proof:
 *   1. CPL bound: No schedule can finish before the critical path
 *      completes, because each task on the critical path must wait
 *      for its predecessor. This is true regardless of processor count.
 *   2. Work bound: Even with perfect load balancing, P processors
 *      can execute at most P units of work per time unit.
 *      Total time must be at least W_total / P.
 *
 * This is a fundamental lower bound - no scheduler, however clever,
 * can produce a schedule with lower makespan than this bound.
 *
 * Reference: Graham, R.L. "Bounds for Certain Multiprocessing
 *   Anomalies" (Bell System Technical Journal, 1966)
 * ======================================================================== */

double makespan_lower_bound(double critical_path, double total_work,
                             int num_processors) {
    if (num_processors <= 0) return total_work;
    double work_bound = total_work / num_processors;
    return (critical_path > work_bound) ? critical_path : work_bound;
}

/* ========================================================================
 * L4: Graham's List Scheduling Bound
 *
 * Theorem (Graham, 1966): For any list scheduling algorithm
 * (greedy, non-preemptive), the makespan satisfies:
 *
 *   Makespan <= (2 - 1/P) * Makespan_optimal
 *
 * In the worst case (P -> infinity):
 *   Makespan <= 2 * Makespan_optimal
 *
 * This is tight: there exist task graphs where list scheduling
 * achieves exactly 2x the optimal makespan.
 *
 * For build systems, this means a simple "schedule any ready task"
 * approach is at most 2x worse than optimal. In practice,
 * with critical-path-aware scheduling (like Ninja's), the
 * actual ratio is much closer to 1.0.
 * ======================================================================== */

double graham_list_bound(double optimal_makespan, int num_processors) {
    if (num_processors <= 0) return optimal_makespan;
    return (2.0 - 1.0 / num_processors) * optimal_makespan;
}

/* ========================================================================
 * L7: Application - Analyze Build Bottleneck
 *
 * Identifies which serialized portions of the build most constrain
 * overall parallelism. Bottlenecks are segments where:
 *   1. The segment is NOT parallelizable
 *   2. The segment lies on the critical path
 *   3. The segment's duration is significant relative to total time
 *
 * This analysis guides optimization: should you parallelize
 * the link step, optimize header includes, or cache intermediates?
 *
 * Algorithm:
 *   1. Scan all segments on the critical path
 *   2. Identify non-parallelizable segments
 *   3. Rank by duration (longest first = biggest bottleneck)
 *   4. Return top bottlenecks
 * ======================================================================== */

int analyze_build_bottleneck(const CriticalPathSegment *segments,
                              int num_segments,
                              int bottleneck_ids[],
                              int *num_bottlenecks) {
    *num_bottlenecks = 0;

    /* Collect non-parallelizable segments */
    typedef struct { int idx; int duration; } Bottleneck;
    Bottleneck bneck[THEOREM_MAX_SEGMENTS];
    int bn_count = 0;

    for (int i = 0; i < num_segments; i++) {
        if (!segments[i].is_parallelizable && segments[i].duration_ms > 0) {
            bneck[bn_count].idx = i;
            bneck[bn_count].duration = segments[i].duration_ms;
            bn_count++;
        }
    }

    /* Sort bottlenecks by duration descending (simple insertion sort) */
    for (int i = 1; i < bn_count; i++) {
        Bottleneck key = bneck[i];
        int j = i - 1;
        while (j >= 0 && bneck[j].duration < key.duration) {
            bneck[j + 1] = bneck[j];
            j--;
        }
        bneck[j + 1] = key;
    }

    /* Return top bottlenecks */
    for (int i = 0; i < bn_count && i < THEOREM_MAX_SEGMENTS; i++) {
        bottleneck_ids[i] = bneck[i].idx;
        (*num_bottlenecks)++;
    }

    return bn_count;
}

/* ========================================================================
 * L4: Optimal Parallelism Calculation
 *
 * Finding the "knee" of the Amdahl curve - the point where
 * adding more processors yields diminishing returns.
 *
 * The derivative of Amdahl's Law with respect to P:
 *   dS/dP = (1-S) / (S*P + (1-S))^2
 *
 * We find the smallest P where the marginal speedup gain
 * (speedup(P) - speedup(P-1)) / speedup(P-1) < threshold.
 *
 * This gives the cost-effective processor count for a build farm.
 * ======================================================================== */

int compute_optimal_parallelism(double serial_fraction,
                                 double efficiency_threshold) {
    if (serial_fraction <= 0.0) return 64;  /* embarrassingly parallel */
    if (serial_fraction >= 1.0) return 1;    /* purely serial */

    int P = 1;
    double prev_speedup = 1.0;

    for (P = 2; P <= 256; P++) {
        double speedup = 1.0 / (serial_fraction + (1.0 - serial_fraction) / P);
        double marginal_gain = (speedup - prev_speedup) / prev_speedup;

        if (marginal_gain < efficiency_threshold) {
            return P - 1;  /* Previous P was the last efficient one */
        }
        prev_speedup = speedup;
    }

    return 256;  /* Still gaining value at 256 processors */
}

/* ========================================================================
 * L4: Deterministic Build Verification
 *
 * A build is deterministic if, given the same inputs, it always
 * produces the same outputs in the same order. This is critical for:
 *   - Reproducible builds (security/audit trail)
 *   - Cache correctness (two builds with same key -> same result)
 *   - CI/CD consistency (no environmental dependency)
 *
 * Verification approach:
 *   1. Hash all input files
 *   2. Hash all commands
 *   3. Compare output hashes
 *   4. If all match, builds are deterministic
 *
 * In practice, this is implemented via Bazel's --experimental_strict_action_env
 * or Debian's reproducible builds infrastructure.
 * ======================================================================== */

bool verify_build_determinism(const char *log1, const char *log2) {
    /* In production, this would parse and compare build event logs.
     * Here we accept string representations of log files and
     * compare them structurally. */
    if (!log1 || !log2) return false;
    return strcmp(log1, log2) == 0;
}

/* ========================================================================
 * Full Speedup Analysis
 *
 * Computes and fills a BuildSpeedupResult with all relevant
 * parallel performance metrics.
 *
 * L4 applies: Amdahl, Gustafson, efficiency, optimal parallelism.
 * ======================================================================== */

void speedup_analyze(double parallel_fraction, int num_processors,
                     BuildSpeedupResult *result) {
    memset(result, 0, sizeof(*result));
    result->parallel_fraction = parallel_fraction;
    result->serial_fraction = 1.0 - parallel_fraction;
    result->num_processors = num_processors;
    result->amdahl_speedup = amdahl_speedup(parallel_fraction, num_processors);
    result->gustafson_speedup = gustafson_speedup(parallel_fraction, num_processors);
    result->efficiency = result->amdahl_speedup / num_processors;
    result->optimal_processors = compute_optimal_parallelism(
        result->serial_fraction, 0.05);  /* 5% marginal gain threshold */
}

void speedup_print(const BuildSpeedupResult *result) {
    printf("\n=== Parallel Speedup Analysis ===\n");
    printf("  Workload:\n");
    printf("    Parallel fraction:  %.1f%%\n", result->parallel_fraction * 100.0);
    printf("    Serial fraction:    %.1f%%\n", result->serial_fraction * 100.0);
    printf("    Processors:         %d\n", result->num_processors);
    printf("\n  Theoretical Speedup:\n");
    printf("    Amdahl (fixed):     %.2fx\n", result->amdahl_speedup);
    printf("    Gustafson (scaled): %.2fx\n", result->gustafson_speedup);
    printf("\n  Efficiency:\n");
    printf("    Speedup/Processors: %.1f%%\n", result->efficiency * 100.0);
    printf("    Optimal processors: %d (at 5%% marginal gain threshold)\n",
           result->optimal_processors);

    /* L4: Show that as P increases, returns diminish */
    printf("\n  Speedup vs Processors:\n");
    printf("    P=1:  1.00x\n");
    for (int p = 2; p <= 32; p *= 2) {
        double sp = amdahl_speedup(result->parallel_fraction, p);
        printf("    P=%-2d: %.2fx\n", p, sp);
    }
    printf("    P=inf: %.2fx (Amdahl limit: 1/%.4f)\n",
           1.0 / result->serial_fraction, result->serial_fraction);
    printf("=================================\n");
}
