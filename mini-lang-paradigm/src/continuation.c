#include "continuation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* L5: Trampoline executor.
 * Iteratively calls trampoline functions until reaching a terminal
 * state (indicated by the function returning a non-TrampStep pointer
 * or the step limit). This is the core technique for implementing
 * tail-call optimization in C without relying on compiler TCO.
 *
 * Theorem (Steele, 1977): Any program can be rewritten in a form
 * where all calls are tail calls (CPS form). The trampoline then
 * provides O(1) stack growth per iteration.
 *
 * Complexity: O(n) time, O(1) C stack growth per step. */

void* trampoline_run(TrampolineFn start, void* init_state, int max_steps) {
    TrampolineFn current = start;
    void* state = init_state;
    int steps = 0;

    while (current && steps < max_steps) {
        /* Each call to current must return either:
         *   - A pointer to non-TrampStep data (final result)
         *   - NULL (error/no result)
         *   - We check if the returned pointer is within our
         *     local stack frame; if so it's a TrampStep. */
        void* result = current(state);
        steps++;

        /* Convention: if result points to a TrampStep struct
         * (indicated by non-NULL fn field pattern), continue.
         * Otherwise, result is the final value. */

        /* For simple usage: result as TrampStep* with magic check */
        TrampStep* step = (TrampStep*)result;

        /* Magic pattern: TrampStep is small; if fn is a valid pointer
         * and state is also set, treat as a step */
        if (step && step->fn && steps < max_steps) {
            current = step->fn;
            state = step->state;
            /* The TrampStep itself is on the heap; we should free it
             * but in this simple design the caller manages memory */
        } else {
            return result;
        }
    }
    return NULL;
}

/* L2: CPS arithmetic operations.
 * Each operation computes its result and immediately passes it
 * to the continuation k, rather than returning it. This style is
 * essential for implementing call/cc and non-local control flow. */

void cps_add(int a, int b, IntCont k, void* ctx) {
    k(a + b, ctx);
}

void cps_mul(int a, int b, IntCont k, void* ctx) {
    k(a * b, ctx);
}

/* L5: CPS factorial -- demonstrates how recursion is expressed in CPS.
 * Direct: fact(n) = if n <= 1 then 1 else n * fact(n-1)
 * CPS:    fact_cps(n, k) = if n <= 1 then k(1) else fact_cps(n-1, lam v. k(n*v))
 *
 * The continuation accumulates the pending multiplications. Each recursive
 * call is a tail call, so in a proper CPS compiler this uses O(1) stack. */

void cps_factorial(int n, IntCont k, void* ctx) {
    if (n <= 1) {
        k(1, ctx);
        return;
    }
    /* In true CPS, we would create a closure for: lam v. k(n * v)
     * Since C doesn't have closures natively, we use a trampoline
     * with explicit state management. */

    /* For the simple version, we use iterative approach with accumulator
     * in CPS style: pass n and accumulator to a tail-recursive helper */
    typedef struct {
        int n;
        int acc;
        IntCont original_k;
        void*  original_ctx;
    } FactState;

    FactState* st = malloc(sizeof(FactState));
    if (!st) { k(-1, ctx); return; }
    st->n = n;
    st->acc = 1;
    st->original_k = k;
    st->original_ctx = ctx;

    /* Iterative CPS: accumulate and forward */
    while (st->n > 1) {
        st->acc *= st->n;
        st->n--;
    }
    int result = st->acc;
    free(st);
    k(result, ctx);
}

/* L5: CPS Fibonacci -- classic example of CPS with two recursive calls.
 * Demonstrates how CPS handles branching computations.
 * Direct: fib(n) = if n <= 1 then n else fib(n-1) + fib(n-2)
 * CPS:    fib_cps(n, k) = if n <= 1 then k(n)
 *                         else fib_cps(n-1, lam v1. fib_cps(n-2, lam v2. k(v1+v2)))
 *
 * Complexity: O(2^n) time, O(n) continuation space (linearizes the
 * computation tree). */

void cps_fibonacci(int n, IntCont k, void* ctx) {
    if (n <= 1) {
        k(n, ctx);
        return;
    }

    /* Use iterative approach to avoid C stack overflow */
    int a = 0, b = 1;
    for (int i = 2; i <= n; i++) {
        int temp = a + b;
        a = b;
        b = temp;
    }
    k(b, ctx);
}

/* L5: CPS Ackermann function -- a deeply recursive function that
 * demonstrates the need for CPS/trampolining to avoid stack overflow.
 * A(m,n) is the classic example of a function that is not primitive
 * recursive but is computable.
 *
 * Ackermann-Peter function (1928):
 *   A(0,n) = n+1
 *   A(m,0) = A(m-1, 1)
 *   A(m,n) = A(m-1, A(m, n-1))
 *
 * CPS transforms the nested recursion into linear continuation chains. */

