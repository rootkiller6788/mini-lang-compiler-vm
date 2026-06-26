#ifndef TYPE_INFER_H
#define TYPE_INFER_H

#include <stdbool.h>
#include <stddef.h>
#include "graph_ir.h"

/*
 * Type & Shape Inference — Module 14.9
 *
 * Hindley-Milner-style type inference adapted for tensor computation graphs.
 * References:
 *   - Damas & Milner, "Principal type-schemes for functional programs" (POPL 1982)
 *   - Pierce, "Types and Programming Languages" (TAPL, 2002)
 *   - TVM Relay Type Inference: type solver with unification
 *   - XLA Shape Inference: shape function per operation
 *
 * L1: Definitions — Type lattice, shape variables
 * L2: Core Concepts — unification, principal types, shape polymorphism
 * L4: Standards — Hindley-Milner algorithm, type soundness
 * L5: Algorithms — Algorithm W, unification, occurs check
 */

#define INFER_MAX_TYPE_VARS    64
#define INFER_MAX_CONSTRAINTS 128
#define INFER_MAX_SUBST        64
#define INFER_MAX_NAME_LEN     64

/* ---- L1: Definitions — Expriment Kind ---- */
typedef enum {
    TypeVar_FREE,        /* Unknown type variable (α, β, γ, ...) */
    TypeVar_BOUND,       /* Bound to a concrete type */
    TypeVar_SHAPE_KNOWN, /* Shape is known, dtype may be inferred */
    TypeVar_SHAPE_PARTIAL /* Some dimensions known */
} TypeVarKind;

typedef enum {
    TypeCon_INT8,
    TypeCon_INT32,
    TypeCon_FLOAT32,
    TypeCon_FLOAT64,
    TypeCon_BOOL,
    TypeCon_TENSOR,   /* Takes shape + element type */
    TypeCon_FUNC,     /* Takes argument types + return type */
    TypeCon_SHAPE,    /* List of dimension variables */
    TypeCon_DIM,      /* Single dimension (can be symbolic) */
    TypeCon_COUNT
} TypeConstructor;

/* ---- L1: Definitions — Type AST ---- */
typedef struct TypeNode {
    TypeConstructor con;
    int var_id;                  /* For type variables */
    struct TypeNode *args[4];    /* For compound types */
    int arg_count;
    bool is_free;
    char name[INFER_MAX_NAME_LEN];
    int shape_dims[GRAPH_MAX_SHAPE_DIMS];
    int ndim;
} TypeNode;

/* ---- L1: Definitions — Type Constraint ---- */
typedef enum {
    Constraint_EQUAL,       /* τ₁ = τ₂ */
    Constraint_HAS_SHAPE,   /* τ has shape [d₀, d₁, ...] */
    Constraint_BROADCAST,   /* τ₁ broadcast-compatible with τ₂ */
    Constraint_COUNT
} ConstraintKind;

typedef struct {
    ConstraintKind kind;
    int type_var_a;
    int type_var_b;
    int shape_a[GRAPH_MAX_SHAPE_DIMS];
    int ndim_a;
    int shape_b[GRAPH_MAX_SHAPE_DIMS];
    int ndim_b;
} TypeConstraint;

/* ---- L1: Definitions — Type Substitution ---- */
typedef struct {
    int var_id;
    TypeNode replacement;
} TypeSubst;

/* ---- L1: Definitions — Type Environment ---- */
typedef struct {
    TypeNode type_vars[INFER_MAX_TYPE_VARS];
    int type_var_count;
    TypeConstraint constraints[INFER_MAX_CONSTRAINTS];
    int constraint_count;
    TypeSubst substitutions[INFER_MAX_SUBST];
    int subst_count;
    TypeNode node_types[GRAPH_MAX_NODES];
    int node_count;
    bool solved;
} TypeEnv;

/* ---- L1: Definitions — Shape Function ---- */
typedef struct {
    GraphOpType op_type;
    int (*shape_func)(const int *input_dims, int ndim,
                      const GraphAttrs *attrs,
                      int *output_dims, int *out_ndim);
    char description[INFER_MAX_NAME_LEN];
} ShapeFunction;

/* ---- L1: API Declarations ---- */

/* L1: Type constructor operations */
TypeNode type_new_var(int id);
TypeNode type_new_tensor(TypeNode elem_type, int ndim, const int *dims);
TypeNode type_new_dim(int value);
TypeNode type_new_concrete(TypeConstructor con);

/* L2: Unification — core of Hindley-Milner type inference */
bool type_unify(TypeEnv *env, int var_a, int var_b);
bool type_occurs_check(int var_a, TypeNode *type_b);

/* L5: Algorithm W — principal type inference */
int  type_infer_graph(TypeEnv *env, ComputeGraph *g);
void type_collect_constraints(TypeEnv *env, ComputeGraph *g);
bool type_solve_constraints(TypeEnv *env);

/* L3: Shape inference with broadcasting */
bool type_broadcast_shapes(const int *shape_a, int ndim_a,
                           const int *shape_b, int ndim_b,
                           int *result_shape, int *result_ndim);

/* L5: Shape function dispatch */
const ShapeFunction *type_get_shape_function(GraphOpType op);
int  type_invoke_shape_function(GraphOpType op,
                                const int *input_dims, int ndim,
                                const GraphAttrs *attrs,
                                int *output_dims, int *out_ndim);

/* L4: Type soundness — verify all operations are well-typed */
bool type_check_graph(TypeEnv *env, ComputeGraph *g);
bool type_check_operation(GraphOpType op, const TypeNode *input_types,
                          int n_inputs, const TypeNode *output_type);

/* L7: Application — type-driven optimizations */
bool type_can_fuse(GraphOpType a, GraphOpType b);
int  type_specialize_for_shape(ComputeGraph *g, const int *fixed_dims,
                                int ndim);

/* L8: Advanced — polymorphism with let-generalization */
TypeNode type_generalize(TypeNode monotype, TypeEnv *env);
TypeNode type_instantiate(TypeNode polytype, TypeEnv *env);

/* L8: Advanced — dependent types for shape-parametric programs */
bool type_verify_shape_dependent(TypeNode *t, const int *runtime_shape);

/* Print/debug */
void type_print_node(TypeNode *t);
void type_print_env(TypeEnv *env);
void type_print_constraint(TypeConstraint *c);
void type_print_subst(TypeSubst *s);
const char *type_constructor_name(TypeConstructor con);

#endif /* TYPE_INFER_H */
