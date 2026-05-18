#include "type_system.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fresh_var_counter = 0;

int fresh_id(void) { return fresh_var_counter++; }

Type* type_create_primitive(TypeTag tag) {
    Type* t = malloc(sizeof(Type));
    if (!t) return NULL;
    t->tag = tag;
    t->var_id = -1;
    t->element_type = NULL;
    t->param_type = NULL;
    t->return_type = NULL;
    t->left_type = NULL;
    t->right_type = NULL;
    t->name[0] = '\0';
    t->next = NULL;
    return t;
}

Type* type_create_var(int id) {
    Type* t = malloc(sizeof(Type));
    if (!t) return NULL;
    t->tag = T_VAR;
    t->var_id = id;
    t->element_type = NULL;
    t->param_type = NULL;
    t->return_type = NULL;
    t->left_type = NULL;
    t->right_type = NULL;
    t->name[0] = '\0';
    t->next = NULL;
    return t;
}

Type* type_create_func(Type* param, Type* ret) {
    Type* t = malloc(sizeof(Type));
    if (!t) return NULL;
    t->tag = T_FUNC;
    t->var_id = -1;
    t->param_type = param;
    t->return_type = ret;
    t->element_type = NULL;
    t->left_type = NULL;
    t->right_type = NULL;
    t->name[0] = '\0';
    t->next = NULL;
    return t;
}

Type* type_create_pair(Type* left, Type* right) {
    Type* t = malloc(sizeof(Type));
    if (!t) return NULL;
    t->tag = T_PAIR;
    t->var_id = -1;
    t->left_type = left;
    t->right_type = right;
    t->element_type = NULL;
    t->param_type = NULL;
    t->return_type = NULL;
    t->name[0] = '\0';
    t->next = NULL;
    return t;
}

Type* type_create_list(Type* elem) {
    Type* t = malloc(sizeof(Type));
    if (!t) return NULL;
    t->tag = T_LIST;
    t->var_id = -1;
    t->element_type = elem;
    t->param_type = NULL;
    t->return_type = NULL;
    t->left_type = NULL;
    t->right_type = NULL;
    t->name[0] = '\0';
    t->next = NULL;
    return t;
}

void type_print(const Type* t) {
    if (!t) { printf("NULL"); return; }
    switch (t->tag) {
    case T_INT:    printf("Int"); break;
    case T_BOOL:   printf("Bool"); break;
    case T_STRING: printf("String"); break;
    case T_FLOAT:  printf("Float"); break;
    case T_VOID:   printf("Void"); break;
    case T_VAR:    printf("t%d", t->var_id); break;
    case T_ARRAY:
        printf("Array<");
        type_print(t->element_type);
        printf(">");
        break;
    case T_FUNC:
        printf("(");
        type_print(t->param_type);
        printf(" -> ");
        type_print(t->return_type);
        printf(")");
        break;
    case T_PAIR:
        printf("(");
        type_print(t->left_type);
        printf(" * ");
        type_print(t->right_type);
        printf(")");
        break;
    case T_LIST:
        printf("[");
        type_print(t->element_type);
        printf("]");
        break;
    }
}

bool type_equal(const Type* a, const Type* b) {
    if (!a || !b) return a == b;
    if (a->tag != b->tag) return false;
    switch (a->tag) {
    case T_VAR: return a->var_id == b->var_id;
    case T_INT: case T_BOOL: case T_STRING: case T_FLOAT: case T_VOID: return true;
    case T_ARRAY: return type_equal(a->element_type, b->element_type);
    case T_FUNC: return type_equal(a->param_type, b->param_type) &&
                        type_equal(a->return_type, b->return_type);
    case T_PAIR: return type_equal(a->left_type, b->left_type) &&
                        type_equal(a->right_type, b->right_type);
    case T_LIST: return type_equal(a->element_type, b->element_type);
    }
    return false;
}

Type* type_clone(Type* t) {
    if (!t) return NULL;
    Type* copy = malloc(sizeof(Type));
    memcpy(copy, t, sizeof(Type));
    copy->next = NULL;
    if (t->tag == T_ARRAY) copy->element_type = type_clone(t->element_type);
    if (t->tag == T_FUNC) {
        copy->param_type = type_clone(t->param_type);
        copy->return_type = type_clone(t->return_type);
    }
    if (t->tag == T_PAIR) {
        copy->left_type = type_clone(t->left_type);
        copy->right_type = type_clone(t->right_type);
    }
    if (t->tag == T_LIST) copy->element_type = type_clone(t->element_type);
    return copy;
}

