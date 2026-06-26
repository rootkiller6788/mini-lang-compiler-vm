#ifndef SSA_H
#define SSA_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_BB_COUNT     256
#define MAX_PHI_NODES    128
#define MAX_VAR_NAMES    512

typedef struct {
    int32_t id;
    int32_t *predecessors;
    int32_t pred_count;
    int32_t pred_capacity;
    int32_t *successors;
    int32_t succ_count;
    int32_t succ_capacity;
} BasicBlock;

typedef struct {
    int32_t id;
    int32_t *doms;
    int32_t dom_count;
    int32_t idom;
    int32_t *dfrontier;
    int32_t df_count;
    int32_t df_capacity;
} DomTree;

typedef struct {
    int32_t var_id;
    int32_t version;
    int32_t bb_id;
    char name[32];
} SSAVar;

typedef struct {
    int32_t var_id;
    SSAVar sources[8];
    int32_t source_count;
    int32_t bb_id;
} PhiNode;

typedef struct {
    char var_name[32];
    int32_t current_version;
    int32_t *def_blocks;
    int32_t def_count;
    int32_t def_capacity;
} SSADef;

typedef struct {
    BasicBlock *blocks;
    int32_t block_count;
    int32_t block_capacity;
    DomTree dom;
    SSADef *defs;
    int32_t def_count;
    int32_t def_capacity;
    SSAVar *vars;
    int32_t var_count;
    int32_t var_capacity;
    PhiNode *phis;
    int32_t phi_count;
    int32_t phi_capacity;
} SSAContext;

void ssa_init(SSAContext *ctx);
void ssa_free(SSAContext *ctx);
void ssa_build_dominance(SSAContext *ctx);
void ssa_compute_dominance_frontiers(SSAContext *ctx);
void ssa_insert_phi_nodes(SSAContext *ctx);
void ssa_rename_variables(SSAContext *ctx);
void ssa_construct(SSAContext *ctx);
int32_t ssa_add_basic_block(SSAContext *ctx);
void ssa_add_edge(SSAContext *ctx, int32_t from, int32_t to);
int32_t ssa_add_variable(SSAContext *ctx, const char *name);
void ssa_add_definition(SSAContext *ctx, int32_t var_id, int32_t bb_id);
void ssa_dump(SSAContext *ctx, FILE *out);

#endif
