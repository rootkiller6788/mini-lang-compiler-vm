#include "dataflow.h"
#include <stdlib.h>
#include <string.h>

void bv_init(BitVector* bv) {
    if (!bv) return;
    memset(bv->bits, 0, sizeof(bv->bits));
}

void bv_set(BitVector* bv, int idx) {
    if (!bv || idx < 0) return;
    int word = idx / 32;
    int bit = idx % 32;
    bv->bits[word] |= (1u << bit);
}

void bv_clear(BitVector* bv, int idx) {
    if (!bv || idx < 0) return;
    int word = idx / 32;
    int bit = idx % 32;
    bv->bits[word] &= ~(1u << bit);
}

bool bv_test(const BitVector* bv, int idx) {
    if (!bv || idx < 0) return false;
    int word = idx / 32;
    int bit = idx % 32;
    return (bv->bits[word] & (1u << bit)) != 0;
}

void bv_union(BitVector* dst, const BitVector* src) {
    if (!dst || !src) return;
    for (int i = 0; i < BITVECTOR_WORDS; i++) {
        dst->bits[i] |= src->bits[i];
    }
}

void bv_intersect(BitVector* dst, const BitVector* src) {
    if (!dst || !src) return;
    for (int i = 0; i < BITVECTOR_WORDS; i++) {
        dst->bits[i] &= src->bits[i];
    }
}

void bv_copy(BitVector* dst, const BitVector* src) {
    if (!dst || !src) return;
    memcpy(dst->bits, src->bits, sizeof(dst->bits));
}

bool bv_equals(const BitVector* a, const BitVector* b) {
    if (!a || !b) return false;
    return memcmp(a->bits, b->bits, sizeof(a->bits)) == 0;
}

void bv_print(const BitVector* bv, int max_bits, FILE* out) {
    if (!bv || !out) return;
    fprintf(out, "{");
    bool first = true;
    for (int i = 0; i < max_bits; i++) {
        if (bv_test(bv, i)) {
            if (!first) fprintf(out, ", ");
            fprintf(out, "%d", i);
            first = false;
        }
    }
    fprintf(out, "}");
}

void df_analyze(const IRFunction* func, IRBasicBlock blocks[], int num_blocks,
                DataflowAnalysis type, DataflowResult* result) {
    if (!func || !blocks || !result) return;
    memset(result, 0, sizeof(DataflowResult));

    bool is_forward = (type != DF_LIVE_VARIABLES);
    bool use_intersect = (type == DF_AVAILABLE_EXPRS || type == DF_CONSTANT_PROP);

    if (use_intersect) {
        for (int i = 0; i < num_blocks; i++) {
            for (int w = 0; w < BITVECTOR_WORDS; w++) {
                result->IN[i].bits[w] = ~0u;
            }
        }
    }

    bool changed = true;
    int iter_count = 0;
    while (changed && iter_count < 1000) {
        changed = false;
        iter_count++;

        for (int i = 0; i < num_blocks; i++) {
            BitVector meet_val;
            bv_init(&meet_val);

            bool first_pred = true;
            if (is_forward) {
                for (int p = 0; p < blocks[i].num_pred; p++) {
                    int pred = blocks[i].predecessors[p];
                    if (first_pred) {
                        bv_copy(&meet_val, &result->OUT[pred]);
                        first_pred = false;
                    } else if (use_intersect) {
                        bv_intersect(&meet_val, &result->OUT[pred]);
                    } else {
                        bv_union(&meet_val, &result->OUT[pred]);
                    }
                }
                if (i == 0 && first_pred) bv_init(&meet_val);
            } else {
                for (int s = 0; s < blocks[i].num_succ; s++) {
                    int succ = blocks[i].successors[s];
                    if (first_pred) {
                        bv_copy(&meet_val, &result->IN[succ]);
                        first_pred = false;
                    } else {
                        bv_union(&meet_val, &result->IN[succ]);
                    }
                }
                if (!blocks[i].num_succ) bv_init(&meet_val);
            }

            BitVector old_in;
            bv_copy(&old_in, &result->IN[i]);
            bv_copy(&result->IN[i], &meet_val);

            bv_copy(&result->OUT[i], &result->IN[i]);

            if (!bv_equals(&old_in, &result->IN[i])) changed = true;
        }
    }
}

void df_reaching_defs(const IRFunction* func, IRBasicBlock blocks[], int num_blocks,
                      DataflowResult* result) {
    df_analyze(func, blocks, num_blocks, DF_REACHING_DEFS, result);
}

void df_live_variables(const IRFunction* func, IRBasicBlock blocks[], int num_blocks,
                       DataflowResult* result) {
    df_analyze(func, blocks, num_blocks, DF_LIVE_VARIABLES, result);
}

void df_constant_propagation(const IRFunction* func, IRBasicBlock blocks[], int num_blocks,
                             int* value_table) {
    if (!value_table) return;
    for (int i = 0; i < MAX_TEMP_REGS; i++) value_table[i] = INT32_MIN;

    bool changed = true;
    while (changed) {
        changed = false;
        for (int b = 0; b < num_blocks && b < MAX_BLOCKS; b++) {
            for (int i = 0; i < blocks[b].num_inst; i++) {
                const IRInst* inst = &func->instructions[blocks[b].inst_indices[i]];
                int v1 = (inst->src1 >= 0) ? value_table[inst->src1] : INT32_MIN;
                int v2 = (inst->src2 >= 0) ? value_table[inst->src2] : INT32_MIN;

                int result_val = INT32_MIN;
                switch (inst->op) {
                    case IR_MOV:
                        if (v1 != INT32_MIN) result_val = v1;
                        break;
                    case IR_ADD:
                        if (v1 != INT32_MIN && v2 != INT32_MIN) result_val = v1 + v2;
                        break;
                    case IR_SUB:
                        if (v1 != INT32_MIN && v2 != INT32_MIN) result_val = v1 - v2;
                        break;
                    case IR_MUL:
                        if (v1 != INT32_MIN && v2 != INT32_MIN) result_val = v1 * v2;
                        break;
                    case IR_DIV:
                        if (v1 != INT32_MIN && v2 != INT32_MIN && v2 != 0)
                            result_val = v1 / v2;
                        break;
                    default: break;
                }

                if (inst->dest >= 0 && result_val != value_table[inst->dest]) {
                    value_table[inst->dest] = result_val;
                    changed = true;
                }
            }
        }
    }
}

void df_print_result(const DataflowResult* result, int num_blocks, FILE* out) {
    if (!result || !out) return;
    for (int i = 0; i < num_blocks; i++) {
        fprintf(out, "Block %d:\n", i);
        fprintf(out, "  IN:  "); bv_print(&result->IN[i], MAX_TEMP_REGS, out);
        fprintf(out, "\n");
        fprintf(out, "  OUT: "); bv_print(&result->OUT[i], MAX_TEMP_REGS, out);
        fprintf(out, "\n");
    }
}
