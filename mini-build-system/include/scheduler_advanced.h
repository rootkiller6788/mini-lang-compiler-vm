#ifndef SCHEDULER_ADVANCED_H
#define SCHEDULER_ADVANCED_H

#include <stdbool.h>

/* ============================================================================
 * L5: Algorithms - Advanced Scheduling Methods
 * L8: Advanced Topics - Work-Stealing, Build Farm Simulation
 *
 * Beyond basic topological scheduling, advanced build schedulers
 * employ heuristics to minimize makespan and maximize throughput.
 *
 * Reference: Introduction to Algorithms (CLRS), Ch. 27 (Multithreaded Algorithms)
 *            Johnson, S.M. "Optimal two- and three-stage production schedules"
 *            Blumofe, R. "Scheduling Multithreaded Computations by Work Stealing"
 * Course: CMU 15-418 (Parallel Computing), UT Austin CS 380D
 * ========================================================================= */

#define ASCHED_MAX_JOBS    256
#define ASCHED_MAX_MACHINES  16
#define ASCHED_NAME_LEN     128

/* L1: Job2Machine - a job for 2-machine flow shop */
typedef struct {
    int    id;
    char   name[ASCHED_NAME_LEN];
    int    time_m1;    /* time on machine 1 */
    int    time_m2;    /* time on machine 2 */
    int    priority;
} FlowShopJob;

/* L1: WorkItem - a unit of work in a work-stealing pool */
typedef struct {
    int    id;
    int    duration;
    int    dependencies[8];
    int    num_deps;
    int    done_deps;
    bool   is_stolen;
    int    assigned_worker;
} WorkItem;

/* L1: BuildFarmMachine - a machine in a distributed build farm */
typedef struct {
    int    id;
    int    total_work_ms;
    int    tasks_completed;
    bool   is_busy;
    int    current_task_end_ms;
} BuildFarmMachine;

/* L1: WorkStealingPool - lock-free work queue (simulated) */
typedef struct {
    WorkItem items[ASCHED_MAX_JOBS];
    int      num_items;
    int      head;   /* next item to steal */
    int      tail;   /* next write position */
    int      pending_count;
} WorkStealingPool;

/* L1: BuildFarm - distributed build system simulation */
typedef struct {
    BuildFarmMachine machines[ASCHED_MAX_MACHINES];
    int              num_machines;
    int              total_makespan;
    int              idle_time;
} BuildFarm;

/* L5: Johnson's Rule for 2-machine flow shop (optimal makespan)
 *
 * Theorem (Johnson, 1954): For a 2-machine flow shop, sequencing jobs
 * in non-decreasing order of min(M1_time, M2_time), with ties broken
 * by placing M1-shorter jobs first, minimizes makespan.
 *
 * Time complexity: O(n log n)
 * Optimality proof: By exchange argument - any suboptimal schedule
 * can be improved by swapping adjacent jobs that violate Johnson's rule. */
int  johnson_2machine(FlowShopJob *jobs, int n, int *schedule_order);
int  johnson_makespan(const FlowShopJob *jobs, const int *order, int n);

/* L5: Priority-based scheduling heuristics */
void sched_edd(FlowShopJob *jobs, int n, int *order);    /* Earliest Due Date */
void sched_spt(FlowShopJob *jobs, int n, int *order);    /* Shortest Processing Time */
void sched_lpt(FlowShopJob *jobs, int n, int *order);    /* Longest Processing Time */
void sched_critical_ratio(FlowShopJob *jobs, int n, int *order, int current_time);

/* L8: Work stealing scheduler.
 * Workers maintain local double-ended queues. When idle, a worker
 * "steals" work from a random busy worker's queue tail.
 * Expected makespan bound: T1/P + O(T_infinity) (Blumofe & Leiserson, 1999) */
void wsp_init(WorkStealingPool *pool);
int  wsp_add_item(WorkStealingPool *pool, int duration,
                   const int *deps, int num_deps);
int  wsp_steal_work(WorkStealingPool *pool, int worker_id);
void wsp_run(WorkStealingPool *pool, int num_workers);

/* L8: Build farm simulation.
 * Simulates distributed build execution across a cluster of machines,
 * modeling task assignment, idle time, and network transfer delays. */
void bf_init(BuildFarm *farm, int num_machines);
void bf_simulate(BuildFarm *farm, const int *task_durations, int num_tasks);
void bf_print_stats(const BuildFarm *farm);

/* Utility */
const char *sched_strategy_name(int strategy);

#endif
