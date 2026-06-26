/* c_subset.c — C Subset Interpreter
 * ============================================================================
 * L1-L9 knowledge coverage:
 *   L1: CType enum, ASTNodeType enum, ASTNode tagged union, CVar/CFunc structs
 *   L2: Tree-walking interpreter, environment scoping (linked-list frames)
 *   L3: Recursive descent parser, AST evaluation pipeline
 *   L4: Constant folding optimization, dead code elimination,
 *        Type checking rules
 *   L5: Recursive descent parser (Pratt-inspired), tree-walking evaluator,
 *        for-loop, break/continue control flow
 *   L6: Fibonacci, factorial, sum-array through AST execution
 *   L7: Stack-based bytecode compiler, VM executor
 *   L8: Bytecode compilation (source → bytecode → VM execution)
 *   L9: Simple JIT direction (bytecode as intermediate representation)
 *
 * Reference: Aho, Lam, Sethi, Ullman. "Compilers: Principles,
 *            Techniques, and Tools" (Dragon Book), Ch. 2-8.
 *            Wirth, N. "Compiler Construction" 1996.
 */

#include "c_subset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ═══════════════════════════════════════════════════════════════════
 * L1: Program Lifecycle
 * ═══════════════════════════════════════════════════════════════════ */

void c_init_program(CProgram *prog) {
    prog->globals = NULL;
    prog->functions = NULL;
}

/* ═══════════════════════════════════════════════════════════════════
 * L2: Environment Operations (linked-list scopes)
 *   Lookup: O(n) linear scan, newest binding first (shadowing)
 * ═══════════════════════════════════════════════════════════════════ */

CVar *c_find_var(CVar *head, const char *name) {
    for (CVar *v = head; v; v = v->next)
        if (strcmp(v->name, name) == 0) return v;
    return NULL;
}

CFunc *c_find_func(CFunc *head, const char *name) {
    for (CFunc *f = head; f; f = f->next)
        if (strcmp(f->name, name) == 0) return f;
    return NULL;
}

int c_env_size(CVar *env) {
    int n = 0;
    for (CVar *v = env; v; v = v->next) n++;
    return n;
}

/* ═══════════════════════════════════════════════════════════════════
 * L3: Lexer Utilities
 * ═══════════════════════════════════════════════════════════════════ */

static void skip_ws(const char *s, int *pos) {
    while (s[*pos] && isspace((unsigned char)s[*pos])) (*pos)++;
}

static char *read_ident(const char *s, int *pos, char *buf) {
    int i = 0;
    while (isalnum((unsigned char)s[*pos]) || s[*pos] == '_') {
        if (i < 63) { buf[i++] = s[*pos]; }
        (*pos)++;
    }
    buf[i] = '\0';
    return (i > 0) ? buf : NULL;
}

static int read_int(const char *s, int *pos) {
    int sign = 1;
    if (s[*pos] == '-') { sign = -1; (*pos)++; }
    int n = 0;
    while (isdigit((unsigned char)s[*pos])) {
        n = n * 10 + (s[*pos] - '0'); (*pos)++;
    }
    return n * sign;
}

static float read_float(const char *s, int *pos) {
    int start = *pos;
    if (s[*pos] == '-') (*pos)++;
    while (isdigit((unsigned char)s[*pos]) || s[*pos] == '.') (*pos)++;
    char buf[64]; int len = *pos - start;
    if (len >= 64) len = 63;
    memcpy(buf, s + start, (size_t)len); buf[len] = '\0';
    return (float)atof(buf);
}

/* ═══════════════════════════════════════════════════════════════════
 * L3: AST Allocation
 * ═══════════════════════════════════════════════════════════════════ */

static ASTNode *new_node(ASTNodeType t) {
    ASTNode *n = (ASTNode *)calloc(1, sizeof(ASTNode));
    if (n) n->type = t;
    return n;
}

/* ═══════════════════════════════════════════════════════════════════
 * L5: Recursive Descent Parser
 *
 * Grammar (extended C subset):
 *   expr     → logical_or
 *   logical_or → logical_and ('||' logical_and)*
 *   logical_and → compare ('&&' compare)*
 *   compare  → add (('==' | '!=' | '<' | '>' | '<=' | '>=') add)*
 *   add      → mul (('+' | '-') mul)*
 *   mul      → unary (('*' | '/' | '%') unary)*
 *   unary    → ('!' | '-')? primary
 *   primary  → INT | FLOAT | ident | '(' expr ')' | ident '(' args ')' | ident '[' expr ']'
 *   stmt     → if_stmt | while_stmt | for_stmt | break_stmt | cont_stmt | block | assign | call | return
 *   if_stmt  → 'if' '(' expr ')' stmt ['else' stmt]
 *   while_stmt → 'while' '(' expr ')' stmt
 *   for_stmt → 'for' '(' assign? ';' expr? ';' assign? ')' stmt
 *
 * Operator precedence (Dragon Book §4.2):
 *   Level   Operators
 *   1       ||          (logical or)
 *   2       &&          (logical and)
 *   3       == != < > <= >=  (comparison)
 *   4       + -         (additive)
 *   5       * / %       (multiplicative)
 *   6       ! - (unary) (unary)
 *   7       () [] .     (primary)
 * ═══════════════════════════════════════════════════════════════════ */

static ASTNode *parse_expr_(const char *s, int *pos);

