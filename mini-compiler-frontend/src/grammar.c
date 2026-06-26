#include "grammar.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Formal Grammar Analysis â€” LL(1) Property Verification
 *
 * L1: Context-Free Grammar G = (N, T, P, S)
 * L4: Chomsky Hierarchy â€” Type-2 grammars; LL(1) is the largest subclass
 *     of CFGs that can be parsed deterministically with one token lookahead.
 * L5: First/Follow set computation via fixed-point iteration.
 *
 * Theorem (Lewis & Stearns 1968): A grammar is LL(1) iff for every
 * production A â†’ Î± | Î²: First(Î±) âˆ© First(Î²) = âˆ…, and if Îµ âˆˆ First(Î²)
 * then First(Î±) âˆ© Follow(A) = âˆ….
 *
 * Reference:
 *   - Knuth "On the Translation of Languages from Left to Right" (1965)
 *   - Rosenkrantz & Stearns "Properties of Deterministic Top-Down Grammars" (1970)
 */

/* â”€â”€â”€ Grammar Lifecycle â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

Grammar *grammar_create(void) {
    Grammar *g = (Grammar *)calloc(1, sizeof(Grammar));
    if (!g) return NULL;
    g->nsymbols = 0;
    g->nprods = 0;
    g->start_symbol = -1;
    g->analyzed = false;
    g->is_ll1 = false;

    /* Add built-in EOF marker as implicit terminal */
    grammar_add_symbol(g, "$", SYM_TERMINAL);

    return g;
}

void grammar_destroy(Grammar *g) {
    free(g);
}

/* â”€â”€â”€ Symbol Management â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

int grammar_add_symbol(Grammar *g, const char *name, GrammarSymType type) {
    if (g->nsymbols >= GRAMMAR_MAX_SYMBOLS) return -1;
    snprintf(g->symbols[g->nsymbols].name, GRAMMAR_NAME_MAX, "%s", name);
    g->symbols[g->nsymbols].type = type;
    return g->nsymbols++;
}

int grammar_find_symbol(const Grammar *g, const char *name) {
    for (int i = 0; i < g->nsymbols; i++) {
        if (strcmp(g->symbols[i].name, name) == 0) return i;
    }
    return -1;
}

static int grammar_get_or_add_symbol(Grammar *g, const char *name,
                                      GrammarSymType default_type) {
    int idx = grammar_find_symbol(g, name);
    if (idx >= 0) return idx;
    return grammar_add_symbol(g, name, default_type);
}

/* â”€â”€â”€ Production Management â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

int grammar_add_production(Grammar *g, const char *lhs, const char **rhs,
                            int rhs_len) {
    if (g->nprods >= GRAMMAR_MAX_PRODS) return -1;
    if (rhs_len > GRAMMAR_MAX_RHS) return -1;

    int lhs_idx = grammar_get_or_add_symbol(g, lhs, SYM_NONTERMINAL);
    if (lhs_idx < 0) return -1;

    Production *p = &g->productions[g->nprods];
    p->lhs = lhs_idx;
    p->rhs_len = rhs_len;

    for (int i = 0; i < rhs_len; i++) {
        if (strcmp(rhs[i], "epsilon") == 0 || strcmp(rhs[i], "e") == 0) {
            /* Îµ production */
            int eps_idx = grammar_get_or_add_symbol(g, "epsilon", SYM_EPSILON);
            p->rhs[0] = eps_idx;
            p->rhs_len = 1;
            break;
        }
        GrammarSymType st = (rhs[i][0] >= 'A' && rhs[i][0] <= 'Z') ||
                             strchr(rhs[i], '_') ? SYM_NONTERMINAL : SYM_TERMINAL;
        int sidx = grammar_get_or_add_symbol(g, rhs[i], st);
        p->rhs[i] = sidx;
    }

    g->analyzed = false;
    return g->nprods++;
}

