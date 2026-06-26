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

/* ─── Symbol Iterator (L3: Engineering Structure) ──────────────────── */

/*
 * Iterator over all symbols in a scope.
 * Walks the hash table buckets, yielding each symbol in insertion order
 * (within each bucket, linked list order).
 *
 * L5: The iterator pattern decouples traversal from data structure,
 *     enabling generic passes (e.g., "print all symbols", "check all types").
 */

void symtab_iter_init(SymTabIter *iter, const SymTab *tab) {
    if (!iter) return;
    iter->tab = tab;
    iter->bucket = 0;
    iter->current = NULL;

    if (tab) {
        /* Find first non-empty bucket */
        while (iter->bucket < SYMTAB_HASH_SIZE && !tab->buckets[iter->bucket])
            iter->bucket++;
        if (iter->bucket < SYMTAB_HASH_SIZE)
            iter->current = tab->buckets[iter->bucket];
    }
}

bool symtab_iter_next(SymTabIter *iter, Symbol **out) {
    if (!iter || !out) return false;

    if (!iter->current) return false;

    *out = iter->current;

    /* Advance to next symbol */
    if (iter->current->next) {
        iter->current = iter->current->next;
    } else {
        iter->bucket++;
        iter->current = NULL;
        while (iter->bucket < SYMTAB_HASH_SIZE) {
            if (iter->tab && iter->tab->buckets[iter->bucket]) {
                iter->current = iter->tab->buckets[iter->bucket];
                break;
            }
            iter->bucket++;
        }
    }

    return true;
}

/* ─── Symbol Table Statistics ──────────────────────────────────────── */

/*
 * Count symbols in a scope and all parent scopes.
 * L6: Debugging symbol resolution issues often requires counting
 *     symbols per scope to detect shadowing or leaks.
 */
int symtab_count_all(const SymTab *tab) {
    if (!tab) return 0;
    int count = 0;
    for (int i = 0; i < SYMTAB_HASH_SIZE; i++) {
        Symbol *sym = tab->buckets[i];
        while (sym) {
            count++;
            sym = sym->next;
        }
    }
    return count + symtab_count_all(tab->parent);
}

int symtab_count_current(const SymTab *tab) {
    if (!tab) return 0;
    int count = 0;
    for (int i = 0; i < SYMTAB_HASH_SIZE; i++) {
        Symbol *sym = tab->buckets[i];
        while (sym) {
            count++;
            sym = sym->next;
        }
    }
    return count;
}

/*
 * Export symbol table as Graphviz DOT format for visualization.
 * Shows scope nesting and symbol relationships.
 *
 * L7: Compiler debugging tools often use DOT export to visualize
 *     internal data structures (e.g., Clang's -ast-dump, LLVM's -view-cfg).
 */
void symtab_export_dot(const SymTab *tab, FILE *fp) {
    if (!tab || !fp) return;

    fprintf(fp, "digraph SymTab {\n");
    fprintf(fp, "  rankdir=TB;\n");
    fprintf(fp, "  node [shape=record, fontname=\"Courier\"];\n");

    /* BFS-style traversal of scope chain */
    typedef struct { const SymTab *scope; int id; const char *label; } ScopeInfo;
    ScopeInfo scopes[64];
    int nscopes = 0;
    int scope_id = 0;

    const SymTab *t = tab;
    while (t && nscopes < 64) {
        /* Count symbols */
        int sym_count = 0;
        for (int i = 0; i < SYMTAB_HASH_SIZE; i++) {
            Symbol *sym = t->buckets[i];
            while (sym) { sym_count++; sym = sym->next; }
        }

        char label_buf[256];
        snprintf(label_buf, sizeof(label_buf),
                 "Scope %d (level=%d)\\n%d symbols",
                 scope_id, t->scope_level, sym_count);

        scopes[nscopes].scope = t;
        scopes[nscopes].id = scope_id;
        scopes[nscopes].label = strdup(label_buf);
        nscopes++;
        scope_id++;
        t = t->parent;
    }

    /* Emit scope nodes */
    for (int i = 0; i < nscopes; i++) {
        fprintf(fp, "  scope%d [label=\"%s\"];\n",
                scopes[i].id, scopes[i].label);

        /* Emit parent edge */
        if (scopes[i].scope->parent) {
            /* Find parent's ID */
            for (int j = 0; j < nscopes; j++) {
                if (scopes[j].scope == scopes[i].scope->parent) {
                    fprintf(fp, "  scope%d -> scope%d [style=dashed];\n",
                            scopes[i].id, scopes[j].id);
                    break;
                }
            }
        }

        /* Emit symbol nodes */
        for (int bi = 0; bi < SYMTAB_HASH_SIZE; bi++) {
            Symbol *sym = scopes[i].scope->buckets[bi];
            while (sym) {
                const char *type_str = "UNK";
                switch (sym->type) {
                case SYM_INT:  type_str = "int";  break;
                case SYM_FUNC: type_str = "func"; break;
                case SYM_PARAM: type_str = "param"; break;
                }
                fprintf(fp, "  sym_%d_%p [label=\"%s\\n(%s)\\nline %d\"];\n",
                        scopes[i].id, (void *)sym, sym->name, type_str, sym->line);
                fprintf(fp, "  scope%d -> sym_%d_%p;\n",
                        scopes[i].id, scopes[i].id, (void *)sym);
                sym = sym->next;
            }
        }
    }

    /* Cleanup */
    for (int i = 0; i < nscopes; i++) free((void *)scopes[i].label);

    fprintf(fp, "}\n");
}
