#ifndef REGEX_LANG_H
#define REGEX_LANG_H

#include <stdbool.h>
#include <stddef.h>

/* ── L1: Core Definitions ──────────────────────────────────────────
 * Regex AST node types covering Kleene's regular expression algebra.
 * Theorem (Kleene, 1956): Regular expressions ≡ Finite Automata.
 */
typedef enum {
    RN_CHAR,      /* literal character c */
    RN_CONCAT,    /* e1 e2 — concatenation */
    RN_UNION,     /* e1 | e2 — alternation */
    RN_STAR,      /* e* — Kleene star (zero or more) */
    RN_PLUS,      /* e+ — one or more (ee*) */
    RN_QUEST,     /* e? — zero or one (e|ε) */
    RN_DOT,       /* . — any character except \0 */
    RN_RANGE,     /* [a-z] — character class range */
    RN_ANCHOR_BOL,/* ^ — beginning of line (zero-width) */
    RN_ANCHOR_EOL,/* $ — end of line (zero-width) */
    RN_CAPTURE,   /* (e) — capturing group */
    RN_BACKREF,   /* \1..\9 — backreference to capture group */
    RN_EMPTY      /* ε — empty string */
} RegexNodeType;

typedef struct RegexNode {
    RegexNodeType type;
    char ch;                /* literal character for RN_CHAR */
    char range_start;       /* character range [start-end] */
    char range_end;
    int  capture_index;     /* capture group number (1-indexed) */
    int  backref_index;     /* backreference number */
    struct RegexNode *left;
    struct RegexNode *right;
} RegexNode;

/* ── L3: NFA Engineering Structure (Thompson Construction) ─────────
 * NFA state: a set of ε-transitions + character transitions.
 * Thompson (1968) CACM: "Regular Expression Search Algorithm."
 */
typedef struct {
    int state;       /* source state */
    char ch;         /* character to match (0 for ε-transition) */
    int next1;       /* first target state */
    int next2;       /* second target state (for ε-branches, -1 if unused) */
} NFAEdge;

typedef struct {
    int start;       /* start state of fragment */
    int accept;      /* accepting state of fragment */
} NFAFragment;

#define NFA_MAX_STATES  1024
#define NFA_MAX_EDGES   4096
#define NFA_MAX_CAPTURES 32

typedef struct {
    NFAEdge edges[NFA_MAX_EDGES];
    int edge_count;
    int state_count;
    int start_state;
    int accept_state;
    /* Capture group state recording */
    int capture_start[NFA_MAX_CAPTURES];
    int capture_end[NFA_MAX_CAPTURES];
    int num_captures;
} NFA;

/* ── L5: DFA via Subset Construction ──────────────────────────────
 * Rabin & Scott (1959): NFA → DFA conversion via powerset construction.
 */
#define DFA_MAX_STATES  256
#define DFA_MAX_TRANS   256

typedef struct {
    int  transitions[DFA_MAX_STATES][DFA_MAX_TRANS]; /* state × input → next state (-1 = none) */
    bool accept[DFA_MAX_STATES];
    int  num_states;
    int  start_state;
    unsigned char alphabet[DFA_MAX_TRANS]; /* registered input chars */
    int  num_symbols;
} DFA;

/* ── L7: Match Result ───────────────────────────────────────────── */
typedef struct {
    int start;       /* start position in text */
    int end;         /* end position (exclusive) */
    char captured[256]; /* matched substring */
} RegexMatch;

/* High-level compiled pattern (NFA + DFA cache) */
typedef struct RegexPattern {
    NFA  nfa;
    DFA  dfa;
    bool dfa_built;
    char *regex_source;
    int  num_captures;
} RegexPattern;

/* ── API Declarations ───────────────────────────────────────────── */

/* L1: Parse regex string into AST */
RegexNode     *regex_parse(const char *pattern, int *pos);

/* L3: Compile AST to NFA (Thompson construction) */
RegexPattern  *regex_compile(const char *pattern);

/* L3: Compile AST to NFA directly */
NFA            regex_build_nfa(RegexNode *root);

/* L5: NFA simulation (on-the-fly subset construction) */
bool           regex_match_nfa(RegexPattern *pat, const char *text);

/* L5: NFA simulation returning match length */
bool           regex_match_nfa_len(RegexPattern *pat, const char *text, int *match_len);

/* L5: DFA conversion + matching (faster for repeated use) */
DFA            regex_nfa_to_dfa(const NFA *nfa);
bool           regex_match_dfa(const DFA *dfa, const char *text);

/* L5: DFA minimization (Hopcroft, 1971: O(n log n)) */
void           regex_dfa_minimize(DFA *dfa);

/* L7: Match all occurrences in text */
int            regex_match_all(RegexPattern *pat, const char *text,
                               int (*callback)(int start, int end, const char *match, void *ctx),
                               void *ctx);

/* L7: Find first match */
bool           regex_search(RegexPattern *pat, const char *text, RegexMatch *match);

/* L7: Full string match (anchored at both ends) */
bool           regex_full_match(RegexPattern *pat, const char *text);

/* L4: Brzozowski derivative — symbolic regex differentiation */
RegexNode     *regex_derivative(RegexNode *node, char c);

/* L4: Check if regex accepts empty string (nullable) */
bool           regex_nullable(const RegexNode *node);

/* Utility */
void           regex_print_nfa(const RegexPattern *pat);
void           regex_print_dfa(const DFA *dfa);
void           regex_print_ast(const RegexNode *node, int indent);
void           regex_free_node(RegexNode *node);
void           regex_free_pattern(RegexPattern *pat);

#endif
