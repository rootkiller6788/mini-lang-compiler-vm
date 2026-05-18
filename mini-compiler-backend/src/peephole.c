#define _CRT_SECURE_NO_WARNINGS
#include "peephole.h"

void peephole_init_rules(PeepholeContext *ctx) {
    memset(ctx, 0, sizeof(PeepholeContext));
    ctx->rule_count = 0;
    ctx->changed = false;

    peephole_add_rule(ctx, "mov r,r -> nop", "nop");
    peephole_add_rule(ctx, "push r; pop r -> nop", "nop");
    peephole_add_rule(ctx, "add r,0 -> nop", "nop");
    peephole_add_rule(ctx, "sub r,0 -> nop", "nop");
    peephole_add_rule(ctx, "mul r,2 -> shl r,1", "shl r,1");
    peephole_add_rule(ctx, "mul r,0 -> xor r,r", "xor r,r");
    peephole_add_rule(ctx, "cmp a,b; jmp L -> jmp L", "jmp L");
    peephole_add_rule(ctx, "mov r0,0; add r1,r0 -> mov r1,0", "mov r1,0");
    peephole_add_rule(ctx, "push r; pop s -> mov s,r", "mov s,r");
    peephole_add_rule(ctx, "load r,[r] -> nop", "nop");
    peephole_add_rule(ctx, "store [r],r -> nop", "nop");
    peephole_add_rule(ctx, "add r,1 -> inc r", "inc r");
    peephole_add_rule(ctx, "sub r,1 -> dec r", "dec r");
    peephole_add_rule(ctx, "xor r,r -> mov r,0", "mov r,0");
    peephole_add_rule(ctx, "mov r,imm; mov s,r -> mov s,imm", "mov s,imm");
}

static void parse_instr(const char *s, InstructionOp *op, char *dst,
                        char *src1, char *src2) {
    char opstr[32] = {0};
    char rest[128] = {0};
    sscanf(s, "%31s %127[^\n]", opstr, rest);
    *op = ISEL_NOP;
    if (strcmp(opstr, "mov") == 0)  *op = ISEL_MOV;
    if (strcmp(opstr, "add") == 0)  *op = ISEL_ADD;
    if (strcmp(opstr, "sub") == 0)  *op = ISEL_SUB;
    if (strcmp(opstr, "mul") == 0)  *op = ISEL_MUL;
    if (strcmp(opstr, "div") == 0)  *op = ISEL_DIV;
    if (strcmp(opstr, "load") == 0) *op = ISEL_LOAD;
    if (strcmp(opstr, "store") == 0) *op = ISEL_STORE;
    if (strcmp(opstr, "push") == 0) *op = ISEL_PUSH;
    if (strcmp(opstr, "pop") == 0)  *op = ISEL_POP;
    if (strcmp(opstr, "ret") == 0)  *op = ISEL_RET;
    if (strcmp(opstr, "call") == 0) *op = ISEL_CALL;
    if (strcmp(opstr, "cmp") == 0)  *op = ISEL_CMP;
    if (strcmp(opstr, "jmp") == 0)  *op = ISEL_JMP;
    if (strcmp(opstr, "je") == 0)   *op = ISEL_JE;
    if (strcmp(opstr, "jne") == 0)  *op = ISEL_JNE;
    if (strcmp(opstr, "jl") == 0)   *op = ISEL_JL;
    if (strcmp(opstr, "inc") == 0)  *op = ISEL_ADD;
    if (strcmp(opstr, "dec") == 0)  *op = ISEL_SUB;
    if (strcmp(opstr, "shl") == 0)  *op = ISEL_SHL;
    if (strcmp(opstr, "xor") == 0)  *op = ISEL_XOR;
    if (strcmp(opstr, "lea") == 0)  *op = ISEL_LEA;
    if (strcmp(opstr, "nop") == 0)  *op = ISEL_NOP;

    char *comma = strchr(rest, ',');
    if (comma) {
        *comma = '\0';
        char *dpart = rest;
        while (*dpart == ' ') dpart++;
        snprintf(dst, 32, "%s", dpart);
        char *spart = comma + 1;
        while (*spart == ' ') spart++;
        char *comma2 = strchr(spart, ',');
        if (comma2) {
            *comma2 = '\0';
            snprintf(src1, 32, "%s", spart);
            char *s2 = comma2 + 1;
            while (*s2 == ' ') s2++;
            snprintf(src2, 32, "%s", s2);
        } else {
            snprintf(src1, 32, "%s", spart);
            src2[0] = '\0';
        }
    } else {
        snprintf(dst, 32, "%s", rest);
        src1[0] = '\0';
        src2[0] = '\0';
    }
}

