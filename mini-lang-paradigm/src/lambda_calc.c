#include "lambda_calc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* L1: Core constructors */

LCTerm* lc_var(int index) {
    LCTerm* t = malloc(sizeof(LCTerm));
    if (!t) return NULL;
    t->tag = LC_VAR;
    t->index = index;
    t->next = NULL;
    return t;
}

LCTerm* lc_abs(LCTerm* body) {
    LCTerm* t = malloc(sizeof(LCTerm));
    if (!t) return NULL;
    t->tag = LC_ABS;
    t->abs.body = body;
    t->next = NULL;
    return t;
}

LCTerm* lc_app(LCTerm* fn, LCTerm* arg) {
    LCTerm* t = malloc(sizeof(LCTerm));
    if (!t) return NULL;
    t->tag = LC_APP;
    t->app.fn = fn;
    t->app.arg = arg;
    t->next = NULL;
    return t;
}

/* L2: Church encodings.
 * Church boolean: true = lambda t. lambda f. t, false = lambda t. lambda f. f */

LCTerm* lc_church_true(void) {
    return lc_abs(lc_abs(lc_var(1)));
}

LCTerm* lc_church_false(void) {
    return lc_abs(lc_abs(lc_var(0)));
}

/* Church numeral n = lambda f. lambda x. f^n(x)
 * 0 = lambda f. lambda x. x, 1 = lambda f. lambda x. f x */

LCTerm* lc_church_numeral(int n) {
    LCTerm* body = lc_var(0);
    for (int i = 0; i < n; i++) {
        body = lc_app(lc_var(1), body);
    }
    return lc_abs(lc_abs(body));
}

/* succ = lambda n. lambda f. lambda x. f (n f x) */
LCTerm* lc_church_succ(void) {
    LCTerm* nfx = lc_app(lc_app(lc_var(2), lc_var(1)), lc_var(0));
    LCTerm* body = lc_app(lc_var(1), nfx);
    return lc_abs(lc_abs(lc_abs(body)));
}

/* add = lambda m. lambda n. lambda f. lambda x. m f (n f x) */
LCTerm* lc_church_add(void) {
    LCTerm* nfx = lc_app(lc_app(lc_var(2), lc_var(1)), lc_var(0));
    LCTerm* mnfx = lc_app(lc_app(lc_var(3), lc_var(1)), nfx);
    return lc_abs(lc_abs(lc_abs(lc_abs(mnfx))));
}

/* mul = lambda m. lambda n. lambda f. m (n f) */
LCTerm* lc_church_mul(void) {
    LCTerm* nf = lc_app(lc_var(2), lc_var(1));
    LCTerm* mnf = lc_app(lc_var(3), nf);
    return lc_abs(lc_abs(lc_abs(lc_app(mnf, lc_var(0)))));
}

/* L2: Church pair = lambda x. lambda y. lambda f. f x y
 * fst = lambda p. p true */
LCTerm* lc_church_pair(void) {
    LCTerm* fxy = lc_app(lc_app(lc_var(0), lc_var(2)), lc_var(1));
    return lc_abs(lc_abs(lc_abs(fxy)));
}

LCTerm* lc_church_fst(void) {
    return lc_abs(lc_app(lc_var(0), lc_church_true()));
}

LCTerm* lc_church_snd(void) {
    return lc_abs(lc_app(lc_var(0), lc_church_false()));
}

/* L2: SKI Combinator Calculus.
 * I = lambda x. x, K = lambda x. lambda y. x, S = lambda x. lambda y. lambda z. x z (y z)
 * Theorem (Schoenfinkel 1924, Curry): Any closed lambda term can be
 * translated to an equivalent SKI combinator expression. */

LCTerm* lc_combinator_I(void) { return lc_abs(lc_var(0)); }

LCTerm* lc_combinator_K(void) { return lc_abs(lc_abs(lc_var(1))); }

LCTerm* lc_combinator_S(void) {
    LCTerm* xz = lc_app(lc_var(2), lc_var(0));
    LCTerm* yz = lc_app(lc_var(1), lc_var(0));
    return lc_abs(lc_abs(lc_abs(lc_app(xz, yz))));
}

LCTerm* lc_combinator_B(void) {
    LCTerm* yz = lc_app(lc_var(1), lc_var(0));
    return lc_abs(lc_abs(lc_abs(lc_app(lc_var(2), yz))));
}

LCTerm* lc_combinator_C(void) {
    LCTerm* xzy = lc_app(lc_app(lc_var(2), lc_var(0)), lc_var(1));
    return lc_abs(lc_abs(lc_abs(xzy)));
}

LCTerm* lc_combinator_W(void) {
    LCTerm* xyy = lc_app(lc_app(lc_var(1), lc_var(0)), lc_var(0));
    return lc_abs(lc_abs(xyy));
}

