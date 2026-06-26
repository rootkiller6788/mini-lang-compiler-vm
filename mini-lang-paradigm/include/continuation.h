#ifndef CONTINUATION_H
#define CONTINUATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CONT_MAX_STACK     4096
#define CONT_MAX_CAPTURE   64

/* L1: Definitions -- Continuations and CPS
 *
 * A continuation represents "the rest of the computation." In
 * Continuation-Passing Style (CPS), every function takes an explicit
 * continuation parameter (k) and never returns normally -- instead,
 * it calls k with the result.
 *
 * Reference: Reynolds (1972) "Definitional Interpreters for Higher-Order
 * Programming Languages"; Plotkin (1975) "Call-by-name, call-by-value
 * and the lambda calculus"
 */

/* A continuation is a function that takes a value (the result of the
 * current computation) and performs the rest of the program. */
typedef void (*ContFn)(void* value, void* ctx);

/* L1: Continuation frame -- represents a saved execution state.
 * In real compilers (e.g., GHC, SML/NJ), continuations are implemented
 * via heap-allocated closures or stack copying. */
typedef struct ContFrame ContFrame;
struct ContFrame {
    ContFn      handler;
    void*       ctx;
    ContFrame*  next;
};

/* L3: Trampoline -- a control structure for implementing tail-call
 * optimization in C. Instead of recursive calls that could overflow
 * the C stack, each step returns the next computation to execute.
 * The trampoline iteratively executes until reaching a terminal state.
 *
 * This is the standard technique used in Scheme interpreters (e.g.,
 * MIT/GNU Scheme, Chez Scheme) and continuation-based web frameworks. */
typedef void* (*TrampolineFn)(void* state);

typedef struct {
    TrampolineFn fn;
    void*        state;
} TrampStep;

/* L5: Trampoline executor -- iterative loop that avoids stack overflow.
 * Complexity: O(n) time, O(1) stack space where n is the number of
 * trampoline steps. */
void* trampoline_run(TrampolineFn start, void* init_state, int max_steps);

/* L2: CPS transform -- convert a direct-style computation to CPS.
 *
 * Direct style:    f(x) = x + 1
 * CPS:             f_cps(x, k) = k(x + 1)
 *
 * This is the fundamental compilation technique for implementing
 * call/cc, exceptions, generators, and async/await in languages
 * without native support. */

/* CPS integer arithmetic -- each operation takes a continuation k */
typedef void (*IntCont)(int value, void* ctx);

void cps_add(int a, int b, IntCont k, void* ctx);
void cps_mul(int a, int b, IntCont k, void* ctx);
void cps_factorial(int n, IntCont k, void* ctx);
void cps_fibonacci(int n, IntCont k, void* ctx);
void cps_ackermann(int m, int n, IntCont k, void* ctx);

/* L5: Direct-to-CPS conversion for arithmetic expressions.
 * Represents expressions as AST, converts to CPS form.
 * This demonstrates the algorithm used in compiler IR transforms
 * (e.g., ANF in GHC, CPS in MLton). */

typedef enum {
    CE_INT, CE_ADD, CE_MUL, CE_SUB
} CExprTag;

typedef struct CExpr CExpr;
struct CExpr {
    CExprTag tag;
    union {
        int val;
        struct { CExpr* left; CExpr* right; } bin;
    };
};

CExpr* cexpr_int(int v);
CExpr* cexpr_add(CExpr* l, CExpr* r);
CExpr* cexpr_mul(CExpr* l, CExpr* r);
void   cexpr_cps_eval(CExpr* e, IntCont k, void* ctx);
void   cexpr_destroy(CExpr* e);

/* L8: Delimited continuations (shift/reset).
 *
 * Unlike call/cc which captures the *entire* continuation, delimited
 * continuations (Danvy & Filinski, 1990) capture only up to a
 * delimiter (reset). This enables:
 *   - Composability (unlike undelimited continuations)
 *   - Implementation of exceptions, generators, coroutines
 *   - Algebraic effects (see effect_system.h)
 *
 * shift k => body captures the continuation k up to the nearest
 * enclosing reset, then evaluates body with k bound. */

typedef void* (*ShiftFn)(void* captured_k);

/* Delimited continuation stack frame */
typedef struct DCStack DCStack;
struct DCStack {
    void*   value;
    ContFn  handler;
    void*   ctx;
    DCStack* next;
};

/*
 * reset(body_fn, state) -- installs a prompt (delimiter), then executes body.
 * shift(k_handler) -- captures the continuation up to the nearest reset.
 *
 * These are implemented via a global prompt stack (single-threaded).
 */

/* Initialize the delimited continuation runtime */
void dc_init(void);

/* L7: reset -- install a prompt and execute body.
 * Returns the value that reaches the prompt. */
void* dc_reset(void* (*body)(void*), void* state);

/* L8: shift -- capture continuation and invoke handler.
 * Only callable within a dynamic extent of dc_reset. */
void* dc_shift(ShiftFn handler);

/* L7: Application -- simulating exceptions via shift/reset.
 * Using shift/reset, we can implement exception handling:
 *   try { body } catch { handler }
 * becomes:
 *   reset { handler(shift k => body) }  */

typedef struct {
    bool raised;
    int  value;
} ExceptResult;

void* dc_try_body(void* state);
void* dc_try_catch(void* state);

#endif
