/* script_lang.c — Dynamic Scripting Language Runtime
 * ============================================================================
 * L1-L9 knowledge coverage:
 *   L1: ScriptValue tagged union, ScriptVM struct, Table hash map
 *   L2: Dynamic typing, hash tables (djb2), mark-sweep GC
 *   L3: Call stack frames, global variable table, garbage collector
 *   L4: Lua's table design (Ierusalimschy et al., TOPLAS 1998),
 *       Mark-Sweep GC (McCarthy, 1960)
 *   L5: Recursive descent parser, AST evaluator, arithmetic operators,
 *       string operations
 *   L6: Script language with variables, tables, functions, control flow
 *   L7: String library, table manipulation, print/IO
 *   L8: Mark-sweep garbage collection, native function binding
 *   L9: Coroutine basics (yield/resume)
 *
 * Reference: Ierusalimschy, Figueiredo, Celes. "Lua — an extensible
 *            extension language" Software: Practice & Experience, 1996
 *            McCarthy, J. "LISP 1.5 Programmer's Manual" 1962 (GC)
 *            Bernstein, D. "djb2 hash function"
 */

#include "script_lang.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/* ═══════════════════════════════════════════════════════════════════
 * L1+L4: djb2 Hash Function (Daniel J. Bernstein)
 * h₀ = 5381, hᵢ₊₁ = ((hᵢ << 5) + hᵢ) + c
 * Empirical: excellent distribution for string keys.
 * ═══════════════════════════════════════════════════════════════════ */

unsigned int script_hash(const char *str) {
    unsigned int h = 5381;
    while (*str) h = ((h << 5) + h) + (unsigned char)(*str++);
    return h;
}

/* ═══════════════════════════════════════════════════════════════════
 * L2: Hash Table (chained) — Operations
 * ═══════════════════════════════════════════════════════════════════ */

Table *script_table_new(void) {
    Table *t = (Table *)calloc(1, sizeof(Table));
    t->size = 64;
    t->count = 0;
    t->buckets = (TableEntry **)calloc((size_t)t->size, sizeof(TableEntry *));
    return t;
}

void script_table_set(Table *t, const char *key, ScriptValue *val) {
    if (!t || !key) return;
    unsigned int h = script_hash(key) % (unsigned int)t->size;
    for (TableEntry *e = t->buckets[h]; e; e = e->next) {
        if (strcmp(e->key, key) == 0) { e->value = val; return; }
    }
    TableEntry *e = (TableEntry *)calloc(1, sizeof(TableEntry));
    strncpy(e->key, key, 127); e->key[127] = '\0';
    e->value = val;
    e->next = t->buckets[h];
    t->buckets[h] = e;
    t->count++;
}

ScriptValue *script_table_get(Table *t, const char *key) {
    if (!t || !key) return NULL;
    unsigned int h = script_hash(key) % (unsigned int)t->size;
    for (TableEntry *e = t->buckets[h]; e; e = e->next)
        if (strcmp(e->key, key) == 0) return e->value;
    return NULL;
}

void script_table_remove(Table *t, const char *key) {
    if (!t || !key) return;
    unsigned int h = script_hash(key) % (unsigned int)t->size;
    TableEntry *prev = NULL;
    for (TableEntry *e = t->buckets[h]; e; prev = e, e = e->next) {
        if (strcmp(e->key, key) == 0) {
            if (prev) prev->next = e->next;
            else t->buckets[h] = e->next;
            free(e); t->count--; return;
        }
    }
}

void script_table_free(Table *t) {
    if (!t) return;
    for (int i = 0; i < t->size; i++) {
        TableEntry *e = t->buckets[i];
        while (e) { TableEntry *n = e->next; free(e); e = n; }
    }
    free(t->buckets);
    free(t);
}

/* ═══════════════════════════════════════════════════════════════════
 * L1: VM Lifecycle
 * ═══════════════════════════════════════════════════════════════════ */

void script_init_vm(ScriptVM *vm) {
    memset(vm, 0, sizeof(ScriptVM));
    vm->globals = script_table_new();
    vm->gc_head = NULL;
    vm->gc_threshold = 1024;
    vm->gc_alloc_count = 0;
    vm->call_stack.stack = NULL;
    vm->call_stack.stack_size = 0;
    vm->call_stack.stack_top = 0;
}

/* ═══════════════════════════════════════════════════════════════════
 * L8: Mark-Sweep Garbage Collector (McCarthy, 1960)
 *
 * Algorithm:
 *   Mark phase:  Recursively mark all reachable objects from roots
 *                (global table + call stack).
 *   Sweep phase: Free all unmarked objects; reset mark bits.
 *
 * Roots: globals table values, call stack values.
 * ═══════════════════════════════════════════════════════════════════ */

void script_gc_mark(ScriptValue *v) {
    if (!v || v->gc_marked) return;
    v->gc_marked = true;

    /* Mark table keys/values */
    if (v->type == SV_TABLE && v->table) {
        for (int i = 0; i < v->table->size; i++) {
            for (TableEntry *e = v->table->buckets[i]; e; e = e->next)
                script_gc_mark(e->value);
        }
    }
}

static void gc_mark_roots(ScriptVM *vm) {
    if (!vm || !vm->globals) return;
    /* Mark all globals */
    for (int i = 0; i < vm->globals->size; i++) {
        for (TableEntry *e = vm->globals->buckets[i]; e; e = e->next)
            script_gc_mark(e->value);
    }
    /* Mark call stack */
    for (int i = 0; i < vm->call_stack.stack_top; i++)
        script_gc_mark(vm->call_stack.stack[i]);
}

void script_gc_collect(ScriptVM *vm) {
    if (!vm) return;

    /* Mark */
    ScriptValue *v = vm->gc_head;
    while (v) { v->gc_marked = false; v = (ScriptValue *)v->next; }
    gc_mark_roots(vm);

    /* Sweep */
    ScriptValue *prev = NULL;
    v = vm->gc_head;
    while (v) {
        if (!v->gc_marked) {
            ScriptValue *to_free = v;
            if (prev) prev->next = v->next;
            else vm->gc_head = (ScriptValue *)v->next;
            v = (ScriptValue *)v->next;
            if (to_free->type == SV_STRING) { /* string data inline, nothing extra */ }
            free(to_free);
        } else {
            prev = v;
            v = (ScriptValue *)v->next;
        }
    }
    vm->gc_alloc_count = 0;
}

