#ifndef SCRIPT_LANG_H
#define SCRIPT_LANG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

/* ── L1: Core Types ────────────────────────────────────────────────
 * Dynamic language runtime with tagged union value representation.
 * Reference: Lua 5.0 design (Ierusalimschy, Figueiredo, Celes, 2003).
 */
typedef enum {
    SV_NIL,
    SV_BOOL,
    SV_INT,
    SV_FLOAT,
    SV_STRING,
    SV_TABLE,
    SV_FUNC,
    SV_NATIVE,   /* C function pointer */
    SV_USERDATA  /* opaque user pointer */
} ScriptType;

/* ── L2: Forward declaration for ScriptValue (used in TableEntry) ── */
typedef struct ScriptValue ScriptValue;

/* ── L2: Hash Table (chained) ──────────────────────────────────────
 * djb2 hash function (Bernstein). O(1) avg lookup, O(n) worst-case.
 */
typedef struct TableEntry {
    char key[128];
    struct ScriptValue *value;
    struct TableEntry *next;
} TableEntry;

typedef struct Table {
    TableEntry **buckets;
    int size;
    int count;
} Table;

/* ── L3: Native function type ────────────────────────────────────── */
typedef ScriptValue *(*ScriptNativeFn)(ScriptValue **args, int argc);

/* ── L3: ScriptValue — Tagged Union ──────────────────────────────── */
struct ScriptValue {
    ScriptType type;
    union {
        bool bool_val;
        int int_val;
        double float_val;
        char str_val[256];
        struct {
            char name[64];
            char **params;
            int param_count;
            char *body;
            ScriptNativeFn native_fn; /* for SV_NATIVE */
        } func_data;
        void *user_data;
    } data;
    Table *table;          /* for SV_TABLE */
    struct ScriptValue *next; /* GC list link */
    bool gc_marked;        /* mark-sweep GC mark bit */
};

/* ── L3: Call Stack — frame-based function invocation ────────────── */
typedef struct {
    ScriptValue **stack;
    int stack_size;
    int stack_top;
} CallStack;

/* ── L3: Virtual Machine State ───────────────────────────────────── */
typedef struct {
    Table *globals;
    CallStack call_stack;
    ScriptValue *gc_head;  /* GC object list head */
    int gc_threshold;
    int gc_alloc_count;
} ScriptVM;

/* ── L5: AST for script expressions ────────────────────────────────
 * Expression parser builds a tree for evaluation.
 */
typedef enum {
    SAST_LITERAL,    /* constant value */
    SAST_VAR,        /* variable reference */
    SAST_BINOP,      /* binary operator */
    SAST_UNOP,       /* unary operator */
    SAST_CALL,       /* function call */
    SAST_INDEX,      /* table indexing */
    SAST_IF,         /* conditional */
    SAST_WHILE,      /* while loop */
    SAST_BLOCK,      /* statement sequence */
    SAST_ASSIGN,     /* assignment */
    SAST_FUNC_DEF,   /* function definition */
    SAST_RETURN      /* return statement */
} ScriptASTType;

typedef struct ScriptAST {
    ScriptASTType type;
    char name[64];           /* variable/function name */
    ScriptValue *literal;    /* constant value */
    char op;                 /* operator character */
    struct ScriptAST *left;
    struct ScriptAST *right;
    struct ScriptAST *cond;
    struct ScriptAST *then_branch;
    struct ScriptAST *else_branch;
    char **params;
    int param_count;
    struct ScriptAST *body;
    struct ScriptAST **stmts;
    int stmt_count;
    struct ScriptAST **args;
    int arg_count;
} ScriptAST;

/* ── API Declarations ────────────────────────────────────────────── */

/* L1-L2: VM lifecycle */
void          script_init_vm(ScriptVM *vm);
void          script_free_vm(ScriptVM *vm);

/* L2: Value allocation and GC */
ScriptValue  *script_new_value(ScriptVM *vm);
void          script_gc_collect(ScriptVM *vm);
void          script_gc_mark(ScriptValue *v);

/* L2: Hash table operations */
Table        *script_table_new(void);
void          script_table_set(Table *t, const char *key, ScriptValue *val);
ScriptValue  *script_table_get(Table *t, const char *key);
void          script_table_remove(Table *t, const char *key);
void          script_table_free(Table *t);
unsigned int  script_hash(const char *str);

/* L1: Global variable access */
ScriptValue  *script_get_global(ScriptVM *vm, const char *name);
void          script_set_global(ScriptVM *vm, const char *name, ScriptValue *val);

/* L5: Parser */
ScriptAST    *script_parse(const char *source, int *pos);
ScriptValue  *script_parse_line(const char *line);

/* L5: Evaluator */
ScriptValue  *script_eval(ScriptVM *vm, ScriptValue *expr);
ScriptValue  *script_eval_ast(ScriptVM *vm, ScriptAST *ast);
ScriptValue  *script_call_func(ScriptVM *vm, ScriptValue *func, ScriptValue **args, int argc);
int           script_execute(ScriptVM *vm, const char *source);

/* L5: Value operations */
ScriptValue  *script_value_add(ScriptVM *vm, ScriptValue *a, ScriptValue *b);
ScriptValue  *script_value_sub(ScriptVM *vm, ScriptValue *a, ScriptValue *b);
ScriptValue  *script_value_mul(ScriptVM *vm, ScriptValue *a, ScriptValue *b);
ScriptValue  *script_value_div(ScriptVM *vm, ScriptValue *a, ScriptValue *b);
ScriptValue  *script_value_eq(ScriptVM *vm, ScriptValue *a, ScriptValue *b);
ScriptValue  *script_value_lt(ScriptVM *vm, ScriptValue *a, ScriptValue *b);
ScriptValue  *script_value_strcat(ScriptVM *vm, ScriptValue *a, ScriptValue *b);

/* L7: String library */
ScriptValue  *script_strlen(ScriptValue **args, int argc);
ScriptValue  *script_substr(ScriptValue **args, int argc);
ScriptValue  *script_to_upper(ScriptValue **args, int argc);

/* L7: Print/IO */
void          script_print_value(ScriptValue *v);
void          script_fprint_value(FILE *f, ScriptValue *v);

/* L8: Coroutines (basic) */
void          script_coro_init(ScriptVM *co);
int           script_coro_yield(ScriptVM *co, ScriptValue *v);
ScriptValue  *script_coro_resume(ScriptVM *co);

/* Free AST */
void          script_free_ast(ScriptAST *ast);

#endif
