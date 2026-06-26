#ifndef DATAFLOW_H
#define DATAFLOW_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DF_VARS       256
#define MAX_DF_STMTS      512

typedef struct {
    int32_t var_id;
    char name[32];
} DFVar;

typedef enum {
    DF_DEFINE,
    DF_USE,
    DF_STMT_NOP
} DFStmtType;

typedef struct {
    DFStmtType type;
    int32_t var_id;
    int32_t line;
    char text[64];
} DFStatement;

typedef struct {
    DFVar *vars;
    int32_t var_count;
    int32_t var_capacity;
    DFStatement *stmts;
    int32_t stmt_count;
    int32_t stmt_capacity;
    bool **gen_set;
    bool **kill_set;
    bool **in_set;
    bool **out_set;
    int32_t *preds[256];
    int32_t pred_counts[256];
    int32_t block_count;
} DataFlowContext;

typedef void (*TransferFn)(DataFlowContext *ctx, int32_t block_id,
                            const bool *in_bits, bool *out_bits);
typedef void (*MeetFn)(DataFlowContext *ctx, int32_t block_id, bool *result);

void df_init(DataFlowContext *ctx);
void df_free(DataFlowContext *ctx);
int32_t df_add_var(DataFlowContext *ctx, const char *name);
int32_t df_add_stmt(DataFlowContext *ctx, DFStmtType type, int32_t var_id,
                    int32_t line, const char *text);
int32_t df_add_block(DataFlowContext *ctx);
void df_add_cfg_edge(DataFlowContext *ctx, int32_t from, int32_t to);
size_t df_bitvec_words(DataFlowContext *ctx);
void df_set_bit(bool *vec, int32_t idx, bool val);
bool df_get_bit(const bool *vec, int32_t idx);
void df_or_vec(bool *dst, const bool *src, size_t words);
void df_and_vec(bool *dst, const bool *src, size_t words);
void df_copy_vec(bool *dst, const bool *src, size_t words);
bool df_vec_equal(const bool *a, const bool *b, size_t words);
void df_vec_clear(bool *vec, size_t words);
void df_solve_forward(DataFlowContext *ctx, TransferFn transfer, MeetFn meet);
void df_solve_backward(DataFlowContext *ctx, TransferFn transfer, MeetFn meet);
void df_compute_liveness(DataFlowContext *ctx);
void df_compute_reaching_defs(DataFlowContext *ctx);
void df_compute_available_exprs(DataFlowContext *ctx);
void df_compute_very_busy(DataFlowContext *ctx);
void df_dump_sets(DataFlowContext *ctx, FILE *out);

#endif
