#define _CRT_SECURE_NO_WARNINGS
#include "task_scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sched_init(TaskScheduler *ts, int max_parallel) {
    memset(ts, 0, sizeof(*ts));
    ts->max_parallel = max_parallel > 0 ? max_parallel : 1;
}

int sched_add_task(TaskScheduler *ts, const char *command) {
    if (ts->num_tasks >= SCHED_MAX_TASKS) return -1;
    int id = ts->num_tasks++;
    BuildTask *task = &ts->tasks[id];
    task->id = id;
    strncpy(task->command, command, SCHED_MAX_COMMAND - 1);
    task->num_deps = 0;
    task->completed_count = 0;
    task->state = TASK_PENDING;
    task->duration_ms = 100;
    task->result = true;
    return id;
}

bool sched_add_dependency(TaskScheduler *ts, int task_id, int dep_id) {
    if (task_id < 0 || task_id >= ts->num_tasks) return false;
    if (dep_id < 0 || dep_id >= ts->num_tasks) return false;

    BuildTask *task = &ts->tasks[task_id];
    if (task->num_deps >= SCHED_MAX_DEPS) return false;

    for (int i = 0; i < task->num_deps; i++) {
        if (task->dependencies[i] == dep_id) return true;
    }
    task->dependencies[task->num_deps++] = dep_id;
    return true;
}

void sched_mark_dep_complete(TaskScheduler *ts, int task_id) {
    if (task_id < 0 || task_id >= ts->num_tasks) return;
    BuildTask *task = &ts->tasks[task_id];
    task->completed_count++;
    if (task->completed_count >= task->num_deps && task->state == TASK_PENDING) {
        task->state = TASK_READY;
    }
}

int sched_get_ready_task(TaskScheduler *ts) {
    for (int i = 0; i < ts->num_tasks; i++) {
        BuildTask *task = &ts->tasks[i];
        if (task->state == TASK_READY)
            return i;
    }
    return -1;
}

void sched_run_serial(TaskScheduler *ts) {
    printf("\n=== Serial Build ===\n");
    ts->current_time = 0;
    ts->completed_count = 0;
    ts->failed_count = 0;

    for (int i = 0; i < ts->num_tasks; i++) {
        BuildTask *task = &ts->tasks[i];
        if (task->num_deps == 0)
            task->state = TASK_READY;
    }

    while (ts->completed_count + ts->failed_count < ts->num_tasks) {
        int ready = sched_get_ready_task(ts);
        if (ready < 0) {
            printf("  [DEADLOCK] No ready tasks, but %d remaining\n",
                   ts->num_tasks - ts->completed_count - ts->failed_count);
            break;
        }

        BuildTask *task = &ts->tasks[ready];
        task->state = TASK_RUNNING;
        printf("  [%3dms] Task %d: %s\n", ts->current_time, task->id, task->command);
        ts->current_time += task->duration_ms;

        task->state = TASK_DONE;
        task->result = true;
        ts->completed_count++;

        for (int i = 0; i < ts->num_tasks; i++) {
            BuildTask *dep = &ts->tasks[i];
            for (int j = 0; j < dep->num_deps; j++) {
                if (dep->dependencies[j] == ready)
                    sched_mark_dep_complete(ts, i);
            }
        }
    }
    printf("  Total time: %dms\n", ts->current_time);
    printf("=====================\n");
}

static void run_parallel_simulation(TaskScheduler *ts) {
    int ready_queue[SCHED_MAX_TASKS];
    int queue_size = 0;

    for (int i = 0; i < ts->num_tasks; i++) {
        if (ts->tasks[i].num_deps == 0)
            ts->tasks[i].state = TASK_READY;
    }

    ts->current_time = 0;
    ts->completed_count = 0;
    ts->failed_count = 0;

    while (ts->completed_count + ts->failed_count < ts->num_tasks) {
        queue_size = 0;
        for (int i = 0; i < ts->num_tasks; i++) {
            if (ts->tasks[i].state == TASK_READY)
                ready_queue[queue_size++] = i;
        }

        if (queue_size == 0) {
            printf("  [DEADLOCK] No ready tasks at time %dms\n", ts->current_time);
            break;
        }

        int running_this_round = queue_size < ts->max_parallel
                                     ? queue_size : ts->max_parallel;

        printf("  [%3dms] Running %d tasks (parallel):",
               ts->current_time, running_this_round);
        for (int i = 0; i < running_this_round; i++) {
            int tid = ready_queue[i];
            printf(" #%d", tid);
            ts->tasks[tid].state = TASK_RUNNING;
        }
        printf("\n");

        ts->current_time += 100;

        for (int i = 0; i < running_this_round; i++) {
            int tid = ready_queue[i];
            BuildTask *task = &ts->tasks[tid];
            task->state = TASK_DONE;
            task->result = true;
            ts->completed_count++;

            for (int j = 0; j < ts->num_tasks; j++) {
                BuildTask *dep = &ts->tasks[j];
                for (int k = 0; k < dep->num_deps; k++) {
                    if (dep->dependencies[k] == tid)
                        sched_mark_dep_complete(ts, j);
                }
            }
        }
    }
}

void sched_run_parallel(TaskScheduler *ts) {
    printf("\n=== Parallel Build (max_parallel=%d) ===\n", ts->max_parallel);
    for (int i = 0; i < ts->num_tasks; i++) {
        ts->tasks[i].state = TASK_PENDING;
        ts->tasks[i].completed_count = 0;
    }
    run_parallel_simulation(ts);
    printf("  Total time: %dms (serial would be ~%dms)\n",
           ts->current_time, ts->num_tasks * 100);
    printf("=========================================\n");
}

const char *sched_state_name(TaskState state) {
    switch (state) {
        case TASK_PENDING: return "PENDING";
        case TASK_READY:   return "READY";
        case TASK_RUNNING: return "RUNNING";
        case TASK_DONE:    return "DONE";
        case TASK_FAILED:  return "FAILED";
        default:           return "UNKNOWN";
    }
}

void sched_print_status(const TaskScheduler *ts) {
    printf("\n=== Task Scheduler Status ===\n");
    printf("Tasks: %d | Running: %d | Done: %d | Failed: %d | Max parallel: %d\n",
           ts->num_tasks, ts->running_count, ts->completed_count,
           ts->failed_count, ts->max_parallel);
    for (int i = 0; i < ts->num_tasks; i++) {
        const BuildTask *t = &ts->tasks[i];
        printf("  Task %d [%s]: %s", t->id, sched_state_name(t->state), t->command);
        if (t->num_deps > 0) {
            printf(" (deps: %d/%d)", t->completed_count, t->num_deps);
        }
        printf("\n");
    }
    printf("=============================\n");
}

void sched_reset(TaskScheduler *ts) {
    ts->current_time = 0;
    ts->completed_count = 0;
    ts->failed_count = 0;
    ts->running_count = 0;
    for (int i = 0; i < ts->num_tasks; i++) {
        ts->tasks[i].state = TASK_PENDING;
        ts->tasks[i].completed_count = 0;
        ts->tasks[i].result = true;
    }
}

bool sched_all_done(const TaskScheduler *ts) {
    return ts->completed_count + ts->failed_count >= ts->num_tasks;
}
