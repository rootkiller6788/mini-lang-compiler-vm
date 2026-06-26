#include "backend.h"
#include <stdlib.h>
#include <string.h>

/*
 * Simple Backend / Code Generation
 *
 * Translates IR (three-address code) to a simple x86-like assembly.
 * This is a minimal but functional code generator demonstrating the
 * complete compiler pipeline: IR -> optimization -> register allocation
 * -> instruction selection -> assembly output.
 *
 * L1 (Definitions): TargetOp enum, TargetInst, CodeGen, StackFrame
 * L2 (Core Concept): Instruction selection via tree-pattern matching
 * L3 (Engineering): Stack frame layout, calling convention
 * L5 (Algorithm): Maximal munch tiling for instruction selection
 * L6 (Canonical Problem): Complete compiler backend
 *
 * Reference: CMU 15-745 (Code Generation), Dragon Book Ch.8
 *            Appel, "Modern Compiler Implementation in C", Ch.9
 */

const char* tgt_op_name(TargetOp op) {
    switch (op) {
        case TGT_MOV:     return "mov";
        case TGT_ADD:     return "add";
        case TGT_SUB:     return "sub";
        case TGT_IMUL:    return "imul";
        case TGT_IDIV:    return "idiv";
        case TGT_LOAD:    return "mov (load)";
        case TGT_STORE:   return "mov (store)";
        case TGT_JMP:     return "jmp";
        case TGT_JZ:      return "jz";
        case TGT_JNZ:     return "jnz";
        case TGT_CALL:    return "call";
        case TGT_RET:     return "ret";
        case TGT_PUSH:    return "push";
        case TGT_POP:     return "pop";
        case TGT_CMP:     return "cmp";
        case TGT_LEA:     return "lea";
        case TGT_AND:     return "and";
        case TGT_OR:      return "or";
        case TGT_XOR:     return "xor";
        case TGT_SHL:     return "shl";
        case TGT_SHR:     return "shr";
        case TGT_LABEL:   return "label";
        case TGT_COMMENT: return ";";
        default:          return "???";
    }
}

CodeGen* cg_create(const char* func_name) {
    CodeGen* cg = (CodeGen*)malloc(sizeof(CodeGen));
    if (!cg) return NULL;
    memset(cg, 0, sizeof(CodeGen));
    cg->func_name = func_name;
    cg->next_label = 0;
    for (int i = 0; i < MAX_TEMP_REGS; i++) cg->reg_map[i] = -1;
    return cg;
}

void cg_destroy(CodeGen* cg) {
    if (cg) free(cg);
}

int cg_emit(CodeGen* cg, TargetOp op, int o0, int o1, int o2, const char* label) {
    if (!cg || cg->num_inst >= MAX_TARGET_INST) return -1;
    TargetInst* ti = &cg->instructions[cg->num_inst];
    ti->op = op;
    ti->operands[0] = o0;
    ti->operands[1] = o1;
    ti->operands[2] = o2;
    ti->num_operands = (o0 >= -1 ? 1 : 0) + (o1 >= -1 ? 1 : 0) + (o2 >= -1 ? 1 : 0);
    if (label) {
        strncpy(ti->label, label, MAX_LABEL_LEN - 1);
        ti->label[MAX_LABEL_LEN - 1] = '\0';
    } else {
        ti->label[0] = '\0';
    }
    ti->comment[0] = '\0';
    return cg->num_inst++;
}

int cg_new_label(CodeGen* cg) {
    if (!cg) return -1;
    return cg->next_label++;
}

/*
 * Allocate a stack slot for a spilled variable.
 *
 * Stack frame layout (grows downward):
 *   [param N]     <- highest address
 *   ...
 *   [param 1]
 *   [return addr]
 *   [saved EBP]   <- EBP
 *   [local 1]     <- EBP - 4
 *   [local 2]     <- EBP - 8
 *   ...           <- ESP (lowest address)
 *
 * Each slot is 4 bytes (int32). We maintain slots[0..num_slots-1]
 * representing allocated local stack space.
 */
int cg_allocate_stack_slot(CodeGen* cg, int temp_id, int size) {
    if (!cg || cg->frame.num_slots >= STACK_SLOTS) return 0;
    int idx = cg->frame.num_slots;
    cg->frame.slots[idx].temp_id = temp_id;
    cg->frame.slots[idx].size = size;
    cg->frame.slots[idx].used = true;
    cg->frame.slots[idx].offset = -(cg->frame.total_size + size);
    cg->frame.total_size += size;
    cg->frame.num_slots++;
    return cg->frame.slots[idx].offset;
}