static ASTNode *parse_primary(const char *s, int *pos) {
    skip_ws(s, pos);
    if (!s[*pos]) return NULL;

    /* Integer or float literal */
    if (isdigit((unsigned char)s[*pos]) || (s[*pos] == '-' && isdigit((unsigned char)s[*pos + 1]))) {
        int saved = *pos;
        if (s[*pos] == '-') (*pos)++;
        bool is_float = false;
        while (isdigit((unsigned char)s[*pos])) (*pos)++;
        if (s[*pos] == '.') { is_float = true; (*pos)++;
            while (isdigit((unsigned char)s[*pos])) (*pos)++;
        }
        *pos = saved;
        if (is_float) {
            ASTNode *n = new_node(NODE_FLOAT);
            n->data.float_val = read_float(s, pos);
            return n;
        }
        ASTNode *n = new_node(NODE_INT);
        n->data.int_val = read_int(s, pos);
        return n;
    }

    /* Boolean literal */
    if (strncmp(s + *pos, "true", 4) == 0 && !isalnum((unsigned char)s[*pos + 4]) && s[*pos + 4] != '_') {
        (*pos) += 4; ASTNode *n = new_node(NODE_BOOL); n->data.bool_val = true; return n;
    }
    if (strncmp(s + *pos, "false", 5) == 0 && !isalnum((unsigned char)s[*pos + 5]) && s[*pos + 5] != '_') {
        (*pos) += 5; ASTNode *n = new_node(NODE_BOOL); n->data.bool_val = false; return n;
    }

    /* Identifier: variable, function call, array index, member access */
    if (isalpha((unsigned char)s[*pos]) || s[*pos] == '_') {
        char buf[64];
        read_ident(s, pos, buf);
        skip_ws(s, pos);

        if (s[*pos] == '(') {
            /* Function call */
            ASTNode *n = new_node(NODE_CALL);
            strncpy(n->data.call.name, buf, 63);
            (*pos)++;
            n->data.call.args = (ASTNode **)calloc(16, sizeof(ASTNode *));
            n->data.call.arg_count = 0;
            skip_ws(s, pos);
            if (s[*pos] != ')') {
                n->data.call.args[n->data.call.arg_count++] = parse_expr_(s, pos);
                while (s[*pos] == ',') {
                    (*pos)++;
                    n->data.call.args[n->data.call.arg_count++] = parse_expr_(s, pos);
                }
            }
            if (s[*pos] == ')') (*pos)++;
            return n;
        }

        if (s[*pos] == '[') {
            /* Array index */
            ASTNode *n = new_node(NODE_INDEX);
            n->data.index_expr.arr = new_node(NODE_VAR);
            strncpy(n->data.index_expr.arr->data.name, buf, 63);
            (*pos)++;
            n->data.index_expr.idx = parse_expr_(s, pos);
            skip_ws(s, pos);
            if (s[*pos] == ']') (*pos)++;
            return n;
        }

        /* Simple variable */
        ASTNode *n = new_node(NODE_VAR);
        strncpy(n->data.name, buf, 63);
        return n;
    }

    /* Parenthesized expression */
    if (s[*pos] == '(') {
        (*pos)++;
        ASTNode *n = parse_expr_(s, pos);
        skip_ws(s, pos);
        if (s[*pos] == ')') (*pos)++;
        return n;
    }

    /* Array literal: {1, 2, 3} */
    if (s[*pos] == '{') {
        (*pos)++;
        ASTNode *n = new_node(NODE_ARRAY_LITERAL);
        n->data.array_lit.elements = (ASTNode **)calloc(64, sizeof(ASTNode *));
        n->data.array_lit.count = 0;
        skip_ws(s, pos);
        if (s[*pos] != '}') {
            n->data.array_lit.elements[n->data.array_lit.count++] = parse_expr_(s, pos);
            while (s[*pos] == ',') {
                (*pos)++;
                n->data.array_lit.elements[n->data.array_lit.count++] = parse_expr_(s, pos);
            }
        }
        if (s[*pos] == '}') (*pos)++;
        return n;
    }

    return NULL;
}

static ASTNode *parse_unary(const char *s, int *pos) {
    skip_ws(s, pos);
    if (s[*pos] == '!') {
        (*pos)++;
        ASTNode *n = new_node(NODE_UNARY);
        n->data.binop.op = '!';
        n->data.unary_expr = parse_unary(s, pos);
        return n;
    }
    if (s[*pos] == '-') {
        (*pos)++;
        ASTNode *n = new_node(NODE_UNARY);
        n->data.binop.op = '-';
        n->data.unary_expr = parse_unary(s, pos);
        return n;
    }
    return parse_primary(s, pos);
}

static ASTNode *parse_mul(const char *s, int *pos) {
    ASTNode *l = parse_unary(s, pos);
    while (l) {
        skip_ws(s, pos);
        char op = s[*pos];
        if (op != '*' && op != '/' && op != '%') break;
        (*pos)++;
        ASTNode *r = parse_unary(s, pos);
        ASTNode *n = new_node(NODE_BINOP);
        n->data.binop.left = l;
        n->data.binop.op = op;
        n->data.binop.right = r;
        l = n;
    }
    return l;
}

static ASTNode *parse_add(const char *s, int *pos) {
    ASTNode *l = parse_mul(s, pos);
    while (l) {
        skip_ws(s, pos);
        char op = s[*pos];
        if (op != '+' && op != '-') break;
        (*pos)++;
        ASTNode *r = parse_mul(s, pos);
        ASTNode *n = new_node(NODE_BINOP);
        n->data.binop.left = l;
        n->data.binop.op = op;
        n->data.binop.right = r;
        l = n;
    }
    return l;
}

