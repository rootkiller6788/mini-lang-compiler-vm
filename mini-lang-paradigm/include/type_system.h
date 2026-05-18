#ifndef TYPE_SYSTEM_H
#define TYPE_SYSTEM_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define TYPE_MAX_VARS      64
#define TYPE_MAX_BINDINGS  256
#define TYPE_MAX_NAME_LEN  32

typedef enum {
    T_INT,
    T_BOOL,
    T_STRING,
    T_ARRAY,
    T_FUNC,
    T_VOID,
    T_VAR,
    T_FLOAT,
    T_PAIR,
    T_LIST
} TypeTag;

typedef struct Type Type;
struct Type {
    TypeTag tag;
    int     var_id;
    Type*   element_type;
    Type*   param_type;
    Type*   return_type;
    Type*   left_type;
    Type*   right_type;
    char    name[TYPE_MAX_NAME_LEN];
    Type*   next;
};

typedef struct {
    Type* binding[TYPE_MAX_BINDINGS];
} TypeEnv;

typedef struct {
    Type* substitution[TYPE_MAX_VARS];
} TypeSubst;

typedef enum {
    EXPR_VAR,
    EXPR_INT,
    EXPR_BOOL,
    EXPR_LAMBDA,
    EXPR_APPLY,
    EXPR_LET,
    EXPR_IFTHENELSE,
    EXPR_BINOP,
    EXPR_FIX
} ExprTag;

typedef struct Expr Expr;
struct Expr {
    ExprTag tag;
    union {
        int  int_val;
        bool bool_val;
        char var_name[TYPE_MAX_NAME_LEN];
        struct {
            char  param[TYPE_MAX_NAME_LEN];
            Expr* body;
        } lambda;
        struct {
            Expr* fn;
            Expr* arg;
        } apply;
        struct {
            char  var[TYPE_MAX_NAME_LEN];
            Expr* def;
            Expr* body;
        } let;
        struct {
            Expr* cond;
            Expr* then_branch;
            Expr* else_branch;
        } ifthenelse;
        struct {
            int   op;
            Expr* left;
            Expr* right;
        } binop;
        struct {
            Expr* body;
        } fix;
    };
};

Type*    type_create_primitive(TypeTag tag);
Type*    type_create_var(int id);
Type*    type_create_func(Type* param, Type* ret);
Type*    type_create_pair(Type* left, Type* right);
Type*    type_create_list(Type* elem);
void     type_print(const Type* t);
bool     type_equal(const Type* a, const Type* b);
Type*    type_clone(Type* t);
void     type_destroy(Type* t);

TypeSubst type_subst_create(void);
Type*     type_subst_apply(TypeSubst* s, Type* t);
TypeSubst type_subst_compose(TypeSubst* s1, TypeSubst* s2);
bool      type_occurs_check(int var_id, Type* t);
bool      type_unify(Type* a, Type* b, TypeSubst* result);

TypeEnv  type_env_create(void);
void     type_env_extend(TypeEnv* env, const char* name, Type* t);
Type*    type_env_lookup(TypeEnv* env, const char* name);
Type*    type_infer_hm(TypeEnv* env, Expr* expr, TypeSubst* subst);

Expr*    expr_create_var(const char* name);
Expr*    expr_create_int(int value);
Expr*    expr_create_bool(bool value);
Expr*    expr_create_lambda(const char* param, Expr* body);
Expr*    expr_create_apply(Expr* fn, Expr* arg);
Expr*    expr_create_let(const char* var, Expr* def, Expr* body);
void     expr_destroy(Expr* e);
void     expr_print(const Expr* e);

#endif
