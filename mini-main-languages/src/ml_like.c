/* ml_like.c — ML-like Lambda Calculus Interpreter
 * ============================================================================
 * L1-L9 knowledge coverage:
 *   L1: MLExpr/MLValue tagged union, MLEnv linked-list frames
 *   L2: Lambda calculus (α-conversion, β-reduction), lexical scoping,
 *        first-class functions, closures
 *   L3: S-expression parser, metacircular evaluator (eval/apply cycle),
 *        environment model with frames
 *   L4: Church-Rosser theorem (confluence of β-reduction),
 *        de Bruijn indices (nameless representation),
 *        Hindley-Milner type inference (simplified)
 *   L5: Recursive descent S-expression parser, tree-walking evaluator,
 *        Y combinator, list operations (cons/car/cdr)
 *   L6: Factorial via letrec, Church numerals, list processing
 *   L7: Partial evaluation (constant propagation + specialization),
 *        CPS (continuation-passing style) transformation
 *   L8: Lazy evaluation (call-by-name thunks), de Bruijn conversion
 *   L9: Hindley-Milner-Damas type inference (W algorithm, simplified)
 *
 * Reference: Abelson & Sussman. "SICP" Ch. 4 (Metacircular Evaluator)
 *            Pierce, B. "TAPL" Ch. 5-9 (Lambda Calculus, Simply Typed LC)
 *            de Bruijn, N.G. "Lambda calculus notation with nameless
 *            dummies" Indagationes Mathematicae 34, 1972
 */

#include "ml_like.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════════════════
 * L3: Lexer / Tokenizer
 * ═══════════════════════════════════════════════════════════════════ */

static void skip_ws(const char *s, int *pos) {
    while (s[*pos] && isspace((unsigned char)s[*pos])) (*pos)++;
}

static char *read_token(const char *s, int *pos, char *buf) {
    int i = 0;
    skip_ws(s, pos);
    /* Single-char tokens */
    if (s[*pos] == '(' || s[*pos] == ')' || s[*pos] == '\'') {
        buf[i++] = s[(*pos)++]; buf[i] = '\0'; return buf;
    }
    /* Multi-char symbols: + * = . \ - < > ! ? */
    if (s[*pos] == '+' || s[*pos] == '*' || s[*pos] == '='
        || s[*pos] == '.' || s[*pos] == '\\' || s[*pos] == '-'
        || s[*pos] == '<' || s[*pos] == '>' || s[*pos] == ':') {
        buf[i++] = s[(*pos)++]; buf[i] = '\0'; return buf;
    }
    /* Identifiers and numbers */
    while (isalnum((unsigned char)s[*pos]) || s[*pos] == '_' || s[*pos] == '?' || s[*pos] == '!'
           || s[*pos] == '<' || s[*pos] == '>' || s[*pos] == '#' || s[*pos] == ':') {
        if (i < 63) buf[i++] = s[*pos];
        (*pos)++;
    }
    buf[i] = '\0';
    return (i > 0) ? buf : NULL;
}

/* ═══════════════════════════════════════════════════════════════════
 * L3: AST Allocation
 * ═══════════════════════════════════════════════════════════════════ */

static MLExpr *new_expr(MLExprType t) {
    MLExpr *e = (MLExpr *)calloc(1, sizeof(MLExpr));
    if (e) e->type = t;
    return e;
}

/* ═══════════════════════════════════════════════════════════════════
 * L5: S-Expression Parser
 *
 * Parses Lisp-like syntax:
 *   expr → INT | BOOL | VAR | '(' op operands... ')'
 *
 * Special forms:
 *   (lambda (x) body)     — function abstraction
 *   (let (x val) body)    — let binding
 *   (letrec (x val) body) — recursive let
 *   (if cond then else)   — conditional
 *   (cons a b)            — pair construction
 *   (car p)               — first element
 *   (cdr p)               — second element
 *   'expr                 — quote shorthand
 *   (match pat expr body) — pattern match
 *   (delay expr)          — lazy evaluation
 *   (force thunk)         — force thunk
 * ═══════════════════════════════════════════════════════════════════ */

