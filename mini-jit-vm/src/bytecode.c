#include "bytecode.h"
#include <string.h>
#include <stdlib.h>

const char* opcode_name(OpCode op) {
    switch (op) {
        case OP_PUSH:        return "PUSH";
        case OP_POP:         return "POP";
        case OP_ADD:         return "ADD";
        case OP_SUB:         return "SUB";
        case OP_MUL:         return "MUL";
        case OP_DIV:         return "DIV";
        case OP_NEG:         return "NEG";
        case OP_NOT:         return "NOT";
        case OP_AND:         return "AND";
        case OP_OR:          return "OR";
        case OP_LOAD:        return "LOAD";
        case OP_STORE:       return "STORE";
        case OP_JMP:         return "JMP";
        case OP_JMP_IF_FALSE:return "JMP_IF_FALSE";
        case OP_CALL:        return "CALL";
        case OP_RET:         return "RET";
        case OP_PRINT:       return "PRINT";
        case OP_HALT:        return "HALT";
        default:             return "UNKNOWN";
    }
}

int32_t bc_emit(ByteCode* bc, int32_t instr) {
    if (bc->num_inst >= BC_MAX_INSTRUCTIONS) {
        fprintf(stderr, "bc_emit: instruction buffer full\n");
        return -1;
    }
    bc->instructions[bc->num_inst++] = instr;
    return bc->num_inst - 1;
}

int32_t bc_add_constant(ByteCode* bc, Constant c) {
    if (bc->const_count >= BC_MAX_CONSTANTS) {
        fprintf(stderr, "bc_add_constant: constant pool full\n");
        return -1;
    }
    bc->const_pool[bc->const_count] = c;
    return bc->const_count++;
}

void vm_init(StackVM* vm, ByteCode* bc) {
    memset(vm->stack, 0, sizeof(vm->stack));
    vm->sp        = 0;
    vm->ip        = 0;
    vm->frame_ptr = 0;
    vm->bytecode  = bc;
}

void vm_push(StackVM* vm, int64_t value) {
    if (vm->sp >= VM_STACK_SIZE) {
        fprintf(stderr, "vm_push: stack overflow at sp=%d\n", vm->sp);
        return;
    }
    vm->stack[vm->sp++] = value;
}

int64_t vm_pop(StackVM* vm) {
    if (vm->sp <= 0) {
        fprintf(stderr, "vm_pop: stack underflow\n");
        return 0;
    }
    return vm->stack[--vm->sp];
}

void vm_print_stack(const StackVM* vm) {
    printf("Stack (sp=%d, ip=%d, fp=%d): [", vm->sp, vm->ip, vm->frame_ptr);
    for (int32_t i = 0; i < vm->sp; i++) {
        printf("%lld", (long long)vm->stack[i]);
        if (i < vm->sp - 1) printf(", ");
    }
    printf("]\n");
}

static void vm_disassemble_instr(const ByteCode* bc, int32_t ip) {
    int32_t instr = bc->instructions[ip];
    OpCode op = (OpCode)(instr & 0xFF);
    int32_t arg = (instr >> 8) & 0xFFFFFF;

    printf("%04d  %-14s", ip, opcode_name(op));
    switch (op) {
        case OP_PUSH:
        case OP_JMP:
        case OP_JMP_IF_FALSE:
        case OP_LOAD:
        case OP_STORE:
        case OP_CALL:
            printf(" %d", arg);
            break;
        case OP_PRINT:
            break;
        case OP_HALT:
            break;
        default:
            break;
    }
    printf("\n");
}