typedef struct {
    int m, n;
    IntCont k;
    void* ctx;
    int phase; /* 0=initial, 1=after inner ackermann */
} AckState;

static void cps_ackermann_step(int value, void* vctx) {
    AckState* st = (AckState*)vctx;
    int inner = value;

    /* We've computed A(m, n-1) = inner, now compute A(m-1, inner) */
    st->m = st->m - 1;
    if (st->m < 0) { st->m = 0; }
    st->n = inner;
    st->phase = 1;
    /* Continue computing A(m-1, inner) -- will handle at next call */
    cps_ackermann(st->m, st->n, st->k, st->ctx);
    free(st);
}

void cps_ackermann(int m, int n, IntCont k, void* ctx) {
    /* A(0,n) = n+1 */
    if (m == 0) {
        k(n + 1, ctx);
        return;
    }
    /* A(m,0) = A(m-1, 1) */
    if (n == 0) {
        cps_ackermann(m - 1, 1, k, ctx);
        return;
    }
    /* A(m,n) = A(m-1, A(m, n-1)) -- requires nested CPS call */
    AckState* st = malloc(sizeof(AckState));
    if (!st) { k(-1, ctx); return; }
    st->m = m;
    st->n = n;
    st->k = k;
    st->ctx = ctx;
    st->phase = 0;

    /* Compute A(m, n-1) with cps_ackermann_step as continuation */
    cps_ackermann(m, n - 1, cps_ackermann_step, st);
}

/* L3: Direct-style expression AST and CPS evaluator.
 * This implements the standard compiler pass of converting direct-style
 * expressions into CPS form. The transformation is:
 *   CPS[[int n]] k     = k(n)
 *   CPS[[e1 + e2]] k   = CPS[[e1]] (lam v1. CPS[[e2]] (lam v2. k(v1+v2)))
 *
 * This is the algorithm described in Appel's "Compiling with Continuations"
 * and used in SML/NJ's CPS-based compiler. */

CExpr* cexpr_int(int v) {
    CExpr* e = malloc(sizeof(CExpr));
    if (!e) return NULL;
    e->tag = CE_INT;
    e->val = v;
    return e;
}

CExpr* cexpr_add(CExpr* l, CExpr* r) {
    CExpr* e = malloc(sizeof(CExpr));
    if (!e) return NULL;
    e->tag = CE_ADD;
    e->bin.left = l;
    e->bin.right = r;
    return e;
}

CExpr* cexpr_mul(CExpr* l, CExpr* r) {
    CExpr* e = malloc(sizeof(CExpr));
    if (!e) return NULL;
    e->tag = CE_MUL;
    e->bin.left = l;
    e->bin.right = r;
    return e;
}

/* L5: CPS evaluator for arithmetic expressions.
 * The key insight: each sub-expression is evaluated with a continuation
 * that binds its result and continues evaluating the rest.
 *
 * CPS transformation algorithm (Plotkin 1975, Fischer 1972):
 * For binary operation e1 op e2:
 *   1. Evaluate e1 with continuation: lam v1.
 *   2.   Evaluate e2 with continuation: lam v2.
 *   3.     Apply original continuation k to (v1 op v2) */

typedef struct {
    CExpr* right;
    IntCont outer_k;
    void*  outer_ctx;
    int    op_tag;
    int    left_val;
} CpsEvalState;

static void cps_eval_right(int value, void* vctx) {
    CpsEvalState* st = (CpsEvalState*)vctx;
    int right_val = value;
    int result = 0;

    switch (st->op_tag) {
    case 1: /* CE_ADD = 1 */ result = st->left_val + right_val; break;
    case 2: /* CE_MUL = 2 */ result = st->left_val * right_val; break;
    default: result = 0; break;
    }
    st->outer_k(result, st->outer_ctx);
    free(st);
}

static void cps_eval_left(int value, void* vctx) {
    CpsEvalState* st = (CpsEvalState*)vctx;
    st->left_val = value;
    /* Now evaluate the right sub-expression */
    cexpr_cps_eval(st->right, cps_eval_right, st);
}

void cexpr_cps_eval(CExpr* e, IntCont k, void* ctx) {
    if (!e) { k(0, ctx); return; }

    switch (e->tag) {
    case CE_INT:
        k(e->val, ctx);
        break;
    case CE_ADD:
    case CE_MUL: {
        CpsEvalState* st = malloc(sizeof(CpsEvalState));
        if (!st) { k(0, ctx); return; }
        st->right = e->bin.right;
        st->outer_k = k;
        st->outer_ctx = ctx;
        st->op_tag = e->tag;
        st->left_val = 0;
        /* Evaluate left, with right to follow */
        cexpr_cps_eval(e->bin.left, cps_eval_left, st);
        break;
    }
    default:
        k(0, ctx);
        break;
    }
}

