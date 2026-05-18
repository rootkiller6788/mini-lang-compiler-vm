#include "symtab.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned int hash_string(const char *name) {
    unsigned int hash = 5381;
    int c;
    while ((c = (unsigned char)*name++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % SYMTAB_HASH_SIZE;
}

void symtab_init(SymTab *tab, SymTab *parent) {
    if (!tab) return;
    for (int i = 0; i < SYMTAB_HASH_SIZE; i++) {
        tab->buckets[i] = NULL;
    }
    tab->parent = parent;
    tab->scope_level = parent ? parent->scope_level + 1 : 0;
}

SymTab *symtab_enter_scope(SymTab *tab) {
    SymTab *new_tab = (SymTab *)calloc(1, sizeof(SymTab));
    if (!new_tab) {
        fprintf(stderr, "symtab error: failed to allocate scope\n");
        exit(1);
    }
    symtab_init(new_tab, tab);
    return new_tab;
}

SymTab *symtab_exit_scope(SymTab *tab) {
    if (!tab) return NULL;
    SymTab *parent = tab->parent;
    for (int i = 0; i < SYMTAB_HASH_SIZE; i++) {
        Symbol *sym = tab->buckets[i];
        while (sym) {
            Symbol *next = sym->next;
            free(sym);
            sym = next;
        }
    }
    free(tab);
    return parent;
}

static Symbol *symtab_find_in_scope(SymTab *tab, const char *name) {
    unsigned int idx = hash_string(name);
    Symbol *sym = tab->buckets[idx];
    while (sym) {
        if (strcmp(sym->name, name) == 0) {
            return sym;
        }
        sym = sym->next;
    }
    return NULL;
}

bool symtab_insert(SymTab *tab, const char *name, SymbolType type, int line) {
    if (!tab || !name) return false;

    if (symtab_find_in_scope(tab, name)) {
        fprintf(stderr, "symtab error: symbol '%s' already defined in this scope\n", name);
        return false;
    }

    Symbol *sym = (Symbol *)calloc(1, sizeof(Symbol));
    if (!sym) {
        fprintf(stderr, "symtab error: failed to allocate symbol\n");
        exit(1);
    }

    strncpy(sym->name, name, SYMBOL_NAME_MAX - 1);
    sym->name[SYMBOL_NAME_MAX - 1] = '\0';
    sym->type = type;
    sym->scope_level = tab->scope_level;
    sym->defined = true;
    sym->line = line;
    sym->next = NULL;

    unsigned int idx = hash_string(name);
    sym->next = tab->buckets[idx];
    tab->buckets[idx] = sym;

    return true;
}

Symbol *symtab_lookup(SymTab *tab, const char *name) {
    SymTab *current = tab;
    while (current) {
        Symbol *sym = symtab_find_in_scope(current, name);
        if (sym) {
            return sym;
        }
        current = current->parent;
    }
    return NULL;
}

Symbol *symtab_lookup_current(SymTab *tab, const char *name) {
    if (!tab) return NULL;
    return symtab_find_in_scope(tab, name);
}

void symtab_print(SymTab *tab) {
    if (!tab) return;

    printf("=== Symbol Table (scope level %d) ===\n", tab->scope_level);
    bool has_symbols = false;

    for (int i = 0; i < SYMTAB_HASH_SIZE; i++) {
        Symbol *sym = tab->buckets[i];
        while (sym) {
            has_symbols = true;
            const char *type_str = "UNKNOWN";
            switch (sym->type) {
            case SYM_INT:  type_str = "INT";  break;
            case SYM_FUNC: type_str = "FUNC"; break;
            case SYM_PARAM: type_str = "PARAM"; break;
            }
            printf("  %-20s  type=%-5s  scope=%d  line=%d\n",
                   sym->name, type_str, sym->scope_level, sym->line);
            sym = sym->next;
        }
    }

    if (!has_symbols) {
        printf("  (empty)\n");
    }

    if (tab->parent) {
        symtab_print(tab->parent);
    }
}

void symtab_free(SymTab *tab) {
    if (!tab) return;
    for (int i = 0; i < SYMTAB_HASH_SIZE; i++) {
        Symbol *sym = tab->buckets[i];
        while (sym) {
            Symbol *next = sym->next;
            free(sym);
            sym = next;
        }
    }
    free(tab);
}
