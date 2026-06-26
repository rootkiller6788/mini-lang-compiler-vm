#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "fp_closure.h"
#include "oop_vtable.h"
#include "logic_unify.h"
#include "pattern_match.h"
int main(void) {
    printf("A"); fflush(stdout);
    Class* an=class_create("Animal",sizeof(Object)); class_destroy(an);
    printf("B"); fflush(stdout);
    Term* t1=term_create_atom("x"); term_destroy(t1);
    printf("C"); fflush(stdout);
    Pattern* wp=pattern_wild(); MatchValue mv={NULL,0,false};
    Binding bs[PM_MAX_BINDINGS]; int bc=0;
    match_simple(wp,&mv,bs,&bc); pattern_destroy(wp);
    printf("D"); fflush(stdout);
    MatchCase cs[1]; cs[0]=match_case_create(pattern_wild(),(void*)1);
    DTNode* tree=match_compile(cs,1); match_tree_destroy(tree);
    printf("E DONE\n");
    return 0;
}
