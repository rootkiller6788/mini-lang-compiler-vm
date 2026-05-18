#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include <stdbool.h>

#define SCHED_MAX_TASKS     256
#define SCHED_MAX_COMMAND   512
#define SCHED_MAX_DEPS       32

typedef enum {
    TASK_PENDING,
    TASK_READY,
    TASK_RUNNING,
    TASK_DONE,
    TASK_FAILED
} TaskState;

typedef struct {
    int       id;
    char      command[SCHED_MAX_COMMAND];
    int       dependencies[SCHED_MAX_DEPS];
    int       num_deps;
    int       completed_count;
    TaskState state;
    int       duration_ms;
    bool      result;
} BuildTask;

typedef struct {
    BuildTask tasks[SCHED_MAX_TASKS];
    int       num_tasks;
    int       max_parallel;
    int       running_count;
    int       completed_count;
    int       failed_count;
    int       current_time;
} TaskScheduler;

void sched_init(TaskScheduler *ts, int max_parallel);
int  sched_add_task(TaskScheduler *ts, const char *command);
bool sched_add_dependency(TaskScheduler *ts, int task_id, int dep_id);
void sched_mark_dep_complete(TaskScheduler *ts, int task_id);
int  sched_get_ready_task(TaskScheduler *ts);
void sched_run_serial(TaskScheduler *ts);
void sched_run_parallel(TaskScheduler *ts);
void sched_print_status(const TaskScheduler *ts);
void sched_reset(TaskScheduler *ts);
bool sched_all_done(const TaskScheduler *ts);
const char *sched_state_name(TaskState state);

#endif