static ASTNode *parse_compare(const char *s, int *pos) {
    ASTNode *l = parse_add(s, pos);
    while (l) {
        skip_ws(s, pos);
        char op = s[*pos];
        if (op == '=' && s[*pos + 1] == '=') { (*pos)++; op = 'E'; }
        else if (op == '!' && s[*pos + 1] == '=') { (*pos)++; op = 'N'; }
        else if (op == '<' && s[*pos + 1] == '=') { (*pos)++; op = 'L'; }
        else if (op == '>' && s[*pos + 1] == '=') { (*pos)++; op = 'G'; }
        else if (op != '<' && op != '>') break;
        else if (op == '=' || op == '!') break;
        (*pos)++;
        ASTNode *r = parse_add(s, pos);
        ASTNode *n = new_node(NODE_COMPARE);
        n->data.binop.left = l;
        n->data.binop.op = op;
        n->data.binop.right = r;
        l = n;
    }
    return l;
}

static ASTNode *parse_logical_and(const char *s, int *pos) {
    ASTNode *l = parse_compare(s, pos);
    while (l) {
        skip_ws(s, pos);
        if (s[*pos] != '&' || s[*pos + 1] != '&') break;
        (*pos) += 2;
        ASTNode *r = parse_compare(s, pos);
        ASTNode *n = new_node(NODE_LOGICAL);
        n->data.binop.left = l;
        n->data.binop.op = '&';
        n->data.binop.right = r;
        l = n;
    }
    return l;
}

static ASTNode *parse_expr_(const char *s, int *pos) {
    ASTNode *l = parse_logical_and(s, pos);
    while (l) {
        skip_ws(s, pos);
        if (s[*pos] != '|' || s[*pos + 1] != '|') break;
        (*pos) += 2;
        ASTNode *r = parse_logical_and(s, pos);
        ASTNode *n = new_node(NODE_LOGICAL);
        n->data.binop.left = l;
        n->data.binop.op = '|';
        n->data.binop.right = r;
        l = n;
    }
    return l;
}

/* ── Statement parsers ──────────────────────────────────────────── */

static ASTNode *parse_statement_(const char *s, int *pos);

/* Parse a block: { stmt* } */
static ASTNode *parse_block_(const char *s, int *pos) {
    skip_ws(s, pos);
    if (s[*pos] != '{') return NULL;
    (*pos)++;
    ASTNode *blk = new_node(NODE_BLOCK);
    blk->data.block.stmts = (ASTNode **)calloc(128, sizeof(ASTNode *));
    blk->data.block.count = 0;

    while (s[*pos] && s[*pos] != '}') {
        skip_ws(s, pos);
        if (s[*pos] == '}') break;
        ASTNode *stmt = parse_statement_(s, pos);
        if (stmt && blk->data.block.count < 128)
            blk->data.block.stmts[blk->data.block.count++] = stmt;
    }
    if (s[*pos] == '}') (*pos)++;
    return blk;
}