/* ═══════════════════════════════════════════════════════════════════
 * L2: Value Allocation (with GC tracking)
 * ═══════════════════════════════════════════════════════════════════ */

ScriptValue *script_new_value(ScriptVM *vm) {
    ScriptValue *v = (ScriptValue *)calloc(1, sizeof(ScriptValue));
    if (!v) return NULL;
    v->type = SV_NIL;

    if (vm) {
        v->next = (struct ScriptValue *)vm->gc_head;
        vm->gc_head = v;
        vm->gc_alloc_count++;
        if (vm->gc_alloc_count > vm->gc_threshold)
            script_gc_collect(vm);
    }
    return v;
}

ScriptValue *script_get_global(ScriptVM *vm, const char *name) {
    return script_table_get(vm->globals, name);
}

void script_set_global(ScriptVM *vm, const char *name, ScriptValue *val) {
    script_table_set(vm->globals, name, val);
}

/* ═══════════════════════════════════════════════════════════════════
 * L5: Lexer helpers
 * ═══════════════════════════════════════════════════════════════════ */

static void skip_ws(const char *s, int *pos) {
    while (s[*pos] && isspace((unsigned char)s[*pos])) (*pos)++;
}

static void skip_line_comment(const char *s, int *pos) {
    if (s[*pos] == '/' && s[*pos + 1] == '/') {
        while (s[*pos] && s[*pos] != '\n') (*pos)++;
    }
}

static char *read_ident(const char *s, int *pos, char *buf) {
    int i = 0; skip_ws(s, pos); skip_line_comment(s, pos); skip_ws(s, pos);
    if (!isalpha((unsigned char)s[*pos]) && s[*pos] != '_') return NULL;
    while (isalnum((unsigned char)s[*pos]) || s[*pos] == '_') {
        if (i < 127) { buf[i++] = s[*pos]; } (*pos)++;
    }
    buf[i] = '\0';
    return (i > 0) ? buf : NULL;
}

static char *read_string(const char *s, int *pos, char *buf) {
    skip_ws(s, pos);
    if (s[*pos] != '"') return NULL;
    (*pos)++; int i = 0;
    while (s[*pos] && s[*pos] != '"' && i < 255) {
        if (s[*pos] == '\\') {
            (*pos)++;
            switch (s[*pos]) {
                case 'n': buf[i++] = '\n'; break;
                case 't': buf[i++] = '\t'; break;
                case '\\': buf[i++] = '\\'; break;
                case '"': buf[i++] = '"'; break;
                default: buf[i++] = s[*pos]; break;
            }
            (*pos)++;
        } else {
            buf[i++] = s[(*pos)++];
        }
    }
    if (s[*pos] == '"') (*pos)++;
    buf[i] = '\0'; return buf;
}

static double read_number(const char *s, int *pos) {
    skip_ws(s, pos);
    int start = *pos;
    if (s[*pos] == '-') (*pos)++;
    while (isdigit((unsigned char)s[*pos]) || s[*pos] == '.') (*pos)++;
    char buf[64]; int len = *pos - start;
    if (len >= 64) len = 63;
    if (len <= 0) return 0.0;
    memcpy(buf, s + start, (size_t)len); buf[len] = '\0';
    return atof(buf);
}

/* ═══════════════════════════════════════════════════════════════════
 * L5: AST allocation
 * ═══════════════════════════════════════════════════════════════════ */

static ScriptAST *new_ast(ScriptASTType t) {
    ScriptAST *a = (ScriptAST *)calloc(1, sizeof(ScriptAST));
    if (a) a->type = t;
    return a;
}

void script_free_ast(ScriptAST *ast) {
    if (!ast) return;
    script_free_ast(ast->left);
    script_free_ast(ast->right);
    script_free_ast(ast->cond);
    script_free_ast(ast->then_branch);
    script_free_ast(ast->else_branch);
    script_free_ast(ast->body);
    for (int i = 0; i < ast->stmt_count; i++) script_free_ast(ast->stmts[i]);
    free(ast->stmts);
    for (int i = 0; i < ast->arg_count; i++) {
        free(ast->args[i]); /* ast args are ownership-transferred or freed */
    }
    free(ast->args);
    for (int i = 0; i < ast->param_count; i++) free(ast->params[i]);
    free(ast->params);
    if (ast->literal) free(ast->literal);
    free(ast);
}

/* ═══════════════════════════════════════════════════════════════════
 * L5: Recursive Descent Parser
 * Grammar (simplified):
 *   stmt     → if_stmt | while_stmt | block | func_def | return_stmt | assign_stmt
 *   if_stmt  → 'if' expr block ['else' block]
 *   while_stmt → 'while' expr block
 *   assign_stmt → ident '=' expr
 *   func_def → 'func' ident '(' params ')' block
 *   expr     → compare_expr
 *   compare_expr → add_expr [('==' | '!=' | '<' | '>' | '<=' | '>=') add_expr]
 *   add_expr → mul_expr (('+' | '-') mul_expr)*
 *   mul_expr → unary_expr (('*' | '/') unary_expr)*
 *   unary_expr → ('-' | '!')? primary
 *   primary  → INT | FLOAT | STRING | ident ['(' args ')' | '[' expr ']'] | '(' expr ')'
 *
 * Operator precedence (C-like):
 *   1. () [] .
 *   2. - (unary) !
 *   3. * /
 *   4. + -
 *   5. == != < > <= >=
 * ═══════════════════════════════════════════════════════════════════ */

static ScriptAST *parse_expr(const char *s, int *pos);

