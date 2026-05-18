#include "closure_values.h"
#include <string.h>
#include <stdlib.h>

Value value_int(int64_t v) {
    Value val;
    val.type       = VAL_INT;
    val.data.int_val = v;
    return val;
}

Value value_float(double v) {
    Value val;
    val.type         = VAL_FLOAT;
    val.data.float_val = v;
    return val;
}

Value value_bool(bool v) {
    Value val;
    val.type        = VAL_BOOL;
    val.data.bool_val = v;
    return val;
}

Value value_string(const char* s) {
    Value val;
    val.type = VAL_STRING;
    if (s) {
        strncpy(val.data.str_val, s, VALUE_STRING_LEN - 1);
        val.data.str_val[VALUE_STRING_LEN - 1] = '\0';
    } else {
        val.data.str_val[0] = '\0';
    }
    return val;
}

Value value_nil(void) {
    Value val;
    val.type = VAL_NIL;
    return val;
}

Value value_closure(Closure* c) {
    Value val;
    val.type             = VAL_CLOSURE;
    val.data.closure_val = c;
    return val;
}

Value value_native(NativeFn fn) {
    Value val;
    val.type            = VAL_NATIVE;
    val.data.native_val = fn;
    return val;
}

const char* value_type_name(ValueType t) {
    switch (t) {
        case VAL_INT:     return "int";
        case VAL_FLOAT:   return "float";
        case VAL_BOOL:    return "bool";
        case VAL_STRING:  return "string";
        case VAL_CLOSURE: return "closure";
        case VAL_NATIVE:  return "native";
        case VAL_NIL:     return "nil";
        default:          return "unknown";
    }
}

void value_print(const Value* v) {
    switch (v->type) {
        case VAL_INT:
            printf("%lld", (long long)v->data.int_val);
            break;
        case VAL_FLOAT:
            printf("%g", v->data.float_val);
            break;
        case VAL_BOOL:
            printf("%s", v->data.bool_val ? "true" : "false");
            break;
        case VAL_STRING:
            printf("\"%s\"", v->data.str_val);
            break;
        case VAL_CLOSURE:
            printf("<closure %p>", (void*)v->data.closure_val);
            break;
        case VAL_NATIVE:
            printf("<native %p>", (void*)v->data.native_val);
            break;
        case VAL_NIL:
            printf("nil");
            break;
        default:
            printf("<unknown>");
            break;
    }
}

bool value_eq(const Value* a, const Value* b) {
    if (a->type != b->type) return false;

    switch (a->type) {
        case VAL_INT:     return a->data.int_val == b->data.int_val;
        case VAL_FLOAT:   return a->data.float_val == b->data.float_val;
        case VAL_BOOL:    return a->data.bool_val == b->data.bool_val;
        case VAL_STRING:  return strcmp(a->data.str_val, b->data.str_val) == 0;
        case VAL_CLOSURE: return a->data.closure_val == b->data.closure_val;
        case VAL_NATIVE:  return a->data.native_val == b->data.native_val;
        case VAL_NIL:     return true;
        default:          return false;
    }
}

bool value_is_truthy(const Value* v) {
    switch (v->type) {
        case VAL_BOOL:    return v->data.bool_val;
        case VAL_INT:     return v->data.int_val != 0;
        case VAL_FLOAT:   return v->data.float_val != 0.0;
        case VAL_NIL:     return false;
        case VAL_STRING:  return v->data.str_val[0] != '\0';
        case VAL_CLOSURE:
        case VAL_NATIVE:  return true;
        default:          return false;
    }
}

Closure* closure_create(void* func_ptr, void* bytecode_ref) {
    Closure* c = (Closure*)calloc(1, sizeof(Closure));
    if (!c) return NULL;
    c->num_captures = 0;
    c->func_ptr     = func_ptr;
    c->bytecode_ref = bytecode_ref;
    c->is_jit_compiled = false;
    return c;
}

void closure_capture(Closure* c, Value v) {
    if (c && c->num_captures < MAX_CAPTURES) {
        c->captures[c->num_captures++] = v;
    }
}

Value closure_call(Closure* c, Value* args, int32_t argc) {
    if (!c) return value_nil();
    (void)args;
    (void)argc;

    if (c->is_jit_compiled && c->func_ptr) {
        typedef int64_t (*JitFn)(void);
        JitFn fn = (JitFn)c->func_ptr;
        return value_int(fn());
    }

    return value_nil();
}

void vm_context_init(VMContext* ctx) {
    memset(ctx->stack, 0, sizeof(ctx->stack));
    ctx->sp          = 0;
    ctx->num_globals = 0;
    ctx->frame_depth = 0;
    memset(ctx->call_frames, 0, sizeof(ctx->call_frames));
    memset(ctx->constants, 0, sizeof(ctx->constants));
    memset(ctx->globals, 0, sizeof(ctx->globals));
}

void vm_context_mark_roots(VMContext* ctx, GCObject** roots, int32_t* num_roots) {
    int32_t count = *num_roots;

    for (int32_t i = 0; i < ctx->sp && count < GC_MAX_ROOTS; i++) {
        if (ctx->stack[i].type == VAL_CLOSURE && ctx->stack[i].data.closure_val) {
        }
        count++;
    }

    for (int32_t i = 0; i < ctx->num_globals && count < GC_MAX_ROOTS; i++) {
        if (ctx->globals[i].type == VAL_STRING) {
        }
        count++;
    }

    *num_roots = count;
}
