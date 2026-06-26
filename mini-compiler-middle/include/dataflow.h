#ifndef DATAFLOW_H
#define DATAFLOW_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "ir.h"

#define BITVECTOR_WORDS ((MAX_BLOCKS + 31) / 32)

typedef enum {
    DF_REACHING_DEFS,
    DF_LIVE_VARIABLES,
    DF_AVAILABLE_EXPRS,
    DF_CONSTANT_PROP
} DataflowAnalysis;

typedef struct {
    uint32_t bits[BITVECTOR_WORDS];
} BitVector;

typedef struct {
    BitVector IN[MAX_BLOCKS];
    BitVector OUT[MAX_BLOCKS];
} DataflowResult;

void     bv_init(BitVector* bv);
void     bv_set(BitVector* bv, int idx);
void     bv_clear(BitVector* bv, int idx);
bool     bv_test(const BitVector* bv, int idx);
void     bv_union(BitVector* dst, const BitVector* src);
void     bv_intersect(BitVector* dst, const BitVector* src);
void     bv_copy(BitVector* dst, const BitVector* src);
bool     bv_equals(const BitVector* a, const BitVector* b);
void     bv_print(const BitVector* bv, int max_bits, FILE* out);

void     df_analyze(const IRFunction* func, IRBasicBlock blocks[], int num_blocks,
                    DataflowAnalysis type, DataflowResult* result);
void     df_reaching_defs(const IRFunction* func, IRBasicBlock blocks[], int num_blocks,
                          DataflowResult* result);
void     df_live_variables(const IRFunction* func, IRBasicBlock blocks[], int num_blocks,
                           DataflowResult* result);
void     df_constant_propagation(const IRFunction* func, IRBasicBlock blocks[], int num_blocks,
                                 int* value_table);
void     df_print_result(const DataflowResult* result, int num_blocks, FILE* out);

void     df_available_exprs(const IRFunction* func, IRBasicBlock blocks[],
                             int num_blocks, DataflowResult* result);
void     df_very_busy_exprs(const IRFunction* func, IRBasicBlock blocks[],
                              int num_blocks, DataflowResult* result);

#endif
