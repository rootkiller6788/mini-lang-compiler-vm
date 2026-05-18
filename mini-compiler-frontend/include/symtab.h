#ifndef SYMTAB_H
#define SYMTAB_H

#include <stdbool.h>

#define SYMTAB_HASH_SIZE 256
#define SYMBOL_NAME_MAX 128

typedef enum {
    SYM_INT,
    SYM_FUNC,
    SYM_PARAM
} SymbolType;

typedef struct Symbol {
    char name[SYMBOL_NAME_MAX];
    SymbolType type;
    int scope_level;
    bool defined;
    int line;
    struct Symbol *next;
} Symbol;

typedef struct SymTab {
    Symbol *buckets[SYMTAB_HASH_SIZE];
    struct SymTab *parent;
    int scope_level;
} SymTab;

void symtab_init(SymTab *tab, SymTab *parent);
SymTab *symtab_enter_scope(SymTab *tab);
SymTab *symtab_exit_scope(SymTab *tab);
bool symtab_insert(SymTab *tab, const char *name, SymbolType type, int line);
Symbol *symtab_lookup(SymTab *tab, const char *name);
Symbol *symtab_lookup_current(SymTab *tab, const char *name);
void symtab_print(SymTab *tab);
void symtab_free(SymTab *tab);

#endif
