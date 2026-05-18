#ifndef REGEX_LANG_H
#define REGEX_LANG_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
    RN_CHAR,
    RN_CONCAT,
    RN_UNION,
    RN_STAR,
    RN_PLUS,
    RN_QUEST,
    RN_DOT,
    RN_RANGE,
    RN_EMPTY
} RegexNodeType;

typedef struct RegexNode {
    RegexNodeType type;
    char ch;
    char range_start;
    char range_end;
    struct RegexNode *left;
    struct RegexNode *right;
} RegexNode;

typedef struct {
    int state;
    char ch;
    int next1;
    int next2;
} NFAEdge;

typedef struct {
    int start;
    int accept;
} NFAFragment;

typedef struct {
    NFAEdge *edges;
    int edge_count;
    int edge_cap;
    int state_count;
    int start_state;
    int accept_state;
} NFA;

typedef struct {
    char **exprs;
    int count;
} RegexPattern;

RegexNode     *regex_parse(const char *pattern, int *pos);
RegexPattern   regex_compile_to_nfa(RegexNode *root);
bool           regex_match(RegexPattern *pat, const char *text);
int            regex_match_all(RegexPattern *pat, const char *text,
                              int (*callback)(int start, int end, const char *match, void *ctx),
                              void *ctx);
void           regex_print_nfa(RegexPattern *pat);
void           regex_free_node(RegexNode *node);
void           regex_free_pattern(RegexPattern *pat);

NFA            nfa_from_node(RegexNode *node);
bool           nfa_simulate(NFA *nfa, const char *text, int *match_len);
void           nfa_free(NFA *nfa);

#endif