static ScriptAST *parse_primary(const char *s, int *pos) {
    skip_ws(s, pos);
    if (!s[*pos]) return NULL;

    if (s[*pos] == '(') {
        (*pos)++;
        ScriptAST *e = parse_expr(s, pos);
        skip_ws(s, pos);
        if (s[*pos] == ')') (*pos)++;
        return e;
    }

    if (s[*pos] == '"') {
        char buf[256];
        read_string(s, pos, buf);
        ScriptAST *a = new_ast(SAST_LITERAL);
        a->literal = (ScriptValue *)calloc(1, sizeof(ScriptValue));
        a->literal->type = SV_STRING;
        strncpy(a->literal->data.str_val, buf, 255);
        return a;
    }

    if (isdigit((unsigned char)s[*pos]) || (s[*pos] == '-' && isdigit((unsigned char)s[*pos + 1]))) {
        double dv = read_number(s, pos);
        ScriptAST *a = new_ast(SAST_LITERAL);
        a->literal = (ScriptValue *)calloc(1, sizeof(ScriptValue));
        if (dv == (int)dv) {
            a->literal->type = SV_INT;
            a->literal->data.int_val = (int)dv;
        } else {
            a->literal->type = SV_FLOAT;
            a->literal->data.float_val = dv;
        }
        return a;
    }

    char buf[128];
    if (read_ident(s, pos, buf)) {
        /* Keyword literals */
        if (strcmp(buf, "nil") == 0) {
            ScriptAST *a = new_ast(SAST_LITERAL);
            a->literal = (ScriptValue *)calloc(1, sizeof(ScriptValue));
            a->literal->type = SV_NIL;
            return a;
        }
        if (strcmp(buf, "true") == 0) {
            ScriptAST *a = new_ast(SAST_LITERAL);
            a->literal = (ScriptValue *)calloc(1, sizeof(ScriptValue));
            a->literal->type = SV_BOOL;
            a->literal->data.bool_val = true;
            return a;
        }
        if (strcmp(buf, "false") == 0) {
            ScriptAST *a = new_ast(SAST_LITERAL);
            a->literal = (ScriptValue *)calloc(1, sizeof(ScriptValue));
            a->literal->type = SV_BOOL;
            a->literal->data.bool_val = false;
            return a;
        }

        ScriptAST *ident_ast = new_ast(SAST_VAR);
        strncpy(ident_ast->name, buf, 63);

        /* Function call or indexing */
        skip_ws(s, pos);
        if (s[*pos] == '(') {
            ScriptAST *call = new_ast(SAST_CALL);
            strncpy(call->name, buf, 63);
            (*pos)++;
            skip_ws(s, pos);
            call->args = (ScriptAST **)calloc(16, sizeof(ScriptAST *));
            call->arg_count = 0;
            if (s[*pos] != ')') {
                call->args[call->arg_count++] = parse_expr(s, pos);
                while (s[*pos] == ',') { (*pos)++; call->args[call->arg_count++] = parse_expr(s, pos); }
            }
            if (s[*pos] == ')') (*pos)++;
            return call;
        }

        if (s[*pos] == '[') {
            ScriptAST *idx = new_ast(SAST_INDEX);
            strncpy(idx->name, buf, 63);
            (*pos)++;
            idx->left = parse_expr(s, pos);
            skip_ws(s, pos);
            if (s[*pos] == ']') (*pos)++;
            return idx;
        }

        return ident_ast;
    }

    return NULL;
}

static ScriptAST *parse_unary(const char *s, int *pos) {
    skip_ws(s, pos);
    if (s[*pos] == '-' && !isdigit((unsigned char)s[*pos + 1])) {
        (*pos)++;
        ScriptAST *a = new_ast(SAST_UNOP);
        a->op = '-';
        a->left = parse_primary(s, pos);
        return a;
    }
    if (s[*pos] == '!') {
        (*pos)++;
        ScriptAST *a = new_ast(SAST_UNOP);
        a->op = '!';
        a->left = parse_primary(s, pos);
        return a;
    }
    return parse_primary(s, pos);
}

static ScriptAST *parse_mul(const char *s, int *pos) {
    ScriptAST *left = parse_unary(s, pos);
    while (left) {
        skip_ws(s, pos);
        if (s[*pos] != '*' && s[*pos] != '/') break;
        char op = s[(*pos)++];
        ScriptAST *right = parse_unary(s, pos);
        ScriptAST *n = new_ast(SAST_BINOP);
        n->op = op; n->left = left; n->right = right; left = n;
    }
    return left;
}

static ScriptAST *parse_add(const char *s, int *pos) {
    ScriptAST *left = parse_mul(s, pos);
    while (left) {
        skip_ws(s, pos);
        if (s[*pos] != '+' && s[*pos] != '-') break;
        char op = s[(*pos)++];
        ScriptAST *right = parse_mul(s, pos);
        ScriptAST *n = new_ast(SAST_BINOP);
        n->op = op; n->left = left; n->right = right; left = n;
    }
    return left;
}

static ScriptAST *parse_compare(const char *s, int *pos) {
    ScriptAST *left = parse_add(s, pos);
    while (left) {
        skip_ws(s, pos);
        char op = s[*pos];
        if (op != '=' && op != '!' && op != '<' && op != '>') break;
        if (op == '=' && s[*pos + 1] != '=') break;
        if (op == '!' && s[*pos + 1] != '=') break;
        if (op == '=' && s[*pos + 1] == '=') { (*pos)++; op = 'E'; /* == */ }
        else if (op == '!' && s[*pos + 1] == '=') { (*pos)++; op = 'N'; /* != */ }
        else if (op == '<' && s[*pos + 1] == '=') { (*pos)++; op = 'L'; /* <= */ }
        else if (op == '>' && s[*pos + 1] == '=') { (*pos)++; op = 'G'; /* >= */ }
        (*pos)++;
        ScriptAST *right = parse_add(s, pos);
        ScriptAST *n = new_ast(SAST_BINOP);
        n->op = op; n->left = left; n->right = right; left = n;
    }
    return left;
}

static ScriptAST *parse_expr(const char *s, int *pos) {
    return parse_compare(s, pos);
}

/* ── Statement parsers ──────────────────────────────────────────── */

