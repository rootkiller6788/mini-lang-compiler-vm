#ifndef BYTECODE_H
#define BYTECODE_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#define BC_MAX_INSTRUCTIONS 1024
#define BC_MAX_CONSTANTS   256
#define VM_STACK_SIZE      256

typedef enum {
    OP_PUSH = 0,
    OP_POP,
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_NEG,
    OP_NOT,
    OP_AND,
    OP_OR,
    OP_LOAD,
    OP_STORE,
    OP_JMP,
    OP_JMP_IF_FALSE,
    OP_CALL,
    OP_RET,
    OP_PRINT,
    OP_HALT
} OpCode;

typedef enum {
    CONST_INT    = 0,
    CONST_FLOAT  = 1,
    CONST_STRING = 2
} ConstType;

typedef struct {
    ConstType type;
    union {
        int64_t  int_val;
        double   float_val;
        char*    str_val;
    } data;
} Constant;

typedef struct {
    int32_t    instructions[BC_MAX_INSTRUCTIONS];
    int32_t    num_inst;
    Constant   const_pool[BC_MAX_CONSTANTS];
    int32_t    const_count;
} ByteCode;

typedef struct {
    int64_t    stack[VM_STACK_SIZE];
    int32_t    sp;
    int32_t    ip;
    int32_t    frame_ptr;
    ByteCode*  bytecode;
} StackVM;

void       vm_init(StackVM* vm, ByteCode* bc);
bool       vm_execute(StackVM* vm);
void       vm_push(StackVM* vm, int64_t value);
int64_t    vm_pop(StackVM* vm);
void       vm_print_stack(const StackVM* vm);
int32_t    bc_emit(ByteCode* bc, int32_t instr);
int32_t    bc_add_constant(ByteCode* bc, Constant c);
const char* opcode_name(OpCode op);

#endif
