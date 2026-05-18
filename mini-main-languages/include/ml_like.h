#ifndef ML_LIKE_H
#define ML_LIKE_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    ML_INT,
    ML_BOOL,
    ML_VAR,
    ML_LAMBDA,
    ML_APP,
    ML_LET,
    ML_LETREC,
    ML_IF
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
    } data;
} MLExpr;

typedef enum {
    MLV_INT,
    MLV_BOOL,
    MLV_CLOSURE
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
    } data;
} MLValue;

typedef struct MLEnv {
    char name[64];
    MLValue value;
    struct MLEnv *next;
} MLEnv;

MLExpr   *ml_parse(const char *source, int *pos);
MLValue   ml_eval(MLExpr *expr, MLEnv *env);
void      ml_print_value(MLValue val);
void      ml_print_expr(MLExpr *expr);
MLEnv    *ml_env_extend(MLEnv *env, const char *name, MLValue value);
MLValue   ml_env_lookup(MLEnv *env, const char *name);
void      ml_free_expr(MLExpr *expr);
void      ml_free_env(MLEnv *env);
void      ml_repl(void);

#endif