void peephole_add_rule(PeepholeContext *ctx, const char *pattern,
                       const char *replacement) {
    if (ctx->rule_count >= MAX_PEEPHOLE_RULES) return;
    PeepholeRule *r = &ctx->rules[ctx->rule_count++];
    memset(r, 0, sizeof(PeepholeRule));
    snprintf(r->pattern_str, sizeof(r->pattern_str), "%s", pattern);
    snprintf(r->replacement_str, sizeof(r->replacement_str), "%s", replacement);

    char pat_copy[256];
    snprintf(pat_copy, sizeof(pat_copy), "%s", pattern);
    char *token = strtok(pat_copy, ";");
    while (token && r->pattern_len < PEEPHOLE_WINDOW_SIZE) {
        while (*token == ' ') token++;
        parse_instr(token, &r->pattern_ops[r->pattern_len],
                    r->pattern_dsts[r->pattern_len],
                    r->pattern_src1s[r->pattern_len],
                    r->pattern_src2s[r->pattern_len]);
        r->pattern_len++;
        token = strtok(NULL, ";");
    }
    token = strtok(r->replacement_str, ";");
    while (token && r->repl_len < PEEPHOLE_WINDOW_SIZE) {
        while (*token == ' ') token++;
        parse_instr(token, &r->repl_ops[r->repl_len],
                    r->repl_dsts[r->repl_len],
                    r->repl_src1s[r->repl_len],
                    r->repl_src2s[r->repl_len]);
        r->repl_len++;
        token = strtok(NULL, ";");
    }
}

static bool match_instr(InstructionNode *in, InstructionOp op,
                        const char *dst, const char *src1, const char *src2) {
    if (op == ISEL_NOP) return true;
    if (in->op != op) return false;
    if (dst[0] != '\0' && strcmp(in->dst, dst) != 0) return false;
    if (src1[0] != '\0' && strcmp(in->src1, src1) != 0) return false;
    if (src2[0] != '\0' && strcmp(in->src2, src2) != 0) return false;
    return true;
}

static bool try_apply_rule(PeepholeRule *rule, InstructionList *ilist,
                           size_t start) {
    if (start + rule->pattern_len > ilist->count) return false;
    for (size_t j = 0; j < rule->pattern_len; j++) {
        InstructionNode *in = &ilist->instructions[start + j];
        if (!match_instr(in, rule->pattern_ops[j],
                         rule->pattern_dsts[j],
                         rule->pattern_src1s[j],
                         rule->pattern_src2s[j])) {
            return false;
        }
    }

    for (size_t j = 0; j < rule->repl_len; j++) {
        if (j < rule->pattern_len) {
            InstructionNode *in = &ilist->instructions[start + j];
            in->op = rule->repl_ops[j];
            snprintf(in->dst, sizeof(in->dst), "%s", rule->repl_dsts[j]);
            snprintf(in->src1, sizeof(in->src1), "%s", rule->repl_src1s[j]);
            snprintf(in->src2, sizeof(in->src2), "%s", rule->repl_src2s[j]);
        }
    }

    if (rule->repl_len < rule->pattern_len) {
        size_t excess = rule->pattern_len - rule->repl_len;
        for (size_t j = start + rule->repl_len; j < ilist->count - excess; j++) {
            ilist->instructions[j] = ilist->instructions[j + excess];
        }
        ilist->count -= excess;
    }

    rule->applied_count++;
    return true;
}

void peephole_optimize(PeepholeContext *ctx, InstructionList *ilist) {
    if (!ctx || !ilist) return;
    ctx->changed = false;

    bool any_change = true;
    while (any_change) {
        any_change = false;
        for (size_t r = 0; r < ctx->rule_count; r++) {
            for (size_t i = 0; i < ilist->count; i++) {
                if (try_apply_rule(&ctx->rules[r], ilist, i)) {
                    any_change = true;
                    ctx->changed = true;
                    break;
                }
            }
            if (any_change) break;
        }
    }
}

void peephole_print_replacements(PeepholeContext *ctx, FILE *out) {
    if (!ctx || !out) return;
    fprintf(out, ";;; Peephole Optimization Report:\n");
    fprintf(out, ";;; Rules registered: %zu\n", ctx->rule_count);
    fprintf(out, ";;; Rules applied:\n");
    size_t total = 0;
    for (size_t i = 0; i < ctx->rule_count; i++) {
        if (ctx->rules[i].applied_count > 0) {
            fprintf(out, ";;;   %-50s -> %-30s (x%zu)\n",
                    ctx->rules[i].pattern_str,
                    ctx->rules[i].replacement_str,
                    ctx->rules[i].applied_count);
            total += ctx->rules[i].applied_count;
        }
    }
    fprintf(out, ";;; Total replacements: %zu\n", total);
    if (!ctx->changed) {
        fprintf(out, ";;; No optimizations applied.\n");
    }
}