void grammar_set_start(Grammar *g, const char *name) {
    int idx = grammar_find_symbol(g, name);
    if (idx < 0) idx = grammar_add_symbol(g, name, SYM_NONTERMINAL);
    g->start_symbol = idx;
}

/* â”€â”€â”€ Mini-C Grammar Builder â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€â”€ */

/*
 * Build the Mini-C CFG.  This is a simplified version designed to match
 * the language accepted by our recursive descent parser.
 *
 * Terminals are lowercase/operator strings. Nonterminals are Capitalized.
 * The grammar is structured for LL(1) parsing with left-factoring and
 * elimination of left recursion already applied.
 */
Grammar *grammar_build_mini_c(void) {
    Grammar *g = grammar_create();

    #define T(s)  grammar_get_or_add_symbol(g, s, SYM_TERMINAL)
    #define N(s)  grammar_get_or_add_symbol(g, s, SYM_NONTERMINAL)
    #define ADD(lhs, ...) do { \
        const char *_r[] = { __VA_ARGS__ }; \
        int _n = sizeof(_r) / sizeof(_r[0]); \
        grammar_add_production(g, lhs, _r, _n); \
    } while(0)

    /* Ensure all symbols exist */
    T("int"); T("if"); T("while"); T("return"); T("ident"); T("int_lit");
    T("string"); T("("); T(")"); T("{"); T("}"); T(";"); T(",");
    T("="); T("+"); T("-"); T("*"); T("/");
    T("=="); T("!="); T("<"); T(">"); T("<="); T(">=");
    T("&&"); T("||"); T("!");

    /*
     * Grammar productions (LL(1)-structured, left-recursion eliminated):
     *
     * Program      â†’ FuncDef Program'
     * Program'     â†’ FuncDef Program' | Îµ
     * FuncDef      â†’ int ident ( Params ) Block
     * Params       â†’ int ident Params' | Îµ
     * Params'      â†’ , int ident Params' | Îµ
     * Block        â†’ { StmtList }
     * StmtList     â†’ Stmt StmtList' | Îµ
     * StmtList'    â†’ Stmt StmtList' | Îµ
     * Stmt         â†’ if ( Expr ) Stmt ElsePart
     *              | while ( Expr ) Stmt
     *              | return Expr ;
     *              | int ident ;
     *              | Block
     *              | Expr ;
     * ElsePart     â†’ if Stmt | Îµ  (dangling-else resolved via greedy match)
     * Expr         â†’ Assign
     *
     * Precedence chain (no left recursion, right-recursive for LL(1)):
     *   Assign      â†’ LogicalOr Assign'
     *   Assign'     â†’ = Assign | Îµ
     *   LogicalOr   â†’ LogicalAnd LogicalOr'
     *   LogicalOr'  â†’ || LogicalAnd LogicalOr' | Îµ
     *   LogicalAnd  â†’ Equality LogicalAnd'
     *   LogicalAnd' â†’ && Equality LogicalAnd' | Îµ
     *   Equality    â†’ Relational Equality'
     *   Equality'   â†’ == Relational Equality' | != Relational Equality' | Îµ
     *   Relational  â†’ Additive Relational'
     *   Relational' â†’ < Additive Relational' | > Additive Relational'
     *               | <= Additive Relational' | >= Additive Relational' | Îµ
     *   Additive    â†’ Multiplicative Additive'
     *   Additive'   â†’ + Multiplicative Additive' | - Multiplicative Additive' | Îµ
     *   Multiplicative â†’ Unary Multiplicative'
     *   Multiplicative' â†’ * Unary Multiplicative' | / Unary Multiplicative' | Îµ
     *   Unary       â†’ - Unary | ! Unary | Primary
     *   Primary     â†’ ident Primary' | int_lit | string | ( Expr )
     *   Primary'    â†’ ( Args ) | Îµ   (function call or variable reference)
     *   Args        â†’ Expr Args' | Îµ
     *   Args'       â†’ , Expr Args' | Îµ
     */

    /* Program */
    ADD("Program", "FuncDef", "ProgramP");
    ADD("ProgramP", "FuncDef", "ProgramP");
    ADD("ProgramP", "epsilon");

    /* FuncDef */
    ADD("FuncDef", "int", "ident", "(", "Params", ")", "Block");

    /* Params */
    ADD("Params", "int", "ident", "ParamsP");
    ADD("Params", "epsilon");
    ADD("ParamsP", ",", "int", "ident", "ParamsP");
    ADD("ParamsP", "epsilon");

    /* Block */
    ADD("Block", "{", "StmtList", "}");

    /* StmtList */
    ADD("StmtList", "Stmt", "StmtListP");
    ADD("StmtList", "epsilon");
    ADD("StmtListP", "Stmt", "StmtListP");
    ADD("StmtListP", "epsilon");

    /* Stmt */
    ADD("Stmt", "if", "(", "Expr", ")", "Stmt", "ElsePart");
    ADD("Stmt", "while", "(", "Expr", ")", "Stmt");
    ADD("Stmt", "return", "Expr", ";");
    ADD("Stmt", "int", "ident", ";");
    ADD("Stmt", "Block");
    ADD("Stmt", "Expr", ";");

    /* ElsePart */
    ADD("ElsePart", "if", "Stmt", "ElsePart");
    ADD("ElsePart", "epsilon");

    /* Expr */
    ADD("Expr", "LogicalOr", "AssignP");

    /* AssignP */
    ADD("AssignP", "=", "LogicalOr", "AssignP");
    ADD("AssignP", "epsilon");

    /* LogicalOr */
    ADD("LogicalOr", "LogicalAnd", "LogicalOrP");
    ADD("LogicalOrP", "||", "LogicalAnd", "LogicalOrP");
    ADD("LogicalOrP", "epsilon");

    /* LogicalAnd */
    ADD("LogicalAnd", "Equality", "LogicalAndP");
    ADD("LogicalAndP", "&&", "Equality", "LogicalAndP");
    ADD("LogicalAndP", "epsilon");

    /* Equality */
    ADD("Equality", "Relational", "EqualityP");
    ADD("EqualityP", "==", "Relational", "EqualityP");
    ADD("EqualityP", "!=", "Relational", "EqualityP");
    ADD("EqualityP", "epsilon");

    /* Relational */
    ADD("Relational", "Additive", "RelationalP");
    ADD("RelationalP", "<", "Additive", "RelationalP");
    ADD("RelationalP", ">", "Additive", "RelationalP");
    ADD("RelationalP", "<=", "Additive", "RelationalP");
    ADD("RelationalP", ">=", "Additive", "RelationalP");
    ADD("RelationalP", "epsilon");

    /* Additive */
    ADD("Additive", "Multiplicative", "AdditiveP");
    ADD("AdditiveP", "+", "Multiplicative", "AdditiveP");
    ADD("AdditiveP", "-", "Multiplicative", "AdditiveP");
    ADD("AdditiveP", "epsilon");

    /* Multiplicative */
    ADD("Multiplicative", "Unary", "MultiplicativeP");
    ADD("MultiplicativeP", "*", "Unary", "MultiplicativeP");
    ADD("MultiplicativeP", "/", "Unary", "MultiplicativeP");
    ADD("MultiplicativeP", "epsilon");

    /* Unary */
    ADD("Unary", "-", "Unary");
    ADD("Unary", "!", "Unary");
    ADD("Unary", "Primary");

    /* Primary */
    ADD("Primary", "ident", "PrimaryP");
    ADD("Primary", "int_lit");
    ADD("Primary", "string");
    ADD("Primary", "(", "Expr", ")");

    /* PrimaryP (function call or just variable) */
    ADD("PrimaryP", "(", "Args", ")");
    ADD("PrimaryP", "epsilon");

    /* Args */
    ADD("Args", "Expr", "ArgsP");
    ADD("Args", "epsilon");
    ADD("ArgsP", ",", "Expr", "ArgsP");
    ADD("ArgsP", "epsilon");

    grammar_set_start(g, "Program");

    #undef T
    #undef N
    #undef ADD

    return g;
}