void type_destroy(Type* t) {
    if (!t) return;
    type_destroy(t->element_type);
    type_destroy(t->param_type);
    type_destroy(t->return_type);
    type_destroy(t->left_type);
    type_destroy(t->right_type);
    type_destroy(t->next);
    free(t);
}

TypeSubst type_subst_create(void) {
    TypeSubst s;
    memset(s.substitution, 0, sizeof(s.substitution));
    return s;
}

Type* type_subst_apply(TypeSubst* s, Type* t) {
    if (!t) return NULL;
    if (t->tag == T_VAR) {
        if (t->var_id >= 0 && t->var_id < TYPE_MAX_VARS && s->substitution[t->var_id]) {
            return type_subst_apply(s, s->substitution[t->var_id]);
        }
        return t;
    }
    if (t->tag == T_FUNC) {
        Type* p = type_subst_apply(s, t->param_type);
        Type* r = type_subst_apply(s, t->return_type);
        return type_create_func(p, r);
    }
    if (t->tag == T_ARRAY) {
        return type_create_list(type_subst_apply(s, t->element_type));
    }
    if (t->tag == T_LIST) {
        return type_create_list(type_subst_apply(s, t->element_type));
    }
    if (t->tag == T_PAIR) {
        return type_create_pair(type_subst_apply(s, t->left_type),
                                type_subst_apply(s, t->right_type));
    }
    return t;
}

TypeSubst type_subst_compose(TypeSubst* s1, TypeSubst* s2) {
    TypeSubst result = type_subst_create();
    for (int i = 0; i < TYPE_MAX_VARS; i++) {
        if (s2->substitution[i]) {
            result.substitution[i] = s2->substitution[i];
        } else if (s1->substitution[i]) {
            result.substitution[i] = type_subst_apply(s2, s1->substitution[i]);
        }
    }
    return result;
}

bool type_occurs_check(int var_id, Type* t) {
    if (!t) return false;
    if (t->tag == T_VAR) return t->var_id == var_id;
    if (t->tag == T_FUNC) return type_occurs_check(var_id, t->param_type) ||
                                type_occurs_check(var_id, t->return_type);
    return false;
}

bool type_unify(Type* a, Type* b, TypeSubst* result) {
    Type* ta = type_subst_apply(result, a);
    Type* tb = type_subst_apply(result, b);
    if (ta->tag == T_VAR && tb->tag == T_VAR && ta->var_id == tb->var_id) return true;
    if (ta->tag == T_VAR) {
        if (type_occurs_check(ta->var_id, tb)) return false;
        result->substitution[ta->var_id] = tb;
        return true;
    }
    if (tb->tag == T_VAR) {
        if (type_occurs_check(tb->var_id, ta)) return false;
        result->substitution[tb->var_id] = ta;
        return true;
    }
    if (ta->tag != tb->tag) return false;
    switch (ta->tag) {
    case T_INT: case T_BOOL: case T_STRING: case T_FLOAT: case T_VOID: return true;
    case T_FUNC: return type_unify(ta->param_type, tb->param_type, result) &&
                         type_unify(ta->return_type, tb->return_type, result);
    case T_LIST:
    case T_ARRAY: return type_unify(ta->element_type, tb->element_type, result);
    case T_PAIR: return type_unify(ta->left_type, tb->left_type, result) &&
                         type_unify(ta->right_type, tb->right_type, result);
    default: return false;
    }
}

TypeEnv type_env_create(void) {
    TypeEnv env;
    memset(env.binding, 0, sizeof(env.binding));
    return env;
}

static int env_hash(const char* name) {
    int h = 0;
    while (*name) h = (h * 31 + *name++) % TYPE_MAX_BINDINGS;
    return h;
}

void type_env_extend(TypeEnv* env, const char* name, Type* t) {
    int idx = env_hash(name);
    env->binding[idx] = t;
    (void)name;
}

Type* type_env_lookup(TypeEnv* env, const char* name) {
    int idx = env_hash(name);
    return env->binding[idx];
}