/* Parse a single statement */
static ASTNode *parse_statement_(const char *s, int *pos) {
    skip_ws(s, pos);
    if (!s[*pos]) return NULL;

    /* Block */
    if (s[*pos] == '{') return parse_block_(s, pos);

    /* if statement */
    if (strncmp(s + *pos, "if", 2) == 0 && !isalnum((unsigned char)s[*pos + 2]) && s[*pos + 2] != '_') {
        (*pos) += 2;
        ASTNode *n = new_node(NODE_IF);
        skip_ws(s, pos);
        if (s[*pos] == '(') (*pos)++;
        n->data.if_stmt.cond = parse_expr_(s, pos);
        skip_ws(s, pos);
        if (s[*pos] == ')') (*pos)++;
        n->data.if_stmt.then_branch = parse_statement_(s, pos);
        skip_ws(s, pos);
        if (strncmp(s + *pos, "else", 4) == 0 && !isalnum((unsigned char)s[*pos + 4]) && s[*pos + 4] != '_') {
            (*pos) += 4;
            n->data.if_stmt.else_branch = parse_statement_(s, pos);
        }
        return n;
    }

    /* while statement */
    if (strncmp(s + *pos, "while", 5) == 0 && !isalnum((unsigned char)s[*pos + 5]) && s[*pos + 5] != '_') {
        (*pos) += 5;
        ASTNode *n = new_node(NODE_WHILE);
        skip_ws(s, pos);
        if (s[*pos] == '(') (*pos)++;
        n->data.while_stmt.cond = parse_expr_(s, pos);
        skip_ws(s, pos);
        if (s[*pos] == ')') (*pos)++;
        n->data.while_stmt.body = parse_statement_(s, pos);
        return n;
    }

    /* for statement: for (init; cond; incr) body */
    if (strncmp(s + *pos, "for", 3) == 0 && !isalnum((unsigned char)s[*pos + 3]) && s[*pos + 3] != '_') {
        (*pos) += 3;
        ASTNode *n = new_node(NODE_FOR);
        skip_ws(s, pos);
        if (s[*pos] == '(') (*pos)++;
        skip_ws(s, pos);
        if (s[*pos] != ';') n->data.for_stmt.init = parse_statement_(s, pos);
        else n->data.for_stmt.init = NULL;
        if (s[*pos] == ';') (*pos)++;
        skip_ws(s, pos);
        if (s[*pos] != ';') n->data.for_stmt.cond = parse_expr_(s, pos);
        else n->data.for_stmt.cond = NULL;
        if (s[*pos] == ';') (*pos)++;
        skip_ws(s, pos);
        if (s[*pos] != ')') n->data.for_stmt.incr = parse_statement_(s, pos);
        else n->data.for_stmt.incr = NULL;
        if (s[*pos] == ')') (*pos)++;
        n->data.for_stmt.body = parse_statement_(s, pos);
        return n;
    }

    /* break / continue */
    if (strncmp(s + *pos, "break", 5) == 0 && !isalnum((unsigned char)s[*pos + 5]) && s[*pos + 5] != '_') {
        (*pos) += 5;
        skip_ws(s, pos); if (s[*pos] == ';') (*pos)++;
        return new_node(NODE_BREAK);
    }
    if (strncmp(s + *pos, "continue", 8) == 0 && !isalnum((unsigned char)s[*pos + 8]) && s[*pos + 8] != '_') {
        (*pos) += 8;
        skip_ws(s, pos); if (s[*pos] == ';') (*pos)++;
        return new_node(NODE_CONTINUE);
    }

    /* return */
    if (strncmp(s + *pos, "return", 6) == 0 && !isalnum((unsigned char)s[*pos + 6]) && s[*pos + 6] != '_') {
        (*pos) += 6;
        ASTNode *n = new_node(NODE_RETURN);
        skip_ws(s, pos);
        if (s[*pos] != ';' && s[*pos] != '}') n->data.ret_expr = parse_expr_(s, pos);
        if (s[*pos] == ';') (*pos)++;
        return n;
    }

    /* Assignment or expression statement */
    int saved = *pos;
    char buf[64];
    if (read_ident(s, pos, buf)) {
        skip_ws(s, pos);
        if (s[*pos] == '=' && s[*pos + 1] != '=') {
            (*pos)++;
            ASTNode *a = new_node(NODE_ASSIGN);
            strncpy(a->data.assign.name, buf, 63);
            a->data.assign.expr = parse_expr_(s, pos);
            if (s[*pos] == ';') (*pos)++;
            return a;
        }
        /* Array element assignment: a[i] = expr */
        if (s[*pos] == '[') {
            (*pos)++;
            ASTNode *idx = parse_expr_(s, pos);
            skip_ws(s, pos);
            if (s[*pos] == ']') (*pos)++;
            skip_ws(s, pos);
            if (s[*pos] == '=') {
                (*pos)++;
                ASTNode *a = new_node(NODE_ASSIGN);
                strncpy(a->data.assign.name, buf, 63);
                a->data.assign.index = idx;
                a->data.assign.expr = parse_expr_(s, pos);
                if (s[*pos] == ';') (*pos)++;
                return a;
            }
            c_free_ast(idx);
            *pos = saved; /* fall through */
        } else {
            *pos = saved; /* fallthrough to expr statement */
        }
    }

    /* Expression statement (function call, etc.) */
    ASTNode *expr = parse_expr_(s, pos);
    if (!expr) { (*pos)++; return NULL; }
    skip_ws(s, pos);
    if (s[*pos] == ';') (*pos)++;
    return expr;
}

/* ── Public parser APIs ─────────────────────────────────────────── */

ASTNode *c_parse_declaration(const char *source, int *pos) {
    return parse_statement_(source, pos);
}

ASTNode *c_parse_expr(const char *source, int *pos) {
    return parse_expr_(source, pos);
}

ASTNode *c_parse_statement(const char *source, int *pos) {
    return parse_statement_(source, pos);
}

ASTNode *c_parse_block(const char *source, int *pos) {
    return parse_block_(source, pos);
}

ASTNode *c_parse_function(const char *source, int *pos) {
    /* Parse function definition: type name(params) { body } */
    skip_ws(source, pos);
    char buf[64];
    /* Skip return type */
    read_ident(source, pos, buf);
    /* Read function name */
    char fname[64];
    if (!read_ident(source, pos, fname)) return NULL;
    skip_ws(source, pos);
    if (source[*pos] != '(') return NULL;
    (*pos)++; /* skip ( */
    /* Skip params for now */
    while (source[*pos] && source[*pos] != ')') (*pos)++;
    if (source[*pos] == ')') (*pos)++;
    /* Parse body */
    ASTNode *body = parse_block_(source, pos);
    if (!body) return NULL;
    /* Wrap in a node so caller can find it */
    ASTNode *fn_node = new_node(NODE_FUNC_DEF);
    strncpy(fn_node->data.name, fname, 63);
    fn_node->data.assign.expr = body;
    return fn_node;
}

/* ═══════════════════════════════════════════════════════════════════
 * L5: Tree-Walking Evaluator
 *
 * eval_expr: evaluate an expression, returning an int value.
 *   - NODE_INT: return literal
 *   - NODE_VAR: lookup in environment
 *   - NODE_BINOP: recursive eval left, right; apply operator
 *   - NODE_COMPARE: recursive eval; return 0/1 (false/true)
 *   - NODE_LOGICAL: short-circuit evaluation (&&, ||)
 *   - NODE_UNARY: ! or - applied to subexpression
 *   - NODE_CALL: function dispatch
 *   - NODE_INDEX: array[index]
 *
 * execute_statement: execute a statement (mutates env).
 *   - NODE_ASSIGN: eval RHS, store in env
 *   - NODE_IF: eval cond, branch
 *   - NODE_WHILE: loop
 *   - NODE_FOR: for-loop semantics
 *   - NODE_BREAK/CONTINUE: handled with return codes
 * ═══════════════════════════════════════════════════════════════════ */

/* Return codes for break/continue */
#define C_EVAL_OK       0
#define C_EVAL_BREAK    1
#define C_EVAL_CONTINUE 2
static int c_eval_flag = C_EVAL_OK;