/* ©¤©¤©¤ First Set Computation ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤ */

/*
 * First(alpha): The set of terminals that can begin strings derived from alpha.
 *
 * Algorithm uses fixed-point iteration:
 *   For each terminal t: First(t) = {t}
 *   For each production A -> X1...Xk:
 *     Add First(X1) - {epsilon} to First(A)
 *     If X1 derives epsilon, add First(X2) - {epsilon}, etc.
 *     If all Xi derive epsilon, add epsilon to First(A)
 *   Repeat until no changes.
 *
 * Complexity: O(|P| * |N| * |T|) per iteration, typically converges in
 * O(|N|) iterations. (Knuth 1965)
 */
void grammar_compute_first_sets(Grammar *g) {
    if (!g) return;

    int ns = g->nsymbols;

    /* Initialize: terminals get {self}, nonterminals get {} */
    for (int i = 0; i < ns; i++) {
        g->first_set_sizes[i] = 0;
        if (g->symbols[i].type == SYM_TERMINAL) {
            g->first_sets[i][0] = i;
            g->first_set_sizes[i] = 1;
        }
        if (g->symbols[i].type == SYM_EPSILON) {
            g->first_sets[i][0] = i;
            g->first_set_sizes[i] = 1;
        }
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (int pi = 0; pi < g->nprods; pi++) {
            Production *p = &g->productions[pi];
            int A = p->lhs;
            bool all_derive_epsilon = true;

            for (int ri = 0; ri < p->rhs_len; ri++) {
                int X = p->rhs[ri];
                bool epsilon_in_X = false;

                /* Add First(X) - {epsilon} to First(A) */
                for (int fi = 0; fi < g->first_set_sizes[X]; fi++) {
                    int sym = g->first_sets[X][fi];
                    if (g->symbols[sym].type == SYM_EPSILON) {
                        epsilon_in_X = true;
                        continue;
                    }
                    bool found = false;
                    for (int ai = 0; ai < g->first_set_sizes[A]; ai++) {
                        if (g->first_sets[A][ai] == sym) { found = true; break; }
                    }
                    if (!found) {
                        if (g->first_set_sizes[A] < GRAMMAR_MAX_SYMBOLS) {
                            g->first_sets[A][g->first_set_sizes[A]++] = sym;
                            changed = true;
                        }
                    }
                }

                if (!epsilon_in_X) {
                    all_derive_epsilon = false;
                    break;
                }
            }

            /* If all RHS symbols derive epsilon, add epsilon to First(A) */
            if (all_derive_epsilon) {
                int eps_idx = grammar_find_symbol(g, "epsilon");
                if (eps_idx < 0) eps_idx = grammar_find_symbol(g, "e");
                if (eps_idx >= 0) {
                    bool found = false;
                    for (int ai = 0; ai < g->first_set_sizes[A]; ai++) {
                        if (g->first_sets[A][ai] == eps_idx) { found = true; break; }
                    }
                    if (!found && g->first_set_sizes[A] < GRAMMAR_MAX_SYMBOLS) {
                        g->first_sets[A][g->first_set_sizes[A]++] = eps_idx;
                        changed = true;
                    }
                }
            }
        }
    }
}

