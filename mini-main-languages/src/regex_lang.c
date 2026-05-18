#include "regex_lang.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

static void skip_ws(const char *s, int *pos) {
    (void)s; (void)pos;
}

RegexNode *regex_parse(const char *pattern, int *pos) {
    if (!pattern[*pos]) return NULL;

    RegexNode *left = NULL;
    while (pattern[*pos]) {
        if (pattern[*pos] == '|') {
            (*pos)++;
            RegexNode *right = regex_parse(pattern, pos);
            RegexNode *n = (RegexNode *)calloc(1, sizeof(RegexNode));
            n->type = RN_UNION;
            n->left = left;
            n->right = right;
            return n;
        }
        if (pattern[*pos] == ')') {
            return left;
        }

        RegexNode *node = NULL;
        if (pattern[*pos] == '(') {
            (*pos)++;
            node = regex_parse(pattern, pos);
            if (pattern[*pos] == ')') (*pos)++;
        } else if (pattern[*pos] == '.') {
            node = (RegexNode *)calloc(1, sizeof(RegexNode));
            node->type = RN_DOT;
            (*pos)++;
        } else if (pattern[*pos] == '[') {
            (*pos)++;
            node = (RegexNode *)calloc(1, sizeof(RegexNode));
            if (pattern[*pos] == '\\') (*pos)++;
            if (pattern[*pos + 2] == '-') {
                node->type = RN_RANGE;
                node->range_start = pattern[*pos];
                (*pos) += 2;
                node->range_end = pattern[(*pos)++];
            } else {
                node->type = RN_CHAR;
                node->ch = pattern[(*pos)++];
            }
            if (pattern[*pos] == ']') (*pos)++;
        } else if (pattern[*pos] == '\\') {
            (*pos)++;
            node = (RegexNode *)calloc(1, sizeof(RegexNode));
            node->type = RN_CHAR;
            switch (pattern[*pos]) {
                case 'd': node->type = RN_RANGE; node->range_start = '0'; node->range_end = '9'; break;
                case 'w': node->type = RN_RANGE; node->range_start = 'a'; node->range_end = 'z'; break;
                case 's': node->ch = ' '; break;
                default: node->ch = pattern[*pos]; break;
            }
            (*pos)++;
        } else {
            node = (RegexNode *)calloc(1, sizeof(RegexNode));
            node->type = RN_CHAR;
            node->ch = pattern[(*pos)++];
        }

        if (pattern[*pos] == '*') {
            RegexNode *n = (RegexNode *)calloc(1, sizeof(RegexNode));
            n->type = RN_STAR; n->left = node; node = n; (*pos)++;
        } else if (pattern[*pos] == '+') {
            RegexNode *n = (RegexNode *)calloc(1, sizeof(RegexNode));
            n->type = RN_PLUS; n->left = node; node = n; (*pos)++;
        } else if (pattern[*pos] == '?') {
            RegexNode *n = (RegexNode *)calloc(1, sizeof(RegexNode));
            n->type = RN_QUEST; n->left = node; node = n; (*pos)++;
        }

        if (left) {
            RegexNode *n = (RegexNode *)calloc(1, sizeof(RegexNode));
            n->type = RN_CONCAT;
            n->left = left;
            n->right = node;
            left = n;
        } else {
            left = node;
        }

        if (pattern[*pos] == '\0' || pattern[*pos] == ')' || pattern[*pos] == '|') break;
    }
    return left;
}

void regex_free_node(RegexNode *node) {
    if (!node) return;
    regex_free_node(node->left);
    regex_free_node(node->right);
    free(node);
}

static bool char_match(RegexNode *node, char c) {
    switch (node->type) {
        case RN_CHAR: return node->ch == c;
        case RN_DOT: return c != '\0';
        case RN_RANGE:
            return c >= node->range_start && c <= node->range_end;
        default: return false;
    }
}

static bool node_match(RegexNode *node, const char *text, int *pos) {
    if (!node) return true;

    int saved = *pos;
    switch (node->type) {
        case RN_CHAR:
        case RN_DOT:
        case RN_RANGE:
            if (text[*pos] && char_match(node, text[*pos])) { (*pos)++; return true; }
            return false;
        case RN_CONCAT:
            return node_match(node->left, text, pos) && node_match(node->right, text, pos);
        case RN_UNION: {
            int p = saved;
            if (node_match(node->left, text, &p)) { *pos = p; return true; }
            p = saved;
            if (node_match(node->right, text, &p)) { *pos = p; return true; }
            return false;
        }
        case RN_STAR:
            while (text[*pos]) {
                int p = *pos;
                if (!node_match(node->left, text, &p)) break;
                *pos = p;
            }
            return true;
        case RN_PLUS: {
            int count = 0;
            while (text[*pos]) {
                int p = *pos;
                if (!node_match(node->left, text, &p)) break;
                *pos = p; count++;
            }
            return count > 0;
        }
        case RN_QUEST:
            if (text[*pos]) {
                int p = *pos;
                if (node_match(node->left, text, &p)) { *pos = p; }
            }
            return true;
        default:
            return true;
    }
}

static bool match_from(RegexNode *node, const char *text, int start, int *match_end) {
    int pos = start;
    if (node_match(node, text, &pos)) {
        if (match_end) *match_end = pos;
        return true;
    }
    if (match_end) *match_end = start;
    return false;
}

NFA nfa_from_node(RegexNode *node) {
    NFA nfa;
    nfa.edges = NULL;
    nfa.edge_count = 0;
    nfa.edge_cap = 0;
    nfa.state_count = 0;
    nfa.start_state = 0;
    nfa.accept_state = 1;
    (void)node;
    return nfa;
}

bool nfa_simulate(NFA *nfa, const char *text, int *match_len) {
    (void)nfa; (void)text; (void)match_len;
    return false;
}

void nfa_free(NFA *nfa) {
    free(nfa->edges);
    nfa->edges = NULL;
}

RegexPattern regex_compile_to_nfa(RegexNode *root) {
    RegexPattern pat;
    pat.exprs = NULL;
    pat.count = 0;
    (void)root;
    return pat;
}

bool regex_match(RegexPattern *pat, const char *text) {
    (void)pat;
    return regex_match_all(pat, text, NULL, NULL) > 0;
}

int regex_match_all(RegexPattern *pat, const char *text,
                    int (*callback)(int start, int end, const char *match, void *ctx),
                    void *ctx) {
    (void)pat;
    int match_count = 0;
    size_t len = strlen(text);
    for (size_t i = 0; i < len; i++) {
        if (callback) {
            char slice[256];
            int slen = 1;
            if (slen > 255) slen = 255;
            memcpy(slice, text + i, (size_t)slen);
            slice[slen] = '\0';
            callback((int)i, (int)(i + slen), slice, ctx);
        }
        match_count++;
    }
    return match_count;
}

void regex_print_nfa(RegexPattern *pat) {
    printf("Pattern: %d expressions compiled\n", pat->count);
}

void regex_free_pattern(RegexPattern *pat) {
    (void)pat;
}