/* L4: Y combinator = lambda f. (lambda x. f(x x)) (lambda x. f(x x))
 * For any term F, Y F = F (Y F) -- Y finds the fixed point.
 * This enables general recursion in the untyped lambda calculus.
 * Note: Y diverges under applicative order; Z combinator is needed
 * for strict evaluation. */

LCTerm* lc_y_combinator(void) {
    LCTerm* xx = lc_app(lc_var(0), lc_var(0));
    LCTerm* fxx = lc_app(lc_var(1), xx);
    LCTerm* inner = lc_abs(fxx);
    LCTerm* outer = lc_app(inner, inner);
    return lc_abs(outer);
}

/* L5: de Bruijn index shifting.
 * shift(d, cutoff) increases free variable indices by d for variables
 * with index >= cutoff. This is essential for correct substitution
 * under binders. Reference: Pierce, TAPL chapter 6. */

static LCTerm* shift_term(LCTerm* t, int d, int cutoff) {
    if (!t) return NULL;
    LCTerm* r = malloc(sizeof(LCTerm));
    if (!r) return NULL;
    r->tag = t->tag;
    r->next = NULL;
    switch (t->tag) {
    case LC_VAR:
        r->index = (t->index >= cutoff) ? t->index + d : t->index;
        break;
    case LC_ABS:
        r->abs.body = shift_term(t->abs.body, d, cutoff + 1);
        break;
    case LC_APP:
        r->app.fn = shift_term(t->app.fn, d, cutoff);
        r->app.arg = shift_term(t->app.arg, d, cutoff);
        break;
    }
    return r;
}

/* L5: Substitution for de Bruijn terms.
 * [j := s]t substitutes term s for variable j in term t.
 * When going under a binder, we shift s up by 1 to account for
 * the new binding context. */

static LCTerm* subst_term(LCTerm* t, int j, LCTerm* s) {
    if (!t) return NULL;
    LCTerm* r = malloc(sizeof(LCTerm));
    if (!r) return NULL;
    r->tag = t->tag;
    r->next = NULL;
    switch (t->tag) {
    case LC_VAR:
        if (t->index == j) {
            free(r);
            return shift_term(s, 0, 0);
        }
        r->index = t->index;
        break;
    case LC_ABS:
        r->abs.body = subst_term(t->abs.body, j + 1, shift_term(s, 1, 0));
        break;
    case LC_APP:
        r->app.fn = subst_term(t->app.fn, j, s);
        r->app.arg = subst_term(t->app.arg, j, s);
        break;
    }
    return r;
}

/* L5: Single beta-reduction step (leftmost outermost).
 * Beta-reduction: (lambda. M) N -> M[N/0] shifted down by 1.
 * This implements the standard reduction relation from the
 * lambda calculus. Returns a newly allocated term or NULL if
 * no redex exists. */

LCTerm* lc_beta_reduce(LCTerm* term) {
    if (!term) return NULL;
    switch (term->tag) {
    case LC_VAR:
        return NULL;
    case LC_ABS: {
        LCTerm* reduced = lc_beta_reduce(term->abs.body);
        if (reduced) {
            LCTerm* r = malloc(sizeof(LCTerm));
            if (!r) return NULL;
            r->tag = LC_ABS;
            r->abs.body = reduced;
            r->next = NULL;
            return r;
        }
        return NULL;
    }
    case LC_APP: {
        if (term->app.fn && term->app.fn->tag == LC_ABS) {
            LCTerm* substituted = subst_term(term->app.fn->abs.body, 0,
                                              shift_term(term->app.arg, 1, 0));
            LCTerm* result = shift_term(substituted, -1, 0);
            lc_destroy(substituted);
            return result;
        }
        LCTerm* red_fn = lc_beta_reduce(term->app.fn);
        if (red_fn) {
            LCTerm* r = malloc(sizeof(LCTerm));
            if (!r) return NULL;
            r->tag = LC_APP;
            r->app.fn = red_fn;
            r->app.arg = shift_term(term->app.arg, 0, 0);
            r->next = NULL;
            return r;
        }
        LCTerm* red_arg = lc_beta_reduce(term->app.arg);
        if (red_arg) {
            LCTerm* r = malloc(sizeof(LCTerm));
            if (!r) return NULL;
            r->tag = LC_APP;
            r->app.fn = shift_term(term->app.fn, 0, 0);
            r->app.arg = red_arg;
            r->next = NULL;
            return r;
        }
        return NULL;
    }
    }
    return NULL;
}

/* L5: Normal-order reduction (leftmost outermost).
 * Guaranteed by the Standardization Theorem to find a normal form
 * if one exists. This is the strategy used in lazy/evaluation. */

LCTerm* lc_normal_order(LCTerm* term, int max_depth) {
    if (max_depth <= 0) return NULL;
    LCTerm* current = shift_term(term, 0, 0);
    for (int steps = 0; steps < max_depth; steps++) {
        LCTerm* reduced = lc_beta_reduce(current);
        if (!reduced) return current;
        lc_destroy(current);
        current = reduced;
    }
    lc_destroy(current);
    return NULL;
}