/*
 * Follow(A): Terminals that can appear immediately to the right of A.
 *
 * Algorithm:
 *   1. Add EOF to Follow(S)
 *   2. For A -> alpha B beta: add First(beta)-{epsilon} to Follow(B)
 *   3. If epsilon in First(beta): add Follow(A) to Follow(B)
 *   4. Repeat until fixed point
 */
void grammar_compute_follow_sets(Grammar *g) {
    if (!g || g->start_symbol < 0) return;

    int ns = g->nsymbols;
    int eof_idx = grammar_find_symbol(g, "$");
    int eps_idx = grammar_find_symbol(g, "epsilon");
    if (eps_idx < 0) eps_idx = grammar_find_symbol(g, "e");

    for (int i = 0; i < ns; i++) g->follow_set_sizes[i] = 0;

    if (eof_idx >= 0 && g->start_symbol >= 0) {
        g->follow_sets[g->start_symbol][0] = eof_idx;
        g->follow_set_sizes[g->start_symbol] = 1;
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (int pi = 0; pi < g->nprods; pi++) {
            Production *p = &g->productions[pi];
            int A = p->lhs;

            for (int ri = 0; ri < p->rhs_len; ri++) {
                int B = p->rhs[ri];
                if (g->symbols[B].type != SYM_NONTERMINAL) continue;

                bool beta_all_epsilon = true;
                for (int rj = ri + 1; rj < p->rhs_len; rj++) {
                    int X = p->rhs[rj];
                    bool X_has_epsilon = false;
                    for (int fi = 0; fi < g->first_set_sizes[X]; fi++) {
                        int sym = g->first_sets[X][fi];
                        if (g->symbols[sym].type == SYM_EPSILON) {
                            X_has_epsilon = true;
                            continue;
                        }
                        bool found = false;
                        for (int bi = 0; bi < g->follow_set_sizes[B]; bi++) {
                            if (g->follow_sets[B][bi] == sym) { found = true; break; }
                        }
                        if (!found && g->follow_set_sizes[B] < GRAMMAR_MAX_SYMBOLS) {
                            g->follow_sets[B][g->follow_set_sizes[B]++] = sym;
                            changed = true;
                        }
                    }
                    if (!X_has_epsilon) { beta_all_epsilon = false; break; }
                }

                if (beta_all_epsilon) {
                    for (int ai = 0; ai < g->follow_set_sizes[A]; ai++) {
                        int sym = g->follow_sets[A][ai];
                        bool found = false;
                        for (int bi = 0; bi < g->follow_set_sizes[B]; bi++) {
                            if (g->follow_sets[B][bi] == sym) { found = true; break; }
                        }
                        if (!found && g->follow_set_sizes[B] < GRAMMAR_MAX_SYMBOLS) {
                            g->follow_sets[B][g->follow_set_sizes[B]++] = sym;
                            changed = true;
                        }
                    }
                }
            }
        }
    }
}

