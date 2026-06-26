#ifndef GRAMMAR_H
#define GRAMMAR_H

#include <stdbool.h>
#include <stddef.h>

/*
 * Formal Grammar Analysis — LL(1) Property Verification
 *
 * L1: Context-free grammar — G = (N, T, P, S) where N = nonterminals,
 *     T = terminals, P = productions, S = start symbol.
 * L4: Chomsky Hierarchy — Type-2 (context-free) grammars; LL(1) subclass
 *     defined by: for each A→α|β, First(α)∩First(β)=∅, and at most one
 *     of α,β derives ε with First(α)∩Follow(A)=∅.
 * L5: First/Follow set computation algorithms (fixed-point iteration).
 * L6: LL(1) predictive parsing table construction.
 *
 * Reference:
 *   - Aho, Ullman "The Theory of Parsing, Translation, and Compiling" (1972)
 *   - Knuth "On the Translation of Languages from Left to Right" (1965)
 *   - Lewis & Stearns "Syntax-Directed Transduction" (1968)
 */

/* ─── Grammar Representation ────────────────────────────────────────── */

#define GRAMMAR_MAX_PRODS 128
#define GRAMMAR_MAX_SYMBOLS 64
#define GRAMMAR_MAX_RHS 16
#define GRAMMAR_NAME_MAX 32

typedef enum {
    SYM_TERMINAL,
    SYM_NONTERMINAL,
    SYM_EPSILON    /* ε, the empty string */
} GrammarSymType;

typedef struct {
    char name[GRAMMAR_NAME_MAX];
    GrammarSymType type;
} GrammarSymbol;

/* A production rule:  LHS → RHS[0] RHS[1] ... RHS[rhs_len-1] */
typedef struct {
    int lhs;              /* index into symbol table */
    int rhs[GRAMMAR_MAX_RHS];
    int rhs_len;
} Production;

/* A context-free grammar */
typedef struct {
    GrammarSymbol symbols[GRAMMAR_MAX_SYMBOLS];
    int nsymbols;

    Production productions[GRAMMAR_MAX_PRODS];
    int nprods;

    int start_symbol;     /* index of start nonterminal */

    /* Derived analysis data (populated by compute functions) */
    bool analyzed;
    int  first_sets[GRAMMAR_MAX_SYMBOLS][GRAMMAR_MAX_SYMBOLS];
    int  first_set_sizes[GRAMMAR_MAX_SYMBOLS];

    int  follow_sets[GRAMMAR_MAX_SYMBOLS][GRAMMAR_MAX_SYMBOLS];
    int  follow_set_sizes[GRAMMAR_MAX_SYMBOLS];

    /* LL(1) parsing table: table[A][t] = production index, or -1 (error) */
    int  ll1_table[GRAMMAR_MAX_SYMBOLS][GRAMMAR_MAX_SYMBOLS];
    bool is_ll1;
} Grammar;

/* ─── Grammar API ───────────────────────────────────────────────────── */

Grammar *grammar_create(void);
void grammar_destroy(Grammar *g);

/* Add a symbol. Returns its index. */
int grammar_add_symbol(Grammar *g, const char *name, GrammarSymType type);

/* Add a production: LHS → RHS (symbol names). RHS is null-terminated array of names. */
int grammar_add_production(Grammar *g, const char *lhs, const char **rhs, int rhs_len);

/* Set the start symbol */
void grammar_set_start(Grammar *g, const char *name);

/* Look up symbol index by name. Returns -1 if not found. */
int grammar_find_symbol(const Grammar *g, const char *name);

/* ─── Predefined Grammar: Mini-C Language ─────────────────────────── */

