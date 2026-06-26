#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "logic_unify.h"

int main(void) {
    printf("Logic full test\n"); fflush(stdout);
    
    printf("1"); fflush(stdout);
    Term* a1 = term_create_atom("hello");
    Term* a2 = term_create_atom("hello");
    Substitution s = subst_create();
    bool ok = unify(a1, a2, &s);
    printf(" unify_atoms=%d", ok); fflush(stdout);
    term_destroy(a1); term_destroy(a2);
    
    printf(" 2"); fflush(stdout);
    Term* X = term_create_var(0);
    Term* atom_john = term_create_atom("john");
    Substitution s2 = subst_create();
    ok = unify(X, atom_john, &s2);
    printf(" unify_var=%d", ok); fflush(stdout);
    
    printf(" 3"); fflush(stdout);
    /* Re-create X since we need it for the compound */
    Term* X2 = term_create_var(0);
    Term* fX = term_create_compound("f", (Term*[]){X2}, 1);
    bool occurs = occurs_check(0, fX);
    printf(" occurs=%d", occurs); fflush(stdout);
    term_destroy(fX);
    
    printf(" 4"); fflush(stdout);
    LogicProgram prog = logic_program_create("test");
    Term* fact_head = term_create_compound("parent",
        (Term*[]){term_create_atom("john"), term_create_atom("mary")}, 2);
    Clause c = clause_create(fact_head, NULL, 0);
    logic_program_add_clause(&prog, c);
    Term* goal = term_create_compound("parent",
        (Term*[]){term_create_var(0), term_create_atom("mary")}, 2);
    Substitution result;
    ok = logic_solve(&prog, goal, &result);
    printf(" solve=%d", ok); fflush(stdout);
    
    logic_destroy_program(&prog);
    
    printf("\nLOGIC DONE\n");
    return 0;
}
