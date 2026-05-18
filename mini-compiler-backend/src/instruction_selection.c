#include "instruction_selection.h"

static const char *op_names[] = {
    "mov", "add", "sub", "mul", "div",
    "load", "store", "push", "pop", "ret", "call",
    "cmp", "jmp", "je", "jne", "jl",
    "lea", "shl", "xor", "nop"
};

__attribute__((unused))
static const char *ir_op_names[] = {
    "ADD", "SUB", "MUL", "DIV",
    "LOAD", "STORE",
    "MEM", "DEREF",
    "CONST", "TEMP",
    "BASE", "IMM",
    "LABEL", "CALL",
    "RETURN", "CMP",
    "JMP", "JE", "JNE", "JL",
    "PUSH", "POP"
};

const char *isel_op_name(InstructionOp op) {
    if (op >= 0 && op < ISEL_COUNT) return op_names[op];
    return "???";
}

void isel_init(TileSet *ts) {
    ts->count = 0;
    memset(ts->tiles, 0, sizeof(ts->tiles));
}

void isel_register_tile(TileSet *ts, IROp pattern, size_t cost,
                        EmitFn emit, InstructionOp target_op) {
    if (ts->count >= MAX_TILESET_SIZE) return;
    Tile *t = &ts->tiles[ts->count++];
    t->pattern = pattern;
    t->cost = cost;
    t->emit_fn = emit;
    t->target_op = target_op;
    t->is_memory = (pattern == IRO_MEM || pattern == IRO_DEREF ||
                    pattern == IRO_LOAD || pattern == IRO_STORE);
}

IRNode *ir_node_create(IROp op, int32_t val) {
    IRNode *n = (IRNode *)calloc(1, sizeof(IRNode));
    if (!n) return NULL;
    n->op = op;
    n->value = val;
    n->label[0] = '\0';
    n->left = NULL;
    n->right = NULL;
    return n;
}

void ir_tree_free(IRNode *node) {
    if (!node) return;
    ir_tree_free(node->left);
    ir_tree_free(node->right);
    free(node);
}

void instruction_list_init(InstructionList *ilist) {
    ilist->capacity = 64;
    ilist->instructions = (InstructionNode *)calloc(ilist->capacity,
                                                     sizeof(InstructionNode));
    ilist->count = 0;
}

void instruction_list_add(InstructionList *ilist, InstructionOp op,
                          const char *dst, const char *src1, const char *src2) {
    if (ilist->count >= ilist->capacity) {
        ilist->capacity *= 2;
        ilist->instructions = (InstructionNode *)realloc(ilist->instructions,
            ilist->capacity * sizeof(InstructionNode));
    }
    InstructionNode *in = &ilist->instructions[ilist->count++];
    memset(in, 0, sizeof(InstructionNode));
    in->op = op;
    if (dst) snprintf(in->dst, sizeof(in->dst), "%s", dst);
    if (src1) snprintf(in->src1, sizeof(in->src1), "%s", src1);
    if (src2) snprintf(in->src2, sizeof(in->src2), "%s", src2);
}

void instruction_list_free(InstructionList *ilist) {
    free(ilist->instructions);
    ilist->instructions = NULL;
    ilist->count = 0;
    ilist->capacity = 0;
}

__attribute__((unused))
static Tile *find_tile(TileSet *ts, IROp op) {
    for (size_t i = 0; i < ts->count; i++) {
        if (ts->tiles[i].pattern == op) return &ts->tiles[i];
    }
    return NULL;
}

__attribute__((unused))
static void emit_mov(IRNode *node, FILE *out) { (void)node; (void)out; }
__attribute__((unused))
static void emit_alu(IRNode *node, FILE *out) { (void)node; (void)out; }
__attribute__((unused))
static void emit_load(IRNode *node, FILE *out) { (void)node; (void)out; }
__attribute__((unused))
static void emit_store(IRNode *node, FILE *out) { (void)node; (void)out; }
__attribute__((unused))
static void emit_push(IRNode *node, FILE *out) { (void)node; (void)out; }
__attribute__((unused))
static void emit_pop(IRNode *node, FILE *out) { (void)node; (void)out; }
__attribute__((unused))
static void emit_ret(IRNode *node, FILE *out) { (void)node; (void)out; }

