#ifndef ML_LIKE_H
#define ML_LIKE_H

#include <stdbool.h>
#include <stddef.h>

/* ── L1: Core AST Types ────────────────────────────────────────────
 * ML-like language: call-by-value lambda calculus with extensions.
 * Reference: Pierce, B. "Types and Programming Languages" (TAPL)
 *            §5: The Untyped Lambda-Calculus
 */
typedef enum {
    ML_INT,
    ML_BOOL,
    ML_VAR,
    ML_LAMBDA,
    ML_APP,
    ML_LET,
    ML_LETREC,
    ML_IF,
    ML_CONS,         /* cons cell (pair) */
    ML_CAR,          /* first of pair */
    ML_CDR,          /* second of pair */
    ML_NIL,          /* empty list */
    ML_NULLP,        /* null? predicate */
    ML_MATCH,        /* pattern matching */
    ML_CURRY,        /* curried function */
    ML_FIX,          /* fixpoint combinator (Y) */
    ML_DELAY,        /* lazy thunk */
    ML_FORCE,        /* force thunk */
    ML_TYPE_ANNOT    /* type annotation (simply typed) */
} MLExprType;

typedef struct MLExpr {
    MLExprType type;
    union {
        int int_val;
        bool bool_val;
        char var_name[64];
        struct {
            char param[64];
            struct MLExpr *body;
        } lambda;
        struct {
            struct MLExpr *fn;
            struct MLExpr *arg;
        } app;
        struct {
            char name[64];
            struct MLExpr *val;
            struct MLExpr *body;
        } let;
        struct {
            char name[64];
            struct MLExpr *val;
            struct MLExpr *body;
        } letrec;
        struct {
            struct MLExpr *cond;
            struct MLExpr *then_expr;
            struct MLExpr *else_expr;
        } if_expr;
        struct {
            struct MLExpr *car;   /* head */
            struct MLExpr *cdr;   /* tail */
        } cons;
        struct MLExpr *r; /* pointer for car/cdr/force */
        struct {
            char name[64];           /* pattern variable */
            struct MLExpr *pat_expr; /* expression being matched */
            struct MLExpr *body;     /* body if matched */
        } match;
        struct MLExpr *fix_body; /* for fixpoint */
        struct MLExpr *thunk;    /* delayed expression */
        struct {
            char type_name[32];  /* type annotation string */
            struct MLExpr *annotated;
        } type_annot;
    } data;
} MLExpr;

/* ── L3: Value types ────────────────────────────────────────────── */
typedef enum {
    MLV_INT,
    MLV_BOOL,
    MLV_CLOSURE,
    MLV_CONS,      /* pair */
    MLV_NIL,       /* empty list */
    MLV_THUNK      /* lazy/suspended computation */
} MLValueType;

typedef struct MLValue {
    MLValueType type;
    union {
        int int_val;
        bool bool_val;
        struct {
            char param[64];
            struct MLExpr *body;
            struct MLEnv *env;
        } closure;
        struct {
            struct MLValue *car;
            struct MLValue *cdr;
        } cons;
        struct {
            struct MLExpr *body;
            struct MLEnv *env;
        } thunk;
    } data;
} MLValue;

/* ── L3: Environment (lexical scope) ────────────────────────────── */
typedef struct MLEnv {
    char name[64];
    MLValue value;
    struct MLEnv *next;
} MLEnv;

/* ── L4: De Bruijn Indices ────────────────────────────────────────
 * Nameless representation for avoiding variable capture.
 * Reference: de Bruijn, N.G. "Lambda calculus notation with nameless
 *            dummies" Indagationes Mathematicae, 1972.
 */
typedef struct MLDBExpr {
    enum { DB_VAR, DB_ABS, DB_APP, DB_INT, DB_BOOL, DB_IF } type;
    union {
        int index;         /* de Bruijn index (distance to binder) */
        int int_val;
        bool bool_val;
        struct {
            struct MLDBExpr *fn;
            struct MLDBExpr *arg;
        } app;
        struct {
            struct MLDBExpr *body;
        } abs;
        struct {
            struct MLDBExpr *cond;
            struct MLDBExpr *then_expr;
            struct MLDBExpr *else_expr;
        } if_expr;
    } data;
} MLDBExpr;

/* ── L4: Hindley-Milner type inference types ────────────────────── */
typedef enum {
    TY_VAR,    /* type variable */
    TY_ARROW,  /* function type: A → B */
    TY_INT,    /* primitive int */
    TY_BOOL,   /* primitive bool */
    TY_LIST    /* list type */
} MLTypeTag;

typedef struct MLType {
    MLTypeTag tag;
    char var_name[32];   /* for TY_VAR */
    struct MLType *left; /* domain for TY_ARROW */
    struct MLType *right; /* codomain for TY_ARROW */
    /* For TY_LIST: element type in left */
} MLType;

/* ── API Declarations ────────────────────────────────────────────── */

/* L1: Core evaluator */
MLExpr   *ml_parse(const char *source, int *pos);
MLValue   ml_eval(MLExpr *expr, MLEnv *env);

/* L2: Environment */
MLEnv    *ml_env_extend(MLEnv *env, const char *name, MLValue value);
MLValue   ml_env_lookup(MLEnv *env, const char *name);

/* L3: List operations (cons, car, cdr) */
MLValue   ml_cons(MLValue car, MLValue cdr);
MLValue   ml_car(MLValue pair);
MLValue   ml_cdr(MLValue pair);
bool      ml_is_nil(MLValue v);

/* L4: De Bruijn conversion */
MLDBExpr *ml_to_de_bruijn(MLExpr *expr, MLEnv *env);
MLExpr   *ml_from_de_bruijn(MLDBExpr *db, char **names, int *name_count);

/* L4: Hindley-Milner type inference (simplified) */
MLType   *ml_type_infer(MLExpr *expr, MLEnv *type_env);
char     *ml_type_str(MLType *t);
void      ml_type_free(MLType *t);

/* L5: Y combinator */
MLValue   ml_apply_y_combinator(MLExpr *body, MLEnv *env);

/* L5: Pattern matching */
MLValue   ml_match(MLValue value, MLExpr *pattern, MLEnv *env);

/* L7: Partial evaluation (specialization) */
MLValue   ml_partial_eval(MLExpr *expr, MLEnv *env, int depth);

/* L7: Continuation-passing style transformation */
MLExpr   *ml_to_cps(MLExpr *expr);

/* L8: Lazy evaluation (call-by-name thunks) */
MLValue   ml_eval_lazy(MLExpr *expr, MLEnv *env);
MLValue   ml_force(MLValue thunk);

/* Print/debug */
void      ml_print_value(MLValue val);
void      ml_print_expr(MLExpr *expr);
void      ml_print_db_expr(MLDBExpr *db);

/* Memory */
void      ml_free_expr(MLExpr *expr);
void      ml_free_env(MLEnv *env);
void      ml_free_value(MLValue *val);
void      ml_free_db_expr(MLDBExpr *db);

/* REPL */
void      ml_repl(void);

#endif