/*
 * LL(1) Parsing Table Construction.
 *
 * For each production A -> alpha:
 *   For each terminal t in First(alpha): table[A][t] = production
 *   If epsilon in First(alpha): for each t in Follow(A): table[A][t] = production
 *
 * Theorem (Lewis & Stearns 1968): A CFG is LL(1) iff the LL(1) parsing
 * table has no multiply-defined entries.
 */
bool grammar_build_ll1_table(Grammar *g) {
    if (!g) return false;

    if (!g->analyzed) {
        grammar_compute_first_sets(g);
        grammar_compute_follow_sets(g);
        g->analyzed = true;
    }

    int ns = g->nsymbols;
    int eps_idx = grammar_find_symbol(g, "epsilon");
    if (eps_idx < 0) eps_idx = grammar_find_symbol(g, "e");

    for (int i = 0; i < ns; i++)
        for (int j = 0; j < ns; j++)
            g->ll1_table[i][j] = -1;

    g->is_ll1 = true;

    for (int pi = 0; pi < g->nprods; pi++) {
        Production *p = &g->productions[pi];
        int A = p->lhs;

        int rhs_first[GRAMMAR_MAX_SYMBOLS];
        int rhs_first_size = 0;
        bool all_derive_epsilon = (p->rhs_len == 0);

        for (int ri = 0; ri < p->rhs_len; ri++) {
            int X = p->rhs[ri];
            bool X_epsilon = false;
            for (int fi = 0; fi < g->first_set_sizes[X]; fi++) {
                int sym = g->first_sets[X][fi];
                if (g->symbols[sym].type == SYM_EPSILON) {
                    X_epsilon = true;
                    continue;
                }
                bool found = false;
                for (int rfi = 0; rfi < rhs_first_size; rfi++) {
                    if (rhs_first[rfi] == sym) { found = true; break; }
                }
                if (!found && rhs_first_size < GRAMMAR_MAX_SYMBOLS)
                    rhs_first[rhs_first_size++] = sym;
            }
            if (!X_epsilon) { all_derive_epsilon = false; break; }
        }

        if (all_derive_epsilon && eps_idx >= 0) {
            bool has_eps = false;
            for (int rfi = 0; rfi < rhs_first_size; rfi++)
                if (rhs_first[rfi] == eps_idx) has_eps = true;
            if (!has_eps && rhs_first_size < GRAMMAR_MAX_SYMBOLS)
                rhs_first[rhs_first_size++] = eps_idx;
        }

        for (int fi = 0; fi < rhs_first_size; fi++) {
            int t = rhs_first[fi];
            if (g->symbols[t].type != SYM_TERMINAL) continue;
            if (t == eps_idx) continue;

            if (g->ll1_table[A][t] != -1 && g->ll1_table[A][t] != pi) {
                g->is_ll1 = false;
            }
            g->ll1_table[A][t] = pi;
        }

        if (all_derive_epsilon) {
            for (int fi = 0; fi < g->follow_set_sizes[A]; fi++) {
                int t = g->follow_sets[A][fi];
                if (g->symbols[t].type != SYM_TERMINAL) continue;

                if (g->ll1_table[A][t] != -1 && g->ll1_table[A][t] != pi) {
                    g->is_ll1 = false;
                }
                g->ll1_table[A][t] = pi;
            }
        }
    }

    return g->is_ll1;
}

