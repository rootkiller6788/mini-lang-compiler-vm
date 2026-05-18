#ifndef INSTRUCTION_SELECTION_H
#define INSTRUCTION_SELECTION_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_TILESET_SIZE   256
#define MAX_PATTERN_LEN    64
#define MAX_INSTR_OPERANDS 3

typedef enum {
    IRO_ADD, IRO_SUB, IRO_MUL, IRO_DIV,
    IRO_LOAD, IRO_STORE,
    IRO_MEM, IRO_DEREF,
    IRO_CONST, IRO_TEMP,
    IRO_BASE, IRO_IMM,
    IRO_LABEL, IRO_CALL,
    IRO_RETURN, IRO_CMP,
    IRO_JMP, IRO_JE, IRO_JNE, IRO_JL,
    IRO_PUSH, IRO_POP,
    IRO_COUNT
} IROp;

typedef enum {
    ISEL_MOV, ISEL_ADD, ISEL_SUB, ISEL_MUL, ISEL_DIV,
    ISEL_LOAD, ISEL_STORE,
    ISEL_PUSH, ISEL_POP,
    ISEL_RET, ISEL_CALL,
    ISEL_CMP, ISEL_JMP, ISEL_JE, ISEL_JNE, ISEL_JL,
    ISEL_LEA, ISEL_SHL, ISEL_XOR,
    ISEL_NOP,
    ISEL_COUNT
} InstructionOp;

typedef struct IRNode IRNode;

struct IRNode {
    IROp op;
    int32_t value;
    char label[64];
    IRNode *left;
    IRNode *right;
};

typedef void (*EmitFn)(IRNode *node, FILE *out);

typedef struct {
    IROp pattern;
    size_t cost;
    EmitFn emit_fn;
    InstructionOp target_op;
    bool is_memory;
} Tile;

typedef struct {
    Tile tiles[MAX_TILESET_SIZE];
    size_t count;
} TileSet;

typedef struct {
    InstructionOp op;
    char src1[32];
    char src2[32];
    char dst[32];
    bool has_label;
    char label[64];
} InstructionNode;

typedef struct {
    InstructionNode *instructions;
    size_t count;
    size_t capacity;
} InstructionList;

void isel_init(TileSet *ts);
void isel_register_tile(TileSet *ts, IROp pattern, size_t cost, EmitFn emit, InstructionOp target_op);
void isel_tile_tree(IRNode *root, TileSet *ts, InstructionList *ilist);
void isel_print_mapping(InstructionList *ilist, FILE *out);
IRNode *ir_node_create(IROp op, int32_t val);
void ir_tree_free(IRNode *node);
void instruction_list_init(InstructionList *ilist);
void instruction_list_add(InstructionList *ilist, InstructionOp op,
                          const char *dst, const char *src1, const char *src2);
void instruction_list_free(InstructionList *ilist);
const char *isel_op_name(InstructionOp op);

#endif
