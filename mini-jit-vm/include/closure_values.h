#ifndef CLOSURE_VALUES_H
#define CLOSURE_VALUES_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#include "gc.h"

#define MAX_GLOBALS    128
#define MAX_CALL_FRAMES 64
#define MAX_CAPTURES    8
#define VALUE_STRING_LEN 128

typedef enum {
    VAL_INT     = 0,
    VAL_FLOAT   = 1,
    VAL_BOOL    = 2,
    VAL_STRING  = 3,
    VAL_CLOSURE = 4,
    VAL_NATIVE  = 5,
    VAL_NIL     = 6
} ValueType;

typedef int64_t (*NativeFn)(int64_t* args, int32_t argc);

typedef struct Closure Closure;

typedef struct {
    ValueType type;
    union {
        int64_t   int_val;
        double    float_val;
        bool      bool_val;
        char      str_val[VALUE_STRING_LEN];
        Closure*  closure_val;
        NativeFn  native_val;
    } data;
} Value;

struct Closure {
    int32_t  num_captures;
    Value    captures[MAX_CAPTURES];
    void*    func_ptr;
    void*    bytecode_ref;
    bool     is_jit_compiled;
};

typedef struct {
    Value      stack[VM_STACK_SIZE];
    int32_t    sp;
    Value      constants[BC_MAX_CONSTANTS];
    Value      globals[MAX_GLOBALS];
    int32_t    num_globals;
    struct {
        Closure* closure;
        int32_t  return_ip;
        int32_t  base_sp;
    } call_frames[MAX_CALL_FRAMES];
    int32_t    frame_depth;
} VMContext;

Value    value_int(int64_t v);
Value    value_float(double v);
Value    value_bool(bool v);
Value    value_string(const char* s);
Value    value_nil(void);
Value    value_closure(Closure* c);
Value    value_native(NativeFn fn);

void     value_print(const Value* v);
bool     value_eq(const Value* a, const Value* b);
bool     value_is_truthy(const Value* v);
const char* value_type_name(ValueType t);

Closure* closure_create(void* func_ptr, void* bytecode_ref);
void     closure_capture(Closure* c, Value v);
Value    closure_call(Closure* c, Value* args, int32_t argc);

void     vm_context_init(VMContext* ctx);
void     vm_context_mark_roots(VMContext* ctx, GCObject** roots, int32_t* num_roots);

#endif
