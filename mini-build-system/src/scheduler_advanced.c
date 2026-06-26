#define _CRT_SECURE_NO_WARNINGS
#include "scheduler_advanced.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * ============================================================================
 * L5: Algorithms - Advanced Scheduling Methods
 * L8: Advanced Topics - Work Stealing, Build Farm Simulation
 *
 * Implementations of:
 *   1. Johnson's Rule (optimal 2-machine flow shop scheduling)
 *   2. Priority-based heuristics (EDD, SPT, LPT, Critical Ratio)
 *   3. Work-stealing scheduler (Blumofe & Leiserson, 1999)
 *   4. Distributed build farm simulation
 *
 * Reference:
 *   Johnson, S.M. "Optimal two- and three-stage production schedules"
 *     Naval Research Logistics Quarterly, 1954
 *   Blumofe, R.D., Leiserson, C.E. "Scheduling Multithreaded
 *     Computations by Work Stealing" (JACM, 1999)
 *   CLRS "Introduction to Algorithms", Chapter 27
 *
 * Course: CMU 15-418 (Parallel), UT Austin CS 380D, UC Berkeley CS 267
 * ============================================================================
 */

/* ========================================================================
 * L5: Johnson's Rule for 2-Machine Flow Shop
 *
 * Theorem (Johnson, 1954): For n jobs processed on 2 machines in
 * the same order, the following algorithm produces an optimal
 * schedule minimizing makespan:
 *
 * 1. Partition jobs into:
 *    Set A: jobs with M1_time <= M2_time
 *    Set B: jobs with M1_time > M2_time
 * 2. Sort Set A in ascending order of M1_time
 * 3. Sort Set B in descending order of M2_time
 * 4. Sequence: (Set A) followed by (Set B)
 *
 * Proof: Exchange argument - swapping any adjacent jobs that
 * violate Johnson's rule cannot increase makespan.
 *
 * Complexity: O(n log n) for sorting
 * ======================================================================== */

static int cmp_johnson_m1(const void *a, const void *b) {
    const FlowShopJob *ja = (const FlowShopJob *)a;
    const FlowShopJob *jb = (const FlowShopJob *)b;
    return ja->time_m1 - jb->time_m1;
}

static int cmp_johnson_m2_desc(const void *a, const void *b) {
    const FlowShopJob *ja = (const FlowShopJob *)a;
    const FlowShopJob *jb = (const FlowShopJob *)b;
    return jb->time_m2 - ja->time_m2;
}

int johnson_2machine(FlowShopJob *jobs, int n, int *schedule_order) {
    if (n <= 0) return 0;

    FlowShopJob set_a[ASCHED_MAX_JOBS];
    FlowShopJob set_b[ASCHED_MAX_JOBS];
    int a_count = 0, b_count = 0;

    /* Partition: M1 <= M2 -> Set A, M1 > M2 -> Set B */
    for (int i = 0; i < n; i++) {
        if (jobs[i].time_m1 <= jobs[i].time_m2) {
            memcpy(&set_a[a_count++], &jobs[i], sizeof(FlowShopJob));
        } else {
            memcpy(&set_b[b_count++], &jobs[i], sizeof(FlowShopJob));
        }
    }

    /* Sort Set A by M1 ascending */
    qsort(set_a, a_count, sizeof(FlowShopJob), cmp_johnson_m1);
    /* Sort Set B by M2 descending */
    qsort(set_b, b_count, sizeof(FlowShopJob), cmp_johnson_m2_desc);

    /* Combine: Set A then Set B */
    int pos = 0;
    for (int i = 0; i < a_count; i++)
        schedule_order[pos++] = set_a[i].id;
    for (int i = 0; i < b_count; i++)
        schedule_order[pos++] = set_b[i].id;

    return pos;
}

/* L4: Compute makespan for a given schedule.
 *
 * Makespan = max completion time across all jobs.
 * For 2-machine flow shop:
 *   M1_completion[j] = M1_completion[j-1] + job_time_M1
 *   M2_completion[j] = max(M2_completion[j-1], M1_completion[j]) + job_time_M2
 *
 * The second term represents waiting: M2 cannot start a job until
 * M1 finishes it, AND M2 is free from the previous job. */
