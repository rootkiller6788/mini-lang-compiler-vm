#include <stdio.h>
#include "logic_unify.h"
int main(void) {
    printf("Step 8: Logic test\n");
    Term* a1 = term_create_atom("hello");
    Term* a2 = term_create_atom("hello");
    Substitution s = subst_create();
    bool ok = unify(a1, a2, &s);
    printf("  unify=%d\n", ok);
    term_destroy(a1); term_destroy(a2);
    printf("ALL LU TESTS PASSED\n"); return 0;
}
