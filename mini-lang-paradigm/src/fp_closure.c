#include "fp_closure.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

FPClosure* fp_closure_create(FPFnPtr fn, int total_arity, int captured_arity) {
    FPClosure* cl = malloc(sizeof(FPClosure));
    if (!cl) return NULL;
    cl->fn_ptr = fn;
    cl->env_count = captured_arity;
    cl->arity = total_arity - captured_arity;
    cl->total_arity = total_arity;
    memset(cl->captured_env, 0, sizeof(cl->captured_env));
    return cl;
}

void fp_closure_capture(FPClosure* cl, int index, void* value) {
    if (index >= 0 && index < FP_MAX_CAPTURED) {
        cl->captured_env[index] = value;
    }
}

void* fp_apply(FPClosure* cl, void** args) {
    void* combined[FP_MAX_ARGS + FP_MAX_CAPTURED];
    for (int i = 0; i < cl->env_count; i++) {
        combined[i] = cl->captured_env[i];
    }
    for (int i = 0; i < cl->arity; i++) {
        combined[cl->env_count + i] = args[i];
    }
    return cl->fn_ptr(combined);
}

FPClosure* fp_curry(FPFnPtr fn, int arity) {
    if (arity <= 1) {
        FPClosure* cl = fp_closure_create(fn, 1, 0);
        return cl;
    }
    FPClosure* result = fp_closure_create(fn, arity, 0);
    return result;
}

static void* compose_invoke(void** args) {
    void** inner_args = (void**)args[1];
    void* inner_result = ((FPClosure*)args[0])->fn_ptr(inner_args);
    void* outer_args[] = { inner_result };
    return ((FPClosure*)args[2])->fn_ptr(outer_args);
}

FPClosure* fp_compose(FPClosure* f, FPClosure* g) {
    FPClosure* cl = malloc(sizeof(FPClosure));
    if (!cl) return NULL;
    cl->fn_ptr = compose_invoke;
    cl->env_count = 2;
    cl->captured_env[0] = g;
    cl->captured_env[1] = f;
    cl->arity = g->arity;
    cl->total_arity = g->total_arity;
    return cl;
}

FPList* fp_cons(void* value, FPList* tail) {
    FPList* node = malloc(sizeof(FPList));
    if (!node) return NULL;
    node->value = value;
    node->tail = tail;
    return node;
}

FPList* fp_map(FPClosure* fn, FPList* list) {
    if (!list) return NULL;
    void* result = fp_apply(fn, &list->value);
    FPList* rest = fp_map(fn, list->tail);
    return fp_cons(result, rest);
}

FPList* fp_filter(bool (*pred)(void*), FPList* list) {
    if (!list) return NULL;
    if (pred(list->value)) {
        return fp_cons(list->value, fp_filter(pred, list->tail));
    }
    return fp_filter(pred, list->tail);
}

void* fp_foldl(void* (*fn)(void*, void*), void* init, FPList* list) {
    void* acc = init;
    FPList* current = list;
    while (current) {
        acc = fn(acc, current->value);
        current = current->tail;
    }
    return acc;
}

void* fp_foldr(void* (*fn)(void*, void*), void* init, FPList* list) {
    if (!list) return init;
    void* right = fp_foldr(fn, init, list->tail);
    return fn(list->value, right);
}

int fp_list_length(FPList* list) {
    int len = 0;
    while (list) { len++; list = list->tail; }
    return len;
}

FPList* fp_list_reverse(FPList* list) {
    FPList* result = NULL;
    while (list) {
        result = fp_cons(list->value, result);
        list = list->tail;
    }
    return result;
}

FPList* fp_list_append(FPList* a, FPList* b) {
    if (!a) return b;
    return fp_cons(a->value, fp_list_append(a->tail, b));
}

FPList* fp_range(int start, int end) {
    if (start >= end) return NULL;
    int* val = malloc(sizeof(int));
    *val = start;
    return fp_cons(val, fp_range(start + 1, end));
}

void fp_closure_destroy(FPClosure* cl) {
    free(cl);
}

void fp_list_destroy(FPList* list) {
    while (list) {
        FPList* next = list->tail;
        free(list);
        list = next;
    }
}

void* fp_identity(void** args) {
    return args[0];
}

void* fp_constant(void** args) {
    static void* saved = NULL;
    saved = args[0];
    return saved;
    (void)args;
}