bool grammar_check_ll1(const Grammar *g) {
    if (!g) return false;
    return g->is_ll1;
}


/* ©¤©¤©¤ Debug Print Functions ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤ */

void grammar_print(const Grammar *g) {
    if (!g) return;
    printf("=== Grammar (%d symbols, %d productions) ===\n",
           g->nsymbols, g->nprods);
    printf("Nonterminals: ");
    for (int i = 0; i < g->nsymbols; i++)
        if (g->symbols[i].type == SYM_NONTERMINAL)
            printf("%s ", g->symbols[i].name);
    printf("\nTerminals: ");
    for (int i = 0; i < g->nsymbols; i++)
        if (g->symbols[i].type == SYM_TERMINAL)
            printf("%s ", g->symbols[i].name);
    printf("\nStart: %s\n",
           g->start_symbol >= 0 ? g->symbols[g->start_symbol].name : "?");
    printf("Productions:\n");
    for (int pi = 0; pi < g->nprods; pi++) {
        Production *p = &g->productions[pi];
        printf("  %d: %s ->", pi, g->symbols[p->lhs].name);
        for (int ri = 0; ri < p->rhs_len; ri++)
            printf(" %s", g->symbols[p->rhs[ri]].name);
        printf("\n");
    }
}

void grammar_print_first_sets(const Grammar *g) {
    if (!g) return;
    printf("=== First Sets ===\n");
    for (int i = 0; i < g->nsymbols; i++) {
        if (g->symbols[i].type == SYM_TERMINAL) continue;
        printf("  First(%s) = {", g->symbols[i].name);
        for (int fi = 0; fi < g->first_set_sizes[i]; fi++)
            printf("%s%s", fi ? ", " : "",
                   g->symbols[g->first_sets[i][fi]].name);
        printf("}\n");
    }
}

void grammar_print_follow_sets(const Grammar *g) {
    if (!g) return;
    printf("=== Follow Sets ===\n");
    for (int i = 0; i < g->nsymbols; i++) {
        if (g->symbols[i].type != SYM_NONTERMINAL) continue;
        printf("  Follow(%s) = {", g->symbols[i].name);
        for (int fi = 0; fi < g->follow_set_sizes[i]; fi++)
            printf("%s%s", fi ? ", " : "",
                   g->symbols[g->follow_sets[i][fi]].name);
        printf("}\n");
    }
}