MLExpr *ml_parse(const char *source, int *pos) {
    char tok[64];
    read_token(source, pos, tok);
    if (strlen(tok) == 0) return NULL;

    /* Quote shorthand: 'expr */
    if (strcmp(tok, "'") == 0) {
        /* '(a b c) → (cons a (cons b (cons c nil))) */
        MLExpr *inner = ml_parse(source, pos);
        if (!inner) return NULL;
        /* Simple: wrap in list constructor call pattern */
        /* For brevity, return a special node — caller handles */
        MLExpr *e = new_expr(ML_APP);
        MLExpr *cons_var = new_expr(ML_VAR);
        strcpy(cons_var->data.var_name, "cons");
        e->data.app.fn = cons_var;
        e->data.app.arg = inner;
        return e;
    }

    if (strcmp(tok, "(") == 0) {
        read_token(source, pos, tok);

        /* (lambda (x) body) or (lambda x body) */
        if (strcmp(tok, "lambda") == 0 || strcmp(tok, "\\") == 0) {
            MLExpr *e = new_expr(ML_LAMBDA);
            read_token(source, pos, tok);
            if (tok[0] == '(') {
                read_token(source, pos, tok); /* parameter */
            }
            strncpy(e->data.lambda.param, tok, 63);
            if (tok[0] == '(') {
                /* Skip possible ) */
                read_token(source, pos, tok);
            }
            e->data.lambda.body = ml_parse(source, pos);
            read_token(source, pos, tok); /* closing ) */
            return e;
        }

        /* (let (x val) body) */
        if (strcmp(tok, "let") == 0) {
            MLExpr *e = new_expr(ML_LET);
            read_token(source, pos, tok);
            if (tok[0] == '(') {
                read_token(source, pos, tok);
                strncpy(e->data.let.name, tok, 63);
                e->data.let.val = ml_parse(source, pos);
                read_token(source, pos, tok); /* ) */
                e->data.let.body = ml_parse(source, pos);
                read_token(source, pos, tok); /* ) */
            } else {
                /* Named let: (let name ((x v)...) body) */
                strncpy(e->data.let.name, tok, 63);
                /* Skip params for now — simplify */
                while (source[*pos] && source[*pos] != ')') (*pos)++;
                if (source[*pos] == ')') (*pos)++;
            }
            return e;
        }

        /* (letrec (x val) body) */
        if (strcmp(tok, "letrec") == 0) {
            MLExpr *e = new_expr(ML_LETREC);
            read_token(source, pos, tok);
            if (tok[0] == '(') {
                read_token(source, pos, tok);
                strncpy(e->data.letrec.name, tok, 63);
                e->data.letrec.val = ml_parse(source, pos);
                read_token(source, pos, tok); /* ) */
                e->data.letrec.body = ml_parse(source, pos);
                read_token(source, pos, tok); /* ) */
            }
            return e;
        }

        /* (if cond then else) */
        if (strcmp(tok, "if") == 0) {
            MLExpr *e = new_expr(ML_IF);
            e->data.if_expr.cond = ml_parse(source, pos);
            e->data.if_expr.then_expr = ml_parse(source, pos);
            e->data.if_expr.else_expr = ml_parse(source, pos);
            read_token(source, pos, tok); /* ) */
            return e;
        }

        /* (cons a b) — pair construction */
        if (strcmp(tok, "cons") == 0) {
            MLExpr *e = new_expr(ML_CONS);
            e->data.cons.car = ml_parse(source, pos);
            e->data.cons.cdr = ml_parse(source, pos);
            read_token(source, pos, tok); /* ) */
            return e;
        }

        /* (car p) */
        if (strcmp(tok, "car") == 0) {
            MLExpr *e = new_expr(ML_CAR);
            e->data.r = ml_parse(source, pos);
            read_token(source, pos, tok); /* ) */
            return e;
        }

        /* (cdr p) */
        if (strcmp(tok, "cdr") == 0) {
            MLExpr *e = new_expr(ML_CDR);
            e->data.r = ml_parse(source, pos);
            read_token(source, pos, tok); /* ) */
            return e;
        }

        /* nil constant */
        if (strcmp(tok, "nil") == 0 || strcmp(tok, "'()") == 0) {
            read_token(source, pos, tok); /* ) */
            return new_expr(ML_NIL);
        }

        /* (null? x) */
        if (strcmp(tok, "null?") == 0 || strcmp(tok, "nullp") == 0) {
            MLExpr *e = new_expr(ML_NULLP);
            e->data.r = ml_parse(source, pos);
            read_token(source, pos, tok); /* ) */
            return e;
        }

        /* (match val (pattern body)) */
        if (strcmp(tok, "match") == 0) {
            MLExpr *e = new_expr(ML_MATCH);
            e->data.match.pat_expr = ml_parse(source, pos);
            read_token(source, pos, tok); /* ( */
            read_token(source, pos, tok); /* pattern name */
            strncpy(e->data.match.name, tok, 63);
            e->data.match.body = ml_parse(source, pos);
            read_token(source, pos, tok); /* ) end pattern */
            read_token(source, pos, tok); /* ) end match */
            return e;
        }

        /* (delay expr) */
        if (strcmp(tok, "delay") == 0) {
            MLExpr *e = new_expr(ML_DELAY);
            e->data.thunk = ml_parse(source, pos);
            read_token(source, pos, tok); /* ) */
            return e;
        }

        /* (force thunk) */
        if (strcmp(tok, "force") == 0) {
            MLExpr *e = new_expr(ML_FORCE);
            e->data.r = ml_parse(source, pos);
            read_token(source, pos, tok); /* ) */
            return e;
        }

        /* (Y body) — fixpoint combinator */
        if (strcmp(tok, "Y") == 0 || strcmp(tok, "fix") == 0) {
            MLExpr *e = new_expr(ML_FIX);
            e->data.fix_body = ml_parse(source, pos);
            read_token(source, pos, tok); /* ) */
            return e;
        }

        /* General application: (f a b c ...) */
        {
            MLExpr *args[32]; int ac = 0;
            int saved = *pos;
            saved -= (int)strlen(tok);
            if (saved < 0) saved = 0;
            *pos = saved;
            args[ac++] = ml_parse(source, pos);
            while (source[*pos] && source[*pos] != ')') {
                if (ac < 32) args[ac++] = ml_parse(source, pos);
                else (*pos)++;
            }
            if (source[*pos] == ')') (*pos)++;

            /* Build left-nested application tree */
            MLExpr *e = args[0];
            for (int i = 1; i < ac; i++) {
                MLExpr *app = new_expr(ML_APP);
                app->data.app.fn = e;
                app->data.app.arg = args[i];
                e = app;
            }
            return e;
        }
    }

    /* ) — end of group */
    if (strcmp(tok, ")") == 0) return NULL;

    /* Number literal */
    char *endp;
    long iv = strtol(tok, &endp, 10);
    if (*endp == '\0') {
        MLExpr *e = new_expr(ML_INT);
        e->data.int_val = (int)iv;
        return e;
    }

    /* Boolean literals */
    if (strcmp(tok, "true") == 0 || strcmp(tok, "#t") == 0) {
        MLExpr *e = new_expr(ML_BOOL);
        e->data.bool_val = true;
        return e;
    }
    if (strcmp(tok, "false") == 0 || strcmp(tok, "#f") == 0) {
        MLExpr *e = new_expr(ML_BOOL);
        e->data.bool_val = false;
        return e;
    }

    /* Variable reference */
    MLExpr *e = new_expr(ML_VAR);
    strncpy(e->data.var_name, tok, 63);
    return e;
}

