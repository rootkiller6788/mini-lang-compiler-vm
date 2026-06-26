#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdbool.h>
#include "ir.h"
#include "ast.h"

/*
 * Code Generation — IR to target code (C source / stack-VM bytecode)
 *
 * L5: Instruction selection — mapping IR ops to target instructions.
 * L6: Canonical compiler problem — end-to-end compilation pipeline.
 * L7: Application — source-to-source translation, VM bytecode emission.
 * L8: Register allocation — Chaitin-Briggs graph coloring.
 *
 * Two backends:
 *   1. C code emitter — generates compilable C from IR (source-to-source)
 *   2. Stack VM emitter — generates bytecode for a simple stack machine
 */

/* ─── Target C Emitter ──────────────────────────────────────────────── */

/*
 * Emit compilable C code from an IR module.
 * Performs:
 *   - liveness-driven register assignment (when -k is given)
 *   - spills to stack slots when registers exhausted
 *   - C variable declarations for temps and locals
 *
 * Returns a heap-allocated string; caller must free().
 */
char *codegen_emit_c(IRModule *ir, const char *func_name, int num_regs);

/*
 * Emit C code for an entire program (multiple functions).
 */
char *codegen_emit_c_program(IRProgram *prog, int num_regs);

/* ─── Stack VM Bytecode Emitter ─────────────────────────────────────── */

/* Opcodes for a simple stack-based virtual machine.
 * The VM operates on a stack of integers.
 * This is the canonical target for educational compilers (L6). */
typedef enum {
    VM_PUSH_IMM,     /* push immediate value onto stack */
    VM_PUSH_VAR,     /* push variable value onto stack */
    VM_POP_VAR,      /* pop stack top into variable */
    VM_ADD,          /* pop b, pop a, push a+b */
    VM_SUB,          /* pop b, pop a, push a-b */
    VM_MUL,          /* pop b, pop a, push a*b */
    VM_DIV,          /* pop b, pop a, push a/b */
    VM_NEG,          /* pop a, push -a */
    VM_NOT,          /* pop a, push !a */
    VM_EQ,           /* pop b, pop a, push a==b */
    VM_NE,           /* pop b, pop a, push a!=b */
    VM_LT,           /* pop b, pop a, push a<b */
    VM_LE,           /* pop b, pop a, push a<=b */
    VM_GT,           /* pop b, pop a, push a>b */
    VM_GE,           /* pop b, pop a, push a>=b */
    VM_AND,          /* pop b, pop a, push a&&b */
    VM_OR,           /* pop b, pop a, push a||b */
    VM_JMP,          /* unconditional jump to label */
    VM_JMP_FALSE,    /* pop cond, if false jump to label */
    VM_LABEL,        /* label definition (no runtime effect) */
    VM_CALL,         /* call function (expects args on stack, pushes return) */
    VM_RET,          /* return from function */
    VM_HALT,         /* stop execution */
    VM_DUP,          /* duplicate stack top */
    VM_SWAP,         /* swap top two stack elements */
    VM_PRINT,        /* pop and print top of stack (debug) */
    VM_COUNT
} VMOpcode;

typedef struct {
    VMOpcode opcode;
    int operand;         /* immediate value, variable index, or label id */
    int line;            /* source line for debug */
} VMInstr;

typedef struct {
    VMInstr *instructions;
    int count;
    int capacity;
    int *labels;         /* label_id → instruction index mapping */
    int label_count;
} VMProgram;

/* Translate IRModule to VM bytecode. */
VMProgram *vm_program_from_ir(IRModule *ir);
void vm_program_destroy(VMProgram *prog);
void vm_program_print(const VMProgram *prog);
const char *vm_opcode_name(VMOpcode op);

/* Execute VM program (interpreter).  Returns final stack top value. */
int vm_execute(const VMProgram *prog, bool trace);

#endif /* CODEGEN_H */
