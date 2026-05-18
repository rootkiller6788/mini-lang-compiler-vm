#include "pattern_match.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Pattern* pattern_wild(void) {
    Pattern* p = malloc(sizeof(Pattern));
    if (!p) return NULL;
    p->tag = P_WILD;
    p->next = NULL;
    return p;
}

Pattern* pattern_var(const char* name) {
    Pattern* p = malloc(sizeof(Pattern));
    if (!p) return NULL;
    p->tag = P_VAR;
    snprintf(p->var.var_name, PM_MAX_NAME_LEN, "%s", name);
    p->next = NULL;
    return p;
}

Pattern* pattern_int(int value) {
    Pattern* p = malloc(sizeof(Pattern));
    if (!p) return NULL;
    p->tag = P_INT;
    p->int_val = value;
    p->next = NULL;
    return p;
}

Pattern* pattern_bool(bool value) {
    Pattern* p = malloc(sizeof(Pattern));
    if (!p) return NULL;
    p->tag = P_BOOL;
    p->bool_val = value;
    p->next = NULL;
    return p;
}

Pattern* pattern_string(const char* s) {
    Pattern* p = malloc(sizeof(Pattern));
    if (!p) return NULL;
    p->tag = P_STRING;
    snprintf(p->string_val, PM_MAX_NAME_LEN, "%s", s);
    p->next = NULL;
    return p;
}

Pattern* pattern_cons(const char* name, Pattern** subs, int arity) {
    Pattern* p = malloc(sizeof(Pattern));
    if (!p) return NULL;
    p->tag = P_CONS;
    snprintf(p->cons.cons_name, PM_MAX_NAME_LEN, "%s", name);
    p->cons.arity = arity;
    for (int i = 0; i < arity && i < PM_MAX_ARGS; i++) {
        p->cons.sub_patterns[i] = subs[i];
    }
    p->next = NULL;
    return p;
}

Pattern* pattern_tuple(Pattern** elems, int count) {
    Pattern* p = malloc(sizeof(Pattern));
    if (!p) return NULL;
    p->tag = P_TUPLE;
    p->tuple.count = count;
    for (int i = 0; i < count && i < PM_MAX_ARGS; i++) {
        p->tuple.elements[i] = elems[i];
    }
    p->next = NULL;
    return p;
}

Pattern* pattern_or(Pattern* left, Pattern* right) {
    Pattern* p = malloc(sizeof(Pattern));
    if (!p) return NULL;
    p->tag = P_OR;
    p->or_pattern.left = left;
    p->or_pattern.right = right;
    p->next = NULL;
    return p;
}

Pattern* pattern_as(const char* name, Pattern* sub) {
    Pattern* p = malloc(sizeof(Pattern));
    if (!p) return NULL;
    p->tag = P_AS;
    snprintf(p->as_pattern.var_name, PM_MAX_NAME_LEN, "%s", name);
    p->as_pattern.sub = sub;
    p->next = NULL;
    return p;
}

void pattern_print(const Pattern* p) {
    if (!p) { printf("_"); return; }
    switch (p->tag) {
    case P_WILD: printf("_"); break;
    case P_VAR:  printf("%s", p->var.var_name); break;
    case P_INT:  printf("%d", p->int_val); break;
    case P_BOOL: printf("%s", p->bool_val ? "true" : "false"); break;
    case P_STRING: printf("\"%s\"", p->string_val); break;
    case P_CONS:
        printf("%s(", p->cons.cons_name);
        for (int i = 0; i < p->cons.arity; i++) {
            pattern_print(p->cons.sub_patterns[i]);
            if (i < p->cons.arity - 1) printf(", ");
        }
        printf(")");
        break;
    case P_TUPLE:
        printf("(");
        for (int i = 0; i < p->tuple.count; i++) {
            pattern_print(p->tuple.elements[i]);
            if (i < p->tuple.count - 1) printf(", ");
        }
        printf(")");
        break;
    case P_OR:
        printf("(");
        pattern_print(p->or_pattern.left);
        printf(" | ");
        pattern_print(p->or_pattern.right);
        printf(")");
        break;
    case P_AS:
        printf("%s@", p->as_pattern.var_name);
        pattern_print(p->as_pattern.sub);
        break;
    }
}

void pattern_destroy(Pattern* p) {
    if (!p) return;
    if (p->tag == P_CONS) {
        for (int i = 0; i < p->cons.arity; i++) {
            pattern_destroy(p->cons.sub_patterns[i]);
        }
    }
    if (p->tag == P_TUPLE) {
        for (int i = 0; i < p->tuple.count; i++) {
            pattern_destroy(p->tuple.elements[i]);
        }
    }
    if (p->tag == P_OR) {
        pattern_destroy(p->or_pattern.left);
        pattern_destroy(p->or_pattern.right);
    }
    if (p->tag == P_AS) {
        pattern_destroy(p->as_pattern.sub);
    }
    free(p);
}

MatchCase match_case_create(Pattern* pattern, void* expr) {
    MatchCase mc;
    mc.pattern = pattern;
    mc.expression = expr;
    return mc;
}

typedef struct {
    MatchValue value;
    Binding    bindings[PM_MAX_BINDINGS];
    int        binding_count;
} MatchContext;