/*
 * Instruction selection via simple maximal munch.
 *
 * For each IR instruction, we emit one or more target instructions.
 * This is a simplified one-to-one or one-to-few mapping rather than
 * full tree-pattern matching (which would operate on IR expression
 * trees rather than three-address code).
 *
 * The reg_assignments[] array maps IR temps to physical registers;
 * -1 means the temp was spilled (use stack slot).
 *
 * L5 (Algorithm): Simple tiling-based instruction selection.
 * In a full compiler, this would use BURG/IBURG for optimal
 * tree-pattern matching via dynamic programming.
 */
void cg_generate(CodeGen* cg, const IRFunction* func, const int reg_assignments[]) {
    if (!cg || !func) return;

    char buf[64];
    snprintf(buf, sizeof(buf), "%s:", func->name);
    /* Emit function prologue */
    cg_emit(cg, TGT_LABEL, 0, 0, 0, func->name);
    cg_emit(cg, TGT_PUSH, 5, -1, -1, NULL); /* push ebp */
    cg_emit(cg, TGT_MOV, 5, 4, -1, NULL);   /* mov ebp, esp */
    if (cg->frame.total_size > 0) {
        cg_emit(cg, TGT_SUB, 4, cg->frame.total_size, -1, NULL); /* sub esp, N */
    }

    /* Track label positions for branch resolution */
    int label_inst[MAX_BLOCKS];
    int num_labels = 0;

    for (int i = 0; i < func->num_inst; i++) {
        const IRInst* inst = &func->instructions[i];

        /*
         * Map operands:
         * If reg_assignments[op] >= 0: use physical register
         * If reg_assignments[op] < 0: use stack slot
         */
        int map_dest = (inst->dest >= 0 && inst->dest < MAX_TEMP_REGS)
                       ? reg_assignments[inst->dest] : -2;
        int map_src1 = (inst->src1 >= 0 && inst->src1 < MAX_TEMP_REGS)
                       ? reg_assignments[inst->src1] : -2;
        int map_src2 = (inst->src2 >= 0 && inst->src2 < MAX_TEMP_REGS)
                       ? reg_assignments[inst->src2] : -2;

        switch (inst->op) {
            case IR_MOV:
                if (map_src1 >= 0 && map_dest >= 0) {
                    cg_emit(cg, TGT_MOV, map_dest, map_src1, -1, NULL);
                } else if (map_dest >= 0 && inst->src1 >= 0) {
                    /* src1 is a constant value, not a register */
                    cg_emit(cg, TGT_MOV, map_dest, inst->src1, -1, NULL);
                }
                break;

            case IR_ADD:
                if (map_dest >= 0 && map_src1 >= 0 && map_src2 >= 0) {
                    cg_emit(cg, TGT_MOV, map_dest, map_src1, -1, NULL);
                    cg_emit(cg, TGT_ADD, map_dest, map_src2, -1, NULL);
                } else if (map_dest >= 0 && map_src1 >= 0) {
                    cg_emit(cg, TGT_MOV, map_dest, map_src1, -1, NULL);
                    cg_emit(cg, TGT_ADD, map_dest, inst->src2, -1, NULL);
                }
                break;

            case IR_SUB:
                if (map_dest >= 0 && map_src1 >= 0 && map_src2 >= 0) {
                    cg_emit(cg, TGT_MOV, map_dest, map_src1, -1, NULL);
                    cg_emit(cg, TGT_SUB, map_dest, map_src2, -1, NULL);
                }
                break;

            case IR_MUL:
                if (map_dest >= 0 && map_src1 >= 0) {
                    cg_emit(cg, TGT_MOV, map_dest, map_src1, -1, NULL);
                    if (map_src2 >= 0)
                        cg_emit(cg, TGT_IMUL, map_dest, map_src2, -1, NULL);
                    else
                        cg_emit(cg, TGT_IMUL, map_dest, inst->src2, -1, NULL);
                }
                break;

            case IR_DIV:
                /* x86 idiv: eax = edx:eax / operand; edx = remainder */
                if (map_src1 >= 0) cg_emit(cg, TGT_MOV, 0, map_src1, -1, NULL);
                cg_emit(cg, TGT_XOR, 2, 2, -1, NULL); /* xor edx, edx */
                if (map_src2 >= 0)
                    cg_emit(cg, TGT_IDIV, map_src2, -1, -1, NULL);
                else
                    cg_emit(cg, TGT_IDIV, inst->src2, -1, -1, NULL);
                if (map_dest >= 0) cg_emit(cg, TGT_MOV, map_dest, 0, -1, NULL);
                break;

            case IR_BR: {
                char lbl[16];
                snprintf(lbl, sizeof(lbl), "L%s", inst->label);
                cg_emit(cg, TGT_JMP, 0, 0, 0, lbl);
                break;
            }

            case IR_BRCOND: {
                if (map_src1 >= 0) {
                    cg_emit(cg, TGT_CMP, map_src1, 0, -1, NULL);
                }
                char tl[16], fl[16];
                snprintf(tl, sizeof(tl), "L%s", inst->label);
                snprintf(fl, sizeof(fl), "L%s", inst->src1_label);
                cg_emit(cg, TGT_JNZ, 0, 0, 0, tl);
                cg_emit(cg, TGT_JMP, 0, 0, 0, fl);
                break;
            }

            case IR_RET:
                if (map_src1 >= 0) cg_emit(cg, TGT_MOV, 0, map_src1, -1, NULL);
                /* Epilogue */
                cg_emit(cg, TGT_MOV, 4, 5, -1, NULL); /* mov esp, ebp */
                cg_emit(cg, TGT_POP, 5, -1, -1, NULL);  /* pop ebp */
                cg_emit(cg, TGT_RET, 0, 0, -1, NULL);
                break;

            case IR_PHI:
                /* Phi functions are resolved during SSA destruction.
                 * During codegen, phi operands become mov instructions
                 * in predecessor blocks. Here we emit a comment. */
                break;

            case IR_LOAD:
                if (map_dest >= 0 && map_src1 >= 0) {
                    cg_emit(cg, TGT_LOAD, map_dest, map_src1, -1, NULL);
                }
                break;

            case IR_STORE:
                if (map_src1 >= 0 && map_dest >= 0) {
                    cg_emit(cg, TGT_STORE, map_dest, map_src1, -1, NULL);
                }
                break;

            case IR_ALLOCA:
                /* Stack allocation handled by frame setup */
                if (map_dest >= 0) {
                    cg_emit(cg, TGT_LEA, map_dest, 5,
                            cg_allocate_stack_slot(cg, inst->dest, inst->src1), NULL);
                }
                break;

            case IR_CALL:
                /* Simplified: push args, call, clean stack */
                if (map_src2 >= 0) cg_emit(cg, TGT_PUSH, map_src2, -1, -1, NULL);
                cg_emit(cg, TGT_CALL, map_src1, 0, 0, NULL);
                if (map_dest >= 0) cg_emit(cg, TGT_MOV, map_dest, 0, -1, NULL);
                break;

            default:
                break;
        }
    }

    /* If no explicit RET was emitted, add default epilogue */
    if (cg->num_inst > 0) {
        TargetInst* last = &cg->instructions[cg->num_inst - 1];
        if (last->op != TGT_RET) {
            cg_emit(cg, TGT_MOV, 4, 5, -1, NULL);
            cg_emit(cg, TGT_POP, 5, -1, -1, NULL);
            cg_emit(cg, TGT_RET, 0, 0, -1, NULL);
        }
    }
}

