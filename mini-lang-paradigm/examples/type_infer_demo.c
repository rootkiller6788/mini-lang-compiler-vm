#include "type_system.h"

#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("=== Hindley-Milner Type Inference Demo ===\n\n");

    printf("--- Expression 1: identity = lambda x. x ---\n");
    Expr* id_body = expr_create_var("x");
    Expr* id_lambda = expr_create_lambda("x", id_body);
    printf("Expression: ");
    expr_print(id_lambda);
    printf("\n");

    TypeEnv env = type_env_create();
    TypeSubst subst = type_subst_create();
    Type* id_type = type_infer_hm(&env, id_lambda, &subst);
    id_type = type_subst_apply(&subst, id_type);
    printf("Inferred type: ");
    type_print(id_type);
    printf("\n\n");

    printf("--- Expression 2: apply = lambda f. lambda x. f x ---\n");
    Expr* fx  = expr_create_apply(expr_create_var("f"), expr_create_var("x"));
    Expr* lx  = expr_create_lambda("x", fx);
    Expr* lf  = expr_create_lambda("f", lx);
    printf("Expression: ");
    expr_print(lf);
    printf("\n");

    TypeEnv env2 = type_env_create();
    TypeSubst subst2 = type_subst_create();
    Type* apply_type = type_infer_hm(&env2, lf, &subst2);
    apply_type = type_subst_apply(&subst2, apply_type);
    printf("Inferred type: ");
    type_print(apply_type);
    printf("\n\n");

    printf("--- Expression 3: let id = lambda x. x in id 42 ---\n");
    Expr* inner_id = expr_create_lambda("x", expr_create_var("x"));
    Expr* apply_id = expr_create_apply(expr_create_var("id"), expr_create_int(42));
    Expr* let_expr = expr_create_let("id", inner_id, apply_id);
    printf("Expression: ");
    expr_print(let_expr);
    printf("\n");

    TypeEnv env3 = type_env_create();
    TypeSubst subst3 = type_subst_create();
    Type* let_type = type_infer_hm(&env3, let_expr, &subst3);
    let_type = type_subst_apply(&subst3, let_type);
    printf("Inferred type: ");
    type_print(let_type);
    printf("\n\n");

    printf("--- Expression 4: compose = lambda f. lambda g. lambda x. f (g x) ---\n");
    Expr* gx    = expr_create_apply(expr_create_var("g"), expr_create_var("x"));
    Expr* fgx   = expr_create_apply(expr_create_var("f"), gx);
    Expr* lx3   = expr_create_lambda("x", fgx);
    Expr* lg    = expr_create_lambda("g", lx3);
    Expr* compose_expr = expr_create_lambda("f", lg);
    printf("Expression: ");
    expr_print(compose_expr);
    printf("\n");

    TypeEnv env4 = type_env_create();
    TypeSubst subst4 = type_subst_create();
    Type* compose_type = type_infer_hm(&env4, compose_expr, &subst4);
    compose_type = type_subst_apply(&subst4, compose_type);
    printf("Inferred type: ");
    type_print(compose_type);
    printf("\n\n");

    printf("--- Expression 5: const = lambda x. lambda y. x ---\n");
    Expr* ly2  = expr_create_lambda("y", expr_create_var("x"));
    Expr* k_expr = expr_create_lambda("x", ly2);
    printf("Expression: ");
    expr_print(k_expr);
    printf("\n");

    TypeEnv env5 = type_env_create();
    TypeSubst subst5 = type_subst_create();
    Type* k_type = type_infer_hm(&env5, k_expr, &subst5);
    k_type = type_subst_apply(&subst5, k_type);
    printf("Inferred type: ");
    type_print(k_type);
    printf("\n\n");

    expr_destroy(id_lambda);
    expr_destroy(lf);
    expr_destroy(let_expr);
    expr_destroy(compose_expr);
    expr_destroy(k_expr);

    printf("Done.\n");
    return 0;
}
