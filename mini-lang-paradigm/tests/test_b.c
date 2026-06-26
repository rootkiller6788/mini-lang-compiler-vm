#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "fp_closure.h"
#include "oop_vtable.h"
#include "logic_unify.h"
int main(void) {
    printf("A"); fflush(stdout);
    Class* an=class_create("Animal",sizeof(Object));
    class_destroy(an);
    printf("B"); fflush(stdout);
    Term* t1=term_create_atom("x"),*t2=term_create_atom("x");
    Substitution su=subst_create(); unify(t1,t2,&su);
    term_destroy(t1); term_destroy(t2);
    printf("C"); fflush(stdout);
    Term* X=term_create_var(0);
    Term* fX=term_create_compound("f",(Term*[]){X},1);
    term_destroy(fX);
    printf("D DONE\n");
    return 0;
}
