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

static int tests_run = 0;
static int tests_passed = 0;

#define TEST(name) do { tests_run++; printf("  TEST %s... ", name); } while(0)
#define PASS() do { tests_passed++; printf("PASS\n"); } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); } while(0)
#define CHECK(cond, msg) do { if (cond) PASS(); else FAIL(msg); } while(0)

/* ---- File-scope helpers (no nested functions for Windows DEP compat) ---- */

static int speak_count = 0;
static void* animal_speak(void* self, void** args) {
    (void)self; (void)args;
    speak_count++;
    return NULL;
}
static void* dog_speak(void* self, void** args) {
    (void)self; (void)args;
    speak_count += 10;
    return NULL;
}

static void* add_fn(void** args) {
    int* result = malloc(sizeof(int));
    if (!result) return NULL;
    int a = *(int*)args[0];
    int b = *(int*)args[1];
    *result = a + b;
    return result;
}

static void* fold_add_helper(void* acc, void* val) {
    *(int*)acc += *(int*)val;
    return acc;
}

static bool is_even_pred(void* v) {
    return (*(int*)v % 2) == 0;
}

/* ---- OOP Vtable Tests ---- */

static void test_oop_vtable(void) {
    printf("\n[OOP Vtable]\n");

    TEST("class_create");
    Class* animal = class_create("Animal", sizeof(Object));
    CHECK(animal != NULL, "class_create returned NULL");

    TEST("class_add_method");
    class_add_method(animal, "speak", 0, animal_speak);
    CHECK(animal->vtable_size == 1, "vtable_size wrong");

    TEST("object_create");
    Object* a = object_create(animal);
    CHECK(a != NULL, "object_create returned NULL");

    TEST("object_call_virtual");
    speak_count = 0;
    object_call_virtual(a, "speak", NULL);
    CHECK(speak_count == 1, "virtual dispatch failed");

    TEST("class_inherit");
    Class* dog = class_inherit("Dog", animal);
    CHECK(dog != NULL && dog->parent_class == animal, "inheritance failed");

    TEST("class_override_method");
    class_override_method(dog, "speak", dog_speak);
    Object* d = object_create(dog);
    speak_count = 0;
    object_call_virtual(d, "speak", NULL);
    CHECK(speak_count == 10, "override dispatch failed");

    class_destroy(animal);
    class_destroy(dog);
    object_destroy(a);
    object_destroy(d);
}

/* ---- FP Closure Tests ---- */

static void test_fp_closure(void) {
    printf("\n[FP Closure]\n");

    TEST("fp_closure_create and apply");
    FPClosure* add = fp_closure_create(add_fn, 2, 0);
    int a = 3, b = 5;
    int* sum = (int*)fp_apply(add, (void*[]){&a, &b});
    CHECK(sum && *sum == 8, "closure apply failed");
    free(sum);
    fp_closure_destroy(add);

    TEST("fp_cons and fp_list_length");
    int vals[] = {1, 2, 3, 4, 5};
    FPList* list = NULL;
    for (int i = 4; i >= 0; i--) {
        list = fp_cons(&vals[i], list);
    }
    CHECK(fp_list_length(list) == 5, "list length wrong");

    TEST("fp_foldl");
    int zero = 0;
    (void)fp_foldl(fold_add_helper, &zero, list);
    CHECK(zero == 15, "foldl sum failed");

    TEST("fp_list_reverse");
    FPList* rev = fp_list_reverse(list);
    CHECK(fp_list_length(rev) == 5, "reverse length wrong");

    TEST("fp_filter");
    FPList* evens = fp_filter(is_even_pred, list);
    CHECK(fp_list_length(evens) == 2, "filter even failed");

    fp_list_destroy(list);
    fp_list_destroy(rev);
    fp_list_destroy(evens);
}

/* ---- Logic Unify Tests ---- */

