#define _CRT_SECURE_NO_WARNINGS
#include "build_logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * ============================================================================
 * L3: Engineering Structure - Build Event Logger
 * L7: Application - CI/CD Pipeline Integration
 *
 * A structured build event log is essential for:
 *   1. CI dashboards (build duration trends)
 *   2. Regression detection (previously passing targets now fail)
 *   3. Cache efficiency analysis (hit/miss ratios)
 *   4. Developer feedback (which step failed and why)
 *
 * The JSON output format (blog_print_json) produces machine-readable
 * logs compatible with tools like:
 *   - Bazel Build Event Protocol (BEP)
 *   - Chromium trace event format (chrome://tracing)
 *   - GitLab CI / GitHub Actions log parsers
 *
 * Reference: Build Event Protocol (Bazel documentation)
 * Course: CMU 15-410, Georgia Tech CS 6210 (Advanced OS)
 * ============================================================================
 */

void blog_init(BuildLogger *log, const char *output_path) {
    memset(log, 0, sizeof(*log));
    strncpy(log->output_path, output_path, BLOG_MAX_PATH - 1);
    log->build_start = time(NULL);
    log->build_succeeded = true;
}

/* Internal: add an event with bounds checking */
static void add_event(BuildLogger *log, BuildEventType type,
                       const char *target_name, const char *message,
                       int duration_ms, int exit_code, bool succeeded) {
    if (log->num_events >= BLOG_MAX_EVENTS) return;
    BuildEvent *evt = &log->events[log->num_events++];
    memset(evt, 0, sizeof(*evt));
    evt->type = type;
    if (target_name)
        strncpy(evt->target_name, target_name, BLOG_MAX_NAME_LEN - 1);
    if (message)
        strncpy(evt->message, message, BLOG_MAX_MSG_LEN - 1);
    evt->duration_ms = duration_ms;
    evt->exit_code = exit_code;
    evt->succeeded = succeeded;
    evt->timestamp = time(NULL);
}

void blog_event_start(BuildLogger *log, const char *phase_name) {
    add_event(log, BLOG_PHASE_START, phase_name, "started", 0, 0, true);
}

void blog_event_end(BuildLogger *log, const char *phase_name,
                    int duration_ms, bool success) {
    add_event(log, BLOG_PHASE_END, phase_name,
              success ? "completed" : "failed", duration_ms, 0, success);
    log->total_duration_ms += duration_ms;
    if (!success) {
        log->build_succeeded = false;
        log->failed_tasks++;
    }
}

void blog_task_start(BuildLogger *log, int task_id, const char *target_name) {
    char msg[128];
    snprintf(msg, sizeof(msg), "task %d started", task_id);
    BuildEvent *evt = &log->events[log->num_events];
    add_event(log, BLOG_TASK_START, target_name, msg, 0, 0, true);
    evt->task_id = task_id;
    log->total_tasks++;
}

void blog_task_end(BuildLogger *log, int task_id, const char *target_name,
                   int duration_ms, int exit_code) {
    char msg[128];
    snprintf(msg, sizeof(msg), "task %d finished (exit=%d)", task_id, exit_code);
    bool ok = (exit_code == 0);
    BuildEvent *evt = &log->events[log->num_events];
    add_event(log, BLOG_TASK_END, target_name, msg, duration_ms, exit_code, ok);
    evt->task_id = task_id;
    if (ok) log->succeeded_tasks++;
    else    log->failed_tasks++;
}

void blog_cache_hit(BuildLogger *log, const char *target_name) {
    char msg[256];
    snprintf(msg, sizeof(msg), "cache hit for %s", target_name);
    add_event(log, BLOG_CACHE_HIT, target_name, msg, 0, 0, true);
    log->cache_hits++;
}

void blog_cache_miss(BuildLogger *log, const char *target_name) {
    char msg[256];
    snprintf(msg, sizeof(msg), "cache miss for %s", target_name);
    add_event(log, BLOG_CACHE_MISS, target_name, msg, 0, 0, true);
}

void blog_warning(BuildLogger *log, const char *target_name, const char *msg) {
    add_event(log, BLOG_WARNING, target_name, msg, 0, 0, true);
}

void blog_error(BuildLogger *log, const char *target_name, const char *msg) {
    add_event(log, BLOG_ERROR, target_name, msg, 0, 1, false);
    log->build_succeeded = false;
    log->failed_tasks++;
}

void blog_metric(BuildLogger *log, const char *name, int value) {
    char msg[128];
    snprintf(msg, sizeof(msg), "%s=%d", name, value);
    add_event(log, BLOG_METRIC, name, msg, 0, 0, true);
}

/* L3: Compute a human-readable text summary */
int blog_summary(const BuildLogger *log, char *buf, size_t buf_size) {
    time_t end = time(NULL);
    int wall_sec = (int)(end - log->build_start);
    return snprintf(buf, buf_size,
        "=== Build Summary ===\n"
        "  Status:    %s\n"
        "  Duration:  %dms (wall: %ds)\n"
        "  Tasks:     %d total | %d succeeded | %d failed\n"
        "  Cache:     %d hits\n"
        "  Events:    %d\n"
        "=====================",
        log->build_succeeded ? "PASSED" : "FAILED",
        log->total_duration_ms, wall_sec,
        log->total_tasks, log->succeeded_tasks, log->failed_tasks,
        log->cache_hits, log->num_events);
}

bool blog_all_succeeded(const BuildLogger *log) {
    return log->build_succeeded && log->failed_tasks == 0;
}

int blog_flush(BuildLogger *log) {
    /* In production would write to disk; for now return event count */
    (void)log;
    return log->num_events;
}

/* L7: Application - Human-readable log output for CI consoles */
void blog_print(const BuildLogger *log) {
    printf("\n========== Build Event Log ==========\n");
    for (int i = 0; i < log->num_events; i++) {
        const BuildEvent *e = &log->events[i];
        const char *type_str = "UNKNOWN";
        switch (e->type) {
            case BLOG_PHASE_START: type_str = "PHASE_START"; break;
            case BLOG_PHASE_END:   type_str = "PHASE_END";   break;
            case BLOG_TASK_START:  type_str = "TASK_START";  break;
            case BLOG_TASK_END:    type_str = "TASK_END";    break;
            case BLOG_CACHE_HIT:   type_str = "CACHE_HIT";   break;
            case BLOG_CACHE_MISS:  type_str = "CACHE_MISS";  break;
            case BLOG_WARNING:     type_str = "WARNING";      break;
            case BLOG_ERROR:       type_str = "ERROR";        break;
            case BLOG_INFO:        type_str = "INFO";         break;
            case BLOG_METRIC:      type_str = "METRIC";       break;
        }
        printf("  [%s] %s %s %s %dms\n",
               type_str, e->target_name,
               e->succeeded ? "OK" : "FAIL",
               e->message, e->duration_ms);
    }
    char summary[512];
    blog_summary(log, summary, sizeof(summary));
    printf("\n%s\n\n", summary);
}

/* L7: Machine-readable JSON output for CI/CD systems.
 *
 * Produces a JSON array of events compatible with log ingestion
 * pipelines (ELK stack, Splunk, Datadog, etc.).
 *
 * Example output:
 *   {"type":"PHASE_START","target":"compile","msg":"started","time":0}
 *   {"type":"TASK_END","target":"foo.o","msg":"done","time":150,"exit":0}
 */
void blog_print_json(const BuildLogger *log) {
    printf("[\n");
    for (int i = 0; i < log->num_events; i++) {
        const BuildEvent *e = &log->events[i];
        const char *type_str = "UNKNOWN";
        switch (e->type) {
            case BLOG_PHASE_START: type_str = "PHASE_START"; break;
            case BLOG_PHASE_END:   type_str = "PHASE_END";   break;
            case BLOG_TASK_START:  type_str = "TASK_START";  break;
            case BLOG_TASK_END:    type_str = "TASK_END";    break;
            case BLOG_CACHE_HIT:   type_str = "CACHE_HIT";   break;
            case BLOG_CACHE_MISS:  type_str = "CACHE_MISS";  break;
            case BLOG_WARNING:     type_str = "WARNING";     break;
            case BLOG_ERROR:       type_str = "ERROR";       break;
            case BLOG_INFO:        type_str = "INFO";        break;
            case BLOG_METRIC:      type_str = "METRIC";      break;
        }
        printf("  {\"type\":\"%s\",\"target\":\"%s\",\"msg\":\"%s\","
               "\"ok\":%s,\"time\":%d,\"exit\":%d}%s\n",
               type_str, e->target_name, e->message,
               e->succeeded ? "true" : "false",
               e->duration_ms, e->exit_code,
               i < log->num_events - 1 ? "," : "");
    }
    printf("]\n");
}
