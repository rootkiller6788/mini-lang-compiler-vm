#ifndef LOGIC_UNIFY_H
#define LOGIC_UNIFY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LOGIC_MAX_ARGS      16
#define LOGIC_MAX_BINDINGS  256
#define LOGIC_MAX_CLAUSES   128
#define LOGIC_MAX_NAME_LEN  64
#define LOGIC_MAX_DEPTH     64

typedef enum {
    TERM_VAR,
    TERM_ATOM,
    TERM_COMPOUND,
    TERM_INT,
    TERM_NIL
} TermType;

typedef struct Term Term;
struct Term {
    TermType type;
    union {
        int   var_id;
        char  atom[LOGIC_MAX_NAME_LEN];
        int   int_val;
        struct {
            char functor[LOGIC_MAX_NAME_LEN];
            Term* args[LOGIC_MAX_ARGS];
            int   arity;
        } compound;
    };
    Term* next;
};

typedef struct {
    Term* bindings[LOGIC_MAX_BINDINGS];
} Substitution;

typedef struct {
    Term* head;
    Term* body[LOGIC_MAX_ARGS];
    int   body_count;
} Clause;

typedef struct {
    Clause clauses[LOGIC_MAX_CLAUSES];
    int    clause_count;
    char   name[LOGIC_MAX_NAME_LEN];
} LogicProgram;

Term*        term_create_var(int id);
Term*        term_create_atom(const char* name);
Term*        term_create_int(int value);
Term*        term_create_compound(const char* functor, Term** args, int arity);
Term*        term_create_nil(void);
Term*        term_clone(const Term* src);
bool         term_equal(const Term* a, const Term* b);
void         term_print(const Term* t);
void         term_destroy(Term* t);

Substitution subst_create(void);
bool         subst_lookup(const Substitution* s, int var_id, Term** result);
Substitution subst_extend(Substitution s, int var_id, Term* term);
void         subst_print(const Substitution* s);

bool         unify(Term* a, Term* b, Substitution* result);
Term*        subst_apply(Substitution* s, Term* t);
Substitution subst_compose(Substitution* s1, Substitution* s2);
bool         occurs_check(int var_id, Term* t);

Clause       clause_create(Term* head, Term** body, int body_count);
LogicProgram logic_program_create(const char* name);
void         logic_program_add_clause(LogicProgram* prog, Clause c);
bool         logic_solve(const LogicProgram* prog, Term* goal, Substitution* result);
void         logic_program_print(const LogicProgram* prog);
void         logic_destroy_program(LogicProgram* prog);

#endif