static ScriptAST *parse_block(const char *s, int *pos) {
    skip_ws(s, pos);
    if (s[*pos] != '{') return NULL;
    (*pos)++;
    ScriptAST *blk = new_ast(SAST_BLOCK);
    blk->stmts = (ScriptAST **)calloc(64, sizeof(ScriptAST *));
    blk->stmt_count = 0;

    while (s[*pos] && s[*pos] != '}') {
        skip_ws(s, pos); skip_line_comment(s, pos);
        if (s[*pos] == '}') break;
        if (!s[*pos]) break;

        ScriptAST *stmt = NULL;

        if (strncmp(s + *pos, "if", 2) == 0 && !isalnum((unsigned char)s[*pos + 2]) && s[*pos+2] != '_') {
            (*pos) += 2;
            ScriptAST *if_ast = new_ast(SAST_IF);
            if_ast->cond = parse_expr(s, pos);
            if_ast->then_branch = parse_block(s, pos);
            skip_ws(s, pos);
            if (strncmp(s + *pos, "else", 4) == 0 && !isalnum((unsigned char)s[*pos + 4]) && s[*pos+4] != '_') {
                (*pos) += 4;
                if_ast->else_branch = parse_block(s, pos);
            }
            stmt = if_ast;
        } else if (strncmp(s + *pos, "while", 5) == 0 && !isalnum((unsigned char)s[*pos + 5]) && s[*pos+5] != '_') {
            (*pos) += 5;
            ScriptAST *w_ast = new_ast(SAST_WHILE);
            w_ast->cond = parse_expr(s, pos);
            w_ast->body = parse_block(s, pos);
            stmt = w_ast;
        } else if (strncmp(s + *pos, "func", 4) == 0 && !isalnum((unsigned char)s[*pos + 4]) && s[*pos+4] != '_') {
            (*pos) += 4;
            ScriptAST *f_ast = new_ast(SAST_FUNC_DEF);
            char fname[64];
            read_ident(s, pos, fname);
            strncpy(f_ast->name, fname, 63);
            skip_ws(s, pos);
            if (s[*pos] == '(') {
                (*pos)++;
                f_ast->params = (char **)calloc(16, sizeof(char *));
                f_ast->param_count = 0;
                if (s[*pos] != ')') {
                    char pbuf[64];
                    while (read_ident(s, pos, pbuf)) {
                        f_ast->params[f_ast->param_count++] = strdup(pbuf);
                        skip_ws(s, pos);
                        if (s[*pos] == ',') (*pos)++;
                        else break;
                    }
                }
                if (s[*pos] == ')') (*pos)++;
            }
            f_ast->body = parse_block(s, pos);
            stmt = f_ast;
        } else if (strncmp(s + *pos, "return", 6) == 0 && !isalnum((unsigned char)s[*pos + 6]) && s[*pos+6] != '_') {
            (*pos) += 6;
            ScriptAST *r_ast = new_ast(SAST_RETURN);
            skip_ws(s, pos);
            if (s[*pos] != '}' && s[*pos] != ';') {
                r_ast->left = parse_expr(s, pos);
            }
            skip_ws(s, pos);
            if (s[*pos] == ';') (*pos)++;
            stmt = r_ast;
        } else {
            /* Assignment or expression statement */
            int saved = *pos;
            char bf[64];
            if (read_ident(s, pos, bf)) {
                skip_ws(s, pos);
                if (s[*pos] == '=' && s[*pos + 1] != '=') {
                    (*pos)++;
                    ScriptAST *a = new_ast(SAST_ASSIGN);
                    strncpy(a->name, bf, 63);
                    a->left = parse_expr(s, pos);
                    /* Handle table indexing: name[expr] = expr */
                    stmt = a;
                } else if (s[*pos] == '[') {
                    /* name[expr] = expr */
                    (*pos)++;
                    ScriptAST *idx_expr = parse_expr(s, pos);
                    skip_ws(s, pos);
                    if (s[*pos] == ']') (*pos)++;
                    skip_ws(s, pos);
                    if (s[*pos] == '=') {
                        (*pos)++;
                        ScriptAST *a = new_ast(SAST_ASSIGN);
                        strncpy(a->name, bf, 63);
                        a->right = idx_expr; /* index for table set */
                        a->left = parse_expr(s, pos);
                        stmt = a;
                    } else {
                        /* Just an index read expression */
                        ScriptAST *idx = new_ast(SAST_INDEX);
                        strncpy(idx->name, bf, 63);
                        idx->left = idx_expr;
                        stmt = idx;
                        script_free_ast(idx_expr);
                    }
                } else {
                    /* Expression starting with identifier (function call, etc.) */
                    *pos = saved;
                    stmt = parse_expr(s, pos);
                }
            } else {
                /* Unknown */
                (*pos)++;
            }
        }

        skip_ws(s, pos);
        if (s[*pos] == ';') (*pos)++;

        if (stmt && blk->stmt_count < 64) {
            blk->stmts[blk->stmt_count++] = stmt;
        } else if (stmt) {
            script_free_ast(stmt);
        }
    }
    if (s[*pos] == '}') (*pos)++;
    return blk;
}

ScriptAST *script_parse(const char *source, int *pos) {
    if (!source || !pos) return NULL;
    return parse_block(source, pos);
}

/* ═══════════════════════════════════════════════════════════════════
 * L5: Line-based parser (backward compat)
 * ═══════════════════════════════════════════════════════════════════ */