/*
 * Print the generated assembly in a readable format.
 */
void cg_print_asm(const CodeGen* cg, FILE* out) {
    if (!cg || !out) return;
    fprintf(out, "; === Generated Assembly: %s ===\n", cg->func_name);

    const char* reg_names[] = {
        "eax", "ecx", "edx", "ebx", "esp", "ebp", "esi", "edi",
        "r8d", "r9d", "r10d", "r11d", "r12d", "r13d", "r14d", "r15d"
    };
    const int num_reg_names = 16;

    for (int i = 0; i < cg->num_inst; i++) {
        const TargetInst* ti = &cg->instructions[i];

        if (ti->op == TGT_LABEL) {
            fprintf(out, "%s:\n", ti->label);
            continue;
        }
        if (ti->op == TGT_COMMENT) {
            fprintf(out, "  ; %s\n", ti->comment);
            continue;
        }

        fprintf(out, "  %-6s ", tgt_op_name(ti->op));

        int printed = 0;
        for (int j = 0; j < 3; j++) {
            int op = ti->operands[j];
            if (op < -1) break; /* sentinel */

            /*
             * For op == -1: this operand slot is not used (skip).
             */
            if (op == -1) continue;

            if (printed > 0) fprintf(out, ", ");
            printed++;

            if (ti->op == TGT_JMP || ti->op == TGT_JZ || ti->op == TGT_JNZ ||
                ti->op == TGT_CALL) {
                if (ti->label[0]) {
                    fprintf(out, "%s", ti->label);
                    break;
                }
            }

            /*
             * Print register name or immediate.
             * Convention: op >= 0 => register index, op < -1 => immediate value.
             */
            if (op >= 0 && op < num_reg_names) {
                fprintf(out, "%s", reg_names[op]);
            } else if (op >= num_reg_names) {
                fprintf(out, "r%d", op);
            } else {
                fprintf(out, "%d", op); /* immediate/unknown */
            }
        }
        fprintf(out, "\n");
    }
}

