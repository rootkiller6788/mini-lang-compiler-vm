#ifndef SCRIPT_LANG_H
#define SCRIPT_LANG_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    SV_NIL,
    SV_BOOL,
    SV_INT,
    SV_FLOAT,
    SV_STRING,
    SV_TABLE,
    SV_FUNC
} ScriptType;

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

typedef struct ScriptValue {
    ScriptType type;
    union {
        bool bool_val;
        int int_val;
        double float_val;
        char str_val[256];
    } data;
    struct {
        char name[64];
        char **params;
        int param_count;
        char *body;
    } func;
    Table *table;
    struct ScriptValue *next; /* GC list */
} ScriptValue;

typedef struct {
    ScriptValue **stack;
    int stack_size;
    int stack_top;
} CallStack;

typedef struct {
    Table *globals;
    CallStack call_stack;
} ScriptVM;

void          script_init_vm(ScriptVM *vm);
ScriptValue  *script_new_value(ScriptVM *vm);
ScriptValue  *script_parse_line(const char *line);
ScriptValue  *script_eval(ScriptVM *vm, ScriptValue *expr);
ScriptValue  *script_get_global(ScriptVM *vm, const char *name);
void          script_set_global(ScriptVM *vm, const char *name, ScriptValue *val);
ScriptValue  *script_call_func(ScriptVM *vm, ScriptValue *func, ScriptValue **args, int argc);
Table        *script_table_new(void);
void          script_table_set(Table *t, const char *key, ScriptValue *val);
ScriptValue  *script_table_get(Table *t, const char *key);
void          script_print_value(ScriptValue *v);
void          script_free_vm(ScriptVM *vm);
unsigned int  script_hash(const char *str);

#endif