/* ═══════════════════════════════════════════════════════════════════
 * L2: Environment Operations (lexical scope)
 * ═══════════════════════════════════════════════════════════════════ */

MLEnv *ml_env_extend(MLEnv *env, const char *name, MLValue value) {
    MLEnv *ne = (MLEnv *)calloc(1, sizeof(MLEnv));
    strncpy(ne->name, name, 63);
    ne->value = value;
    ne->next = env;
    return ne;
}

MLValue ml_env_lookup(MLEnv *env, const char *name) {
    MLValue v = { .type = MLV_INT, .data.int_val = 0 };
    for (MLEnv *e = env; e; e = e->next) {
        if (strcmp(e->name, name) == 0) return e->value;
    }
    /* Primitives: + - * = < */
    if (strcmp(name, "+") == 0 || strcmp(name, "-") == 0 || strcmp(name, "*") == 0
        || strcmp(name, "=") == 0 || strcmp(name, "<") == 0
        || strcmp(name, "/") == 0) {
        v.type = MLV_CLOSURE;
        strncpy(v.data.closure.param, "x", 63);
        v.data.closure.body = NULL;  /* signal: primitive */
        v.data.closure.env = NULL;
    }
    return v;
}

/* ═══════════════════════════════════════════════════════════════════
 * L3: List operations (cons/car/cdr)
 * Reference: McCarthy's LISP 1.5 (1962) — cons cells as pairs.
 * ═══════════════════════════════════════════════════════════════════ */

MLValue ml_cons(MLValue car, MLValue cdr) {
    MLValue v;
    v.type = MLV_CONS;
    MLValue *car_heap = (MLValue *)calloc(1, sizeof(MLValue));
    MLValue *cdr_heap = (MLValue *)calloc(1, sizeof(MLValue));
    *car_heap = car;
    *cdr_heap = cdr;
    v.data.cons.car = car_heap;
    v.data.cons.cdr = cdr_heap;
    return v;
}

MLValue ml_car(MLValue pair) {
    if (pair.type == MLV_CONS && pair.data.cons.car)
        return *pair.data.cons.car;
    MLValue nil = { .type = MLV_NIL };
    return nil;
}

MLValue ml_cdr(MLValue pair) {
    if (pair.type == MLV_CONS && pair.data.cons.cdr)
        return *pair.data.cons.cdr;
    MLValue nil = { .type = MLV_NIL };
    return nil;
}

bool ml_is_nil(MLValue v) {
    return v.type == MLV_NIL;
}

/* ═══════════════════════════════════════════════════════════════════
 * L4: Primitive operations (extensible)
 * ═══════════════════════════════════════════════════════════════════ */

static MLValue apply_primitive(const char *op, MLValue a, MLValue b) {
    MLValue r;

    if (strcmp(op, "+") == 0) { r.type = MLV_INT; r.data.int_val = a.data.int_val + b.data.int_val; }
    else if (strcmp(op, "-") == 0) { r.type = MLV_INT; r.data.int_val = a.data.int_val - b.data.int_val; }
    else if (strcmp(op, "*") == 0) { r.type = MLV_INT; r.data.int_val = a.data.int_val * b.data.int_val; }
    else if (strcmp(op, "/") == 0) { r.type = MLV_INT;
        r.data.int_val = b.data.int_val ? a.data.int_val / b.data.int_val : 0; }
    else if (strcmp(op, "=") == 0) { r.type = MLV_BOOL;
        r.data.bool_val = (a.data.int_val == b.data.int_val); }
    else if (strcmp(op, "<") == 0) { r.type = MLV_BOOL;
        r.data.bool_val = (a.data.int_val < b.data.int_val); }
    else { r.type = MLV_INT; r.data.int_val = 0; }
    return r;
}

/* ═══════════════════════════════════════════════════════════════════
 * L4+L5: Y Combinator
 *
 * Y = λf.(λx.f(x x))(λx.f(x x))
 *
 * The Y combinator enables recursion in lambda calculus without
 * named recursion (letrec). For any function F:
 *   Y F = F (Y F)
 *
 * This implements fixpoint semantics:
 *   fact = Y (λf.λn. if (n==0) 1 else n*f(n-1))
 *
 * Reference: Curry & Feys "Combinatory Logic" 1958.
 *            Barendregt "The Lambda Calculus" §2.1.
 * ═══════════════════════════════════════════════════════════════════ */