int johnson_makespan(const FlowShopJob *jobs, const int *order, int n) {
    int m1_end = 0, m2_end = 0;
    for (int i = 0; i < n; i++) {
        const FlowShopJob *j = &jobs[order[i]];
        m1_end += j->time_m1;
        /* M2 starts after M1 finishes AND M2 is free */
        m2_end = (m1_end > m2_end ? m1_end : m2_end) + j->time_m2;
    }
    return m2_end;
}

/* ========================================================================
 * L5: Priority-based Scheduling Heuristics
 *
 * These are dispatch rules used in production scheduling:
 *
 * SPT (Shortest Processing Time): minimize average completion time
 * LPT (Longest Processing Time): good for load balancing
 * EDD (Earliest Due Date): minimize maximum lateness (Jackson's Rule)
 * Critical Ratio: (due_date - current_time) / processing_time
 *   - CR < 1: job is late, expedite
 *   - CR > 1: job has slack
 *   - CR = 1: on schedule
 * ======================================================================== */

static int cmp_spt(const void *a, const void *b) {
    const FlowShopJob *ja = (const FlowShopJob *)a;
    const FlowShopJob *jb = (const FlowShopJob *)b;
    return (ja->time_m1 + ja->time_m2) - (jb->time_m1 + jb->time_m2);
}

static int cmp_lpt(const void *a, const void *b) {
    const FlowShopJob *ja = (const FlowShopJob *)a;
    const FlowShopJob *jb = (const FlowShopJob *)b;
    return (jb->time_m1 + jb->time_m2) - (ja->time_m1 + ja->time_m2);
}

void sched_spt(FlowShopJob *jobs, int n, int *order) {
    for (int i = 0; i < n; i++) jobs[i].id = i;
    qsort(jobs, n, sizeof(FlowShopJob), cmp_spt);
    for (int i = 0; i < n; i++) order[i] = jobs[i].id;
}

void sched_lpt(FlowShopJob *jobs, int n, int *order) {
    for (int i = 0; i < n; i++) jobs[i].id = i;
    qsort(jobs, n, sizeof(FlowShopJob), cmp_lpt);
    for (int i = 0; i < n; i++) order[i] = jobs[i].id;
}

void sched_edd(FlowShopJob *jobs, int n, int *order) {
    for (int i = 0; i < n; i++) jobs[i].id = i;
    /* EDD sorts by priority (treating it as due date) ascending */
    /* Using a simple insertion sort for this small scale */
    int ids[ASCHED_MAX_JOBS];
    for (int i = 0; i < n; i++) ids[i] = i;
    for (int i = 1; i < n; i++) {
        int key_id = ids[i];
        int key_priority = jobs[key_id].priority;
        int j = i - 1;
        while (j >= 0 && jobs[ids[j]].priority > key_priority) {
            ids[j + 1] = ids[j];
            j--;
        }
        ids[j + 1] = key_id;
    }
    for (int i = 0; i < n; i++) order[i] = ids[i];
}

void sched_critical_ratio(FlowShopJob *jobs, int n, int *order, int current_time) {
    /* CR = (due_date - current_time) / processing_time
     * Lower CR means more urgent. Jobs with CR < 0 are already late. */
    (void)current_time;
    for (int i = 0; i < n; i++) jobs[i].id = i;
    /* Sort by CR ascending (most critical first) */
    int ids[ASCHED_MAX_JOBS];
    for (int i = 0; i < n; i++) ids[i] = i;
    for (int i = 1; i < n; i++) {
        int key_id = ids[i];
        int key_total = jobs[key_id].time_m1 + jobs[key_id].time_m2;
        double key_cr = (double)jobs[key_id].priority / (key_total > 0 ? key_total : 1);
        int j = i - 1;
        while (j >= 0) {
            int j_total = jobs[ids[j]].time_m1 + jobs[ids[j]].time_m2;
            double j_cr = (double)jobs[ids[j]].priority / (j_total > 0 ? j_total : 1);
            if (j_cr <= key_cr) break;  /* already sorted by ascending CR */
            ids[j + 1] = ids[j];
            j--;
        }
        ids[j + 1] = key_id;
    }
    for (int i = 0; i < n; i++) order[i] = ids[i];
}

/* ========================================================================
 * L8: Work-Stealing Scheduler
 *
 * Work stealing is a randomized distributed scheduling algorithm
 * where idle workers "steal" tasks from busy workers' queues.
 *
 * Key properties (Blumofe & Leiserson, 1999):
 *   - Expected time: O(T1/P + T_infinity)
 *   - Expected steals: O(P * T_infinity)
 *   - Where T1 = total work, T_infinity = critical path length
 *
 * Used in: Cilk, Intel TBB, Go runtime, Java ForkJoinPool
 * ======================================================================== */