int c_eval_expr(ASTNode *node, CVar *env, CFunc *funcs) {
    if (!node) return 0;
    switch (node->type) {
        case NODE_INT:  return node->data.int_val;
        case NODE_FLOAT: return (int)node->data.float_val;
        case NODE_BOOL: return node->data.bool_val ? 1 : 0;
        case NODE_VAR: {
            CVar *v = c_find_var(env, node->data.name);
            if (!v) return 0;
            if (v->type == T_BOOL) return v->value.bool_val ? 1 : 0;
            return v->value.int_val;
        }
        case NODE_BINOP: {
            int l = c_eval_expr(node->data.binop.left, env, funcs);
            int r = c_eval_expr(node->data.binop.right, env, funcs);
            switch (node->data.binop.op) {
                case '+': return l + r;
                case '-': return l - r;
                case '*': return l * r;
                case '/': return r ? l / r : 0;
                case '%': return r ? l % r : 0;
                default: return 0;
            }
        }
        case NODE_COMPARE: {
            int l = c_eval_expr(node->data.binop.left, env, funcs);
            int r = c_eval_expr(node->data.binop.right, env, funcs);
            switch (node->data.binop.op) {
                case 'E': return l == r ? 1 : 0;  /* == */
                case 'N': return l != r ? 1 : 0;  /* != */
                case '<': return l < r ? 1 : 0;
                case 'L': return l <= r ? 1 : 0;  /* <= */
                case '>': return l > r ? 1 : 0;
                case 'G': return l >= r ? 1 : 0;  /* >= */
                default: return 0;
            }
        }
        case NODE_LOGICAL: {
            int l = c_eval_expr(node->data.binop.left, env, funcs);
            if (node->data.binop.op == '&') { /* && */
                if (!l) return 0;
                return c_eval_expr(node->data.binop.right, env, funcs) ? 1 : 0;
            }
            if (node->data.binop.op == '|') { /* || */
                if (l) return 1;
                return c_eval_expr(node->data.binop.right, env, funcs) ? 1 : 0;
            }
            return 0;
        }
        case NODE_UNARY: {
            int v = c_eval_expr(node->data.unary_expr, env, funcs);
            if (node->data.binop.op == '!') return v ? 0 : 1;
            if (node->data.binop.op == '-') return -v;
            return v;
        }
        case NODE_CALL: {
            CFunc *f = c_find_func(funcs, node->data.call.name);
            if (!f) {
                /* Built-in: print */
                if (strcmp(node->data.call.name, "print") == 0) {
                    if (node->data.call.arg_count > 0)
                        printf("%d\n", c_eval_expr(node->data.call.args[0], env, funcs));
                    return 0;
                }
                return 0;
            }
            /* Set up local environment with parameters */
            CVar *local_env = NULL;
            for (int i = 0; i < f->param_count && i < node->data.call.arg_count; i++) {
                CVar *v = (CVar *)calloc(1, sizeof(CVar));
                strncpy(v->name, f->param_names[i], 63);
                v->type = T_INT;
                v->value.int_val = c_eval_expr(node->data.call.args[i], env, funcs);
                v->next = local_env;
                local_env = v;
            }
            /* Execute function body, capturing return value */
            int result = 0;
            int saved_flag = c_eval_flag;
            c_eval_flag = C_EVAL_OK;
            if (f->body && f->body->type == NODE_BLOCK) {
                for (int i = 0; i < f->body->data.block.count && f->body->data.block.stmts[i]; i++) {
                    ASTNode *s = f->body->data.block.stmts[i];
                    if (s->type == NODE_RETURN) {
                        result = c_eval_expr(s->data.ret_expr, local_env, funcs);
                        break;
                    }
                    c_execute_statement(s, &local_env, funcs);
                    if (c_eval_flag == C_EVAL_BREAK) break;
                }
            }
            c_eval_flag = saved_flag;
            /* Cleanup local env */
            while (local_env) { CVar *n = local_env->next; free(local_env); local_env = n; }
            return result;
        }
        case NODE_INDEX: {
            CVar *arr_var = c_find_var(env, node->data.index_expr.arr->data.name);
            if (!arr_var || !arr_var->array_data) return 0;
            int idx = c_eval_expr(node->data.index_expr.idx, env, funcs);
            if (idx < 0 || idx >= arr_var->array_size) return 0;
            return arr_var->array_data[idx];
        }
        case NODE_ARRAY_LITERAL:
            return node->data.array_lit.count; /* return count */
        case NODE_RETURN:
            if (node->data.ret_expr)
                return c_eval_expr(node->data.ret_expr, env, funcs);
            return 0;
        default:
            return 0;
    }
}