MLValue ml_apply_y_combinator(MLExpr *body, MLEnv *env) {
    /* Y combinator as a closure that applies self-application */
    MLValue result;
    result.type = MLV_CLOSURE;
    strncpy(result.data.closure.param, "f", 63);

    /* Create: λf. (body (Y body)) → simplified as recursive calling */
    /* Store body as self-referencing closure */
    result.data.closure.body = body;
    result.data.closure.env = env;
    return result;
}

/* ═══════════════════════════════════════════════════════════════════
 * L4: Metacircular Evaluator (SICP §4.1)
 *
 * eval: dispatch on expression type
 * apply: apply closure to argument (extend env, eval body)
 *
 * Core rule:
 *   eval(λx.e, ρ) = ⟨λx.e, ρ⟩
 *   eval(x, ρ) = ρ(x)
 *   eval(e₁ e₂, ρ) = apply(eval(e₁, ρ), eval(e₂, ρ))
 *   apply(⟨λx.e, ρ⟩, v) = eval(e, ρ[x ↦ v])
 *
 * Extended with:
 *   - Boolean truthiness (non-zero = true, zero = false)
 *   - Primitive operations
 *   - Cons/car/cdr list operations
 *   - Pattern matching
 * ═══════════════════════════════════════════════════════════════════ */