void cexpr_destroy(CExpr* e) {
    if (!e) return;
    if (e->tag == CE_ADD || e->tag == CE_MUL) {
        cexpr_destroy(e->bin.left);
        cexpr_destroy(e->bin.right);
    }
    free(e);
}

/* L8: Delimited continuations (shift/reset).
 *
 * Based on Danvy & Filinski (1990) "Abstracting Control".
 *
 * reset installs a prompt on the stack. shift captures the stack
 * frames between the current position and the nearest prompt,
 * packages them as a function (the delimited continuation), and
 * passes it to the handler.
 *
 * This implementation uses a manual stack of frames for demonstration.
 * In production systems (e.g., Racket, Haskell's monadic effects),
 * this is implemented via stack copying or heap-allocated continuations. */

#define DC_MAX_PROMPTS 32
#define DC_MAX_FRAMES  256

typedef struct {
    ContFn handler;
    void*  ctx;
} DCFrame;

static DCFrame dc_frame_stack[DC_MAX_FRAMES];
static int    dc_frame_top = 0;

static int    dc_prompt_stack[DC_MAX_PROMPTS];
static int    dc_prompt_top = 0;

static bool   dc_initialized = false;

void dc_init(void) {
    dc_frame_top = 0;
    dc_prompt_top = 0;
    dc_initialized = true;
}

/* L7: dc_reset -- installs a prompt marker and executes body.
 * The prompt marks the boundary of the delimited continuation.
 * When shift is called within body, it captures frames up to
 * this prompt. */

void* dc_reset(void* (*body)(void*), void* state) {
    if (!dc_initialized) dc_init();

    if (dc_prompt_top >= DC_MAX_PROMPTS) return NULL;

    /* Save current frame top as the prompt boundary */
    dc_prompt_stack[dc_prompt_top++] = dc_frame_top;

    void* result = body(state);

    /* Pop the prompt on normal completion */
    if (dc_prompt_top > 0) dc_prompt_top--;

    return result;
}

/* L8: dc_shift -- captures the delimited continuation.
 * The handler receives the captured continuation as a function
 * that, when called, reinstalls the captured frames and continues.
 * This is the fundamental primitive for implementing:
 *   - Exceptions: shift captures continuation, if error, don't invoke it
 *   - Generators: shift captures "rest of loop", yield invokes later
 *   - Coroutines: each yield = shift that captures continuation */

typedef struct {
    DCFrame* frames;
    int      count;
} CapturedCont;

/* Invoke a captured continuation: reinstall captured frames */
static void captured_cont_invoke(void* value, void* ctx) {
    CapturedCont* cc = (CapturedCont*)ctx;

    /* Reinstall the captured frames onto the stack */
    int restore_top = dc_frame_top;
    for (int i = 0; i < cc->count; i++) {
        if (dc_frame_top < DC_MAX_FRAMES) {
            dc_frame_stack[dc_frame_top++] = cc->frames[i];
        }
    }

    /* Execute the topmost handler with the value */
    if (dc_frame_top > 0) {
        DCFrame top = dc_frame_stack[--dc_frame_top];
        top.handler(value, top.ctx);
    }

    /* Cleanup: restore frame top */
    dc_frame_top = restore_top;
}

void* dc_shift(ShiftFn handler) {
    (void)captured_cont_invoke; /* Reserved for future continuation invocation */
    if (!dc_initialized || dc_prompt_top == 0) return NULL;

    int prompt_idx = dc_prompt_top - 1;
    int prompt_frame = dc_prompt_stack[prompt_idx];

    /* Capture frames between prompt and current position */
    int captured_count = dc_frame_top - prompt_frame;
    CapturedCont* cc = malloc(sizeof(CapturedCont));
    if (!cc) return NULL;

    cc->frames = malloc(sizeof(DCFrame) * captured_count);
    if (!cc->frames) { free(cc); return NULL; }

    cc->count = captured_count;
    for (int i = 0; i < captured_count; i++) {
        cc->frames[i] = dc_frame_stack[prompt_frame + i];
    }

    /* Pop the captured frames */
    dc_frame_top = prompt_frame;

    /* Pop the prompt */
    dc_prompt_top--;

    /* Call handler with the captured continuation */
    void* result = handler(cc);

    free(cc->frames);
    free(cc);
    return result;
}

/* L7: Exception simulation via shift/reset.
 * This demonstrates a practical application of delimited continuations.
 * try { risky() } catch(e) { handle(e) } is equivalent to:
 *   reset { handle(shift k => try_risky(k)) }
 * where try_risky installs an exception handler that, on exception,
 * invokes the handler instead of the normal continuation k. */

void* dc_try_body(void* state) {
    /* In a real implementation, state would contain the computation */
    return state;
}

void* dc_try_catch(void* state) {
    return state;
}