void wsp_init(WorkStealingPool *pool) {
    memset(pool, 0, sizeof(*pool));
    pool->head = 0;
    pool->tail = 0;
    pool->pending_count = 0;
}

int wsp_add_item(WorkStealingPool *pool, int duration,
                  const int *deps, int num_deps) {
    if (pool->num_items >= ASCHED_MAX_JOBS) return -1;
    int idx = pool->num_items++;
    WorkItem *item = &pool->items[idx];
    memset(item, 0, sizeof(*item));
    item->id = idx;
    item->duration = duration;
    item->num_deps = num_deps < 8 ? num_deps : 8;
    for (int i = 0; i < item->num_deps; i++)
        item->dependencies[i] = deps[i];
    item->done_deps = 0;
    item->is_stolen = false;
    item->assigned_worker = -1;
    pool->tail = pool->num_items;  /* update tail */
    return idx;
}

int wsp_steal_work(WorkStealingPool *pool, int worker_id) {
    /* Try to steal from the tail of a random worker's queue */
    for (int i = pool->tail; i > pool->head; i--) {
        int idx = i - 1;
        if (!pool->items[idx].is_stolen && pool->items[idx].assigned_worker < 0) {
            pool->items[idx].is_stolen = true;
            pool->items[idx].assigned_worker = worker_id;
            return idx;
        }
    }
    return -1;  /* nothing to steal */
}

void wsp_run(WorkStealingPool *pool, int num_workers) {
    printf("\n=== Work-Stealing Scheduler (%d workers) ===\n", num_workers);

    int total_time = 0;
    pool->pending_count = pool->num_items;

    /* Simple work-stealing simulation */
    int worker_time[16] = {0};
    int worker_task[16] = {-1};

    while (pool->pending_count > 0) {
        /* Assign ready tasks to idle workers */
        for (int w = 0; w < num_workers; w++) {
            if (worker_time[w] <= total_time && worker_task[w] < 0) {
                /* Worker is idle - try to find ready task */
                int task = -1;
                for (int i = 0; i < pool->num_items; i++) {
                    WorkItem *item = &pool->items[i];
                    if (item->assigned_worker >= 0) continue;
                    if (item->done_deps >= item->num_deps) {
                        task = i;
                        break;
                    }
                }
                if (task < 0) {
                    /* Try work stealing */
                    task = wsp_steal_work(pool, w);
                }
                if (task >= 0) {
                    pool->items[task].assigned_worker = w;
                    worker_task[w] = task;
                    worker_time[w] = total_time + pool->items[task].duration;
                    printf("  [%3d] Worker %d: task %d (duration=%d)\n",
                           total_time, w, task, pool->items[task].duration);
                }
            }
        }

        /* Advance time to next event */
        int next_time = total_time + 10000;
        for (int w = 0; w < num_workers; w++) {
            if (worker_task[w] >= 0 && worker_time[w] < next_time) {
                next_time = worker_time[w];
            }
        }
        if (next_time > total_time) total_time = next_time;

        /* Complete tasks that finished */
        for (int w = 0; w < num_workers; w++) {
            if (worker_task[w] >= 0 && worker_time[w] <= total_time) {
                int task = worker_task[w];
                pool->pending_count--;
                /* Mark task's dependents */
                for (int i = 0; i < pool->num_items; i++) {
                    for (int d = 0; d < pool->items[i].num_deps; d++) {
                        if (pool->items[i].dependencies[d] == task) {
                            pool->items[i].done_deps++;
                        }
                    }
                }
                worker_task[w] = -1;
            }
        }
    }
    printf("  Total makespan: %d\n", total_time);
    printf("=============================================\n");
}

/* ========================================================================
 * L8: Build Farm Simulation
 *
 * Simulates a distributed build system where tasks are distributed
 * across multiple machines (build farm). Models:
 *   - Task assignment to idle machines
 *   - Machine utilization tracking
 *   - Idle time analysis (Amdahl's Law implications)
 *
 * In production: Bazel's remote execution, distcc, Incredibuild
 * ======================================================================== */