bool vm_execute(StackVM* vm) {
    const ByteCode* bc = vm->bytecode;

    while (vm->ip < bc->num_inst) {
        int32_t instr = bc->instructions[vm->ip];
        OpCode op = (OpCode)(instr & 0xFF);
        int32_t arg = (instr >> 8) & 0xFFFFFF;

#ifdef VM_TRACE
        vm_print_stack(vm);
        vm_disassemble_instr(bc, vm->ip);
#endif

        switch (op) {
            case OP_PUSH: {
                if (arg >= 0 && arg < bc->const_count) {
                    Constant c = bc->const_pool[arg];
                    if (c.type == CONST_INT) {
                        vm_push(vm, c.data.int_val);
                    } else if (c.type == CONST_FLOAT) {
                        vm_push(vm, (int64_t)c.data.float_val);
                    } else {
                        vm_push(vm, (int64_t)(uintptr_t)c.data.str_val);
                    }
                }
                vm->ip++;
                break;
            }
            case OP_POP: {
                vm_pop(vm);
                vm->ip++;
                break;
            }
            case OP_ADD: {
                int64_t b = vm_pop(vm);
                int64_t a = vm_pop(vm);
                vm_push(vm, a + b);
                vm->ip++;
                break;
            }
            case OP_SUB: {
                int64_t b = vm_pop(vm);
                int64_t a = vm_pop(vm);
                vm_push(vm, a - b);
                vm->ip++;
                break;
            }
            case OP_MUL: {
                int64_t b = vm_pop(vm);
                int64_t a = vm_pop(vm);
                vm_push(vm, a * b);
                vm->ip++;
                break;
            }
            case OP_DIV: {
                int64_t b = vm_pop(vm);
                int64_t a = vm_pop(vm);
                if (b == 0) {
                    fprintf(stderr, "vm_execute: division by zero\n");
                    return false;
                }
                vm_push(vm, a / b);
                vm->ip++;
                break;
            }
            case OP_NEG: {
                int64_t a = vm_pop(vm);
                vm_push(vm, -a);
                vm->ip++;
                break;
            }
            case OP_NOT: {
                int64_t a = vm_pop(vm);
                vm_push(vm, a ? 0 : 1);
                vm->ip++;
                break;
            }
            case OP_AND: {
                int64_t b = vm_pop(vm);
                int64_t a = vm_pop(vm);
                vm_push(vm, (a && b) ? 1 : 0);
                vm->ip++;
                break;
            }
            case OP_OR: {
                int64_t b = vm_pop(vm);
                int64_t a = vm_pop(vm);
                vm_push(vm, (a || b) ? 1 : 0);
                vm->ip++;
                break;
            }
            case OP_LOAD: {
                int32_t idx = arg + vm->frame_ptr;
                if (idx >= 0 && idx < VM_STACK_SIZE) {
                    vm_push(vm, vm->stack[idx]);
                }
                vm->ip++;
                break;
            }
            case OP_STORE: {
                int64_t val = vm_pop(vm);
                int32_t idx = arg + vm->frame_ptr;
                if (idx >= 0 && idx < VM_STACK_SIZE) {
                    vm->stack[idx] = val;
                }
                vm->ip++;
                break;
            }
            case OP_JMP: {
                vm->ip = arg;
                break;
            }
            case OP_JMP_IF_FALSE: {
                int64_t cond = vm_pop(vm);
                if (!cond) {
                    vm->ip = arg;
                } else {
                    vm->ip++;
                }
                break;
            }
            case OP_CALL: {
                vm->frame_ptr = vm->sp - arg;
                vm->ip++;
                break;
            }
            case OP_RET: {
                int64_t ret_val = vm->stack[vm->sp - 1];
                vm->sp = vm->frame_ptr;
                vm_push(vm, ret_val);
                vm->ip++;
                break;
            }
            case OP_PRINT: {
                int64_t top = vm_pop(vm);
                printf("%lld\n", (long long)top);
                vm->ip++;
                break;
            }
            case OP_HALT: {
                return true;
            }
            default: {
                fprintf(stderr, "vm_execute: unknown opcode %d at ip=%d\n",
                        op, vm->ip);
                return false;
            }
        }
    }
    return true;
}
