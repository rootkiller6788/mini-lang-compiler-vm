#ifndef INST_SCHED_H
#define INST_SCHED_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "instruction_selection.h"

#define MAX_SCHED_NODES    256
#define MAX_SCHED_DEPS     1024

typedef struct {
    int32_t latency;
    int32_t resource_class;
    char resource_name[16];
    int32_t issue_cycle;
    int32_t finish_cycle;
} SchedInfo;

typedef struct {
    int32_t node_id;
    InstructionNode instr;
    SchedInfo sched_info;
    int32_t *deps;
    int32_t dep_count;
    int32_t dep_capacity;
    int32_t *rev_deps;
    int32_t rev_count;
    int32_t rev_capacity;
    bool scheduled;
    int32_t sched_cycle;
} SchedNode;

typedef struct {
    SchedNode *nodes;
    int32_t node_count;
    int32_t node_capacity;
    int32_t total_cycles;
    int32_t *ready_list;
    int32_t ready_count;
    int32_t ready_capacity;
} SchedDAG;

typedef struct {
    int32_t *schedule;
    int32_t count;
    int32_t makespan;
} SchedResult;

void sched_init(SchedDAG *dag);
void sched_free(SchedDAG *dag);
int32_t sched_add_node(SchedDAG *dag, InstructionNode *instr,
                        int32_t latency, const char *resource);
void sched_add_dep(SchedDAG *dag, int32_t from, int32_t to);
void sched_list_schedule(SchedDAG *dag, SchedResult *result);
void sched_priority_schedule(SchedDAG *dag, SchedResult *result,
                              int32_t *priorities);
void sched_print_schedule(SchedResult *result, SchedDAG *dag, FILE *out);
void sched_result_free(SchedResult *r);
void sched_compute_delay_slots(SchedDAG *dag, int32_t branch_node,
                                int32_t num_slots, int32_t *fill_nodes);

#endif
