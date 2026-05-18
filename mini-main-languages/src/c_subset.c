#include "c_subset.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

void c_init_program(CProgram *prog) {
    prog->globals = NULL;
    prog->functions = NULL;
}

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

static void skip_ws(const char *s, int *pos) {
    while (s[*pos] && isspace((unsigned char)s[*pos])) (*pos)++;
}

static char *read_ident(const char *s, int *pos, char *buf) {
    int i = 0;
    while (isalnum((unsigned char)s[*pos]) || s[*pos] == '_') {
        if (i < 63) buf[i++] = s[*pos];
        (*pos)++;
    }
    buf[i] = '\0';
    return (i > 0) ? buf : NULL;
}

static int read_int(const char *s, int *pos) {
    int n = 0;
    while (isdigit((unsigned char)s[*pos])) {
        n = n * 10 + (s[*pos] - '0');
        (*pos)++;
    }
    return n;
}

static ASTNode *new_node(ASTNodeType t) {
    ASTNode *n = (ASTNode *)calloc(1, sizeof(ASTNode));
    if (n) n->type = t;
    return n;
}

static ASTNode *parse_expr(const char *s, int *pos);

static ASTNode *parse_primary(const char *s, int *pos) {
    skip_ws(s, pos);
    if (isdigit((unsigned char)s[*pos])) {
        ASTNode *n = new_node(NODE_INT);
        n->data.int_val = read_int(s, pos);
        return n;
    }
    if (isalpha((unsigned char)s[*pos]) || s[*pos] == '_') {
        char buf[64];
        read_ident(s, pos, buf);
        skip_ws(s, pos);
        if (s[*pos] == '(') {
            ASTNode *n = new_node(NODE_CALL);
            strcpy(n->data.call.name, buf);
            (*pos)++;
            n->data.call.args = (ASTNode **)calloc(16, sizeof(ASTNode *));
            n->data.call.arg_count = 0;
            if (s[*pos] != ')') {
                n->data.call.args[n->data.call.arg_count++] = parse_expr(s, pos);
                while (s[*pos] == ',') { (*pos)++; n->data.call.args[n->data.call.arg_count++] = parse_expr(s, pos); }
            }
            if (s[*pos] == ')') (*pos)++;
            return n;
        }
        if (s[*pos] == '[') {
            ASTNode *n = new_node(NODE_INDEX);
            n->data.index_expr.arr = new_node(NODE_VAR);
            strcpy(n->data.index_expr.arr->data.name, buf);
            (*pos)++;
            n->data.index_expr.idx = parse_expr(s, pos);
            if (s[*pos] == ']') (*pos)++;
            return n;
        }
        ASTNode *n = new_node(NODE_VAR);
        strcpy(n->data.name, buf);
        return n;
    }
    if (s[*pos] == '(') {
        (*pos)++;
        ASTNode *n = parse_expr(s, pos);
        skip_ws(s, pos);
        if (s[*pos] == ')') (*pos)++;
        return n;
    }
    return NULL;
}

static ASTNode *parse_term(const char *s, int *pos) {
    ASTNode *l = parse_primary(s, pos);
    while (l) {
        skip_ws(s, pos);
        char op = s[*pos];
        if (op != '*' && op != '/' && op != '%') break;
        (*pos)++;
        ASTNode *r = parse_primary(s, pos);
        ASTNode *n = new_node(NODE_BINOP);
        n->data.binop.left = l;
        n->data.binop.op = op;
        n->data.binop.right = r;
        l = n;
    }
    return l;
}

static ASTNode *parse_expr(const char *s, int *pos) {
    ASTNode *l = parse_term(s, pos);
    while (l) {
        skip_ws(s, pos);
        char op = s[*pos];
        if (op != '+' && op != '-') break;
        (*pos)++;
        ASTNode *r = parse_term(s, pos);
        ASTNode *n = new_node(NODE_BINOP);
        n->data.binop.left = l;
        n->data.binop.op = op;
        n->data.binop.right = r;
        l = n;
    }
    return l;
}

ASTNode *c_parse_declaration(const char *source, int *pos) {
    skip_ws(source, pos);
    if (source[*pos] == '{') return c_parse_function(source, pos);
    ASTNode *n = new_node(NODE_ASSIGN);
    char buf[64];
    if (!read_ident(source, pos, buf)) { free(n); return NULL; }
    strcpy(n->data.assign.name, buf);
    skip_ws(source, pos);
    if (source[*pos] == '[') {
        (*pos)++;
        ASTNode *idx = parse_expr(source, pos);
        skip_ws(source, pos);
        if (source[*pos] == ']') (*pos)++;
        skip_ws(source, pos);
        if (source[*pos] == '=') {
            (*pos)++;
            ASTNode *arr_idx = new_node(NODE_INDEX);
            ASTNode *var = new_node(NODE_VAR);
            strcpy(var->data.name, buf);
            arr_idx->data.index_expr.arr = var;
            arr_idx->data.index_expr.idx = idx;
            n->data.assign.expr = parse_expr(source, pos);
            ASTNode *block = new_node(NODE_BLOCK);
            block->data.block.stmts = (ASTNode **)calloc(2, sizeof(ASTNode *));
            block->data.block.stmts[0] = n;
            block->data.block.stmts[1] = arr_idx;
            block->data.block.count = 2;
            n = block;
        }
        free(idx);
    } else {
        if (source[*pos] == '=') { (*pos)++; n->data.assign.expr = parse_expr(source, pos); }
    }
    return n;
}