/*
 * Peephole optimization on the target instruction stream.
 *
 * Applies simple local rewrites:
 * - mov r, r  =>  (eliminate redundant moves)
 * - push r; pop r  =>  (eliminate push-pop pair)
 * - add r, 0  =>  (eliminate add with zero)
 * - mov r1, r2; mov r2, r1  =>  mov r1, r2  (eliminate redundant copy-back)
 *
 * L7 (Application): Practical post-pass optimization used in
 * GCC, LLVM, and other production compilers.
 */
void cg_peephole_optimize(CodeGen* cg) {
    if (!cg) return;

    for (int i = 0; i < cg->num_inst - 1; i++) {
        TargetInst* ti = &cg->instructions[i];
        TargetInst* next = &cg->instructions[i + 1];

        /* Rule 1: mov r, r is redundant */
        if (ti->op == TGT_MOV &&
            ti->operands[0] == ti->operands[1] &&
            ti->operands[0] >= 0) {
            ti->op = TGT_COMMENT;
            snprintf(ti->comment, sizeof(ti->comment),
                     "eliminated: mov r%d, r%d",
                     ti->operands[0], ti->operands[1]);
            continue;
        }

        /* Rule 2: add r, 0 is redundant */
        if (ti->op == TGT_ADD &&
            ti->operands[1] == 0) {
            ti->op = TGT_COMMENT;
            snprintf(ti->comment, sizeof(ti->comment),
                     "eliminated: add r%d, 0", ti->operands[0]);
            continue;
        }

        /* Rule 3: push r / pop r (same register) cancels out */
        if (ti->op == TGT_PUSH && next->op == TGT_POP &&
            ti->operands[0] == next->operands[0]) {
            ti->op = TGT_COMMENT;
            snprintf(ti->comment, sizeof(ti->comment),
                     "eliminated: push/pop r%d pair", ti->operands[0]);
            next->op = TGT_COMMENT;
            snprintf(next->comment, sizeof(next->comment),
                     "eliminated: push/pop r%d pair", ti->operands[0]);
            continue;
        }

        /* Rule 4: consecutive mov r1, r2 then mov r2, r1 => keep first only */
        if (ti->op == TGT_MOV && next->op == TGT_MOV &&
            ti->operands[0] == next->operands[1] &&
            ti->operands[1] == next->operands[0] &&
            ti->operands[0] >= 0 && ti->operands[1] >= 0) {
            next->op = TGT_COMMENT;
            snprintf(next->comment, sizeof(next->comment),
                     "eliminated: redundant mov swap");
        }
    }
}

CGStats cg_get_stats(const CodeGen* cg) {
    CGStats stats = {0};
    if (!cg) return stats;
    stats.total_count = cg->num_inst;
    for (int i = 0; i < cg->num_inst; i++) {
        switch (cg->instructions[i].op) {
            case TGT_MOV:  stats.irmov_count++; break;
            case TGT_ADD: case TGT_SUB: case TGT_IMUL: case TGT_IDIV:
            case TGT_AND: case TGT_OR: case TGT_XOR:
            case TGT_SHL: case TGT_SHR:
                stats.arith_count++; break;
            case TGT_LOAD: case TGT_STORE:
            case TGT_PUSH: case TGT_POP: case TGT_LEA:
                stats.mem_count++; break;
            case TGT_JMP: case TGT_JZ: case TGT_JNZ:
            case TGT_CALL: case TGT_RET: case TGT_CMP:
                stats.branch_count++; break;
            default: break;
        }
    }
    return stats;
}