void bf_init(BuildFarm *farm, int num_machines) {
    memset(farm, 0, sizeof(*farm));
    farm->num_machines = num_machines < ASCHED_MAX_MACHINES ?
                         num_machines : ASCHED_MAX_MACHINES;
    for (int i = 0; i < farm->num_machines; i++) {
        farm->machines[i].id = i;
        farm->machines[i].total_work_ms = 0;
        farm->machines[i].tasks_completed = 0;
        farm->machines[i].is_busy = false;
    }
}

void bf_simulate(BuildFarm *farm, const int *task_durations, int num_tasks) {
    int time_ms = 0;
    int tasks_done = 0;
    int task_ptr = 0;

    printf("\n=== Build Farm Simulation (%d machines, %d tasks) ===\n",
           farm->num_machines, num_tasks);

    while (tasks_done < num_tasks) {
        /* Assign new tasks to idle machines */
        for (int m = 0; m < farm->num_machines && task_ptr < num_tasks; m++) {
            if (!farm->machines[m].is_busy) {
                farm->machines[m].is_busy = true;
                farm->machines[m].current_task_end_ms = time_ms + task_durations[task_ptr];
                printf("  [%4dms] Machine %d: start task %d (duration=%d)\n",
                       time_ms, m, task_ptr, task_durations[task_ptr]);
                task_ptr++;
            }
        }

        /* Find next completion event */
        int next_completion = time_ms + 1000000;
        for (int m = 0; m < farm->num_machines; m++) {
            if (farm->machines[m].is_busy &&
                farm->machines[m].current_task_end_ms < next_completion) {
                next_completion = farm->machines[m].current_task_end_ms;
            }
        }

        if (next_completion > time_ms) {
            /* Record idle time for machines without work during this gap */
            if (task_ptr >= num_tasks) {
                for (int m = 0; m < farm->num_machines; m++) {
                    if (!farm->machines[m].is_busy) {
                        farm->idle_time += (next_completion - time_ms);
                    }
                }
            }
            time_ms = next_completion;
        }

        /* Complete tasks that finished */
        for (int m = 0; m < farm->num_machines; m++) {
            if (farm->machines[m].is_busy &&
                farm->machines[m].current_task_end_ms <= time_ms) {
                /* duration = farm->machines[m].current_task_end_ms -
                   (time_ms - task_durations[tasks_done]); */
                farm->machines[m].total_work_ms += task_durations[tasks_done];
                farm->machines[m].tasks_completed++;
                farm->machines[m].is_busy = false;
                tasks_done++;
            }
        }

        /* Safety: prevent infinite loop */
        if (time_ms > 1000000) break;
    }

    farm->total_makespan = time_ms;

    /* Calculate tail idle time */
    for (int m = 0; m < farm->num_machines; m++) {
        if (farm->machines[m].is_busy &&
            farm->machines[m].current_task_end_ms > time_ms) {
            int tail = farm->machines[m].current_task_end_ms - time_ms;
            if (tail > farm->total_makespan - time_ms)
                farm->total_makespan = farm->machines[m].current_task_end_ms;
        }
    }
}

void bf_print_stats(const BuildFarm *farm) {
    printf("\n=== Build Farm Statistics ===\n");
    printf("  Machines:      %d\n", farm->num_machines);
    printf("  Makespan:      %dms\n", farm->total_makespan);
    printf("  Total idle:    %dms\n", farm->idle_time);

    int total_work = 0;
    for (int i = 0; i < farm->num_machines; i++)
        total_work += farm->machines[i].total_work_ms;
    printf("  Total work:    %dms\n", total_work);

    /* L4: Amdahl's Law analysis */
    if (total_work > 0 && farm->total_makespan > 0) {
        double efficiency = (double)total_work /
                           (farm->num_machines * farm->total_makespan) * 100.0;
        printf("  Efficiency:    %.1f%%\n", efficiency);
    }

    printf("  Per machine:\n");
    for (int i = 0; i < farm->num_machines; i++) {
        const BuildFarmMachine *m = &farm->machines[i];
        printf("    M%d: %d tasks, %dms work\n",
               m->id, m->tasks_completed, m->total_work_ms);
    }
    printf("=============================\n");
}

const char *sched_strategy_name(int strategy) {
    switch (strategy) {
        case 0: return "Johnson-2Machine";
        case 1: return "SPT";
        case 2: return "LPT";
        case 3: return "EDD";
        case 4: return "Critical-Ratio";
        case 5: return "Work-Stealing";
        default: return "Unknown";
    }
}
