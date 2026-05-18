#ifndef FP_CLOSURE_H
#define FP_CLOSURE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FP_MAX_CAPTURED 16
#define FP_MAX_ARGS     16

typedef void* (*FPFnPtr)(void** args);

typedef struct {
    FPFnPtr fn_ptr;
    void*   captured_env[FP_MAX_CAPTURED];
    int     env_count;
    int     arity;
    int     total_arity;
} FPClosure;

typedef enum {
    FP_TYPE_INT,
    FP_TYPE_FLOAT,
    FP_TYPE_BOOL,
    FP_TYPE_STRING,
    FP_TYPE_FUNC,
    FP_TYPE_VOID
} FPBaseType;

typedef struct {
    FPBaseType param_types[FP_MAX_ARGS];
    int        param_count;
    FPBaseType return_type;
} FPType;

typedef struct FPList FPList;
struct FPList {
    void*   value;
    FPList* tail;
};

FPClosure* fp_closure_create(FPFnPtr fn, int total_arity, int captured_arity);
void       fp_closure_capture(FPClosure* cl, int index, void* value);
void*      fp_apply(FPClosure* cl, void** args);
FPClosure* fp_curry(FPFnPtr fn, int arity);
FPClosure* fp_compose(FPClosure* f, FPClosure* g);
FPList*    fp_cons(void* value, FPList* tail);
FPList*    fp_map(FPClosure* fn, FPList* list);
FPList*    fp_filter(bool (*pred)(void*), FPList* list);
void*      fp_foldl(void* (*fn)(void*, void*), void* init, FPList* list);
void*      fp_foldr(void* (*fn)(void*, void*), void* init, FPList* list);
int        fp_list_length(FPList* list);
FPList*    fp_list_reverse(FPList* list);
FPList*    fp_list_append(FPList* a, FPList* b);
FPList*    fp_range(int start, int end);
void       fp_closure_destroy(FPClosure* cl);
void       fp_list_destroy(FPList* list);

void* fp_identity(void** args);
void* fp_constant(void** args);

#endif