ScriptValue *script_parse_line(const char *line) {
    int pos = 0;
    skip_ws(line, &pos);
    if (!line[pos]) return NULL;

    char buf[128];
    char *ident = read_ident(line, &pos, buf);
    ScriptValue *v = (ScriptValue *)calloc(1, sizeof(ScriptValue));
    if (!v) return NULL;

    if (ident) {
        skip_ws(line, &pos);
        if (line[pos] == '=') {
            pos++;
            skip_ws(line, &pos);
            if (line[pos] == '{') {
                v->type = SV_TABLE;
                v->table = script_table_new();
                pos++;
                while (line[pos] && line[pos] != '}') {
                    skip_ws(line, &pos);
                    char k[128]; read_ident(line, &pos, k);
                    skip_ws(line, &pos);
                    if (line[pos] == '=') {
                        pos++;
                        skip_ws(line, &pos);
                        if (line[pos] == '"') {
                            char sv[256]; read_string(line, &pos, sv);
                            ScriptValue *svv = script_new_value(NULL);
                            svv->type = SV_STRING;
                            strncpy(svv->data.str_val, sv, 255);
                            script_table_set(v->table, k, svv);
                        } else {
                            double dn = read_number(line, &pos);
                            ScriptValue *svv = script_new_value(NULL);
                            svv->type = (dn == (int)dn) ? SV_INT : SV_FLOAT;
                            if (svv->type == SV_INT) svv->data.int_val = (int)dn;
                            else svv->data.float_val = dn;
                            script_table_set(v->table, k, svv);
                        }
                    }
                    skip_ws(line, &pos);
                    if (line[pos] == ',') pos++;
                }
                if (line[pos] == '}') pos++;
                return v;
            }
            if (line[pos] == '"') {
                v->type = SV_STRING;
                char sv[256]; read_string(line, &pos, sv);
                strncpy(v->data.str_val, sv, 255);
            } else if (isdigit((unsigned char)line[pos]) || line[pos] == '-') {
                double dn = read_number(line, &pos);
                v->type = (dn == (int)dn) ? SV_INT : SV_FLOAT;
                if (v->type == SV_INT) v->data.int_val = (int)dn;
                else v->data.float_val = dn;
            }
        }
        return v;
    }

    if (isdigit((unsigned char)line[pos]) || line[pos] == '-') {
        double dn = read_number(line, &pos);
        v->type = (dn == (int)dn) ? SV_INT : SV_FLOAT;
        if (v->type == SV_INT) v->data.int_val = (int)dn;
        else v->data.float_val = dn;
        return v;
    }

    if (line[pos] == '"') {
        v->type = SV_STRING;
        char sv[256]; read_string(line, &pos, sv);
        strncpy(v->data.str_val, sv, 255);
        return v;
    }

    free(v);
    return NULL;
}

/* ═══════════════════════════════════════════════════════════════════
 * L5: AST Evaluator (Tree-Walking)
 *
 * Evaluation walks the AST, dispatching on node type.
 * For binary/unary operations, dynamically resolve based on operand types.
 * ═══════════════════════════════════════════════════════════════════ */

static ScriptValue *eval_ast(ScriptVM *vm, ScriptAST *ast);

static ScriptValue *value_copy(ScriptVM *vm, ScriptValue *v) {
    if (!v) return NULL;
    ScriptValue *nv = script_new_value(vm);
    memcpy(nv, v, sizeof(ScriptValue));
    nv->next = NULL;
    nv->gc_marked = false;
    return nv;
}

ScriptValue *script_eval_ast(ScriptVM *vm, ScriptAST *ast) {
    return eval_ast(vm, ast);
}

