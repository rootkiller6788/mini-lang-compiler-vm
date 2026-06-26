#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdbool.h>
#include "fp_closure.h"
#include "logic_unify.h"
#include "oop_vtable.h"
#include "pattern_match.h"
#include "type_system.h"
#include "lambda_calc.h"
#include "continuation.h"
#include "generic_prog.h"

static void* add_fn(void** args) {
    int* r = malloc(sizeof(int)); *r = *(int*)args[0] + *(int*)args[1]; return r;
}
static void* animal_speak_fn(void* s, void** a) { (void)s;(void)a; return NULL; }
static int cps_result = 0;
static void capture_result(int val, void* ctx) { (void)ctx; cps_result = val; }
static int int_cmp(const void* a, const void* b) {
    int ia=*(const int*)a, ib=*(const int*)b; return (ia>ib)-(ia<ib);
}

int main(void) {
    printf("START\n"); fflush(stdout);
    printf("A"); fflush(stdout);
    
    /* OOP */
    Class* animal = class_create("Animal", sizeof(Object));
    class_add_method(animal, "speak", 0, animal_speak_fn);
    Object* a = object_create(animal);
    object_call_virtual(a, "speak", NULL);
    class_destroy(animal); object_destroy(a);
    printf("B"); fflush(stdout);

    /* FP */
    FPClosure* add = fp_closure_create(add_fn, 2, 0);
    int x=3,y=5; int* sum=(int*)fp_apply(add, (void*[]){&x,&y});
    free(sum); fp_closure_destroy(add);
    printf("C"); fflush(stdout);

    /* Logic */
    Term* t1=term_create_atom("a"), *t2=term_create_atom("a");
    Substitution s=subst_create(); unify(t1,t2,&s);
    term_destroy(t1); term_destroy(t2);
    printf("D"); fflush(stdout);

    /* Pattern */
    Pattern* wp=pattern_wild(); MatchValue mv={NULL,0,false};
    Binding bs[PM_MAX_BINDINGS]; int bc=0;
    match_simple(wp, &mv, bs, &bc);
    pattern_destroy(wp);
    printf("E"); fflush(stdout);

    /* Type */
    Type* tt=type_create_primitive(T_INT);
    type_destroy(tt);
    printf("F"); fflush(stdout);

    /* Lambda */
    LCTerm* ct=lc_church_true();
    lc_destroy(ct);
    printf("G"); fflush(stdout);

    /* CPS */
    cps_add(1,2,capture_result,NULL);
    printf("H"); fflush(stdout);

    /* Generic */
    GVector* v=gvec_create(sizeof(int), free);
    gvec_destroy(v);
    printf("I"); fflush(stdout);

    printf("\nALL DONE\n");
    return 0;
}