/*
 * Build the Mini-C grammar (the language accepted by our parser).
 *
 *   Program      → FuncDef Program | ε
 *   FuncDef      → int ident ( Params ) Block
 *   Params       → int ident MoreParams | ε
 *   MoreParams   → , int ident MoreParams | ε
 *   Block        → { Stmts }
 *   Stmts        → Stmt Stmts | ε
 *   Stmt         → if ( Expr ) Stmt | while ( Expr ) Stmt |
 *                  return Expr ; | int ident ; |
 *                  Expr ; | Block
 *   Expr         → Assign
 *   Assign       → LogicalOr = Assign | LogicalOr
 *   LogicalOr    → LogicalAnd || LogicalOr | LogicalAnd
 *   LogicalAnd   → Equality && LogicalAnd | Equality
 *   Equality     → Relational == Equality | Relational != Equality | Relational
 *   Relational   → Additive < Relational | Additive > Relational |
 *                  Additive <= Relational | Additive >= Relational | Additive
 *   Additive     → Multiplicative + Additive | Multiplicative - Additive |
 *                  Multiplicative
 *   Multiplicative → Unary * Multiplicative | Unary / Multiplicative | Unary
 *   Unary        → - Unary | ! Unary | Primary
 *   Primary      → ident ( Args ) | ident | int_lit | string | ( Expr )
 *   Args         → Expr MoreArgs | ε
 *   MoreArgs     → , Expr MoreArgs | ε
 */
Grammar *grammar_build_mini_c(void);

/* ─── First Set Computation ─────────────────────────────────────────── */

/*
 * First(α): The set of terminals that can begin strings derived from α.
 * If α ⇒* ε, then ε ∈ First(α).
 *
 * Algorithm (fixed-point iteration):
 *   1. If X is terminal, First(X) = {X}.
 *   2. If X → ε, add ε to First(X).
 *   3. If X → Y1 Y2 ... Yk:
 *      a. Add First(Y1) − {ε} to First(X).
 *      b. For i from 1 to k-1, if ε ∈ First(Y1)...First(Yi),
 *         add First(Yi+1) − {ε} to First(X).
 *      c. If ε ∈ First(Yj) for all j, add ε to First(X).
 *   4. Repeat step 3 until no changes.
 */
void grammar_compute_first_sets(Grammar *g);

/* ─── Follow Set Computation ────────────────────────────────────────── */

/*
 * Follow(A): The set of terminals that can appear immediately to the
 * right of A in some sentential form.
 *
 * Algorithm:
 *   1. Add $ (EOF) to Follow(S) where S is the start symbol.
 *   2. For each production A → α B β:
 *      a. Add First(β) − {ε} to Follow(B).
 *      b. If ε ∈ First(β) or β is empty, add Follow(A) to Follow(B).
 *   3. Repeat step 2 until no changes.
 */
void grammar_compute_follow_sets(Grammar *g);

/* ─── LL(1) Table Construction ──────────────────────────────────────── */

/*
 * For each production A → α:
 *   1. For each terminal t in First(α), set table[A][t] = production index.
 *   2. If ε ∈ First(α), for each terminal t in Follow(A),
 *      set table[A][t] = production index.
 *
 * The grammar is LL(1) iff every table entry has at most one production.
 */
bool grammar_build_ll1_table(Grammar *g);

/* Print the LL(1) parsing table */
void grammar_print_ll1_table(const Grammar *g);

/* Verify LL(1) property: check for conflicts (multiple productions per cell) */
bool grammar_check_ll1(const Grammar *g);

/* Print First/Follow sets */
void grammar_print_first_sets(const Grammar *g);
void grammar_print_follow_sets(const Grammar *g);
void grammar_print(const Grammar *g);

/* ─── LL(1) Predictive Parser Driver ────────────────────────────────── */

/*
 * A table-driven predictive parser (as opposed to hand-written recursive descent).
 * Uses the computed LL(1) table to parse a token stream.
 *
 * L5: Algorithm — LL(1) predictive parsing is O(n) time, O(|P|·|T|) space.
 */

typedef struct {
    Grammar *grammar;
    /* The parser uses a stack of symbol indices */
    int *stack;
    int stack_top;
    int stack_cap;
} LLParser;

LLParser *ll_parser_create(Grammar *g);
void ll_parser_destroy(LLParser *parser);

/*
 * Parse a token stream using LL(1) table.
 * tokens: array of terminal symbol indices (from the grammar's symbol table)
 * n: number of tokens
 * Returns: true if input accepted, false on syntax error.
 */
bool ll_parser_parse(LLParser *parser, const int *tokens, int n);

#endif /* GRAMMAR_H */