/* L5: Helper - check if any redex exists */
static bool has_redex(LCTerm* t) {
    if (!t) return false;
    if (t->tag == LC_APP && t->app.fn && t->app.fn->tag == LC_ABS) return true;
    if (t->tag == LC_ABS) return has_redex(t->abs.body);
    if (t->tag == LC_APP) return has_redex(t->app.fn) || has_redex(t->app.arg);
    return false;
}

static LCTerm* reduce_one_innermost(LCTerm* t, bool* changed) {
    if (!t) return NULL;
    switch (t->tag) {
    case LC_VAR: return lc_var(t->index);
    case LC_ABS: {
        LCTerm* body = reduce_one_innermost(t->abs.body, changed);
        return lc_abs(body);
    }
    case LC_APP: {
        if (t->app.fn && t->app.fn->tag == LC_ABS && !*changed) {
            *changed = true;
            LCTerm* sub = subst_term(t->app.fn->abs.body, 0,
                                      shift_term(t->app.arg, 1, 0));
            return shift_term(sub, -1, 0);
        }
        LCTerm* arg = reduce_one_innermost(t->app.arg, changed);
        if (*changed) return lc_app(shift_term(t->app.fn, 0, 0), arg);
        LCTerm* fn = reduce_one_innermost(t->app.fn, changed);
        return lc_app(fn, shift_term(t->app.arg, 0, 0));
    }
    }
    return NULL;
}

/* L5: Applicative-order reduction (leftmost innermost).
 * Matches eager/strict evaluation. May not find normal form for
 * terms that diverge under strict evaluation. */

LCTerm* lc_applicative_order(LCTerm* term, int max_depth) {
    if (max_depth <= 0) return NULL;
    LCTerm* current = shift_term(term, 0, 0);
    for (int steps = 0; steps < max_depth; steps++) {
        if (!has_redex(current)) return current;
        bool changed = false;
        LCTerm* next = reduce_one_innermost(current, &changed);
        lc_destroy(current);
        if (!changed) return next;
        current = next;
        if (!current) return NULL;
    }
    return current;
}

bool lc_is_normal_form(LCTerm* term) {
    return !has_redex(term);
}

/* L3: Named variable context */
LCTerm* lc_named_var(LCNamedContext* ctx, const char* name) {
    for (int i = ctx->count - 1; i >= 0; i--) {
        if (strcmp(ctx->names[i], name) == 0) {
            return lc_var(ctx->count - 1 - i);
        }
    }
    return lc_var(-1);
}

LCTerm* lc_named_abs(LCNamedContext* ctx, const char* param, LCTerm* body) {
    if (ctx->count < LC_MAX_VARS) {
        snprintf(ctx->names[ctx->count], LC_MAX_NAME_LEN, "%s", param);
        ctx->count++;
    }
    return lc_abs(body);
}

/* L7: Convert Church encodings to C values */
int lc_church_to_int(LCTerm* term) {
    if (!term || term->tag != LC_ABS) return -1;
    LCTerm* inner = term->abs.body;
    if (!inner) return -1;
    int count = 0;
    LCTerm* cur = inner;
    while (cur && cur->tag == LC_ABS) cur = cur->abs.body;
    while (cur && cur->tag == LC_APP && cur->app.fn &&
           cur->app.fn->tag == LC_VAR && cur->app.fn->index == 1) {
        count++;
        cur = cur->app.arg;
    }
    if (cur && cur->tag == LC_VAR && cur->index == 0) return count;
    return -1;
}

bool lc_church_to_bool(LCTerm* term) {
    if (!term || term->tag != LC_ABS) return false;
    LCTerm* inner = term->abs.body;
    if (!inner || inner->tag != LC_ABS) return false;
    LCTerm* var = inner->abs.body;
    if (var && var->tag == LC_VAR && var->index == 1) return true;
    return false;
}

/* Printing */
static void lc_print_inner(const LCTerm* t) {
    if (!t) { printf("?"); return; }
    switch (t->tag) {
    case LC_VAR: printf("%d", t->index); break;
    case LC_ABS: printf("L."); lc_print_inner(t->abs.body); break;
    case LC_APP:
        printf("(");
        lc_print_inner(t->app.fn);
        printf(" ");
        lc_print_inner(t->app.arg);
        printf(")");
        break;
    }
}

void lc_print(const LCTerm* t) { lc_print_inner(t); }

void lc_destroy(LCTerm* t) {
    if (!t) return;
    switch (t->tag) {
    case LC_ABS: lc_destroy(t->abs.body); break;
    case LC_APP: lc_destroy(t->app.fn); lc_destroy(t->app.arg); break;
    default: break;
    }
    free(t);
}