static void test_logic_unify(void) {
    printf("\n[Logic Unify]\n");

    TEST("unify atoms");
    Term* a1 = term_create_atom("hello");
    Term* a2 = term_create_atom("hello");
    Substitution s = subst_create();
    bool ok = unify(a1, a2, &s);
    CHECK(ok, "identical atoms should unify");

    TEST("unify var with atom");
    Term* X = term_create_var(0);
    Term* atom_john = term_create_atom("john");
    Substitution s2 = subst_create();
    ok = unify(X, atom_john, &s2);
    CHECK(ok && s2.bindings[0] != NULL, "var-atom unification failed");

    TEST("occurs check");
    Term* fX = term_create_compound("f", (Term*[]){X}, 1);
    bool occurs = occurs_check(0, fX);
    CHECK(occurs, "occurs check should detect cycle");

    TEST("logic_solve simple");
    LogicProgram prog = logic_program_create("test");
    Term* fact_head = term_create_compound("parent",
        (Term*[]){term_create_atom("john"), term_create_atom("mary")}, 2);
    Clause c = clause_create(fact_head, NULL, 0);
    logic_program_add_clause(&prog, c);
    Term* goal = term_create_compound("parent",
        (Term*[]){term_create_var(0), term_create_atom("mary")}, 2);
    Substitution result;
    ok = logic_solve(&prog, goal, &result);
    CHECK(ok, "logic solver should find solution");

    term_destroy(a1); term_destroy(a2);
    term_destroy(fX);
    logic_destroy_program(&prog);
}

/* ---- Pattern Match Tests ---- */

static void test_pattern_match(void) {
    printf("\n[Pattern Match]\n");

    TEST("pattern_wild match");
    Pattern* wp = pattern_wild();
    MatchValue mv = {NULL, 42, false};
    Binding bindings[PM_MAX_BINDINGS];
    int bc = 0;
    bool ok = match_simple(wp, &mv, bindings, &bc);
    CHECK(ok && mv.matched, "wild should match anything");

    TEST("pattern_int exact");
    Pattern* ip = pattern_int(42);
    mv.value = 42; mv.matched = false;
    ok = match_simple(ip, &mv, bindings, &bc);
    CHECK(ok, "int pattern exact match failed");

    TEST("pattern_int mismatch");
    mv.value = 99; mv.matched = false;
    ok = match_simple(ip, &mv, bindings, &bc);
    CHECK(!ok, "int pattern should not match 99 != 42");

    TEST("match_compile case 1");
    MatchCase cases[2];
    cases[0] = match_case_create(pattern_int(1), (void*)100);
    cases[1] = match_case_create(pattern_wild(), (void*)200);
    DTNode* tree = match_compile(cases, 2);
    mv.value = 1;
    void* result = match_execute(tree, &mv);
    CHECK(result == (void*)100, "decision tree case 1 failed");

    TEST("match_compile wildcard fallback");
    mv.value = 999;
    result = match_execute(tree, &mv);
    CHECK(result == (void*)200, "decision tree wild failed");

    pattern_destroy(wp);
    pattern_destroy(ip);
    match_tree_destroy(tree);
}

/* ---- Type System Tests ---- */

static void test_type_system(void) {
    printf("\n[Type System]\n");

    TEST("type_unify primitives");
    Type* t1 = type_create_primitive(T_INT);
    Type* t2 = type_create_primitive(T_INT);
    TypeSubst ts = type_subst_create();
    bool ok = type_unify(t1, t2, &ts);
    CHECK(ok, "identical primitives should unify");

    TEST("type_unify var with primitive");
    Type* tv = type_create_var(0);
    TypeSubst ts2 = type_subst_create();
    ok = type_unify(tv, t1, &ts2);
    CHECK(ok && ts2.substitution[0] != NULL, "var-primitive unification failed");

    TEST("HM inference identity");
    TypeEnv env = type_env_create();
    Expr* id = expr_create_lambda("x", expr_create_var("x"));
    TypeSubst subst = type_subst_create();
    Type* inferred = type_infer_hm(&env, id, &subst);
    CHECK(inferred != NULL && inferred->tag == T_FUNC, "HM should infer function type");

    TEST("HM inference int literal");
    Expr* int_expr = expr_create_int(42);
    Type* int_type = type_infer_hm(&env, int_expr, &subst);
    CHECK(int_type != NULL && int_type->tag == T_INT, "int literal should be Int");

    type_destroy(t1); type_destroy(t2);
    type_destroy(tv);
    expr_destroy(id); expr_destroy(int_expr);
}

/* ---- Lambda Calculus Tests ---- */

