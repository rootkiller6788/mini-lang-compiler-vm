#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "fp_closure.h"
#include "oop_vtable.h"
#include "logic_unify.h"
#include "pattern_match.h"
#include "type_system.h"
int main(void) {
    printf("A"); fflush(stdout);
    Class* an=class_create("Animal",sizeof(Object)); class_destroy(an);
    printf("B"); fflush(stdout);
    Term* t1=term_create_atom("x"); term_destroy(t1);
    printf("C"); fflush(stdout);
    Pattern* wp=pattern_wild(); pattern_destroy(wp);
    printf("D"); fflush(stdout);
    Type* tt=type_create_primitive(T_INT);
    TypeSubst ts=type_subst_create();
    Type* tv=type_create_var(0);
    type_unify(tv,tt,&ts);
    type_destroy(tt); type_destroy(tv);
    printf("E"); fflush(stdout);
    TypeEnv env=type_env_create();
    Expr* id=expr_create_lambda("x",expr_create_var("x"));
    TypeSubst sub=type_subst_create();
    type_infer_hm(&env,id,&sub);
    expr_destroy(id);
    printf("F DONE\n");
    return 0;
}