ASTNode *c_parse_function(const char *source, int *pos) {
    return parse_expr(source, pos);
}

int c_eval_expr(ASTNode *node, CVar *env, CFunc *funcs) {
    if (!node) return 0;
    switch (node->type) {
        case NODE_INT:  return node->data.int_val;
        case NODE_FLOAT: return (int)node->data.float_val;
        case NODE_VAR: {
            CVar *v = c_find_var(env, node->data.name);
            return v ? v->value.int_val : 0;
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
        case NODE_CALL: {
            CFunc *f = c_find_func(funcs, node->data.call.name);
            if (!f) {
                if (strcmp(node->data.call.name, "print") == 0) {
                    if (node->data.call.arg_count > 0) {
                        printf("%d\n", c_eval_expr(node->data.call.args[0], env, funcs));
                    }
                    return 0;
                }
                return 0;
            }
            CVar *local_env = NULL;
            for (int i = 0; i < f->param_count && i < node->data.call.arg_count; i++) {
                CVar *v = (CVar *)calloc(1, sizeof(CVar));
                strcpy(v->name, f->param_names[i]);
                v->value.int_val = c_eval_expr(node->data.call.args[i], env, funcs);
                v->next = local_env;
                local_env = v;
            }
            int result = 0;
            if (f->body && f->body->type == NODE_BLOCK) {
                for (int i = 0; i < f->body->data.block.count && f->body->data.block.stmts[i]; i++) {
                    ASTNode *s = f->body->data.block.stmts[i];
                    if (s->type == NODE_RETURN)
                        result = c_eval_expr(s->data.ret_expr, local_env, funcs);
                    else if (s->type == NODE_INT)
                        result = s->data.int_val;
                }
            }
            while (local_env) { CVar *n = local_env->next; free(local_env); local_env = n; }
            return result;
        }
        case NODE_RETURN:
            return c_eval_expr(node->data.ret_expr, env, funcs);
        default:
            return 0;
    }
}

void c_execute_statement(ASTNode *node, CVar **env, CFunc *funcs) {
    if (!node) return;
    switch (node->type) {
        case NODE_ASSIGN: {
            int val = c_eval_expr(node->data.assign.expr, *env, funcs);
            CVar *v = c_find_var(*env, node->data.assign.name);
            if (v) { v->value.int_val = val; }
            else {
                CVar *nv = (CVar *)calloc(1, sizeof(CVar));
                strcpy(nv->name, node->data.assign.name);
                nv->type = T_INT;
                nv->value.int_val = val;
                nv->next = *env;
                *env = nv;
            }
            break;
        }
        case NODE_WHILE: {
            int i = 0;
            while (c_eval_expr(node->data.while_stmt.cond, *env, funcs) && i++ < 10000) {
                c_execute_statement(node->data.while_stmt.body, env, funcs);
            }
            break;
        }
        case NODE_IF: {
            if (c_eval_expr(node->data.if_stmt.cond, *env, funcs))
                c_execute_statement(node->data.if_stmt.then_branch, env, funcs);
            else if (node->data.if_stmt.else_branch)
                c_execute_statement(node->data.if_stmt.else_branch, env, funcs);
            break;
        }
        case NODE_BLOCK:
            c_execute_block(node, env, funcs);
            break;
        case NODE_CALL:
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
        }
        return;
    }
    c_execute_statement(node, env, funcs);
}

void c_print_env(CVar *env) {
    printf("=== Environment ===\n");
    for (CVar *v = env; v; v = v->next) {
        printf("  %s = %d\n", v->name, v->value.int_val);
    }
}

void c_free_ast(ASTNode *node) {
    if (!node) return;
    switch (node->type) {
        case NODE_BINOP:
            c_free_ast(node->data.binop.left);
            c_free_ast(node->data.binop.right);
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
        case NODE_BLOCK:
            for (int i = 0; i < node->data.block.count; i++)
                c_free_ast(node->data.block.stmts[i]);
            free(node->data.block.stmts);
            break;
        case NODE_ASSIGN:
            c_free_ast(node->data.assign.expr);
            break;
        case NODE_RETURN:
            c_free_ast(node->data.ret_expr);
            break;
        case NODE_INDEX:
            c_free_ast(node->data.index_expr.arr);
            c_free_ast(node->data.index_expr.idx);
            break;
        default:
            break;
    }
    free(node);
}

void c_free_var_list(CVar *head) {
    while (head) { CVar *n = head->next; free(head); head = n; }
}

void c_free_func_list(CFunc *head) {
    while (head) {
        CFunc *n = head->next;
        free(head->param_names);
        free(head->param_types);
        c_free_ast(head->body);
        free(head);
        head = n;
    }
}