static void test_lambda_calc(void) {
    printf("\n[Lambda Calculus]\n");

    TEST("Church true");
    LCTerm* ctrue = lc_church_true();
    CHECK(lc_church_to_bool(ctrue) == true, "Church true should be true");
    lc_destroy(ctrue);

    TEST("Church false");
    LCTerm* cfalse = lc_church_false();
    CHECK(lc_church_to_bool(cfalse) == false, "Church false should be false");
    lc_destroy(cfalse);

    TEST("Church numeral 3");
    LCTerm* three = lc_church_numeral(3);
    CHECK(lc_church_to_int(three) == 3, "Church 3 should decode to 3");
    lc_destroy(three);

    TEST("Church numeral 5");
    LCTerm* five = lc_church_numeral(5);
    CHECK(lc_church_to_int(five) == 5, "Church 5 should decode to 5");
    lc_destroy(five);

    TEST("SKI combinators exist");
    LCTerm* I = lc_combinator_I();
    LCTerm* K = lc_combinator_K();
    LCTerm* S = lc_combinator_S();
    CHECK(I != NULL && K != NULL && S != NULL, "SKI combinators should exist");

    TEST("beta reduction (I x -> x)");
    /* I = L.L.0, Ix = (I 0), beta reduces to 0.
     * Note: lc_app shares pointers, so we don't destroy I separately.
     * lc_destroy on the app will recurse into fn and arg. */
    LCTerm* I2 = lc_combinator_I();
    LCTerm* Ix = lc_app(I2, lc_var(0));
    LCTerm* reduced = lc_beta_reduce(Ix);
    CHECK(reduced != NULL, "I x should reduce");
    lc_destroy(reduced);
    lc_destroy(Ix);
    /* I2 was freed by lc_destroy(Ix) since it recurses into fn */

    lc_destroy(I); lc_destroy(K); lc_destroy(S);
}

/* ---- CPS / Continuation Tests ---- */

static int cps_result = 0;
static void capture_result(int val, void* ctx) {
    (void)ctx;
    cps_result = val;
}

static void test_continuation(void) {
    printf("\n[Continuation / CPS]\n");

    TEST("cps_add");
    cps_add(3, 7, capture_result, NULL);
    CHECK(cps_result == 10, "CPS add failed");

    TEST("cps_factorial");
    cps_factorial(5, capture_result, NULL);
    CHECK(cps_result == 120, "CPS factorial(5) should be 120");

    TEST("cps_fibonacci");
    cps_fibonacci(10, capture_result, NULL);
    CHECK(cps_result == 55, "CPS fib(10) should be 55");

    TEST("CPS expression eval");
    /* (2 + 3) * 4 = 20 */
    CExpr* e = cexpr_mul(cexpr_add(cexpr_int(2), cexpr_int(3)), cexpr_int(4));
    cexpr_cps_eval(e, capture_result, NULL);
    CHECK(cps_result == 20, "CPS expr eval failed");
    cexpr_destroy(e);
}

/* ---- Generic Programming Tests ---- */

static int int_cmp(const void* a, const void* b) {
    int ia = *(const int*)a, ib = *(const int*)b;
    return (ia > ib) - (ia < ib);
}

static void test_generic_prog(void) {
    printf("\n[Generic Programming]\n");

    TEST("gvec_create and push");
    GVector* v = gvec_create(sizeof(int), free);
    int* p1 = malloc(sizeof(int)); *p1 = 42;
    int* p2 = malloc(sizeof(int)); *p2 = -7;
    int* p3 = malloc(sizeof(int)); *p3 = 99;
    gvec_push(v, p1); gvec_push(v, p2); gvec_push(v, p3);
    CHECK(gvec_len(v) == 3, "vector length should be 3");

    TEST("gvec_sort");
    gvec_sort(v, int_cmp);
    int* first = (int*)gvec_get(v, 0);
    int* mid = (int*)gvec_get(v, 1);
    int* last = (int*)gvec_get(v, 2);
    CHECK(*first == -7 && *mid == 42 && *last == 99, "sort order wrong");

    TEST("gbst_insert and search");
    GBST* t = gbst_create(int_cmp, free, free);
    int* k1 = malloc(sizeof(int)); *k1 = 10;
    int* v1 = malloc(sizeof(int)); *v1 = 100;
    int* k2 = malloc(sizeof(int)); *k2 = 20;
    int* v2 = malloc(sizeof(int)); *v2 = 200;
    gbst_insert(t, k1, v1);
    gbst_insert(t, k2, v2);
    CHECK(gbst_contains(t, k1) && gbst_contains(t, k2), "BST insert/contains failed");

    TEST("gbst_search");
    int search_key = 10;
    int* found = (int*)gbst_search(t, &search_key);
    CHECK(found && *found == 100, "BST search returned wrong value");

    gvec_destroy(v);
    gbst_destroy(t);
}

int main(void) {
    printf("=== mini-lang-paradigm Test Suite ===\n");

    test_oop_vtable();
    test_fp_closure();
    test_logic_unify();
    test_pattern_match();
    test_type_system();
    test_lambda_calc();
    test_continuation();
    test_generic_prog();

    printf("\n=== Results: %d/%d checks passed ===\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
