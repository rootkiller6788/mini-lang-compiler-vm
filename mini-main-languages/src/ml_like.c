#include "ml_like.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void skip_ws(const char *s, int *pos) {
    while (s[*pos] && isspace((unsigned char)s[*pos])) (*pos)++;
}

static char *read_token(const char *s, int *pos, char *buf) {
    int i = 0;
    skip_ws(s, pos);
    if (s[*pos] == '(' || s[*pos] == ')' || s[*pos] == '+' || s[*pos] == '*' || s[*pos] == '='
        || s[*pos] == '.' || s[*pos] == '\\' || s[*pos] == '-') {
        buf[i++] = s[(*pos)++]; buf[i] = '\0'; return buf;
    }
    while (isalnum((unsigned char)s[*pos]) || s[*pos] == '_' || s[*pos] == '?' || s[*pos] == '!'
           || s[*pos] == '<' || s[*pos] == '>') {
        if (i < 63) buf[i++] = s[*pos];
        (*pos)++;
    }
    buf[i] = '\0';
    return (i > 0) ? buf : NULL;
}

static MLExpr *new_expr(MLExprType t) {
    MLExpr *e = (MLExpr *)calloc(1, sizeof(MLExpr));
    if (e) e->type = t;
    return e;
}

MLExpr *ml_parse(const char *source, int *pos) {
    char tok[64];
    read_token(source, pos, tok);
    if (strlen(tok) == 0) return NULL;

    if (strcmp(tok, "(") == 0) {
        read_token(source, pos, tok);
        if (strcmp(tok, "lambda") == 0 || strcmp(tok, "\\") == 0) {
            MLExpr *e = new_expr(ML_LAMBDA);
            read_token(source, pos, tok);
            if (tok[0] == '(') {
                read_token(source, pos, tok);
            }
            strcpy(e->data.lambda.param, tok);
            e->data.lambda.body = ml_parse(source, pos);
            if (tok[0] == '(') { read_token(source, pos, tok); }
            read_token(source, pos, tok);
            return e;
        }
        if (strcmp(tok, "let") == 0) {
            MLExpr *e = new_expr(ML_LET);
            read_token(source, pos, tok);
            if (tok[0] == '(') {
                read_token(source, pos, tok);
                strcpy(e->data.let.name, tok);
                e->data.let.val = ml_parse(source, pos);
                read_token(source, pos, tok);
                e->data.let.body = ml_parse(source, pos);
                read_token(source, pos, tok);
            }
            return e;
        }
        if (strcmp(tok, "letrec") == 0) {
            MLExpr *e = new_expr(ML_LETREC);
            read_token(source, pos, tok);
            if (tok[0] == '(') {
                read_token(source, pos, tok);
                strcpy(e->data.letrec.name, tok);
                e->data.letrec.val = ml_parse(source, pos);
                read_token(source, pos, tok);
                e->data.letrec.body = ml_parse(source, pos);
                read_token(source, pos, tok);
            }
            return e;
        }
        if (strcmp(tok, "if") == 0) {
            MLExpr *e = new_expr(ML_IF);
            e->data.if_expr.cond = ml_parse(source, pos);
            e->data.if_expr.then_expr = ml_parse(source, pos);
            e->data.if_expr.else_expr = ml_parse(source, pos);
            read_token(source, pos, tok);
            return e;
        }
        MLExpr *e = new_expr(ML_APP);
        MLExpr *args[32]; int ac = 0;
        int saved = *pos;
        saved -= (int)strlen(tok);
        *pos = saved;
        args[ac++] = ml_parse(source, pos);
        while (source[*pos] && source[*pos] != ')') {
            args[ac++] = ml_parse(source, pos);
        }
        if (source[*pos] == ')') (*pos)++;
        e->data.app.fn = args[0];
        for (int i = 1; i < ac; i++) {
            MLExpr *app = new_expr(ML_APP);
            app->data.app.fn = e;
            app->data.app.arg = args[i];
            e = app;
        }
        return e;
    }

    if (strcmp(tok, ")") == 0) return NULL;

    char *endp;
    long iv = strtol(tok, &endp, 10);
    if (*endp == '\0') {
        MLExpr *e = new_expr(ML_INT);
        e->data.int_val = (int)iv;
        return e;
    }

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

    MLExpr *e = new_expr(ML_VAR);
    strcpy(e->data.var_name, tok);
    return e;
}

MLEnv *ml_env_extend(MLEnv *env, const char *name, MLValue value) {
    MLEnv *ne = (MLEnv *)calloc(1, sizeof(MLEnv));
    strcpy(ne->name, name);
    ne->value = value;
    ne->next = env;
    return ne;
}

