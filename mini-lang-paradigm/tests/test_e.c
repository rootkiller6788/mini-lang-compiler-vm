#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "fp_closure.h"
#include "oop_vtable.h"
#include "logic_unify.h"
#include "pattern_match.h"
#include "type_system.h"
#include "lambda_calc.h"
int main(void) {
    printf("A"); fflush(stdout);
    Class* an=class_create("Animal",sizeof(Object)); class_destroy(an);
    printf("B"); fflush(stdout);
    Term* t1=term_create_atom("x"); term_destroy(t1);
    printf("C"); fflush(stdout);
    Pattern* wp=pattern_wild(); pattern_destroy(wp);
    printf("D"); fflush(stdout);
    Type* tt=type_create_primitive(T_INT); type_destroy(tt);
    printf("E"); fflush(stdout);
    LCTerm* ct=lc_church_true(); lc_destroy(ct);
    LCTerm* three=lc_church_numeral(3);
    printf(" num=%d", lc_church_to_int(three));
    lc_destroy(three);
    printf("F"); fflush(stdout);
    LCTerm* I=lc_combinator_I();
    LCTerm* Ix=lc_app(I,lc_var(0));
    LCTerm* red=lc_beta_reduce(Ix);
    lc_destroy(I); lc_destroy(Ix); lc_destroy(red);
    printf("G DONE\n");
    return 0;
}
