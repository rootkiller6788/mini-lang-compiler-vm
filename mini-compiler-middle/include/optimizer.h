#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include <stdbool.h>
#include <stdio.h>
#include "ir.h"

typedef enum {
    OPT_DCE,
    OPT_CSE,
    OPT_COPY_PROP,
    OPT_CONST_FOLD,
    OPT_LOOP_INVARIANT,
    OPT_SIMPLIFY_CFG
} OptPass;

typedef struct {
    int removed_instructions;
    int replaced_expressions;
    int folded_constants;
    int copies_propagated;
} OptStats;

OptStats opt_run_pass(IRFunction* func, OptPass pass);
OptStats opt_dce(IRFunction* func);
OptStats opt_cse(IRFunction* func);
OptStats opt_constant_folding(IRFunction* func);
OptStats opt_copy_propagation(IRFunction* func);
OptStats opt_simplify_cfg(IRFunction* func);
void     opt_print_changes(OptStats stats, FILE* out);
OptStats opt_run_pipeline(IRFunction* func, const OptPass passes[], int num_passes);
const char* opt_pass_name(OptPass pass);

#endif