Type* type_infer_hm(TypeEnv* env, Expr* expr, TypeSubst* subst) {
    if (!expr) return NULL;
    switch (expr->tag) {
    case EXPR_INT: return type_create_primitive(T_INT);
    case EXPR_BOOL: return type_create_primitive(T_BOOL);
    case EXPR_VAR: {
        Type* t = type_env_lookup(env, expr->var_name);
        if (!t) {
            Type* v = type_create_var(fresh_id());
            type_env_extend(env, expr->var_name, v);
            return v;
        }
        return t;
    }
    case EXPR_LAMBDA: {
        Type* tv = type_create_var(fresh_id());
        TypeEnv new_env = *env;
        type_env_extend(&new_env, expr->lambda.param, tv);
        Type* body_type = type_infer_hm(&new_env, expr->lambda.body, subst);
        Type* func_type = type_create_func(tv, body_type);
        return type_subst_apply(subst, func_type);
    }
    case EXPR_APPLY: {
        Type* fn_type = type_infer_hm(env, expr->apply.fn, subst);
        Type* arg_type = type_infer_hm(env, expr->apply.arg, subst);
        Type* ret_type = type_create_var(fresh_id());
        Type* expected = type_create_func(arg_type, ret_type);
        type_unify(fn_type, expected, subst);
        return type_subst_apply(subst, ret_type);
    }
    case EXPR_LET: {
        Type* def_type = type_infer_hm(env, expr->let.def, subst);
        TypeEnv new_env = *env;
        type_env_extend(&new_env, expr->let.var, def_type);
        return type_infer_hm(&new_env, expr->let.body, subst);
    }
    case EXPR_IFTHENELSE: {
        Type* cond_type = type_infer_hm(env, expr->ifthenelse.cond, subst);
        type_unify(cond_type, type_create_primitive(T_BOOL), subst);
        Type* then_type = type_infer_hm(env, expr->ifthenelse.then_branch, subst);
        Type* else_type = type_infer_hm(env, expr->ifthenelse.else_branch, subst);
        type_unify(then_type, else_type, subst);
        return type_subst_apply(subst, then_type);
    }
    case EXPR_FIX:
    case EXPR_BINOP:
    default:
        return type_create_var(fresh_id());
    }
}

Expr* expr_create_var(const char* name) {
    Expr* e = malloc(sizeof(Expr));
    e->tag = EXPR_VAR;
    snprintf(e->var_name, TYPE_MAX_NAME_LEN, "%s", name);
    return e;
}

Expr* expr_create_int(int value) {
    Expr* e = malloc(sizeof(Expr));
    e->tag = EXPR_INT;
    e->int_val = value;
    return e;
}

Expr* expr_create_bool(bool value) {
    Expr* e = malloc(sizeof(Expr));
    e->tag = EXPR_BOOL;
    e->bool_val = value;
    return e;
}

Expr* expr_create_lambda(const char* param, Expr* body) {
    Expr* e = malloc(sizeof(Expr));
    e->tag = EXPR_LAMBDA;
    snprintf(e->lambda.param, TYPE_MAX_NAME_LEN, "%s", param);
    e->lambda.body = body;
    return e;
}

Expr* expr_create_apply(Expr* fn, Expr* arg) {
    Expr* e = malloc(sizeof(Expr));
    e->tag = EXPR_APPLY;
    e->apply.fn = fn;
    e->apply.arg = arg;
    return e;
}

Expr* expr_create_let(const char* var, Expr* def, Expr* body) {
    Expr* e = malloc(sizeof(Expr));
    e->tag = EXPR_LET;
    snprintf(e->let.var, TYPE_MAX_NAME_LEN, "%s", var);
    e->let.def = def;
    e->let.body = body;
    return e;
}

void expr_print(const Expr* e) {
    if (!e) { printf("nil"); return; }
    switch (e->tag) {
    case EXPR_VAR:  printf("%s", e->var_name); break;
    case EXPR_INT:  printf("%d", e->int_val); break;
    case EXPR_BOOL: printf("%s", e->bool_val ? "true" : "false"); break;
    case EXPR_LAMBDA:
        printf("(lambda %s. ", e->lambda.param);
        expr_print(e->lambda.body);
        printf(")");
        break;
    case EXPR_APPLY:
        printf("(");
        expr_print(e->apply.fn);
        printf(" ");
        expr_print(e->apply.arg);
        printf(")");
        break;
    case EXPR_LET:
        printf("(let %s = ", e->let.var);
        expr_print(e->let.def);
        printf(" in ");
        expr_print(e->let.body);
        printf(")");
        break;
    default: printf("?");
    }
}

void expr_destroy(Expr* e) {
    if (!e) return;
    switch (e->tag) {
    case EXPR_LAMBDA: expr_destroy(e->lambda.body); break;
    case EXPR_APPLY:  expr_destroy(e->apply.fn); expr_destroy(e->apply.arg); break;
    case EXPR_LET:    expr_destroy(e->let.def); expr_destroy(e->let.body); break;
    default: break;
    }
    free(e);
}