MLValue ml_env_lookup(MLEnv *env, const char *name) {
    MLValue v = { .type = MLV_INT, .data.int_val = 0 };
    for (MLEnv *e = env; e; e = e->next) {
        if (strcmp(e->name, name) == 0) return e->value;
    }
    if (strcmp(name, "+") == 0 || strcmp(name, "-") == 0 || strcmp(name, "*") == 0
        || strcmp(name, "=") == 0 || strcmp(name, "<") == 0) {
        v.type = MLV_CLOSURE;
        strcpy(v.data.closure.param, "x");
        v.data.closure.body = NULL;
        v.data.closure.env = NULL;
    }
    return v;
}

static MLValue apply_primitive(const char *op, MLValue a, MLValue b) {
    MLValue r;
    r.type = (strcmp(op, "=") == 0 || strcmp(op, "<") == 0) ? MLV_BOOL : MLV_INT;
    if (strcmp(op, "+") == 0) { r.data.int_val = a.data.int_val + b.data.int_val; }
    else if (strcmp(op, "-") == 0) { r.data.int_val = a.data.int_val - b.data.int_val; }
    else if (strcmp(op, "*") == 0) { r.data.int_val = a.data.int_val * b.data.int_val; }
    else if (strcmp(op, "=") == 0) { r.data.bool_val = (a.data.int_val == b.data.int_val); }
    else if (strcmp(op, "<") == 0) { r.data.bool_val = (a.data.int_val < b.data.int_val); }
    return r;
}

MLValue ml_eval(MLExpr *expr, MLEnv *env) {
    MLValue result;
    result.type = MLV_INT;
    result.data.int_val = 0;
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
            strcpy(result.data.closure.param, expr->data.lambda.param);
            result.data.closure.body = expr->data.lambda.body;
            result.data.closure.env = env;
            return result;
        case ML_APP: {
            MLValue fn = ml_eval(expr->data.app.fn, env);
            MLValue arg = ml_eval(expr->data.app.arg, env);
            if (fn.type == MLV_CLOSURE && fn.data.closure.body) {
                MLEnv *ne = ml_env_extend(fn.data.closure.env ? fn.data.closure.env : env,
                                          fn.data.closure.param, arg);
                return ml_eval(fn.data.closure.body, ne);
            }
            MLValue rhs = ml_eval(expr->data.app.fn, env);
            if (rhs.type == MLV_CLOSURE && rhs.data.closure.body == NULL) {
                char op[64];
                MLExpr *inner = expr->data.app.fn;
                while (inner && inner->type == ML_APP) inner = inner->data.app.fn;
                if (inner && inner->type == ML_VAR) strcpy(op, inner->data.var_name);
                MLValue arg2 = ml_eval(expr->data.app.arg, env);
                return apply_primitive(op, arg, arg2);
            }
            return result;
        }
        case ML_LET: {
            MLValue val = ml_eval(expr->data.let.val, env);
            MLEnv *ne = ml_env_extend(env, expr->data.let.name, val);
            return ml_eval(expr->data.let.body, ne);
        }
        case ML_LETREC: {
            MLValue dummy;
            dummy.type = MLV_INT;
            dummy.data.int_val = 0;
            MLEnv *ne = ml_env_extend(env, expr->data.letrec.name, dummy);
            MLValue val = ml_eval(expr->data.letrec.val, ne);
            ne->value = val;
            return ml_eval(expr->data.letrec.body, ne);
        }
        case ML_IF: {
            MLValue cond = ml_eval(expr->data.if_expr.cond, env);
            if (cond.type == MLV_BOOL && cond.data.bool_val)
                return ml_eval(expr->data.if_expr.then_expr, env);
            return ml_eval(expr->data.if_expr.else_expr, env);
        }
    }
    return result;
}

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
        case ML_IF: printf("(if "); ml_print_expr(expr->data.if_expr.cond); printf(" ");
            ml_print_expr(expr->data.if_expr.then_expr); printf(" ");
            ml_print_expr(expr->data.if_expr.else_expr); printf(")"); break;
        default: printf("?"); break;
    }
}

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
        default: break;
    }
    free(expr);
}

void ml_free_env(MLEnv *env) {
    while (env) { MLEnv *n = env->next; free(env); env = n; }
}

void ml_repl(void) {
    printf("ML-like Language REPL\n");
    printf("Examples: ((lambda (x) (+ x 1)) 5)\n");
    printf("          (letrec (fact (lambda (n) (if (= n 0) 1 (* n (fact (- n 1)))))) (fact 5))\n");
    printf("Type 'quit' to exit.\n\n");

    char buf[1024];
    MLEnv *globals = NULL;

    while (1) {
        printf("ml> ");
        if (!fgets(buf, sizeof(buf), stdin)) break;
        if (strncmp(buf, "quit", 4) == 0) break;
        int pos = 0;
        MLExpr *expr = ml_parse(buf, &pos);
        if (!expr) { printf("Parse error\n"); continue; }
        MLValue result = ml_eval(expr, globals);
        printf("=> ");
        ml_print_value(result);
        printf("\n");
        ml_free_expr(expr);
    }
    ml_free_env(globals);
    printf("Goodbye.\n");
}
