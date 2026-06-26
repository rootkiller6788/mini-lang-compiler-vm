#ifndef LOOP_ANALYSIS_H
#define LOOP_ANALYSIS_H

#include <stdbool.h>
#include <stdio.h>
#include "ir.h"
#include "cfg.h"

#define MAX_LOOPS 64
#define MAX_IVARS 32

typedef enum {
    IV_BASIC,
    IV_DERIVED,
    IV_UNKNOWN
} IVarKind;

typedef struct {
    int var_id;
    IVarKind kind;
    int base;
    int step;
    int bound;
    bool is_integer;
    int derive_from;
} InductionVar;

typedef struct {
    int header;
    int blocks[MAX_BLOCKS];
    int num_blocks;
    int back_edges[MAX_BLOCKS][2];
    int num_back_edges;
    int depth;
    int parent_loop;
    int children[MAX_LOOPS];
    int num_children;
    InductionVar ivars[MAX_IVARS];
    int num_ivars;
    bool has_invariant_code;
} LoopInfo;

typedef struct {
    LoopInfo loops[MAX_LOOPS];
    int num_loops;
    int root;
} LoopTree;

void loop_find_natural_loops(const CFG* cfg, LoopTree* tree);
void loop_build_tree(LoopTree* tree);
void loop_detect_induction_variables(const IRFunction* func, const CFG* cfg,
                                      LoopInfo* loop);
bool loop_is_invariant(const IRFunction* func, int var_id,
                        const LoopInfo* loop, const CFG* cfg);
void loop_detect_invariant_code(const IRFunction* func, const CFG* cfg,
                                 LoopInfo* loop);
void loop_strength_reduction(IRFunction* func, LoopInfo* loop);
void loop_induction_var_elimination(IRFunction* func, LoopInfo* loop);
bool loop_can_unroll(const LoopInfo* loop, const CFG* cfg);
void loop_print_tree(const LoopTree* tree, const CFG* cfg, FILE* out);

int  loop_dependence_distance(const IRInst* inst_a, const IRInst* inst_b);
bool banerjee_test(int a1, int b1, int c1, int a2, int b2, int c2, int n);

#endif
