#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "instruction_selection.h"
#include "reg_alloc.h"
#include "codegen.h"
#include "peephole.h"
#include "abi_target.h"
#include "ssa.h"
#include "cfg.h"
#include "dataflow.h"
#include "stackframe.h"
#include "jit.h"
#include "inst_sched.h"

static int tests_run = 0;
static int tests_passed = 0;

int main(void) {
    printf("\n=== Test ===\n\n"); fflush(stdout);

    printf("1-isel\n"); fflush(stdout); tests_run++;
    IRNode *n = ir_node_create(IRO_CONST, 42);
    if (!n || n->op != IRO_CONST || n->value != 42) { printf("FAIL1\n"); return 1; }
    ir_tree_free(n); tests_passed++;

    printf("2-regalloc\n"); fflush(stdout); tests_run++;
    RegAllocContext ra; ra_init_context(&ra, 4);
    ra_add_interval(&ra, 0, 0, 5); ra_add_interval(&ra, 1, 3, 8);
    ra_linear_scan(&ra); tests_passed++;

    printf("3-codegen\n"); fflush(stdout); tests_run++;
    CodeGen cg; codegen_init(&cg, ARCH_X86);
    IRFunction f = {"t", 0, 0};
    IRNode *r = ir_node_create(IRO_CONST, 42);
    codegen_run(&cg, &f, r); ir_tree_free(r); codegen_free(&cg); tests_passed++;

    printf("4-peephole\n"); fflush(stdout); tests_run++;
    PeepholeContext pc; peephole_init_rules(&pc);
    if (pc.rule_count == 0) { printf("FAIL4\n"); return 1; }
    tests_passed++;

    printf("5-abi\n"); fflush(stdout); tests_run++;
    ABIInfo a; abi_init(&a, ABI_X86_64_SYSV);
    if (a.num_arg_regs != 6) { printf("FAIL5\n"); return 1; }
    tests_passed++;

    printf("6-ssa\n"); fflush(stdout); tests_run++;
    SSAContext s; ssa_init(&s);
    ssa_add_basic_block(&s); ssa_add_basic_block(&s);
    ssa_add_basic_block(&s); ssa_add_basic_block(&s);
    ssa_add_edge(&s, 0, 1); ssa_add_edge(&s, 0, 2);
    ssa_add_edge(&s, 1, 3); ssa_add_edge(&s, 2, 3);
    ssa_build_dominance(&s); ssa_free(&s); tests_passed++;

    printf("7-cfg\n"); fflush(stdout); tests_run++;
    CFG c; cfg_init(&c);
    cfg_add_node(&c, "A"); cfg_add_node(&c, "B");
    cfg_add_edge(&c, 0, 1, EDGE_FALLTHROUGH);
    cfg_set_entry(&c, 0); int32_t idom[2];
    cfg_compute_dominators(&c, idom);
    if (idom[0] != 0) { printf("FAIL7\n"); return 1; }
    cfg_free(&c); tests_passed++;

    printf("8-dataflow\n"); fflush(stdout); tests_run++;
    DataFlowContext d; df_init(&d);
    df_add_var(&d, "x"); df_add_block(&d); df_add_block(&d);
    df_add_stmt(&d, DF_DEFINE, 0, 1, "x=1");
    df_add_stmt(&d, DF_USE, 0, 2, "use x");
    df_compute_liveness(&d); df_free(&d); tests_passed++;

    printf("9-stackframe\n"); fflush(stdout); tests_run++;
    StackFrame sf; stackframe_init(&sf, 16);
    stackframe_alloc_local(&sf, "v", 8, 8);
    stackframe_alloc_spill(&sf, 0);
    stackframe_layout(&sf, FRAME_LAYOUT_X86);
    if (sf.total_size <= 0) { printf("FAIL9\n"); return 1; }
    tests_passed++;

    printf("10-jit\n"); fflush(stdout); tests_run++;
    JITCompiler j; jit_init(&j); jit_free(&j); tests_passed++;

    printf("11-sched\n"); fflush(stdout); tests_run++;
    SchedDAG dag; sched_init(&dag);
    InstructionNode in; memset(&in, 0, sizeof(in)); in.op = ISEL_ADD;
    sched_add_node(&dag, &in, 1, "ALU");
    SchedResult sr; memset(&sr, 0, sizeof(sr));
    sched_list_schedule(&dag, &sr);
    if (sr.makespan <= 0) { printf("FAIL11\n"); return 1; }
    sched_result_free(&sr); sched_free(&dag); tests_passed++;

    printf("\nResults: %d/%d\n", tests_passed, tests_run);
    return (tests_passed == tests_run) ? 0 : 1;
}
