#ifndef LAMBDA_CALC_H
#define LAMBDA_CALC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define LC_MAX_VARS       256
#define LC_MAX_DEPTH      1024
#define LC_MAX_NAME_LEN   32

/* L1: Definitions — Lambda calculus AST with de Bruijn indices.
 * de Bruijn indices eliminate alpha-conversion by replacing variable names
 * with natural numbers counting binders from occurrence to binder.
 * Reference: Nicolaas Govert de Bruijn (1972). */

typedef enum {
    LC_VAR,
    LC_ABS,
    LC_APP
} LCTag;

typedef struct LCTerm LCTerm;
struct LCTerm {
    LCTag    tag;
    union {
        int  index;
        struct { LCTerm* body; } abs;
        struct { LCTerm* fn; LCTerm* arg; } app;
    };
    LCTerm* next;
};

LCTerm* lc_var(int index);
LCTerm* lc_abs(LCTerm* body);
LCTerm* lc_app(LCTerm* fn, LCTerm* arg);

/* L2: Church encodings — data as functions.
 * Reference: Alonzo Church (1936). */
LCTerm* lc_church_true(void);
LCTerm* lc_church_false(void);
LCTerm* lc_church_numeral(int n);
LCTerm* lc_church_succ(void);
LCTerm* lc_church_add(void);
LCTerm* lc_church_mul(void);
LCTerm* lc_church_pair(void);
LCTerm* lc_church_fst(void);
LCTerm* lc_church_snd(void);

/* L2: SKI combinators — Combinatory Logic.
 * Proved by Schönfinkel (1924) and Curry: S and K suffice for all
 * computable functions. I = SKK is derivable. */
LCTerm* lc_combinator_I(void);
LCTerm* lc_combinator_K(void);
LCTerm* lc_combinator_S(void);
LCTerm* lc_combinator_B(void);
LCTerm* lc_combinator_C(void);
LCTerm* lc_combinator_W(void);

/* L4: Fixed-point combinator — Y = λf.(λx.f(x x))(λx.f(x x)).
 * Proves that every term in untyped lambda calculus has a fixed point.
 * Enables general recursion without named recursion. */
LCTerm* lc_y_combinator(void);

/* L5: Reduction algorithms.
 * Church-Rosser Theorem (L4): reduction order does not affect final normal form.
 * Standardization Theorem: normal order always finds normal form if one exists. */
LCTerm* lc_beta_reduce(LCTerm* term);
LCTerm* lc_normal_order(LCTerm* term, int max_depth);
LCTerm* lc_applicative_order(LCTerm* term, int max_depth);
bool    lc_is_normal_form(LCTerm* term);

/* L3: Named variable context for human-readable construction */
typedef struct {
    char   names[LC_MAX_VARS][LC_MAX_NAME_LEN];
    int    count;
} LCNamedContext;

LCTerm* lc_named_var(LCNamedContext* ctx, const char* name);
LCTerm* lc_named_abs(LCNamedContext* ctx, const char* param, LCTerm* body);

void    lc_print(const LCTerm* t);
void    lc_destroy(LCTerm* t);
int     lc_church_to_int(LCTerm* term);
bool    lc_church_to_bool(LCTerm* term);

#endif
