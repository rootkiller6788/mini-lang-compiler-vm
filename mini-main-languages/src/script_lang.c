#include "script_lang.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

unsigned int script_hash(const char *str) {
    unsigned int h = 5381;
    while (*str) h = ((h << 5) + h) + (unsigned char)(*str++);
    return h;
}

Table *script_table_new(void) {
    Table *t = (Table *)calloc(1, sizeof(Table));
    t->size = 64;
    t->count = 0;
    t->buckets = (TableEntry **)calloc((size_t)t->size, sizeof(TableEntry *));
    return t;
}

void script_table_set(Table *t, const char *key, ScriptValue *val) {
    unsigned int h = script_hash(key) % (unsigned int)t->size;
    TableEntry *e = t->buckets[h];
    while (e) {
        if (strcmp(e->key, key) == 0) { e->value = val; return; }
        e = e->next;
    }
    e = (TableEntry *)calloc(1, sizeof(TableEntry));
    strcpy(e->key, key);
    e->value = val;
    e->next = t->buckets[h];
    t->buckets[h] = e;
    t->count++;
}

ScriptValue *script_table_get(Table *t, const char *key) {
    unsigned int h = script_hash(key) % (unsigned int)t->size;
    for (TableEntry *e = t->buckets[h]; e; e = e->next)
        if (strcmp(e->key, key) == 0) return e->value;
    return NULL;
}

void script_init_vm(ScriptVM *vm) {
    vm->globals = script_table_new();
    vm->call_stack.stack = NULL;
    vm->call_stack.stack_size = 0;
    vm->call_stack.stack_top = 0;
}

ScriptValue *script_new_value(ScriptVM *vm) {
    (void)vm;
    return (ScriptValue *)calloc(1, sizeof(ScriptValue));
}

ScriptValue *script_get_global(ScriptVM *vm, const char *name) {
    return script_table_get(vm->globals, name);
}

void script_set_global(ScriptVM *vm, const char *name, ScriptValue *val) {
    script_table_set(vm->globals, name, val);
}

static void skip_ws(const char *s, int *pos) {
    while (s[*pos] && isspace((unsigned char)s[*pos])) (*pos)++;
}

static char *read_ident(const char *s, int *pos, char *buf) {
    int i = 0; skip_ws(s, pos);
    while (isalnum((unsigned char)s[*pos]) || s[*pos] == '_') {
        if (i < 127) buf[i++] = s[*pos]; (*pos)++;
    }
    buf[i] = '\0';
    return (i > 0) ? buf : NULL;
}

static char *read_string(const char *s, int *pos, char *buf) {
    skip_ws(s, pos);
    if (s[*pos] != '"') return NULL;
    (*pos)++; int i = 0;
    while (s[*pos] && s[*pos] != '"' && i < 255) {
        if (s[*pos] == '\\') { (*pos)++; }
        buf[i++] = s[(*pos)++];
    }
    if (s[*pos] == '"') (*pos)++;
    buf[i] = '\0'; return buf;
}

static double read_number(const char *s, int *pos) {
    skip_ws(s, pos);
    int start = *pos;
    while (isdigit((unsigned char)s[*pos]) || s[*pos] == '.') (*pos)++;
    char buf[64]; int len = *pos - start;
    if (len >= 64) len = 63;
    memcpy(buf, s + start, (size_t)len); buf[len] = '\0';
    return atof(buf);
}

ScriptValue *script_parse_line(const char *line) {
    int pos = 0;
    skip_ws(line, &pos);
    if (!line[pos]) return NULL;

    char buf[128];
    char *ident = read_ident(line, &pos, buf);
    ScriptValue *v = script_new_value(NULL);
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
                            strcpy(svv->data.str_val, sv);
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
                free(ident);
                return v;
            }
            if (isdigit((unsigned char)line[pos]) || line[pos] == '"') {
                if (line[pos] == '"') {
                    v->type = SV_STRING;
                    char sv[256]; read_string(line, &pos, sv);
                    strcpy(v->data.str_val, sv);
                } else {
                    double dn = read_number(line, &pos);
                    v->type = (dn == (int)dn) ? SV_INT : SV_FLOAT;
                    if (v->type == SV_INT) v->data.int_val = (int)dn;
                    else v->data.float_val = dn;
                }
            }
        }
        free(v);
        return NULL;
    }

    if (isdigit((unsigned char)line[pos])) {
        double dn = read_number(line, &pos);
        v->type = (dn == (int)dn) ? SV_INT : SV_FLOAT;
        if (v->type == SV_INT) v->data.int_val = (int)dn;
        else v->data.float_val = dn;
        return v;
    }

    if (line[pos] == '"') {
        v->type = SV_STRING;
        char sv[256]; read_string(line, &pos, sv);
        strcpy(v->data.str_val, sv);
        return v;
    }

    free(v);
    return NULL;
}

ScriptValue *script_eval(ScriptVM *vm, ScriptValue *expr) {
    (void)vm;
    return expr;
}

ScriptValue *script_call_func(ScriptVM *vm, ScriptValue *func, ScriptValue **args, int argc) {
    (void)vm; (void)func; (void)args; (void)argc;
    return script_new_value(vm);
}

void script_print_value(ScriptValue *v) {
    if (!v) { printf("nil"); return; }
    switch (v->type) {
        case SV_NIL:    printf("nil"); break;
        case SV_BOOL:   printf(v->data.bool_val ? "true" : "false"); break;
        case SV_INT:    printf("%d", v->data.int_val); break;
        case SV_FLOAT:  printf("%g", v->data.float_val); break;
        case SV_STRING: printf("\"%s\"", v->data.str_val); break;
        case SV_TABLE:
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
        case SV_FUNC: printf("<func:%s>", v->func.name); break;
    }
}

void script_free_vm(ScriptVM *vm) {
    if (vm->globals) {
        for (int i = 0; i < vm->globals->size; i++) {
            TableEntry *e = vm->globals->buckets[i];
            while (e) { TableEntry *n = e->next; free(e->value); free(e); e = n; }
        }
        free(vm->globals->buckets);
        free(vm->globals);
    }
    free(vm->call_stack.stack);
}
