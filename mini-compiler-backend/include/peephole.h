#ifndef PEEPHOLE_H
#define PEEPHOLE_H

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "instruction_selection.h"

#define MAX_PEEPHOLE_RULES   64
#define PEEPHOLE_WINDOW_SIZE 4

typedef struct {
    char pattern_str[128];
    char replacement_str[128];
    size_t pattern_len;
    size_t repl_len;
    InstructionOp pattern_ops[PEEPHOLE_WINDOW_SIZE];
    char pattern_dsts[PEEPHOLE_WINDOW_SIZE][32];
    char pattern_src1s[PEEPHOLE_WINDOW_SIZE][32];
    char pattern_src2s[PEEPHOLE_WINDOW_SIZE][32];
    InstructionOp repl_ops[PEEPHOLE_WINDOW_SIZE];
    char repl_dsts[PEEPHOLE_WINDOW_SIZE][32];
    char repl_src1s[PEEPHOLE_WINDOW_SIZE][32];
    char repl_src2s[PEEPHOLE_WINDOW_SIZE][32];
    size_t applied_count;
} PeepholeRule;

typedef struct {
    PeepholeRule rules[MAX_PEEPHOLE_RULES];
    size_t rule_count;
    bool changed;
} PeepholeContext;

void peephole_init_rules(PeepholeContext *ctx);
void peephole_optimize(PeepholeContext *ctx, InstructionList *ilist);
void peephole_print_replacements(PeepholeContext *ctx, FILE *out);
void peephole_add_rule(PeepholeContext *ctx, const char *pattern, const char *replacement);

#endif
