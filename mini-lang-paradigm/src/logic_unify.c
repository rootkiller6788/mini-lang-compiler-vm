#include "logic_unify.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int var_counter = 0;

Term* term_create_var(int id) {
    Term* t = malloc(sizeof(Term));
    if (!t) return NULL;
    t->type = TERM_VAR;
    t->var_id = id;
    t->next = NULL;
    return t;
}

Term* term_create_atom(const char* name) {
    Term* t = malloc(sizeof(Term));
    if (!t) return NULL;
    t->type = TERM_ATOM;
    snprintf(t->atom, LOGIC_MAX_NAME_LEN, "%s", name);
    t->next = NULL;
    return t;
}

Term* term_create_int(int value) {
    Term* t = malloc(sizeof(Term));
    if (!t) return NULL;
    t->type = TERM_INT;
    t->int_val = value;
    t->next = NULL;
    return t;
}

Term* term_create_compound(const char* functor, Term** args, int arity) {
    Term* t = malloc(sizeof(Term));
    if (!t) return NULL;
    t->type = TERM_COMPOUND;
    snprintf(t->compound.functor, LOGIC_MAX_NAME_LEN, "%s", functor);
    t->compound.arity = arity;
    for (int i = 0; i < arity && i < LOGIC_MAX_ARGS; i++) {
        t->compound.args[i] = args[i];
    }
    t->next = NULL;
    return t;
}

Term* term_create_nil(void) {
    Term* t = malloc(sizeof(Term));
    if (!t) return NULL;
    t->type = TERM_NIL;
    t->next = NULL;
    return t;
}

Term* term_clone(const Term* src) {
    if (!src) return NULL;
    Term* t = malloc(sizeof(Term));
    if (!t) return NULL;
    memcpy(t, src, sizeof(Term));
    t->next = NULL;
    if (t->type == TERM_COMPOUND) {
        for (int i = 0; i < t->compound.arity; i++) {
            t->compound.args[i] = term_clone(src->compound.args[i]);
        }
    }
    return t;
}

bool term_equal(const Term* a, const Term* b) {
    if (!a || !b) return a == b;
    if (a->type != b->type) return false;
    switch (a->type) {
    case TERM_VAR:   return a->var_id == b->var_id;
    case TERM_ATOM:  return strcmp(a->atom, b->atom) == 0;
    case TERM_INT:   return a->int_val == b->int_val;
    case TERM_NIL:   return true;
    case TERM_COMPOUND:
        if (strcmp(a->compound.functor, b->compound.functor) != 0) return false;
        if (a->compound.arity != b->compound.arity) return false;
        for (int i = 0; i < a->compound.arity; i++) {
            if (!term_equal(a->compound.args[i], b->compound.args[i])) return false;
        }
        return true;
    }
    return false;
}

void term_print(const Term* t) {
    if (!t) { printf("NULL"); return; }
    switch (t->type) {
    case TERM_VAR:   printf("X%d", t->var_id); break;
    case TERM_ATOM:  printf("%s", t->atom); break;
    case TERM_INT:   printf("%d", t->int_val); break;
    case TERM_NIL:   printf("nil"); break;
    case TERM_COMPOUND:
        printf("%s(", t->compound.functor);
        for (int i = 0; i < t->compound.arity; i++) {
            term_print(t->compound.args[i]);
            if (i < t->compound.arity - 1) printf(", ");
        }
        printf(")");
        break;
    }
}

void term_destroy(Term* t) {
    if (!t) return;
    if (t->type == TERM_COMPOUND) {
        for (int i = 0; i < t->compound.arity; i++) {
            term_destroy(t->compound.args[i]);
        }
    }
    free(t);
}

Substitution subst_create(void) {
    Substitution s;
    memset(s.bindings, 0, sizeof(s.bindings));
    return s;
}

bool subst_lookup(const Substitution* s, int var_id, Term** result) {
    if (var_id >= 0 && var_id < LOGIC_MAX_BINDINGS && s->bindings[var_id]) {
        *result = s->bindings[var_id];
        return true;
    }
    return false;
}

Substitution subst_extend(Substitution s, int var_id, Term* term) {
    if (var_id >= 0 && var_id < LOGIC_MAX_BINDINGS) {
        s.bindings[var_id] = term;
    }
    return s;
}

void subst_print(const Substitution* s) {
    printf("{ ");
    bool first = true;
    for (int i = 0; i < LOGIC_MAX_BINDINGS; i++) {
        if (s->bindings[i]) {
            if (!first) printf(", ");
            printf("X%d -> ", i);
            term_print(s->bindings[i]);
            first = false;
        }
    }
    printf(" }");
}

bool occurs_check(int var_id, Term* t) {
    if (!t) return false;
    switch (t->type) {
    case TERM_VAR: return t->var_id == var_id;
    case TERM_ATOM: case TERM_INT: case TERM_NIL: return false;
    case TERM_COMPOUND:
        for (int i = 0; i < t->compound.arity; i++) {
            if (occurs_check(var_id, t->compound.args[i])) return true;
        }
        return false;
    }
    return false;
}