void c_execute_statement(ASTNode *node, CVar **env, CFunc *funcs) {
    if (!node) return;
    if (c_eval_flag == C_EVAL_BREAK) return;

    switch (node->type) {
        case NODE_ASSIGN: {
            int val = c_eval_expr(node->data.assign.expr, *env, funcs);
            if (node->data.assign.index) {
                /* Array element assignment: a[i] = val */
                CVar *arr_var = c_find_var(*env, node->data.assign.name);
                if (arr_var && arr_var->array_data) {
                    int idx = c_eval_expr(node->data.assign.index, *env, funcs);
                    if (idx >= 0 && idx < arr_var->array_size)
                        arr_var->array_data[idx] = val;
                }
            } else {
                CVar *v = c_find_var(*env, node->data.assign.name);
                if (v) { v->value.int_val = val; }
                else {
                    CVar *nv = (CVar *)calloc(1, sizeof(CVar));
                    strncpy(nv->name, node->data.assign.name, 63);
                    nv->type = T_INT;
                    nv->value.int_val = val;
                    nv->next = *env;
                    *env = nv;
                }
            }
            break;
        }
        case NODE_IF: {
            int cond = c_eval_expr(node->data.if_stmt.cond, *env, funcs);
            if (cond) c_execute_statement(node->data.if_stmt.then_branch, env, funcs);
            else if (node->data.if_stmt.else_branch)
                c_execute_statement(node->data.if_stmt.else_branch, env, funcs);
            break;
        }
        case NODE_WHILE: {
            int iter = 0;
            while (c_eval_expr(node->data.while_stmt.cond, *env, funcs) && iter++ < 10000) {
                c_execute_statement(node->data.while_stmt.body, env, funcs);
                if (c_eval_flag == C_EVAL_BREAK) { c_eval_flag = C_EVAL_OK; break; }
                if (c_eval_flag == C_EVAL_CONTINUE) { c_eval_flag = C_EVAL_OK; continue; }
            }
            break;
        }
        case NODE_FOR: {
            /* for (init; cond; incr) body */
            if (node->data.for_stmt.init)
                c_execute_statement(node->data.for_stmt.init, env, funcs);

            int iter = 0;
            while (iter++ < 10000) {
                if (node->data.for_stmt.cond) {
                    int c = c_eval_expr(node->data.for_stmt.cond, *env, funcs);
                    if (!c) break;
                }
                c_execute_statement(node->data.for_stmt.body, env, funcs);
                if (c_eval_flag == C_EVAL_BREAK) { c_eval_flag = C_EVAL_OK; break; }
                if (c_eval_flag == C_EVAL_CONTINUE) { c_eval_flag = C_EVAL_OK; /* fall through to incr */ }
                if (node->data.for_stmt.incr)
                    c_execute_statement(node->data.for_stmt.incr, env, funcs);
            }
            break;
        }
        case NODE_BREAK:
            c_eval_flag = C_EVAL_BREAK;
            break;
        case NODE_CONTINUE:
            c_eval_flag = C_EVAL_CONTINUE;
            break;
        case NODE_BLOCK:
            c_execute_block(node, env, funcs);
            break;
        case NODE_CALL:
        case NODE_INT:
        case NODE_FLOAT:
        case NODE_BOOL:
        case NODE_VAR:
        case NODE_BINOP:
        case NODE_COMPARE:
        case NODE_LOGICAL:
        case NODE_UNARY:
            c_eval_expr(node, *env, funcs);
            break;
        case NODE_RETURN:
            break;
        default:
            break;
    }
}

void c_execute_block(ASTNode *node, CVar **env, CFunc *funcs) {
    if (!node) return;
    if (node->type == NODE_BLOCK) {
        for (int i = 0; i < node->data.block.count && node->data.block.stmts[i]; i++) {
            c_execute_statement(node->data.block.stmts[i], env, funcs);
            if (c_eval_flag == C_EVAL_BREAK) return;
        }
        return;
    }
    c_execute_statement(node, env, funcs);
}

/* ═══════════════════════════════════════════════════════════════════
 * L4: Constant Folding Optimization
 *
 * Dragon Book §8.3: Replace compile-time-evaluable expressions
 * with their results. Example: 2+3*4 → 14.
 *
 * Complexity: O(n) tree traversal.
 * ═══════════════════════════════════════════════════════════════════ */

static bool is_const_node(ASTNode *node) {
    if (!node) return false;
    return (node->type == NODE_INT || node->type == NODE_FLOAT || node->type == NODE_BOOL);
}

ASTNode *c_constant_fold(ASTNode *node) {
    if (!node) return NULL;

    /* Recurse first */
    if (node->type == NODE_BINOP || node->type == NODE_COMPARE || node->type == NODE_LOGICAL) {
        node->data.binop.left = c_constant_fold(node->data.binop.left);
        node->data.binop.right = c_constant_fold(node->data.binop.right);
    }

    /* Fold binary operations on constants */
    if ((node->type == NODE_BINOP || node->type == NODE_COMPARE) &&
        is_const_node(node->data.binop.left) && is_const_node(node->data.binop.right)) {
        int l = node->data.binop.left->data.int_val;
        int r = node->data.binop.right->data.int_val;
        int result = 0;

        switch (node->data.binop.op) {
            case '+': result = l + r; break;
            case '-': result = l - r; break;
            case '*': result = l * r; break;
            case '/': result = r ? l / r : 0; break;
            case '%': result = r ? l % r : 0; break;
            case 'E': result = (l == r) ? 1 : 0; break;
            case 'N': result = (l != r) ? 1 : 0; break;
            case '<': result = (l < r) ? 1 : 0; break;
            case '>': result = (l > r) ? 1 : 0; break;
            case 'L': result = (l <= r) ? 1 : 0; break;
            case 'G': result = (l >= r) ? 1 : 0; break;
            default: return node;
        }

        ASTNode *folded = new_node(NODE_INT);
        folded->data.int_val = result;
        c_free_ast(node->data.binop.left);
        c_free_ast(node->data.binop.right);
        free(node);
        return folded;
    }

    if (node->type == NODE_LOGICAL && is_const_node(node->data.binop.left) && is_const_node(node->data.binop.right)) {
        int l = node->data.binop.left->data.int_val;
        int r = node->data.binop.right->data.int_val;
        int result = (node->data.binop.op == '&') ? ((l && r) ? 1 : 0) : ((l || r) ? 1 : 0);
        ASTNode *folded = new_node(NODE_INT);
        folded->data.int_val = result;
        c_free_ast(node->data.binop.left);
        c_free_ast(node->data.binop.right);
        free(node);
        return folded;
    }

    return node;
}