static ScriptValue *eval_ast(ScriptVM *vm, ScriptAST *ast) {
    if (!ast) return NULL;

    switch (ast->type) {
        case SAST_LITERAL: {
            return value_copy(vm, ast->literal);
        }
        case SAST_VAR: {
            ScriptValue *v = script_table_get(vm->globals, ast->name);
            if (!v) {
                /* Return nil for undefined vars */
                ScriptValue *nil = script_new_value(vm);
                nil->type = SV_NIL;
                return nil;
            }
            return v;
        }
        case SAST_BINOP: {
            ScriptValue *l = eval_ast(vm, ast->left);
            ScriptValue *r = eval_ast(vm, ast->right);
            if (!l || !r) { ScriptValue *nil = script_new_value(vm); nil->type = SV_NIL; return nil; }

            switch (ast->op) {
                case '+': return script_value_add(vm, l, r);
                case '-': return script_value_sub(vm, l, r);
                case '*': return script_value_mul(vm, l, r);
                case '/': return script_value_div(vm, l, r);
                case 'E': /* == */ return script_value_eq(vm, l, r);
                case 'N': { /* != */
                    ScriptValue *eq = script_value_eq(vm, l, r);
                    eq->data.bool_val = !eq->data.bool_val;
                    return eq;
                }
                case '<': return script_value_lt(vm, l, r);
                case 'L': { /* <= */
                    ScriptValue *lt = script_value_lt(vm, l, r);
                    ScriptValue *eq = script_value_eq(vm, l, r);
                    lt->data.bool_val = lt->data.bool_val || eq->data.bool_val;
                    return lt;
                }
                case '>': {
                    ScriptValue *lt = script_value_lt(vm, r, l);
                    return lt;
                }
                case 'G': { /* >= */
                    ScriptValue *lt = script_value_lt(vm, r, l);
                    ScriptValue *eq = script_value_eq(vm, l, r);
                    lt->data.bool_val = lt->data.bool_val || eq->data.bool_val;
                    return lt;
                }
                default: {
                    ScriptValue *nil = script_new_value(vm); nil->type = SV_NIL; return nil;
                }
            }
        }
        case SAST_UNOP: {
            ScriptValue *v = eval_ast(vm, ast->left);
            if (!v) { ScriptValue *nil = script_new_value(vm); nil->type = SV_NIL; return nil; }
            if (ast->op == '-') {
                if (v->type == SV_INT) {
                    ScriptValue *r = script_new_value(vm);
                    r->type = SV_INT;
                    r->data.int_val = -v->data.int_val;
                    return r;
                }
                if (v->type == SV_FLOAT) {
                    ScriptValue *r = script_new_value(vm);
                    r->type = SV_FLOAT;
                    r->data.float_val = -v->data.float_val;
                    return r;
                }
            }
            if (ast->op == '!') {
                ScriptValue *r = script_new_value(vm);
                r->type = SV_BOOL;
                r->data.bool_val = (v->type == SV_NIL) || (v->type == SV_BOOL && !v->data.bool_val);
                return r;
            }
            ScriptValue *nil = script_new_value(vm); nil->type = SV_NIL; return nil;
        }
        case SAST_CALL: {
            ScriptValue *fn = script_table_get(vm->globals, ast->name);
            if (!fn) {
                /* Check builtins */
                if (strcmp(ast->name, "print") == 0) {
                    for (int i = 0; i < ast->arg_count; i++) {
                        ScriptValue *arg = eval_ast(vm, ast->args[i]);
                        if (i > 0) printf(" ");
                        script_print_value(arg);
                    }
                    printf("\n");
                    ScriptValue *nil = script_new_value(vm); nil->type = SV_NIL; return nil;
                }
                if (strcmp(ast->name, "len") == 0 && ast->arg_count > 0) {
                    ScriptValue *arg = eval_ast(vm, ast->args[0]);
                    ScriptValue *r = script_new_value(vm);
                    r->type = SV_INT;
                    if (arg->type == SV_STRING) r->data.int_val = (int)strlen(arg->data.str_val);
                    else r->data.int_val = 0;
                    return r;
                }
                ScriptValue *nil = script_new_value(vm); nil->type = SV_NIL; return nil;
            }

            if (fn->type == SV_NATIVE && fn->data.func_data.native_fn) {
                ScriptValue **args = (ScriptValue **)calloc((size_t)ast->arg_count, sizeof(ScriptValue *));
                for (int i = 0; i < ast->arg_count; i++)
                    args[i] = eval_ast(vm, ast->args[i]);
                ScriptValue *r = fn->data.func_data.native_fn(args, ast->arg_count);
                free(args);
                return r;
            }

            if (fn->type == SV_FUNC && fn->data.func_data.body) {
                ScriptValue **args = (ScriptValue **)calloc((size_t)ast->arg_count, sizeof(ScriptValue *));
                for (int i = 0; i < ast->arg_count; i++)
                    args[i] = eval_ast(vm, ast->args[i]);
                ScriptValue *r = script_call_func(vm, fn, args, ast->arg_count);
                free(args);
                return r;
            }

            ScriptValue *nil = script_new_value(vm); nil->type = SV_NIL; return nil;
        }
        case SAST_INDEX: {
            ScriptValue *tbl = script_table_get(vm->globals, ast->name);
            if (!tbl || tbl->type != SV_TABLE) {
                ScriptValue *nil = script_new_value(vm); nil->type = SV_NIL; return nil;
            }
            ScriptValue *idx = eval_ast(vm, ast->left);
            if (!idx) { ScriptValue *nil = script_new_value(vm); nil->type = SV_NIL; return nil; }
            char key[128];
            if (idx->type == SV_STRING) strncpy(key, idx->data.str_val, 127);
            else if (idx->type == SV_INT) snprintf(key, 128, "%d", idx->data.int_val);
            else { key[0] = '\0'; }
            ScriptValue *r = script_table_get(tbl->table, key);
            if (!r) { ScriptValue *nil = script_new_value(vm); nil->type = SV_NIL; return nil; }
            return r;
        }
        case SAST_ASSIGN: {
            ScriptValue *val = eval_ast(vm, ast->left);
            if (!val) return NULL;
            if (ast->right) {
                /* Indexed assignment: t[expr] = val, stored in ast->right = index */
                ScriptValue *tbl = script_table_get(vm->globals, ast->name);
                if (!tbl || tbl->type != SV_TABLE) {
                    /* Create table if needed */
                    ScriptValue *nt = script_new_value(vm);
                    nt->type = SV_TABLE;
                    nt->table = script_table_new();
                    script_table_set(vm->globals, ast->name, nt);
                    tbl = nt;
                }
                ScriptValue *idx = eval_ast(vm, ast->right);
                char key[128];
                if (idx->type == SV_STRING) strncpy(key, idx->data.str_val, 127);
                else if (idx->type == SV_INT) snprintf(key, 128, "%d", idx->data.int_val);
                else strncpy(key, "_unknown", 127);
                script_table_set(tbl->table, key, val);
                return val;
            }
            script_table_set(vm->globals, ast->name, val);
            return val;
        }
        case SAST_IF: {
            ScriptValue *cond = eval_ast(vm, ast->cond);
            bool truthy = cond && (cond->type != SV_NIL) && !(cond->type == SV_BOOL && !cond->data.bool_val);
            if (truthy)
                return eval_ast(vm, ast->then_branch);
            else if (ast->else_branch)
                return eval_ast(vm, ast->else_branch);
            ScriptValue *nil = script_new_value(vm); nil->type = SV_NIL; return nil;
        }
        case SAST_WHILE: {
            ScriptValue *last = script_new_value(vm);
            last->type = SV_NIL;
            int iter = 0;
            while (iter++ < 10000) {
                ScriptValue *cond = eval_ast(vm, ast->cond);
                bool truthy = cond && (cond->type != SV_NIL) && !(cond->type == SV_BOOL && !cond->data.bool_val);
                if (!truthy) break;
                last = eval_ast(vm, ast->body);
            }
            return last;
        }
        case SAST_BLOCK: {
            ScriptValue *last = script_new_value(vm);
            last->type = SV_NIL;
            for (int i = 0; i < ast->stmt_count && ast->stmts[i]; i++) {
                last = eval_ast(vm, ast->stmts[i]);
                if (ast->stmts[i]->type == SAST_RETURN) break;
            }
            return last;
        }
        case SAST_FUNC_DEF: {
            ScriptValue *fn = script_new_value(vm);
            fn->type = SV_FUNC;
            strncpy(fn->data.func_data.name, ast->name, 63);
            fn->data.func_data.params = ast->params; /* ownership transfer */
            fn->data.func_data.param_count = ast->param_count;
            /* Store source as text for body */
            fn->data.func_data.body = strdup(ast->name); /* function name stored for identification */
            fn->data.func_data.native_fn = NULL;
            /* Store AST body pointer via user_data for now */
            fn->data.user_data = ast->body;
            ast->params = NULL; ast->param_count = 0;
            ast->body = NULL;
            script_table_set(vm->globals, ast->name, fn);
            return fn;
        }
        case SAST_RETURN: {
            if (ast->left) return eval_ast(vm, ast->left);
            ScriptValue *nil = script_new_value(vm); nil->type = SV_NIL; return nil;
        }
        default: {
            ScriptValue *nil = script_new_value(vm); nil->type = SV_NIL; return nil;
        }
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * L5: Top-level eval / execute
 * ═══════════════════════════════════════════════════════════════════ */

ScriptValue *script_eval(ScriptVM *vm, ScriptValue *expr) {
    (void)vm;
    return expr;
}

int script_execute(ScriptVM *vm, const char *source) {
    if (!vm || !source) return -1;
    int pos = 0;
    ScriptAST *ast = script_parse(source, &pos);
    if (!ast) return -2;
    ScriptValue *result = eval_ast(vm, ast);
    if (result) script_print_value(result);
    printf("\n");
    script_free_ast(ast);
    return 0;
}

/* ═══════════════════════════════════════════════════════════════════
 * L5: Function calling
 * ═══════════════════════════════════════════════════════════════════ */

ScriptValue *script_call_func(ScriptVM *vm, ScriptValue *func, ScriptValue **args, int argc) {
    if (!vm || !func) return script_new_value(vm);

    if (func->type == SV_NATIVE && func->data.func_data.native_fn) {
        return func->data.func_data.native_fn(args, argc);
    }

    if (func->type == SV_FUNC && func->data.user_data) {
        /* Interpret user-defined function: body is an AST from func_def */
        ScriptAST *body = (ScriptAST *)func->data.user_data;
        /* Set up parameters as local variables */
        for (int i = 0; i < func->data.func_data.param_count && i < argc; i++) {
            /* Temporarily set args as globals (simplified — should use scoped env) */
            ScriptValue *arg_copy = script_new_value(vm);
            memcpy(arg_copy, args[i], sizeof(ScriptValue));
            arg_copy->next = NULL;
            arg_copy->gc_marked = false;
            script_table_set(vm->globals, func->data.func_data.params[i], arg_copy);
        }
        /* Evaluate body */
        ScriptValue *result = eval_ast(vm, body);
        if (!result) { result = script_new_value(vm); result->type = SV_NIL; }
        return result;
    }

    return script_new_value(vm);
}

/* ═══════════════════════════════════════════════════════════════════
 * L7: Value Arithmetic — Dynamic type coercion
 * ═══════════════════════════════════════════════════════════════════ */

static double to_number(ScriptValue *v) {
    if (!v) return 0.0;
    switch (v->type) {
        case SV_INT: return (double)v->data.int_val;
        case SV_FLOAT: return v->data.float_val;
        case SV_BOOL: return v->data.bool_val ? 1.0 : 0.0;
        case SV_STRING: return atof(v->data.str_val);
        default: return 0.0;
    }
}

ScriptValue *script_value_add(ScriptVM *vm, ScriptValue *a, ScriptValue *b) {
    ScriptValue *r = script_new_value(vm);
    /* String concatenation */
    if (a->type == SV_STRING || b->type == SV_STRING) {
        r->type = SV_STRING;
        char abuf[256], bbuf[256];
        if (a->type == SV_STRING) strncpy(abuf, a->data.str_val, 255);
        else snprintf(abuf, 256, "%g", to_number(a));
        if (b->type == SV_STRING) strncpy(bbuf, b->data.str_val, 255);
        else snprintf(bbuf, 256, "%g", to_number(b));
        snprintf(r->data.str_val, 256, "%s%s", abuf, bbuf);
        return r;
    }
    /* Numeric */
    if (a->type == SV_FLOAT || b->type == SV_FLOAT) {
        r->type = SV_FLOAT;
        r->data.float_val = to_number(a) + to_number(b);
    } else {
        r->type = SV_INT;
        r->data.int_val = (int)(to_number(a) + to_number(b));
    }
    return r;
}

ScriptValue *script_value_sub(ScriptVM *vm, ScriptValue *a, ScriptValue *b) {
    ScriptValue *r = script_new_value(vm);
    if (a->type == SV_FLOAT || b->type == SV_FLOAT) {
        r->type = SV_FLOAT;
        r->data.float_val = to_number(a) - to_number(b);
    } else {
        r->type = SV_INT;
        r->data.int_val = (int)(to_number(a) - to_number(b));
    }
    return r;
}

ScriptValue *script_value_mul(ScriptVM *vm, ScriptValue *a, ScriptValue *b) {
    ScriptValue *r = script_new_value(vm);
    if (a->type == SV_FLOAT || b->type == SV_FLOAT) {
        r->type = SV_FLOAT;
        r->data.float_val = to_number(a) * to_number(b);
    } else {
        r->type = SV_INT;
        r->data.int_val = (int)(to_number(a) * to_number(b));
    }
    return r;
}

ScriptValue *script_value_div(ScriptVM *vm, ScriptValue *a, ScriptValue *b) {
    ScriptValue *r = script_new_value(vm);
    double db = to_number(b);
    if (db == 0.0) { r->type = SV_NIL; return r; }
    double da = to_number(a);
    if (a->type == SV_FLOAT || b->type == SV_FLOAT || da / db != (int)(da / db)) {
        r->type = SV_FLOAT;
        r->data.float_val = da / db;
    } else {
        r->type = SV_INT;
        r->data.int_val = (int)(da / db);
    }
    return r;
}

ScriptValue *script_value_eq(ScriptVM *vm, ScriptValue *a, ScriptValue *b) {
    ScriptValue *r = script_new_value(vm);
    r->type = SV_BOOL;
    if (a->type != b->type) {
        r->data.bool_val = false;
        return r;
    }
    switch (a->type) {
        case SV_NIL: r->data.bool_val = true; break;
        case SV_BOOL: r->data.bool_val = (a->data.bool_val == b->data.bool_val); break;
        case SV_INT: r->data.bool_val = (a->data.int_val == b->data.int_val); break;
        case SV_FLOAT: r->data.bool_val = (a->data.float_val == b->data.float_val); break;
        case SV_STRING: r->data.bool_val = (strcmp(a->data.str_val, b->data.str_val) == 0); break;
        default: r->data.bool_val = false; break;
    }
    return r;
}

ScriptValue *script_value_lt(ScriptVM *vm, ScriptValue *a, ScriptValue *b) {
    ScriptValue *r = script_new_value(vm);
    r->type = SV_BOOL;
    r->data.bool_val = (to_number(a) < to_number(b));
    return r;
}

ScriptValue *script_value_strcat(ScriptVM *vm, ScriptValue *a, ScriptValue *b) {
    return script_value_add(vm, a, b);
}

/* ═══════════════════════════════════════════════════════════════════
 * L7: String library (native functions)
 * ═══════════════════════════════════════════════════════════════════ */

ScriptValue *script_strlen(ScriptValue **args, int argc) {
    ScriptValue *r = (ScriptValue *)calloc(1, sizeof(ScriptValue));
    r->type = SV_INT;
    if (argc > 0 && args[0] && args[0]->type == SV_STRING) {
        r->data.int_val = (int)strlen(args[0]->data.str_val);
    }
    return r;
}

ScriptValue *script_substr(ScriptValue **args, int argc) {
    ScriptValue *r = (ScriptValue *)calloc(1, sizeof(ScriptValue));
    r->type = SV_STRING;
    r->data.str_val[0] = '\0';
    if (argc >= 3 && args[0] && args[0]->type == SV_STRING &&
        args[1] && args[2] && args[1]->type == SV_INT && args[2]->type == SV_INT) {
        int start = args[1]->data.int_val;
        int len = args[2]->data.int_val;
        int slen = (int)strlen(args[0]->data.str_val);
        if (start < 0) start = 0;
        if (start + len > slen) len = slen - start;
        if (len > 255) len = 255;
        if (len > 0) {
            memcpy(r->data.str_val, args[0]->data.str_val + start, (size_t)len);
            r->data.str_val[len] = '\0';
        }
    }
    return r;
}

ScriptValue *script_to_upper(ScriptValue **args, int argc) {
    ScriptValue *r = (ScriptValue *)calloc(1, sizeof(ScriptValue));
    r->type = SV_STRING;
    r->data.str_val[0] = '\0';
    if (argc > 0 && args[0] && args[0]->type == SV_STRING) {
        int i;
        for (i = 0; args[0]->data.str_val[i] && i < 255; i++) {
            r->data.str_val[i] = (char)toupper((unsigned char)args[0]->data.str_val[i]);
        }
        r->data.str_val[i] = '\0';
    }
    return r;
}

/* ═══════════════════════════════════════════════════════════════════
 * L7: Print / IO
 * ═══════════════════════════════════════════════════════════════════ */

void script_print_value(ScriptValue *v) {
    if (!v) { printf("nil"); return; }
    switch (v->type) {
        case SV_NIL:      printf("nil"); break;
        case SV_BOOL:     printf(v->data.bool_val ? "true" : "false"); break;
        case SV_INT:      printf("%d", v->data.int_val); break;
        case SV_FLOAT:    printf("%g", v->data.float_val); break;
        case SV_STRING:   printf("%s", v->data.str_val); break;
        case SV_TABLE: {
            printf("{");
            if (v->table) {
                bool first = true;
                for (int i = 0; i < v->table->size; i++) {
                    for (TableEntry *e = v->table->buckets[i]; e; e = e->next) {
                        if (!first) printf(", ");
                        printf("%s=", e->key);
                        script_print_value(e->value);
                        first = false;
                    }
                }
            }
            printf("}");
            break;
        }
        case SV_FUNC:     printf("<func:%s>", v->data.func_data.name); break;
        case SV_NATIVE:   printf("<native:%s>", v->data.func_data.name); break;
        case SV_USERDATA: printf("<userdata:%p>", v->data.user_data); break;
    }
}

void script_fprint_value(FILE *f, ScriptValue *v) {
    if (!v) { fprintf(f, "nil"); return; }
    switch (v->type) {
        case SV_NIL:    fprintf(f, "nil"); break;
        case SV_BOOL:   fprintf(f, v->data.bool_val ? "true" : "false"); break;
        case SV_INT:    fprintf(f, "%d", v->data.int_val); break;
        case SV_FLOAT:  fprintf(f, "%g", v->data.float_val); break;
        case SV_STRING: fprintf(f, "%s", v->data.str_val); break;
        default:        fprintf(f, "<%d>", v->type); break;
    }
}

/* ═══════════════════════════════════════════════════════════════════
 * L8: Coroutines — basic yield/resume
 * ═══════════════════════════════════════════════════════════════════ */

void script_coro_init(ScriptVM *co) {
    if (co) memset(co, 0, sizeof(ScriptVM));
}

int script_coro_yield(ScriptVM *co, ScriptValue *v) {
    if (!co) return -1;
    script_table_set(co->globals, "_yield_val", v);
    return 0;
}

ScriptValue *script_coro_resume(ScriptVM *co) {
    if (!co) return NULL;
    ScriptValue *v = script_table_get(co->globals, "_yield_val");
    if (!v) {
        v = script_new_value(co);
        v->type = SV_NIL;
    }
    return v;
}

/* ═══════════════════════════════════════════════════════════════════
 * L3: VM Cleanup
 * ═══════════════════════════════════════════════════════════════════ */

void script_free_vm(ScriptVM *vm) {
    if (!vm) return;
    /* Free GC list */
    ScriptValue *v = vm->gc_head;
    while (v) { ScriptValue *n = (ScriptValue *)v->next; free(v); v = n; }
    vm->gc_head = NULL;
    /* Free globals table */
    if (vm->globals) {
        for (int i = 0; i < vm->globals->size; i++) {
            TableEntry *e = vm->globals->buckets[i];
            while (e) { TableEntry *n = e->next; free(e); e = n; }
        }
        free(vm->globals->buckets);
        free(vm->globals);
        vm->globals = NULL;
    }
    free(vm->call_stack.stack);
}
