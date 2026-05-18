#include <stdio.h>
#include <stdlib.h>

#include "instruction_selection.h"

int main(void) {
    printf("=== Instruction Selection Demo - Maximal Munch ===\n\n");

    IRNode *imm_8  = ir_node_create(IRO_CONST, 8);
    IRNode *base   = ir_node_create(IRO_BASE, 0);
    snprintf(base->label, sizeof(base->label), "rbp");

    IRNode *addr   = ir_node_create(IRO_ADD, 0);
    addr->left  = base;
    addr->right = imm_8;

    IRNode *mem    = ir_node_create(IRO_MEM, 0);
    mem->left = addr;

    IRNode *deref  = ir_node_create(IRO_DEREF, 0);
    deref->left = mem;

    printf("IR tree: mem(deref(add(base(rbp), const(8))))\n\n");

    TileSet ts;
    isel_init(&ts);

    InstructionList ilist;
    instruction_list_init(&ilist);

    isel_tile_tree(deref, &ts, &ilist);

    printf("Generated instructions (%zu):\n", ilist.count);
    isel_print_mapping(&ilist, stdout);

    ir_tree_free(deref);
    instruction_list_free(&ilist);

    printf("\n=== Second example: add(load(addr), const(4)) ===\n\n");

    IRNode *addr2  = ir_node_create(IRO_BASE, 0);
    snprintf(addr2->label, sizeof(addr2->label), "rsp");

    IRNode *load2  = ir_node_create(IRO_LOAD, 0);
    load2->left = addr2;

    IRNode *const4 = ir_node_create(IRO_CONST, 4);

    IRNode *add2   = ir_node_create(IRO_ADD, 0);
    add2->left  = load2;
    add2->right = const4;

    InstructionList ilist2;
    instruction_list_init(&ilist2);
    isel_tile_tree(add2, &ts, &ilist2);

    printf("Generated instructions (%zu):\n", ilist2.count);
    isel_print_mapping(&ilist2, stdout);

    ir_tree_free(add2);
    instruction_list_free(&ilist2);

    printf("\n=== Third example: store(deref(addr), const(42)) ===\n\n");

    IRNode *addr3  = ir_node_create(IRO_BASE, 0);
    snprintf(addr3->label, sizeof(addr3->label), "rbp");
    IRNode *off3   = ir_node_create(IRO_CONST, 16);
    IRNode *add3   = ir_node_create(IRO_ADD, 0);
    add3->left = addr3;
    add3->right = off3;

    IRNode *mem3   = ir_node_create(IRO_MEM, 0);
    mem3->left = add3;

    IRNode *const42 = ir_node_create(IRO_CONST, 42);

    IRNode *store3 = ir_node_create(IRO_STORE, 0);
    store3->left = mem3;
    store3->right = const42;

    InstructionList ilist3;
    instruction_list_init(&ilist3);
    isel_tile_tree(store3, &ts, &ilist3);

    printf("Generated instructions (%zu):\n", ilist3.count);
    isel_print_mapping(&ilist3, stdout);

    ir_tree_free(store3);
    instruction_list_free(&ilist3);

    return 0;
}