void grammar_print_ll1_table(const Grammar *g) {
    if (!g) return;
    printf("=== LL(1) Parsing Table ===\n");

    int terms[GRAMMAR_MAX_SYMBOLS];
    int nterms = 0;
    for (int i = 0; i < g->nsymbols; i++)
        if (g->symbols[i].type == SYM_TERMINAL)
            terms[nterms++] = i;

    printf("%-16s", "NT \ T");
    for (int ti = 0; ti < nterms; ti++)
        printf("%-12s", g->symbols[terms[ti]].name);
    printf("\n");

    for (int i = 0; i < g->nsymbols; i++) {
        if (g->symbols[i].type != SYM_NONTERMINAL) continue;
        printf("%-16s", g->symbols[i].name);
        for (int ti = 0; ti < nterms; ti++) {
            int prod = g->ll1_table[i][terms[ti]];
            if (prod >= 0)
                printf("%-12d", prod);
            else
                printf("%-12s", "-");
        }
        printf("\n");
    }
}


/* ©¤©¤©¤ LL(1) Predictive Parser Driver ©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤©¤ */
/* L5: Table-driven predictive parsing is O(n) time and O(|P|*|T|) space. */

LLParser *ll_parser_create(Grammar *g) {
    if (!g) return NULL;
    LLParser *parser = (LLParser *)calloc(1, sizeof(LLParser));
    parser->grammar = g;
    parser->stack_cap = 256;
    parser->stack = (int *)malloc(parser->stack_cap * sizeof(int));
    parser->stack_top = 0;
    return parser;
}

void ll_parser_destroy(LLParser *parser) {
    if (!parser) return;
    free(parser->stack);
    free(parser);
}

bool ll_parser_parse(LLParser *parser, const int *tokens, int n) {
    if (!parser || !tokens || n < 0) return false;
    Grammar *g = parser->grammar;

    int eof_idx = grammar_find_symbol(g, "$");
    int eps_idx = grammar_find_symbol(g, "epsilon");
    if (eps_idx < 0) eps_idx = grammar_find_symbol(g, "e");

    /* Initialize: push EOF then start symbol */
    parser->stack_top = 0;
    parser->stack[parser->stack_top++] = eof_idx;
    if (parser->stack_top < parser->stack_cap)
        parser->stack[parser->stack_top++] = g->start_symbol;

    int ip = 0;
    int *input = (int *)malloc((n + 1) * sizeof(int));
    memcpy(input, tokens, n * sizeof(int));
    input[n] = eof_idx;
    int input_len = n + 1;

    while (parser->stack_top > 0) {
        int X = parser->stack[--parser->stack_top];
        int a = (ip < input_len) ? input[ip] : eof_idx;

        if (X == a) {
            ip++;
            if (a == eof_idx) {
                free(input);
                return true;  /* accept */
            }
        } else if (g->symbols[X].type == SYM_TERMINAL) {
            fprintf(stderr, "LL parse error: expected '%s', got '%s'\n",
                    g->symbols[X].name, g->symbols[a].name);
            free(input);
            return false;
        } else {
            int prod = g->ll1_table[X][a];
            if (prod < 0) {
                fprintf(stderr, "LL parse error: no rule for %s on '%s'\n",
                        g->symbols[X].name, g->symbols[a].name);
                free(input);
                return false;
            }

            Production *p = &g->productions[prod];
            for (int ri = p->rhs_len - 1; ri >= 0; ri--) {
                int sym = p->rhs[ri];
                if (sym == eps_idx) continue;
                if (parser->stack_top >= parser->stack_cap) {
                    parser->stack_cap *= 2;
                    parser->stack = (int *)realloc(parser->stack,
                                                    parser->stack_cap * sizeof(int));
                }
                parser->stack[parser->stack_top++] = sym;
            }
        }
    }

    free(input);
    return false;
}