static TileSet *global_ts = NULL;
static InstructionList *global_ilist = NULL;

static void munch_node(IRNode *node);

static void munch_add(IRNode *node) {
    munch_node(node->left);
    munch_node(node->right);
    instruction_list_add(global_ilist, ISEL_ADD, "r0", "r0", "r1");
}

static void munch_sub(IRNode *node) {
    munch_node(node->left);
    munch_node(node->right);
    instruction_list_add(global_ilist, ISEL_SUB, "r0", "r0", "r1");
}

static void munch_mul(IRNode *node) {
    munch_node(node->left);
    munch_node(node->right);
    instruction_list_add(global_ilist, ISEL_MUL, "r0", "r0", "r1");
}

static void munch_load(IRNode *node) {
    munch_node(node->left);
    instruction_list_add(global_ilist, ISEL_LOAD, "r0", "[r0]", "");
}

static void munch_store(IRNode *node) {
    munch_node(node->left);
    munch_node(node->right);
    instruction_list_add(global_ilist, ISEL_STORE, "[r1]", "r0", "");
}

static void munch_const(IRNode *node) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%d", node->value);
    instruction_list_add(global_ilist, ISEL_MOV, "r0", buf, "");
}

static void munch_temp(IRNode *node) {
    (void)node;
    instruction_list_add(global_ilist, ISEL_MOV, "r0", "r0", "");
}

static void munch_mem(IRNode *node) {
    munch_node(node->left);
    instruction_list_add(global_ilist, ISEL_LEA, "r0", "[r0]", "");
}

static void munch_default(IRNode *node) {
    if (node->left) munch_node(node->left);
    if (node->right) munch_node(node->right);
}

static void munch_node(IRNode *node) {
    if (!node) return;
    switch (node->op) {
        case IRO_ADD:    munch_add(node);    break;
        case IRO_SUB:    munch_sub(node);    break;
        case IRO_MUL:    munch_mul(node);    break;
        case IRO_LOAD:   munch_load(node);   break;
        case IRO_STORE:  munch_store(node);  break;
        case IRO_CONST:  munch_const(node);  break;
        case IRO_TEMP:   munch_temp(node);   break;
        case IRO_MEM:    munch_mem(node);    break;
        default:         munch_default(node); break;
    }
}

void isel_tile_tree(IRNode *root, TileSet *ts, InstructionList *ilist) {
    global_ts = ts;
    global_ilist = ilist;
    munch_node(root);
    global_ts = NULL;
    global_ilist = NULL;
}

void isel_print_mapping(InstructionList *ilist, FILE *out) {
    if (!ilist || !out) return;
    fprintf(out, ";;; Instruction Selection Mapping:\n");
    for (size_t i = 0; i < ilist->count; i++) {
        InstructionNode *in = &ilist->instructions[i];
        const char *opn = isel_op_name(in->op);
        if (in->op == ISEL_LOAD) {
            fprintf(out, "  %-6s %s, %s\n", opn, in->dst, in->src1);
        } else if (in->op == ISEL_STORE) {
            fprintf(out, "  %-6s %s, %s\n", opn, in->dst, in->src1);
        } else if (in->op == ISEL_RET || in->op == ISEL_NOP) {
            fprintf(out, "  %-6s\n", opn);
        } else if (in->src2[0] != '\0') {
            fprintf(out, "  %-6s %s, %s, %s\n", opn, in->dst, in->src1, in->src2);
        } else if (in->src1[0] != '\0') {
            fprintf(out, "  %-6s %s, %s\n", opn, in->dst, in->src1);
        } else if (in->dst[0] != '\0') {
            fprintf(out, "  %-6s %s\n", opn, in->dst);
        } else {
            fprintf(out, "  %-6s\n", opn);
        }
    }
}