static bool match_pattern(Pattern* p, MatchContext* ctx, int depth) {
    if (!p) return true;
    if (depth > PM_MAX_ARGS * 2) return false;
    switch (p->tag) {
    case P_WILD: return true;
    case P_VAR:
        ctx->bindings[ctx->binding_count].name[0] = '\0';
        ctx->bindings[ctx->binding_count].value = (void*)(intptr_t)ctx->value.value;
        snprintf(ctx->bindings[ctx->binding_count].name, PM_MAX_NAME_LEN, "%s",
                 p->var.var_name);
        ctx->binding_count++;
        return true;
    case P_INT:
        return ctx->value.value == p->int_val;
    case P_BOOL:
        return ctx->value.matched == p->bool_val;
    case P_STRING:
        return true;
    case P_CONS:
        if (ctx->value.value != (int)(intptr_t)p->cons.cons_name) return false;
        return true;
    case P_TUPLE:
        return true;
    case P_OR:
        return match_pattern(p->or_pattern.left, ctx, depth + 1) ||
               match_pattern(p->or_pattern.right, ctx, depth + 1);
    case P_AS:
        return match_pattern(p->as_pattern.sub, ctx, depth + 1);
    }
    return false;
}

bool match_simple(Pattern* pattern, MatchValue* value, Binding* bindings, int* binding_count) {
    MatchContext ctx;
    ctx.value = *value;
    ctx.binding_count = 0;
    if (match_pattern(pattern, &ctx, 0)) {
        memcpy(bindings, ctx.bindings, sizeof(Binding) * ctx.binding_count);
        *binding_count = ctx.binding_count;
        value->matched = true;
        return true;
    }
    value->matched = false;
    *binding_count = 0;
    return false;
}

DTNode* match_compile(MatchCase* cases, int case_count) {
    if (case_count == 0) return NULL;
    DTNode* root = malloc(sizeof(DTNode));
    if (!root) return NULL;
    root->type = DT_LEAF;
    root->leaf.binding_count = 0;
    root->leaf.action = NULL;
    if (case_count == 1 && cases[0].pattern->tag == P_WILD) {
        root->leaf.action = cases[0].expression;
        return root;
    }
    MatchCase first = cases[0];
    switch (first.pattern->tag) {
    case P_INT:
        root->type = DT_TEST_INT;
        root->test_int.value = first.pattern->int_val;
        root->test_int.success = malloc(sizeof(DTNode));
        root->test_int.success->type = DT_LEAF;
        root->test_int.success->leaf.action = first.expression;
        root->test_int.success->leaf.binding_count = 0;
        if (case_count > 1) {
            root->test_int.failure = match_compile(cases + 1, case_count - 1);
        } else {
            root->test_int.failure = NULL;
        }
        break;
    case P_BOOL:
        root->type = DT_TEST_BOOL;
        root->test_bool.value = first.pattern->bool_val;
        root->test_bool.success = malloc(sizeof(DTNode));
        root->test_bool.success->type = DT_LEAF;
        root->test_bool.success->leaf.action = first.expression;
        root->test_bool.success->leaf.binding_count = 0;
        if (case_count > 1) {
            root->test_bool.failure = match_compile(cases + 1, case_count - 1);
        } else {
            root->test_bool.failure = NULL;
        }
        break;
    default:
        root->type = DT_LEAF;
        root->leaf.action = first.expression;
        root->leaf.binding_count = 0;
        break;
    }
    return root;
}

void* match_execute(DTNode* tree, MatchValue* value) {
    if (!tree) return NULL;
    switch (tree->type) {
    case DT_TEST_INT:
        if (value->value == tree->test_int.value) {
            return match_execute(tree->test_int.success, value);
        }
        return match_execute(tree->test_int.failure, value);
    case DT_TEST_BOOL:
        if (value->matched == tree->test_bool.value) {
            return match_execute(tree->test_bool.success, value);
        }
        return match_execute(tree->test_bool.failure, value);
    case DT_LEAF:
        return tree->leaf.action;
    case DT_SWITCH:
        return match_execute(tree->sw.default_branch, value);
    default:
        return NULL;
    }
}

void match_print_decision_tree(DTNode* node, int depth) {
    if (!node) return;
    for (int i = 0; i < depth; i++) printf("  ");
    switch (node->type) {
    case DT_TEST_INT:
        printf("TestInt(%d)\n", node->test_int.value);
        for (int i = 0; i < depth + 1; i++) printf("  ");
        printf("Success:\n");
        match_print_decision_tree(node->test_int.success, depth + 2);
        if (node->test_int.failure) {
            for (int i = 0; i < depth + 1; i++) printf("  ");
            printf("Failure:\n");
            match_print_decision_tree(node->test_int.failure, depth + 2);
        }
        break;
    case DT_TEST_BOOL:
        printf("TestBool(%s)\n", node->test_bool.value ? "true" : "false");
        match_print_decision_tree(node->test_bool.success, depth + 1);
        if (node->test_bool.failure)
            match_print_decision_tree(node->test_bool.failure, depth + 1);
        break;
    case DT_LEAF:
        printf("Leaf(action=%p)\n", node->leaf.action);
        break;
    case DT_SWITCH:
        printf("Switch\n");
        match_print_decision_tree(node->sw.default_branch, depth + 1);
        break;
    default:
        printf("Unknown\n");
        break;
    }
}

void match_tree_destroy(DTNode* node) {
    if (!node) return;
    switch (node->type) {
    case DT_TEST_INT:
        match_tree_destroy(node->test_int.success);
        match_tree_destroy(node->test_int.failure);
        break;
    case DT_TEST_BOOL:
        match_tree_destroy(node->test_bool.success);
        match_tree_destroy(node->test_bool.failure);
        break;
    case DT_SWITCH:
        for (int i = 0; i < node->sw.branch_count; i++) {
            match_tree_destroy(node->sw.branches[i]);
        }
        match_tree_destroy(node->sw.default_branch);
        break;
    default: break;
    }
    free(node);
}

void match_print_bindings(Binding* bindings, int count) {
    for (int i = 0; i < count; i++) {
        printf("  %s = %d\n", bindings[i].name, (int)(intptr_t)bindings[i].value);
    }
}
