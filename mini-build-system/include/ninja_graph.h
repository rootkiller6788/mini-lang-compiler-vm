#ifndef NINJA_GRAPH_H
#define NINJA_GRAPH_H

#include <stdbool.h>
#include <time.h>

#define NINJA_MAX_NODES   256
#define NINJA_MAX_EDGES   256
#define NINJA_MAX_INPUTS  16
#define NINJA_MAX_IMPLICIT 8
#define NINJA_MAX_ORDER_ONLY 8
#define NINJA_MAX_LINE    512
#define NINJA_MAX_VARS    128

typedef enum {
    NINJA_NODE_FILE,
    NINJA_NODE_RULE,
    NINJA_NODE_PHONY
} NinjaNodeType;

typedef enum {
    NINJA_NODE_CLEAN,
    NINJA_NODE_DIRTY,
    NINJA_NODE_RESTAT
} NinjaDirtyState;

typedef struct {
    char           path[256];
    NinjaNodeType  type;
    time_t         mtime;
    NinjaDirtyState dirty;
    bool           exists;
    int            critical_path;
} NinjaNode;

typedef struct {
    char  rule_name[128];
    char  inputs[NINJA_MAX_INPUTS][256];
    int   num_inputs;
    char  implicit_inputs[NINJA_MAX_IMPLICIT][256];
    int   num_implicit;
    char  order_only_deps[NINJA_MAX_ORDER_ONLY][256];
    int   num_order_only;
    char  outputs[1][256];
    char  variables[NINJA_MAX_VARS][2][256];
    int   num_vars;
} NinjaEdge;

typedef struct {
    NinjaNode  nodes[NINJA_MAX_NODES];
    int        num_nodes;
    NinjaEdge  edges[NINJA_MAX_EDGES];
    int        num_edges;
    char       default_targets[8][256];
    int        num_default_targets;
} NinjaBuild;

bool ninja_parse(NinjaBuild *nb, const char *filepath);
void ninja_compute_dirty(NinjaBuild *nb);
void ninja_schedule(NinjaBuild *nb);
void ninja_execute(NinjaBuild *nb, const char *target);
void ninja_print_graph(const NinjaBuild *nb);
int  ninja_find_node(NinjaBuild *nb, const char *path);
int  ninja_add_node(NinjaBuild *nb, const char *path, NinjaNodeType type);
NinjaEdge *ninja_find_edge(NinjaBuild *nb, const char *output);
void ninja_compute_critical_path(NinjaBuild *nb);
const char *ninja_lookup_var(const NinjaBuild *nb, const char *name);

#endif
