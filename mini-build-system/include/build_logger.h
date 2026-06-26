#ifndef BUILD_LOGGER_H
#define BUILD_LOGGER_H

#include <stdbool.h>
#include <time.h>

/* ============================================================================
 * L3: Engineering Structure - Structured Build Logging
 * L7: Application - CI/CD pipeline integration
 *
 * A structured build event log captures timing, success/failure, and
 * dependency information for post-build analysis, CI dashboards,
 * and regression detection.
 *
 * Reference: Build Event Protocol (Bazel), JUnit XML, Chromium trace format
 * Course: CMU 15-410, Georgia Tech CS 6210
 * ========================================================================= */

#define BLOG_MAX_EVENTS    512
#define BLOG_MAX_PATH      256
#define BLOG_MAX_MSG_LEN   512
#define BLOG_MAX_NAME_LEN  128

/* L1: BuildEventType - categories of build lifecycle events */
typedef enum {
    BLOG_PHASE_START,      /* Build phase begins (configure, compile, link) */
    BLOG_PHASE_END,        /* Build phase ends */
    BLOG_TASK_START,       /* Individual task starts */
    BLOG_TASK_END,         /* Individual task completes (success/fail) */
    BLOG_CACHE_HIT,        /* Cache hit avoided rebuild */
    BLOG_CACHE_MISS,       /* Cache miss triggered rebuild */
    BLOG_WARNING,          /* Non-fatal warning */
    BLOG_ERROR,            /* Fatal error */
    BLOG_INFO,             /* Informational message */
    BLOG_METRIC            /* Performance metric (compile time, mem usage) */
} BuildEventType;

/* L1: BuildEvent - a single structured log entry */
typedef struct {
    BuildEventType type;
    char           target_name[BLOG_MAX_NAME_LEN];
    char           message[BLOG_MAX_MSG_LEN];
    int            duration_ms;
    int            exit_code;
    time_t         timestamp;
    bool           succeeded;
    int            task_id;
} BuildEvent;

/* L1: BuildLogger - accumulates and flushes structured build events */
typedef struct {
    BuildEvent events[BLOG_MAX_EVENTS];
    int        num_events;
    char       output_path[BLOG_MAX_PATH];
    int        total_tasks;
    int        succeeded_tasks;
    int        failed_tasks;
    int        cache_hits;
    int        total_duration_ms;
    time_t     build_start;
    bool       build_succeeded;
} BuildLogger;

/* Logger lifecycle */
void blog_init(BuildLogger *log, const char *output_path);
void blog_event_start(BuildLogger *log, const char *phase_name);
void blog_event_end(BuildLogger *log, const char *phase_name,
                    int duration_ms, bool success);
void blog_task_start(BuildLogger *log, int task_id, const char *target_name);
void blog_task_end(BuildLogger *log, int task_id, const char *target_name,
                   int duration_ms, int exit_code);
void blog_cache_hit(BuildLogger *log, const char *target_name);
void blog_cache_miss(BuildLogger *log, const char *target_name);
void blog_warning(BuildLogger *log, const char *target_name, const char *msg);
void blog_error(BuildLogger *log, const char *target_name, const char *msg);
void blog_metric(BuildLogger *log, const char *name, int value);

/* L7: Application - CI/CD output formats */
int  blog_flush(BuildLogger *log);
int  blog_summary(const BuildLogger *log, char *buf, size_t buf_size);
bool blog_all_succeeded(const BuildLogger *log);
void blog_print(const BuildLogger *log);
void blog_print_json(const BuildLogger *log);  /* Machine-readable format */

#endif
