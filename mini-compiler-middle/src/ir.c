#include "ir.h"
#include <stdlib.h>
#include <string.h>

IRFunction* ir_create_function(const char* name) {
    IRFunction* func = (IRFunction*)malloc(sizeof(IRFunction));
    if (!func) return NULL;
    memset(func, 0, sizeof(IRFunction));
    strncpy(func->name, name, sizeof(func->name) - 1);
    func->name[sizeof(func->name) - 1] = '\0';
    func->num_inst = 0;
    func->next_temp = 0;
    return func;
}

void ir_destroy_function(IRFunction* func) {
    if (func) free(func);
}

int ir_emit(IRFunction* func, IROp op, int dest, int src1, int src2, const char* label) {
    if (!func || func->num_inst >= MAX_INSTRUCTIONS) return -1;
    IRInst* inst = &func->instructions[func->num_inst];
    inst->op = op;
    inst->dest = dest;
    inst->src1 = src1;
    inst->src2 = src2;
    if (label) {
        strncpy(inst->label, label, MAX_LABEL_LEN - 1);
        inst->label[MAX_LABEL_LEN - 1] = '\0';
    } else {
        inst->label[0] = '\0';
    }
    inst->src1_label[0] = '\0';
    inst->src2_label[0] = '\0';
    return func->num_inst++;
}

static int global_label_counter = 0;

int ir_new_label(IRFunction* func) {
    (void)func;
    return global_label_counter++;
}

int ir_new_temp(IRFunction* func) {
    if (!func) return -1;
    return func->next_temp++;
}

const char* ir_op_name(IROp op) {
    switch (op) {
        case IR_ADD:    return "add";
        case IR_SUB:    return "sub";
        case IR_MUL:    return "mul";
        case IR_DIV:    return "div";
        case IR_LOAD:   return "load";
        case IR_STORE:  return "store";
        case IR_BR:     return "br";
        case IR_BRCOND: return "brcond";
        case IR_CALL:   return "call";
        case IR_RET:    return "ret";
        case IR_MOV:    return "mov";
        case IR_PHI:    return "phi";
        case IR_ALLOCA: return "alloca";
        default:        return "unknown";
    }
}

static bool is_terminator(IROp op) {
    return op == IR_BR || op == IR_BRCOND || op == IR_RET;
}

void ir_print_function(const IRFunction* func, FILE* out) {
    if (!func || !out) return;
    fprintf(out, "function %s:\n", func->name);
    for (int i = 0; i < func->num_inst; i++) {
        const IRInst* inst = &func->instructions[i];
        fprintf(out, "  %3d: ", i);
        switch (inst->op) {
            case IR_ADD:
            case IR_SUB:
            case IR_MUL:
            case IR_DIV:
                fprintf(out, "%%t%d = %s %%t%d, %%t%d\n",
                        inst->dest, ir_op_name(inst->op), inst->src1, inst->src2);
                break;
            case IR_LOAD:
                fprintf(out, "%%t%d = load %%t%d\n", inst->dest, inst->src1);
                break;
            case IR_STORE:
                fprintf(out, "store %%t%d, %%t%d\n", inst->src1, inst->dest);
                break;
            case IR_BR:
                fprintf(out, "br %s\n", inst->label);
                break;
            case IR_BRCOND:
                fprintf(out, "brcond %%t%d, %s, %s\n",
                        inst->src1, inst->label, inst->src1_label);
                break;
            case IR_CALL:
                fprintf(out, "%%t%d = call %%t%d(%%t%d)\n",
                        inst->dest, inst->src1, inst->src2);
                break;
            case IR_RET:
                if (inst->src1 >= 0)
                    fprintf(out, "ret %%t%d\n", inst->src1);
                else
                    fprintf(out, "ret\n");
                break;
            case IR_MOV:
                fprintf(out, "%%t%d = mov %%t%d\n", inst->dest, inst->src1);
                break;
            case IR_PHI:
                fprintf(out, "%%t%d = phi(%%t%d:%s, %%t%d:%s)\n",
                        inst->dest, inst->src1, inst->label, inst->src2, inst->src1_label);
                break;
            case IR_ALLOCA:
                fprintf(out, "%%t%d = alloca %%t%d\n", inst->dest, inst->src1);
                break;
            default:
                fprintf(out, "unknown op\n");
                break;
        }
    }
}

int ir_build_cfg(const IRFunction* func, IRBasicBlock blocks[], int max_blocks) {
    if (!func || !blocks) return 0;
    memset(blocks, 0, (size_t)max_blocks * sizeof(IRBasicBlock));

    int num_blocks = 0;
    int block_label = 0;

    for (int i = 0; i < func->num_inst; i++) {
        blocks[num_blocks].inst_indices[blocks[num_blocks].num_inst++] = i;
        if (is_terminator(func->instructions[i].op) || i == func->num_inst - 1) {
            blocks[num_blocks].label = block_label++;
            num_blocks++;
            if (num_blocks >= max_blocks) break;
            if (i < func->num_inst - 1) {
                blocks[num_blocks].num_inst = 0;
                blocks[num_blocks].num_pred = 0;
                blocks[num_blocks].num_succ = 0;
            }
        }
    }

    for (int i = 0; i < num_blocks; i++) {
        if (blocks[i].num_inst == 0) continue;
        int last_idx = blocks[i].inst_indices[blocks[i].num_inst - 1];
        const IRInst* last = &func->instructions[last_idx];

        if (last->op == IR_BR) {
            int target = atoi(last->label);
            for (int j = 0; j < num_blocks; j++) {
                if (blocks[j].label == target) {
                    blocks[i].successors[blocks[i].num_succ++] = j;
                    blocks[j].predecessors[blocks[j].num_pred++] = i;
                    break;
                }
            }
        } else if (last->op == IR_BRCOND) {
            int true_target = atoi(last->label);
            int false_target = atoi(last->src1_label);
            for (int j = 0; j < num_blocks; j++) {
                if (blocks[j].label == true_target) {
                    blocks[i].successors[blocks[i].num_succ++] = j;
                    blocks[j].predecessors[blocks[j].num_pred++] = i;
                }
                if (blocks[j].label == false_target) {
                    blocks[i].successors[blocks[i].num_succ++] = j;
                    blocks[j].predecessors[blocks[j].num_pred++] = i;
                }
            }
        } else if (last->op != IR_RET) {
            if (i + 1 < num_blocks) {
                blocks[i].successors[blocks[i].num_succ++] = i + 1;
                blocks[i + 1].predecessors[blocks[i + 1].num_pred++] = i;
            }
        }
    }

    return num_blocks;
}