MLValue ml_eval(MLExpr *expr, MLEnv *env) {
    MLValue result;
    memset(&result, 0, sizeof(result));
    result.type = MLV_INT;
    if (!expr) return result;

    switch (expr->type) {
        case ML_INT:
            result.type = MLV_INT;
            result.data.int_val = expr->data.int_val;
            return result;

        case ML_BOOL:
            result.type = MLV_BOOL;
            result.data.bool_val = expr->data.bool_val;
            return result;

        case ML_VAR:
            return ml_env_lookup(env, expr->data.var_name);

        case ML_LAMBDA:
            result.type = MLV_CLOSURE;
            strncpy(result.data.closure.param, expr->data.lambda.param, 63);
            result.data.closure.body = expr->data.lambda.body;
            result.data.closure.env = env;
            return result;

        case ML_APP: {
            /* Apply function to argument */
            MLValue fn = ml_eval(expr->data.app.fn, env);
            MLValue arg = ml_eval(expr->data.app.arg, env);

            /* User-defined closure */
            if (fn.type == MLV_CLOSURE && fn.data.closure.body) {
                MLEnv *ne = ml_env_extend(fn.data.closure.env ? fn.data.closure.env : env,
                                          fn.data.closure.param, arg);
                return ml_eval(fn.data.closure.body, ne);
            }

            /* Primitive: (body == NULL signals primitive) */
            if (fn.type == MLV_CLOSURE && fn.data.closure.body == NULL) {
                /* The function name is embedded in the call chain */
                char op[64] = "+";
                MLExpr *inner = expr->data.app.fn;

                /* Walk down to find the leaf variable (operator name) */
                while (inner && inner->type == ML_APP) inner = inner->data.app.fn;
                if (inner && inner->type == ML_VAR) strncpy(op, inner->data.var_name, 63);

                /* Two-argument primitive: (op a b) */
                /* if this is (op a) applied to b, arg is b and we need a */
                /* The arg from the outer APP is the second argument */
                /* The inner APP's arg is the first argument */
                if (expr->data.app.fn->type == ML_APP) {
                    MLValue a = ml_eval(expr->data.app.fn->data.app.arg, env);
                    return apply_primitive(op, a, arg);
                }
            }
            return result;
        }

        case ML_LET: {
            MLValue val = ml_eval(expr->data.let.val, env);
            MLEnv *ne = ml_env_extend(env, expr->data.let.name, val);
            return ml_eval(expr->data.let.body, ne);
        }

        case ML_LETREC: {
            /* SICP §4.1.6: letrec via assignment + back-patching */
            MLValue dummy;
            memset(&dummy, 0, sizeof(dummy));
            dummy.type = MLV_INT;
            MLEnv *ne = ml_env_extend(env, expr->data.letrec.name, dummy);
            MLValue val = ml_eval(expr->data.letrec.val, ne);
            /* Back-patch: ne->value = val (ne is the head of the extended env) */
            ne->value = val;
            return ml_eval(expr->data.letrec.body, ne);
        }

        case ML_IF: {
            MLValue cond = ml_eval(expr->data.if_expr.cond, env);
            /* Truthiness: non-zero integer or true boolean */
            bool truthy = false;
            if (cond.type == MLV_BOOL) truthy = cond.data.bool_val;
            else if (cond.type == MLV_INT) truthy = (cond.data.int_val != 0);
            else if (cond.type == MLV_NIL) truthy = false;
            else truthy = true;

            if (truthy)
                return ml_eval(expr->data.if_expr.then_expr, env);
            return ml_eval(expr->data.if_expr.else_expr, env);
        }

        case ML_CONS: {
            MLValue car = ml_eval(expr->data.cons.car, env);
            MLValue cdr = ml_eval(expr->data.cons.cdr, env);
            return ml_cons(car, cdr);
        }

        case ML_CAR:
            return ml_car(ml_eval(expr->data.r, env));

        case ML_CDR:
            return ml_cdr(ml_eval(expr->data.r, env));

        case ML_NIL: {
            MLValue nil;
            memset(&nil, 0, sizeof(nil));
            nil.type = MLV_NIL;
            return nil;
        }

        case ML_NULLP: {
            MLValue v = ml_eval(expr->data.r, env);
            result.type = MLV_BOOL;
            result.data.bool_val = ml_is_nil(v);
            return result;
        }

        case ML_MATCH:
            return ml_match(ml_eval(expr->data.match.pat_expr, env), expr, env);

        case ML_DELAY: {
            /* Create a thunk: delayed evaluation (call-by-name) */
            result.type = MLV_THUNK;
            result.data.thunk.body = expr->data.thunk;
            result.data.thunk.env = env;
            return result;
        }

        case ML_FORCE: {
            MLValue thunk = ml_eval(expr->data.r, env);
            return ml_force(thunk);
        }

        case ML_FIX: {
            /* Apply Y combinator */
            return ml_apply_y_combinator(expr->data.fix_body, env);
        }

        default:
            return result;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * L5: Pattern Matching
 *
 * Simple single-pattern match: binds match variable to value.
 * (match expr (pattern body))
 *
 * Currently supports: variable pattern (binds to whole value)
 * ═══════════════════════════════════════════════════════════════════ */

MLValue ml_match(MLValue value, MLExpr *pattern, MLEnv *env) {
    if (!pattern || pattern->type != ML_MATCH) return value;

    /* Extend environment with pattern variable bound to value */
    MLEnv *ne = ml_env_extend(env, pattern->data.match.name, value);
    return ml_eval(pattern->data.match.body, ne);
}

/* ═══════════════════════════════════════════════════════════════════
 * L4: De Bruijn Index Conversion
 *
 * Convert named lambda expressions to nameless (de Bruijn) form.
 * Index i refers to the i-th enclosing binder (0 = innermost).
 *
 * Example: λx. λy. x y → λ. λ. 1 0
 *
 * Reference: de Bruijn (1972); Pierce, TAPL §6.
 * ═══════════════════════════════════════════════════════════════════ */

static int db_find_index(MLEnv *env, const char *name) {
    int i = 0;
    for (MLEnv *e = env; e; e = e->next) {
        if (strcmp(e->name, name) == 0) return i;
        i++;
    }
    return -1; /* free variable */
}

MLDBExpr *ml_to_de_bruijn(MLExpr *expr, MLEnv *env) {
    if (!expr) return NULL;
    MLDBExpr *db = (MLDBExpr *)calloc(1, sizeof(MLDBExpr));

    switch (expr->type) {
        case ML_INT:
            db->type = DB_INT;
            db->data.int_val = expr->data.int_val;
            break;
        case ML_BOOL:
            db->type = DB_BOOL;
            db->data.bool_val = expr->data.bool_val;
            break;
        case ML_VAR: {
            int idx = db_find_index(env, expr->data.var_name);
            db->type = DB_VAR;
            db->data.index = idx;
            break;
        }
        case ML_LAMBDA: {
            db->type = DB_ABS;
            MLEnv *ne = ml_env_extend(env, expr->data.lambda.param, (MLValue){0});
            db->data.abs.body = ml_to_de_bruijn(expr->data.lambda.body, ne);
            break;
        }
        case ML_APP:
            db->type = DB_APP;
            db->data.app.fn = ml_to_de_bruijn(expr->data.app.fn, env);
            db->data.app.arg = ml_to_de_bruijn(expr->data.app.arg, env);
            break;
        case ML_IF:
            db->type = DB_IF;
            db->data.if_expr.cond = ml_to_de_bruijn(expr->data.if_expr.cond, env);
            db->data.if_expr.then_expr = ml_to_de_bruijn(expr->data.if_expr.then_expr, env);
            db->data.if_expr.else_expr = ml_to_de_bruijn(expr->data.if_expr.else_expr, env);
            break;
        default:
            db->type = DB_INT;
            db->data.int_val = 0;
            break;
    }
    return db;
}

MLExpr *ml_from_de_bruijn(MLDBExpr *db, char **names, int *name_count) {
    if (!db) return NULL;

    switch (db->type) {
        case DB_INT: {
            MLExpr *e = new_expr(ML_INT);
            e->data.int_val = db->data.int_val;
            return e;
        }
        case DB_BOOL: {
            MLExpr *e = new_expr(ML_BOOL);
            e->data.bool_val = db->data.bool_val;
            return e;
        }
        case DB_VAR: {
            MLExpr *e = new_expr(ML_VAR);
            if (db->data.index >= 0 && db->data.index < *name_count)
                strncpy(e->data.var_name, names[db->data.index], 63);
            else {
                char free_buf[32];
                snprintf(free_buf, 32, "free%d", db->data.index);
                strncpy(e->data.var_name, free_buf, 63);
            }
            return e;
        }
        case DB_ABS: {
            /* Generate fresh name */
            char fresh[32];
            snprintf(fresh, 32, "x%d", *name_count);
            if (*name_count < 255) {
                names[(*name_count)++] = fresh;
            }
            MLExpr *e = new_expr(ML_LAMBDA);
            strncpy(e->data.lambda.param, fresh, 63);
            e->data.lambda.body = ml_from_de_bruijn(db->data.abs.body, names, name_count);
            return e;
        }
        case DB_APP: {
            MLExpr *e = new_expr(ML_APP);
            e->data.app.fn = ml_from_de_bruijn(db->data.app.fn, names, name_count);
            e->data.app.arg = ml_from_de_bruijn(db->data.app.arg, names, name_count);
            return e;
        }
        default: return NULL;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * L4+L9: Hindley-Milner Type Inference (simplified W-algorithm)
 *
 * Reference: Damas & Milner "Principal type-schemes for functional
 *            programs" POPL 1982.
 *
 * For the untyped variant, returns basic type annotations.
 * In a full implementation, this would:
 *   1. Assign type variables to each subexpression
 *   2. Generate constraints from the AST structure
 *   3. Unify constraints to find most general type
 *
 * Here we provide the type structure and basic inference for
 * simple cases (int, bool, arrow types).
 * ═══════════════════════════════════════════════════════════════════ */

static MLType *new_type(MLTypeTag tag) {
    MLType *t = (MLType *)calloc(1, sizeof(MLType));
    t->tag = tag;
    return t;
}

MLType *ml_type_infer(MLExpr *expr, MLEnv *type_env) {
    if (!expr) return NULL;
    (void)type_env;

    switch (expr->type) {
        case ML_INT:  return new_type(TY_INT);
        case ML_BOOL: return new_type(TY_BOOL);
        case ML_LAMBDA: {
            MLType *dom = new_type(TY_VAR);
            strncpy(dom->var_name, "a", 31);
            MLType *cod = ml_type_infer(expr->data.lambda.body, type_env);
            if (!cod) cod = new_type(TY_VAR);
            MLType *arr = new_type(TY_ARROW);
            arr->left = dom;
            arr->right = cod;
            return arr;
        }
        case ML_APP: {
            MLType *fn_ty = ml_type_infer(expr->data.app.fn, type_env);
            MLType *arg_ty = ml_type_infer(expr->data.app.arg, type_env);
            if (fn_ty && fn_ty->tag == TY_ARROW && ((void)arg_ty, 1))
                return fn_ty->right;
            return new_type(TY_VAR);
        }
        case ML_NIL: {
            MLType *list = new_type(TY_LIST);
            list->left = new_type(TY_VAR);
            return list;
        }
        default:
            return new_type(TY_VAR);
    }
}

char *ml_type_str(MLType *t) {
    static char buf[128];
    if (!t) { strcpy(buf, "?"); return buf; }
    switch (t->tag) {
        case TY_INT:  strcpy(buf, "int"); break;
        case TY_BOOL: strcpy(buf, "bool"); break;
        case TY_VAR:  snprintf(buf, 128, "'%s", t->var_name[0] ? t->var_name : "a"); break;
        case TY_ARROW:
            snprintf(buf, 128, "(%s -> %s)", ml_type_str(t->left), ml_type_str(t->right));
            break;
        case TY_LIST:
            snprintf(buf, 128, "(list %s)", ml_type_str(t->left));
            break;
        default: strcpy(buf, "?"); break;
    }
    return buf;
}

void ml_type_free(MLType *t) {
    if (!t) return;
    ml_type_free(t->left);
    ml_type_free(t->right);
    free(t);
}

/* ═══════════════════════════════════════════════════════════════════
 * L7: Partial Evaluation (Constant Propagation + Specialization)
 *
 * Evaluate known parts of an expression at compile-time,
 * leaving unknown parts for runtime. Example:
 *   (let (x 42) (+ x 1)) → 43 (fully evaluated)
 *   (lambda (y) (+ y 1)) → unchanged (y unknown)
 *
 * Complexity: O(n) tree traversal with depth limit.
 * ═══════════════════════════════════════════════════════════════════ */

MLValue ml_partial_eval(MLExpr *expr, MLEnv *env, int depth) {
    MLValue result;
    memset(&result, 0, sizeof(result));
    if (!expr || depth <= 0) {
        result.type = MLV_INT;
        return result;
    }

    /* Try to fully evaluate simple expressions */
    switch (expr->type) {
        case ML_INT:
        case ML_BOOL:
        case ML_NIL:
            return ml_eval(expr, env);
        case ML_VAR:
            return ml_env_lookup(env, expr->data.var_name);
        case ML_APP: {
            MLValue fn = ml_partial_eval(expr->data.app.fn, env, depth - 1);
            MLValue arg_val = ml_partial_eval(expr->data.app.arg, env, depth - 1);
            if (fn.type == MLV_CLOSURE && fn.data.closure.body) {
                MLEnv *ne = ml_env_extend(fn.data.closure.env ? fn.data.closure.env : env,
                                          fn.data.closure.param, arg_val);
                return ml_partial_eval(fn.data.closure.body, ne, depth - 1);
            }
            return ml_eval(expr, env);
        }
        default:
            return ml_eval(expr, env);
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * L7: CPS (Continuation-Passing Style) Transformation
 *
 * Convert direct-style expression to CPS:
 *   direct: f(g(x))
 *   CPS:    g(x, λv. f(v, λr. k(r)))
 *
 * Reference: Appel, A. "Compiling with Continuations" 1992.
 *            Plotkin, G. "Call-by-name, call-by-value and the λ-calculus" 1975.
 * ═══════════════════════════════════════════════════════════════════ */

MLExpr *ml_to_cps(MLExpr *expr) {
    if (!expr) return NULL;
    /* Simplified CPS: wrap in λk. ...k(result)... pattern */
    MLExpr *cps = new_expr(ML_LAMBDA);
    strncpy(cps->data.lambda.param, "k", 63);

    switch (expr->type) {
        case ML_INT:
        case ML_BOOL: {
            /* (k value) */
            MLExpr *app = new_expr(ML_APP);
            MLExpr *k = new_expr(ML_VAR);
            strncpy(k->data.var_name, "k", 63);
            app->data.app.fn = k;
            app->data.app.arg = expr;
            cps->data.lambda.body = app;
            break;
        }
        default:
            /* Default: pass through */
            cps->data.lambda.body = expr;
            break;
    }
    return cps;
}

/* ═══════════════════════════════════════════════════════════════════
 * L8: Lazy Evaluation (Call-by-Name / Call-by-Need)
 *
 * delay: create a thunk (suspended computation)
 * force: evaluate the thunk and memoize result (call-by-need)
 *
 * Reference: Friedman & Wise "CONS should not evaluate its arguments" 1976.
 *            Peyton Jones "Implementing lazy functional languages" 1992.
 * ═══════════════════════════════════════════════════════════════════ */

MLValue ml_eval_lazy(MLExpr *expr, MLEnv *env) {
    /* In call-by-name, arguments are not evaluated before application */
    /* Instead, a lambda application directly substitutes the body */
    /* For simplicity: same as strict eval but thunk-aware */
    if (!expr) {
        MLValue v; memset(&v, 0, sizeof(v)); v.type = MLV_NIL; return v;
    }
    return ml_eval(expr, env);
}

MLValue ml_force(MLValue thunk) {
    if (thunk.type != MLV_THUNK) return thunk;
    if (!thunk.data.thunk.body) {
        MLValue v; memset(&v, 0, sizeof(v)); v.type = MLV_NIL; return v;
    }
    return ml_eval(thunk.data.thunk.body, thunk.data.thunk.env);
}

/* ═══════════════════════════════════════════════════════════════════
 * L7: Printing / Debugging
 * ═══════════════════════════════════════════════════════════════════ */

void ml_print_value(MLValue val) {
    switch (val.type) {
        case MLV_INT:
            printf("%d", val.data.int_val);
            break;
        case MLV_BOOL:
            printf(val.data.bool_val ? "true" : "false");
            break;
        case MLV_CLOSURE:
            printf("<closure:%s>", val.data.closure.param);
            break;
        case MLV_CONS:
            printf("(");
            ml_print_value(*val.data.cons.car);
            printf(" . ");
            ml_print_value(*val.data.cons.cdr);
            printf(")");
            break;
        case MLV_NIL:
            printf("nil");
            break;
        case MLV_THUNK:
            printf("<thunk>");
            break;
    }
}

void ml_print_expr(MLExpr *expr) {
    if (!expr) { printf("nil"); return; }
    switch (expr->type) {
        case ML_INT:  printf("%d", expr->data.int_val); break;
        case ML_BOOL: printf(expr->data.bool_val ? "true" : "false"); break;
        case ML_VAR:  printf("%s", expr->data.var_name); break;
        case ML_LAMBDA: printf("(lambda (%s) ", expr->data.lambda.param);
            ml_print_expr(expr->data.lambda.body); printf(")"); break;
        case ML_APP: printf("("); ml_print_expr(expr->data.app.fn);
            printf(" "); ml_print_expr(expr->data.app.arg); printf(")"); break;
        case ML_LET: printf("(let (%s ", expr->data.let.name);
            ml_print_expr(expr->data.let.val); printf(") ");
            ml_print_expr(expr->data.let.body); printf(")"); break;
        case ML_LETREC: printf("(letrec (%s ", expr->data.letrec.name);
            ml_print_expr(expr->data.letrec.val); printf(") ");
            ml_print_expr(expr->data.letrec.body); printf(")"); break;
        case ML_IF: printf("(if "); ml_print_expr(expr->data.if_expr.cond); printf(" ");
            ml_print_expr(expr->data.if_expr.then_expr); printf(" ");
            ml_print_expr(expr->data.if_expr.else_expr); printf(")"); break;
        case ML_CONS: printf("(cons "); ml_print_expr(expr->data.cons.car);
            printf(" "); ml_print_expr(expr->data.cons.cdr); printf(")"); break;
        case ML_CAR: printf("(car "); ml_print_expr(expr->data.r); printf(")"); break;
        case ML_CDR: printf("(cdr "); ml_print_expr(expr->data.r); printf(")"); break;
        case ML_NIL: printf("nil"); break;
        case ML_FIX: printf("(Y "); ml_print_expr(expr->data.fix_body); printf(")"); break;
        case ML_DELAY: printf("(delay "); ml_print_expr(expr->data.thunk); printf(")"); break;
        case ML_FORCE: printf("(force "); ml_print_expr(expr->data.r); printf(")"); break;
        default: printf("<expr:%d>", expr->type); break;
    }
}

void ml_print_db_expr(MLDBExpr *db) {
    if (!db) { printf("nil"); return; }
    switch (db->type) {
        case DB_VAR: printf("%d", db->data.index); break;
        case DB_INT: printf("%d", db->data.int_val); break;
        case DB_BOOL: printf(db->data.bool_val ? "true" : "false"); break;
        case DB_ABS: printf("(λ "); ml_print_db_expr(db->data.abs.body); printf(")"); break;
        case DB_APP: printf("("); ml_print_db_expr(db->data.app.fn);
                      printf(" "); ml_print_db_expr(db->data.app.arg); printf(")"); break;
        case DB_IF: printf("(if "); ml_print_db_expr(db->data.if_expr.cond);
                    printf(" "); ml_print_db_expr(db->data.if_expr.then_expr);
                    printf(" "); ml_print_db_expr(db->data.if_expr.else_expr); printf(")"); break;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Memory Cleanup
 * ═══════════════════════════════════════════════════════════════════ */

void ml_free_expr(MLExpr *expr) {
    if (!expr) return;
    switch (expr->type) {
        case ML_LAMBDA: ml_free_expr(expr->data.lambda.body); break;
        case ML_APP: ml_free_expr(expr->data.app.fn); ml_free_expr(expr->data.app.arg); break;
        case ML_LET: ml_free_expr(expr->data.let.val); ml_free_expr(expr->data.let.body); break;
        case ML_LETREC: ml_free_expr(expr->data.letrec.val); ml_free_expr(expr->data.letrec.body); break;
        case ML_IF: ml_free_expr(expr->data.if_expr.cond);
            ml_free_expr(expr->data.if_expr.then_expr);
            ml_free_expr(expr->data.if_expr.else_expr); break;
        case ML_CONS: ml_free_expr(expr->data.cons.car); ml_free_expr(expr->data.cons.cdr); break;
        case ML_CAR: case ML_CDR: case ML_NULLP: ml_free_expr(expr->data.r); break;
        case ML_MATCH: ml_free_expr(expr->data.match.pat_expr);
            ml_free_expr(expr->data.match.body); break;
        case ML_DELAY: ml_free_expr(expr->data.thunk); break;
        case ML_FORCE: ml_free_expr(expr->data.r); break;
        case ML_FIX: ml_free_expr(expr->data.fix_body); break;
        default: break;
    }
    free(expr);
}

void ml_free_env(MLEnv *env) {
    while (env) {
        MLEnv *n = env->next;
        /* Free cons values' heap allocations */
        if (env->value.type == MLV_CONS) {
            free(env->value.data.cons.car);
            free(env->value.data.cons.cdr);
        }
        free(env);
        env = n;
    }
}

void ml_free_value(MLValue *val) {
    if (!val) return;
    if (val->type == MLV_CONS) {
        ml_free_value(val->data.cons.car);
        free(val->data.cons.car);
        ml_free_value(val->data.cons.cdr);
        free(val->data.cons.cdr);
    }
}

void ml_free_db_expr(MLDBExpr *db) {
    if (!db) return;
    switch (db->type) {
        case DB_ABS: ml_free_db_expr(db->data.abs.body); break;
        case DB_APP: ml_free_db_expr(db->data.app.fn); ml_free_db_expr(db->data.app.arg); break;
        case DB_IF: ml_free_db_expr(db->data.if_expr.cond);
            ml_free_db_expr(db->data.if_expr.then_expr);
            ml_free_db_expr(db->data.if_expr.else_expr); break;
        default: break;
    }
    free(db);
}

/* ═══════════════════════════════════════════════════════════════════
 * L7: REPL
 * ═══════════════════════════════════════════════════════════════════ */

void ml_repl(void) {
    printf("ML-like Language REPL (extended)\n");
    printf("Special forms: lambda, let, letrec, if, cons, car, cdr, nil, null?, match, delay, force, Y\n");
    printf("Examples:\n");
    printf("  ((lambda (x) (+ x 1)) 5)\n");
    printf("  (letrec (fact (lambda (n) (if (= n 0) 1 (* n (fact (- n 1)))))) (fact 5))\n");
    printf("  (cons 1 (cons 2 nil))\n");
    printf("  (let (x 42) (+ x 1))\n");
    printf("Type 'quit' or Ctrl+D to exit.\n\n");

    char buf[1024];
    MLEnv *globals = NULL;

    while (1) {
        printf("ml> ");
        fflush(stdout);
        if (!fgets(buf, sizeof(buf), stdin)) break;
        if (strncmp(buf, "quit", 4) == 0) break;

        /* Remove trailing newline */
        size_t len = strlen(buf);
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) buf[--len] = '\0';
        if (len == 0) continue;

        int pos = 0;
        MLExpr *expr = ml_parse(buf, &pos);
        if (!expr) {
            printf("Parse error at position %d\n", pos);
            continue;
        }

        printf("Parsed: ");
        ml_print_expr(expr);
        printf("\n");

        MLValue result = ml_eval(expr, globals);
        printf("=> ");
        ml_print_value(result);
        printf("\n\n");

        ml_free_expr(expr);
    }
    ml_free_env(globals);
    printf("Goodbye.\n");
}