bool unify(Term* a, Term* b, Substitution* result) {
    Term* ta = subst_apply(result, a);
    Term* tb = subst_apply(result, b);
    if (!ta || !tb) return false;
    if (term_equal(ta, tb)) return true;
    if (ta->type == TERM_VAR) {
        if (occurs_check(ta->var_id, tb)) return false;
        *result = subst_extend(*result, ta->var_id, tb);
        return true;
    }
    if (tb->type == TERM_VAR) {
        if (occurs_check(tb->var_id, ta)) return false;
        *result = subst_extend(*result, tb->var_id, ta);
        return true;
    }
    if (ta->type == TERM_COMPOUND && tb->type == TERM_COMPOUND) {
        if (strcmp(ta->compound.functor, tb->compound.functor) != 0) return false;
        if (ta->compound.arity != tb->compound.arity) return false;
        for (int i = 0; i < ta->compound.arity; i++) {
            if (!unify(ta->compound.args[i], tb->compound.args[i], result)) return false;
        }
        return true;
    }
    return false;
}

Term* subst_apply(Substitution* s, Term* t) {
    if (!t) return NULL;
    if (t->type == TERM_VAR) {
        Term* bound = NULL;
        if (subst_lookup(s, t->var_id, &bound)) {
            return subst_apply(s, bound);
        }
        return t;
    }
    if (t->type == TERM_COMPOUND) {
        Term* result = malloc(sizeof(Term));
        memcpy(result, t, sizeof(Term));
        result->next = NULL;
        for (int i = 0; i < t->compound.arity; i++) {
            result->compound.args[i] = subst_apply(s, t->compound.args[i]);
        }
        return result;
    }
    return t;
}

Substitution subst_compose(Substitution* s1, Substitution* s2) {
    Substitution result = subst_create();
    for (int i = 0; i < LOGIC_MAX_BINDINGS; i++) {
        if (s2->bindings[i]) {
            result.bindings[i] = s2->bindings[i];
        } else if (s1->bindings[i]) {
            result.bindings[i] = subst_apply(s2, s1->bindings[i]);
        }
    }
    return result;
}

Clause clause_create(Term* head, Term** body, int body_count) {
    Clause c;
    c.head = head;
    c.body_count = body_count;
    for (int i = 0; i < body_count && i < LOGIC_MAX_ARGS; i++) {
        c.body[i] = body[i];
    }
    return c;
}

LogicProgram logic_program_create(const char* name) {
    LogicProgram prog;
    prog.clause_count = 0;
    snprintf(prog.name, LOGIC_MAX_NAME_LEN, "%s", name);
    return prog;
}

void logic_program_add_clause(LogicProgram* prog, Clause c) {
    if (prog->clause_count < LOGIC_MAX_CLAUSES) {
        prog->clauses[prog->clause_count++] = c;
    }
}

static bool logic_solve_inner(const LogicProgram* prog, Term** goals, int goal_count,
                               Substitution current, Substitution* result, int depth) {
    if (depth > LOGIC_MAX_DEPTH) return false;
    if (goal_count == 0) {
        *result = current;
        return true;
    }
    Term* goal = subst_apply(&current, goals[0]);
    for (int ci = 0; ci < prog->clause_count; ci++) {
        Clause clause = prog->clauses[ci];
        Term* head_copy = term_clone(clause.head);
        Substitution trial = current;
        if (unify(goal, head_copy, &trial)) {
            int new_body_count = clause.body_count + goal_count - 1;
            Term* new_goals[LOGIC_MAX_ARGS * 2];
            for (int i = 0; i < clause.body_count; i++) {
                new_goals[i] = subst_apply(&trial, clause.body[i]);
            }
            for (int i = 1; i < goal_count; i++) {
                new_goals[clause.body_count + i - 1] = subst_apply(&trial, goals[i]);
            }
            if (logic_solve_inner(prog, new_goals, new_body_count, trial, result, depth + 1)) {
                term_destroy(head_copy);
                return true;
            }
        }
        term_destroy(head_copy);
    }
    return false;
}

bool logic_solve(const LogicProgram* prog, Term* goal, Substitution* result) {
    Term* goals[] = { goal };
    Substitution init = subst_create();
    return logic_solve_inner(prog, goals, 1, init, result, 0);
}

void logic_program_print(const LogicProgram* prog) {
    printf("LogicProgram: %s (%d clauses)\n", prog->name, prog->clause_count);
    for (int i = 0; i < prog->clause_count; i++) {
        term_print(prog->clauses[i].head);
        if (prog->clauses[i].body_count > 0) {
            printf(" :- ");
            for (int j = 0; j < prog->clauses[i].body_count; j++) {
                term_print(prog->clauses[i].body[j]);
                if (j < prog->clauses[i].body_count - 1) printf(", ");
            }
        }
        printf(".\n");
    }
}

void logic_destroy_program(LogicProgram* prog) {
    for (int i = 0; i < prog->clause_count; i++) {
        term_destroy(prog->clauses[i].head);
        for (int j = 0; j < prog->clauses[i].body_count; j++) {
            term_destroy(prog->clauses[i].body[j]);
        }
    }
    (void)prog;
}