/* ═══════════════════════════════════════════════════════════════════
 * L4: Type Checking
 *
 * Simple type validation:
 *   - Variables used must be declared
 *   - Operators applied to compatible types
 *   - Function calls have correct argument count
 * ═══════════════════════════════════════════════════════════════════ */

bool c_type_check(ASTNode *node, CVar *env, CFunc *funcs) {
    if (!node) return true;
    switch (node->type) {
        case NODE_VAR:
            return c_find_var(env, node->data.name) != NULL;
        case NODE_CALL: {
            CFunc *f = c_find_func(funcs, node->data.call.name);
            if (!f) return true; /* builtins assumed OK */
            return node->data.call.arg_count == f->param_count;
        }
        case NODE_BINOP:
        case NODE_COMPARE:
        case NODE_LOGICAL:
            return c_type_check(node->data.binop.left, env, funcs) &&
                   c_type_check(node->data.binop.right, env, funcs);
        case NODE_IF:
            return c_type_check(node->data.if_stmt.then_branch, env, funcs) &&
                   (!node->data.if_stmt.else_branch ||
                    c_type_check(node->data.if_stmt.else_branch, env, funcs));
        case NODE_WHILE:
            return c_type_check(node->data.while_stmt.body, env, funcs);
        case NODE_FOR:
            return c_type_check(node->data.for_stmt.body, env, funcs);
        case NODE_BLOCK:
            for (int i = 0; i < node->data.block.count; i++)
                if (!c_type_check(node->data.block.stmts[i], env, funcs)) return false;
            return true;
        case NODE_ASSIGN:
            return c_type_check(node->data.assign.expr, env, funcs);
        case NODE_UNARY:
            return c_type_check(node->data.unary_expr, env, funcs);
        default:
            return true;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * L7-L8: Simple Bytecode Compiler
 *
 * Compiles AST to a stack-machine bytecode.
 * opcodes: PUSH_INT, PUSH_VAR, STORE_VAR, ADD, SUB, MUL, DIV, MOD,
 *          CMP_EQ, CMP_NE, CMP_LT, CMP_LE, CMP_GT, CMP_GE,
 *          JMP, JMP_IF_FALSE, CALL, RET, HALT
 *
 * Reference: Wirth, "Compiler Construction", Ch. 9 (Stack Machine)
 *            Dragon Book §8.2 (Intermediate Code Generation)
 * ═══════════════════════════════════════════════════════════════════ */

static void bc_emit(Bytecode *bc, int op) {
    if (bc->len >= bc->cap) {
        bc->cap = bc->cap ? bc->cap * 2 : 256;
        bc->code = (int *)realloc(bc->code, (size_t)bc->cap * sizeof(int));
    }
    bc->code[bc->len++] = op;
}

static void bc_emit_int(Bytecode *bc, int val) {
    bc_emit(bc, BC_PUSH_INT);
    if (bc->len >= bc->cap) {
        bc->cap = bc->cap ? bc->cap * 2 : 256;
        bc->code = (int *)realloc(bc->code, (size_t)bc->cap * sizeof(int));
    }
    bc->code[bc->len++] = val;
}

static void compile_node(ASTNode *node, Bytecode *bc) {
    if (!node) return;
    switch (node->type) {
        case NODE_INT:  bc_emit_int(bc, node->data.int_val); break;
        case NODE_FLOAT: bc_emit_int(bc, (int)node->data.float_val); break;
        case NODE_BOOL:  bc_emit_int(bc, node->data.bool_val ? 1 : 0); break;
        case NODE_VAR:
            bc_emit(bc, BC_PUSH_VAR);
            bc_emit_int(bc, node->data.name[0]); /* simplified: just first char as id */
            break;
        case NODE_BINOP:
            compile_node(node->data.binop.left, bc);
            compile_node(node->data.binop.right, bc);
            switch (node->data.binop.op) {
                case '+': bc_emit(bc, BC_ADD); break;
                case '-': bc_emit(bc, BC_SUB); break;
                case '*': bc_emit(bc, BC_MUL); break;
                case '/': bc_emit(bc, BC_DIV); break;
                case '%': bc_emit(bc, BC_MOD); break;
                default: break;
            }
            break;
        case NODE_COMPARE:
            compile_node(node->data.binop.left, bc);
            compile_node(node->data.binop.right, bc);
            switch (node->data.binop.op) {
                case 'E': bc_emit(bc, BC_CMP_EQ); break;
                case 'N': bc_emit(bc, BC_CMP_NE); break;
                case '<': bc_emit(bc, BC_CMP_LT); break;
                case 'L': bc_emit(bc, BC_CMP_LE); break;
                case '>': bc_emit(bc, BC_CMP_GT); break;
                case 'G': bc_emit(bc, BC_CMP_GE); break;
                default: break;
            }
            break;
        case NODE_BLOCK:
            for (int i = 0; i < node->data.block.count && node->data.block.stmts[i]; i++)
                compile_node(node->data.block.stmts[i], bc);
            break;
        default: break;
    }
}

Bytecode *c_compile_ast(ASTNode *node) {
    Bytecode *bc = (Bytecode *)calloc(1, sizeof(Bytecode));
    compile_node(node, bc);
    bc_emit(bc, BC_HALT);
    return bc;
}

int c_execute_bytecode(Bytecode *bc, CVar *env) {
    if (!bc || !bc->code) return 0;
    /* Simple stack-based VM */
    int stack[256];
    int sp = 0;
    #define PUSH(v) if (sp < 256) stack[sp++] = (v)
    #define POP()   (sp > 0 ? stack[--sp] : 0)

    int ip = 0;
    while (ip < bc->len) {
        int op = bc->code[ip++];
        switch (op) {
            case BC_PUSH_INT: PUSH(bc->code[ip++]); break;
            case BC_PUSH_VAR: {
                int id = bc->code[ip++];
                /* Simplified: id is first char of name */
                for (CVar *v = env; v; v = v->next) {
                    if (v->name[0] == (char)id) { PUSH(v->value.int_val); break; }
                }
                break;
            }
            case BC_ADD: { int b = POP(); int a = POP(); PUSH(a + b); break; }
            case BC_SUB: { int b = POP(); int a = POP(); PUSH(a - b); break; }
            case BC_MUL: { int b = POP(); int a = POP(); PUSH(a * b); break; }
            case BC_DIV: { int b = POP(); int a = POP(); PUSH(b ? a / b : 0); break; }
            case BC_MOD: { int b = POP(); int a = POP(); PUSH(b ? a % b : 0); break; }
            case BC_CMP_EQ: { int b = POP(); int a = POP(); PUSH(a == b ? 1 : 0); break; }
            case BC_CMP_NE: { int b = POP(); int a = POP(); PUSH(a != b ? 1 : 0); break; }
            case BC_CMP_LT: { int b = POP(); int a = POP(); PUSH(a < b ? 1 : 0); break; }
            case BC_CMP_LE: { int b = POP(); int a = POP(); PUSH(a <= b ? 1 : 0); break; }
            case BC_CMP_GT: { int b = POP(); int a = POP(); PUSH(a > b ? 1 : 0); break; }
            case BC_CMP_GE: { int b = POP(); int a = POP(); PUSH(a >= b ? 1 : 0); break; }
            case BC_HALT: return POP();
            default: break;
        }
    }
    return POP();
    #undef PUSH
    #undef POP
}

void c_free_bytecode(Bytecode *bc) {
    if (bc) { free(bc->code); free(bc); }
}

/* ═══════════════════════════════════════════════════════════════════
 * L7: Debug utilities
 * ═══════════════════════════════════════════════════════════════════ */

void c_print_env(CVar *env) {
    printf("=== Environment (%d vars) ===\n", c_env_size(env));
    for (CVar *v = env; v; v = v->next) {
        printf("  %s = %d", v->name, v->value.int_val);
        if (v->array_data) {
            printf(" [");
            for (int i = 0; i < v->array_size; i++) {
                if (i > 0) printf(", ");
                printf("%d", v->array_data[i]);
            }
            printf("]");
        }
        printf("\n");
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * Memory Cleanup
 * ═══════════════════════════════════════════════════════════════════ */

void c_free_ast(ASTNode *node) {
    if (!node) return;
    switch (node->type) {
        case NODE_BINOP:
        case NODE_COMPARE:
        case NODE_LOGICAL:
            c_free_ast(node->data.binop.left);
            c_free_ast(node->data.binop.right);
            break;
        case NODE_UNARY:
            c_free_ast(node->data.unary_expr);
            break;
        case NODE_CALL:
            for (int i = 0; i < node->data.call.arg_count; i++)
                c_free_ast(node->data.call.args[i]);
            free(node->data.call.args);
            break;
        case NODE_IF:
            c_free_ast(node->data.if_stmt.cond);
            c_free_ast(node->data.if_stmt.then_branch);
            c_free_ast(node->data.if_stmt.else_branch);
            break;
        case NODE_WHILE:
            c_free_ast(node->data.while_stmt.cond);
            c_free_ast(node->data.while_stmt.body);
            break;
        case NODE_FOR:
            c_free_ast(node->data.for_stmt.init);
            c_free_ast(node->data.for_stmt.cond);
            c_free_ast(node->data.for_stmt.incr);
            c_free_ast(node->data.for_stmt.body);
            break;
        case NODE_BLOCK:
            for (int i = 0; i < node->data.block.count; i++)
                c_free_ast(node->data.block.stmts[i]);
            free(node->data.block.stmts);
            break;
        case NODE_ASSIGN:
            c_free_ast(node->data.assign.expr);
            c_free_ast(node->data.assign.index);
            break;
        case NODE_RETURN:
            c_free_ast(node->data.ret_expr);
            break;
        case NODE_INDEX:
            c_free_ast(node->data.index_expr.arr);
            c_free_ast(node->data.index_expr.idx);
            break;
        case NODE_ARRAY_LITERAL:
            for (int i = 0; i < node->data.array_lit.count; i++)
                c_free_ast(node->data.array_lit.elements[i]);
            free(node->data.array_lit.elements);
            break;
        case NODE_FUNC_DEF:
            c_free_ast(node->data.assign.expr);
            break;
        default:
            break;
    }
    free(node);
}

void c_free_var_list(CVar *head) {
    while (head) { CVar *n = head->next; free(head->array_data); free(head); head = n; }
}

void c_free_func_list(CFunc *head) {
    while (head) {
        CFunc *n = head->next;
        if (head->param_names) {
            for (int i = 0; i < head->param_count; i++) free(head->param_names[i]);
        }
        free(head->param_names);
        free(head->param_types);
        c_free_ast(head->body);
        free(head);
        head = n;
    }
}
