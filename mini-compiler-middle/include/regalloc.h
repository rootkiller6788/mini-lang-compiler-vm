#ifndef REGALLOC_H
#define REGALLOC_H

#include <stdbool.h>
#include <stdio.h>
#include "ir.h"
#include "dataflow.h"

#define MAX_REGS 32
#define MAX_NODES 256

typedef enum {
    RA_SIMPLIFY,
    RA_COALESCE,
    RA_FREEZE,
    RA_SPILL,
    RA_SELECT,
    RA_BUILD
} RAWorkPhase;

typedef struct {
    int node;
    int degree;
    bool frozen;
    bool spilled;
    bool selected;
    int color;
} RANode;

typedef struct {
    bool matrix[MAX_NODES][MAX_NODES];
} InterferenceGraph;

typedef struct {
    RANode nodes[MAX_NODES];
    InterferenceGraph ig;
    int num_nodes;
    int num_regs;
    int colors[MAX_NODES];
    bool coalesced_pairs[MAX_NODES][MAX_NODES];
    int coalesces;
} RegisterAllocator;

typedef struct {
    int min_colors;
    int spills;
    int coalesces;
    int total_nodes;
} RAStats;

RegisterAllocator* ra_create(int num_regs);
void ra_destroy(RegisterAllocator* ra);
int  ra_add_live_range(RegisterAllocator* ra, int var_id);
void ra_add_interference(RegisterAllocator* ra, int node_a, int node_b);
void ra_build_interference_graph(RegisterAllocator* ra, const IRFunction* func,
                                  const IRBasicBlock blocks[], int num_blocks,
                                  const DataflowResult* liveness);
RAStats ra_color_graph(RegisterAllocator* ra);
void ra_assign_registers(RegisterAllocator* ra, IRFunction* func);
void ra_print_coloring(const RegisterAllocator* ra, FILE* out);
const char* ra_phase_name(RAWorkPhase phase);

int  ra_spill_cost(const RegisterAllocator* ra, int node);
int  ra_degree(const RegisterAllocator* ra, int node);
bool ra_is_coalescable(const RegisterAllocator* ra, int a, int b);
void ra_coalesce(RegisterAllocator* ra, int a, int b);

void ra_linear_scan(IRFunction* func, int num_regs, int* reg_assignments);

#endif
