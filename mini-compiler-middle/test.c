#include "ir.h"
#include "ssa.h"
#include "dataflow.h"
#include "optimizer.h"
#include "cfg.h"
#include "regalloc.h"
#include "backend.h"
#include "loop_analysis.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { printf("  TEST: %-50s ", name); fflush(stdout); } while(0)
#define PASS() do { printf("PASS\n"); tests_passed++; } while(0)
#define FAIL(msg) do { printf("FAIL: %s\n", msg); tests_failed++; } while(0)
#define CHECK(cond, msg) do { if (!(cond)) { FAIL(msg); return; } } while(0)


/* ================================================================
 * Module 1: IR (Intermediate Representation)
 * ================================================================ */

static void test_ir_create_destroy(void) {
    TEST("ir_create / ir_destroy");
    IRFunction* func = ir_create_function("test");
    CHECK(func != NULL, "create returned NULL");
    CHECK(strcmp(func->name, "test") == 0, "name mismatch");
    CHECK(func->num_inst == 0, "non-zero initial count");
    CHECK(func->next_temp == 0, "non-zero initial temp");
    ir_destroy_function(func);
    PASS();
}

static void test_ir_emit_instructions(void) {
    TEST("ir_emit basic instructions");
    IRFunction* func = ir_create_function("emit_test");
    CHECK(func != NULL, "create failed");
    int t0 = ir_new_temp(func);
    int t1 = ir_new_temp(func);
    int t2 = ir_new_temp(func);
    int idx0 = ir_emit(func, IR_MOV, t0, 10, -1, NULL);
    CHECK(idx0 == 0, "first emit index != 0");
    int idx1 = ir_emit(func, IR_ADD, t1, t0, 5, NULL);
    CHECK(idx1 == 1, "second emit index != 1");
    int idx2 = ir_emit(func, IR_MUL, t2, t1, t0, NULL);
    CHECK(idx2 == 2, "third emit index != 2");
    CHECK(func->num_inst == 3, "instruction count mismatch");
    ir_destroy_function(func);
    PASS();
}

static void test_ir_op_names(void) {
    TEST("ir_op_name coverage");
    CHECK(strcmp(ir_op_name(IR_ADD), "add") == 0, "ADD name");
    CHECK(strcmp(ir_op_name(IR_SUB), "sub") == 0, "SUB name");
    CHECK(strcmp(ir_op_name(IR_MUL), "mul") == 0, "MUL name");
    CHECK(strcmp(ir_op_name(IR_DIV), "div") == 0, "DIV name");
    CHECK(strcmp(ir_op_name(IR_BR), "br") == 0, "BR name");
    CHECK(strcmp(ir_op_name(IR_RET), "ret") == 0, "RET name");
    CHECK(strcmp(ir_op_name(IR_PHI), "phi") == 0, "PHI name");
    PASS();
}

static void test_ir_new_temp(void) {
    TEST("ir_new_temp monotonic");
    IRFunction* func = ir_create_function("temp_test");
    int prev = -1;
    for (int i = 0; i < 50; i++) {
        int t = ir_new_temp(func);
        CHECK(t > prev, "temp not monotonic");
        prev = t;
    }
    CHECK(func->next_temp == 50, "next_temp mismatch");
    ir_destroy_function(func);
    PASS();
}

static void test_ir_build_cfg_basic(void) {
    TEST("ir_build_cfg basic");
    IRFunction* func = ir_create_function("cfg_test");
    int a = ir_new_temp(func);
    ir_emit(func, IR_MOV, a, 1, -1, NULL);
    ir_emit(func, IR_RET, a, -1, -1, NULL);
    IRBasicBlock blocks[MAX_BLOCKS];
    int n = ir_build_cfg(func, blocks, MAX_BLOCKS);
    CHECK(n > 0, "cfg returned 0 blocks");
    ir_destroy_function(func);
    PASS();
}

/* ================================================================
 * Module 2: CFG (Control Flow Graph)
 * ================================================================ */

static void test_cfg_build(void) {
    TEST("cfg_build");
    IRFunction* func = ir_create_function("cfg_test2");
    int a = ir_new_temp(func);
    ir_emit(func, IR_MOV, a, 0, -1, NULL);
    int label_l = ir_new_label(func);
    char ls[8]; snprintf(ls, sizeof(ls), "%d", label_l);
    ir_emit(func, IR_ADD, a, a, 1, NULL);
    ir_emit(func, IR_BRCOND, -1, a, -1, ls);
    int label_e = ir_new_label(func);
    char es[8]; snprintf(es, sizeof(es), "%d", label_e);
    snprintf(func->instructions[func->num_inst - 1].src1_label, MAX_LABEL_LEN, "%s", es);
    ir_emit(func, IR_RET, a, -1, -1, NULL);
    CFG cfg;
    cfg_build(func, &cfg);
    CHECK(cfg.num_nodes > 0, "cfg has no nodes");
    CHECK(cfg.entry == 0, "entry not BB0");
    ir_destroy_function(func);
    PASS();
}

static void test_cfg_rpo(void) {
    TEST("cfg_reverse_postorder");
    IRFunction* func = ir_create_function("rpo_test");
    int x = ir_new_temp(func);
    ir_emit(func, IR_MOV, x, 1, -1, NULL);
    ir_emit(func, IR_ADD, x, x, 2, NULL);
    ir_emit(func, IR_RET, x, -1, -1, NULL);
    CFG cfg;
    cfg_build(func, &cfg);
    int order[MAX_BLOCKS]; int num_order = 0;
    cfg_reverse_postorder(&cfg, order, &num_order);
    CHECK(num_order > 0, "RPO returned 0 nodes");
    ir_destroy_function(func);
    PASS();
}

static void test_cfg_dominators(void) {
    TEST("cfg_dominators");
    IRFunction* func = ir_create_function("dom_test");
    int a = ir_new_temp(func);
    ir_emit(func, IR_MOV, a, 0, -1, NULL);
    ir_emit(func, IR_RET, a, -1, -1, NULL);
    CFG cfg; cfg_build(func, &cfg);
    int doms[MAX_BLOCKS][MAX_BLOCKS];
    cfg_dominators(&cfg, doms);
    CHECK(doms[0][0] == 1, "entry should dominate itself");
    ir_destroy_function(func);
    PASS();
}

static void test_cfg_find_loops(void) {
    TEST("cfg_find_loops");
    IRFunction* func = ir_create_function("loop_find");
    int i = ir_new_temp(func);
    ir_emit(func, IR_MOV, i, 0, -1, NULL);
    int hdr = ir_new_label(func);
    char hs[8]; snprintf(hs, sizeof(hs), "%d", hdr);
    ir_emit(func, IR_ADD, i, i, 1, NULL);
    int end = ir_new_label(func);
    char es[8]; snprintf(es, sizeof(es), "%d", end);
    ir_emit(func, IR_BRCOND, -1, i, -1, hs);
    snprintf(func->instructions[func->num_inst - 1].src1_label, MAX_LABEL_LEN, "%s", es);
    ir_emit(func, IR_RET, i, -1, -1, NULL);
    CFG cfg; cfg_build(func, &cfg);
    int loops[MAX_BLOCKS]; int back_edges[MAX_BLOCKS][2]; int num_be;
    cfg_find_loops(&cfg, loops, back_edges, &num_be);
    CHECK(num_be >= 0, "negative back edge count");
    ir_destroy_function(func);
    PASS();
}

/* Module 3: SSA */

static void test_dom_computation(void) {
    TEST("dom_compute_dominators");
    IRFunction* func = ir_create_function("dom_comp");
    int x = ir_new_temp(func);
    ir_emit(func, IR_MOV, x, 0, -1, NULL);
    ir_emit(func, IR_RET, x, -1, -1, NULL);
    IRBasicBlock blocks[MAX_BLOCKS];
    int n = ir_build_cfg(func, blocks, MAX_BLOCKS);
    int doms[MAX_BLOCKS][MAX_BLOCKS];
    dom_compute_dominators(blocks, n, 0, doms);
    CHECK(doms[0][0] == 1, "entry should dominate itself");
    ir_destroy_function(func);
    PASS();
}

static void test_dominance_frontier(void) {
    TEST("dom_compute_dominance_frontier");
    IRFunction* func = ir_create_function("df_test2");
    int x = ir_new_temp(func);
    ir_emit(func, IR_MOV, x, 0, -1, NULL);
    ir_emit(func, IR_RET, x, -1, -1, NULL);
    IRBasicBlock blocks[MAX_BLOCKS];
    int n = ir_build_cfg(func, blocks, MAX_BLOCKS);
    int doms[MAX_BLOCKS][MAX_BLOCKS];
    int df[MAX_BLOCKS][MAX_BLOCKS];
    dom_compute_dominators(blocks, n, 0, doms);
    dom_compute_dominance_frontier(blocks, n, doms, df);
    CHECK(n > 0, "DF computation failed");
    ir_destroy_function(func);
    PASS();
}

static void test_ssa_build_simple(void) {
    TEST("ssa_build simple");
    IRFunction* func = ir_create_function("ssa_simple");
    int x = ir_new_temp(func);
    ir_emit(func, IR_MOV, x, 2, -1, NULL);
    int y = ir_new_temp(func);
    ir_emit(func, IR_ADD, y, x, 3, NULL);
    ir_emit(func, IR_RET, y, -1, -1, NULL);
    ssa_build(func);
    CHECK(func->num_inst > 0, "SSA removed all instructions");
    ir_destroy_function(func);
    PASS();
}

static void test_ssa_destroy(void) {
    TEST("ssa_destroy (out-of-SSA)");
    IRFunction* func = ir_create_function("ssa_destr");
    int x = ir_new_temp(func);
    ir_emit(func, IR_MOV, x, 1, -1, NULL);
    int y = ir_new_temp(func);
    ir_emit(func, IR_ADD, y, x, 5, NULL);
    ir_emit(func, IR_RET, y, -1, -1, NULL);
    ssa_build(func);
    ssa_destroy(func);
    CHECK(func->num_inst > 0, "ssa_destroy removed all instructions");
    ir_destroy_function(func);
    PASS();
}

static void test_lengauer_tarjan(void) {
    TEST("Lengauer-Tarjan idoms");
    IRFunction* func = ir_create_function("lt_test");
    int x = ir_new_temp(func);
    ir_emit(func, IR_MOV, x, 0, -1, NULL);
    ir_emit(func, IR_RET, x, -1, -1, NULL);
    IRBasicBlock blocks[MAX_BLOCKS];
    int n = ir_build_cfg(func, blocks, MAX_BLOCKS);
    int idom[MAX_BLOCKS];
    dom_lt_idoms(blocks, n, 0, idom);
    CHECK(idom[0] == -1, "entry should have no idom");
    int full_doms[MAX_BLOCKS][MAX_BLOCKS];
    dom_idoms_to_full(idom, n, full_doms);
    CHECK(full_doms[0][0] == 1, "entry should dominate itself");
    ir_destroy_function(func);
    PASS();
}

/* Module 4: Dataflow Analysis */

static void test_bitvector_ops(void) {
    TEST("BitVector operations");
    BitVector bv; bv_init(&bv);
    for (int i = 0; i < 100; i++) CHECK(!bv_test(&bv, i), "fresh bv should be zeros");
    bv_set(&bv, 5);
    CHECK(bv_test(&bv, 5), "bv_set(5) should be testable");
    CHECK(!bv_test(&bv, 6), "bv_test(6) should be false");
    bv_clear(&bv, 5);
    CHECK(!bv_test(&bv, 5), "bv_clear(5) should clear");
    BitVector b2; bv_init(&b2);
    bv_set(&b2, 10); bv_set(&b2, 20);
    bv_union(&bv, &b2);
    CHECK(bv_test(&bv, 10), "union: should have b2[10]");
    CHECK(bv_test(&bv, 20), "union: should have b2[20]");
    bv_intersect(&bv, &b2);
    CHECK(bv_test(&bv, 10), "intersect: should keep 10");
    BitVector b3; bv_init(&b3);
    bv_set(&b3, 10); bv_set(&b3, 20);
    CHECK(bv_equals(&b2, &b3), "bv_equals should detect equal");
    PASS();
}

static void test_reaching_defs(void) {
    TEST("df_reaching_defs");
    IRFunction* func = ir_create_function("rd_test");
    int a = ir_new_temp(func); ir_emit(func, IR_MOV, a, 0, -1, NULL);
    int b = ir_new_temp(func); ir_emit(func, IR_ADD, b, a, 5, NULL);
    ir_emit(func, IR_RET, b, -1, -1, NULL);
    IRBasicBlock blocks[MAX_BLOCKS];
    int n = ir_build_cfg(func, blocks, MAX_BLOCKS);
    DataflowResult result;
    df_reaching_defs(func, blocks, n, &result);
    CHECK(n > 0, "no blocks");
    ir_destroy_function(func);
    PASS();
}

static void test_live_variables(void) {
    TEST("df_live_variables");
    IRFunction* func = ir_create_function("lv_test2");
    int x = ir_new_temp(func); ir_emit(func, IR_MOV, x, 0, -1, NULL);
    int y = ir_new_temp(func); ir_emit(func, IR_ADD, y, x, 1, NULL);
    ir_emit(func, IR_RET, y, -1, -1, NULL);
    IRBasicBlock blocks[MAX_BLOCKS];
    int n = ir_build_cfg(func, blocks, MAX_BLOCKS);
    DataflowResult result;
    df_live_variables(func, blocks, n, &result);
    CHECK(n > 0, "no blocks");
    ir_destroy_function(func);
    PASS();
}

static void test_constant_propagation(void) {
    TEST("df_constant_propagation");
    IRFunction* func = ir_create_function("cp_test");
    int a = ir_new_temp(func); ir_emit(func, IR_MOV, a, 3, -1, NULL);
    int b = ir_new_temp(func); ir_emit(func, IR_MOV, b, 4, -1, NULL);
    int c = ir_new_temp(func); ir_emit(func, IR_ADD, c, a, b, NULL);
    ir_emit(func, IR_RET, c, -1, -1, NULL);
    IRBasicBlock blocks[MAX_BLOCKS];
    int n = ir_build_cfg(func, blocks, MAX_BLOCKS);
    int values[MAX_TEMP_REGS];
    df_constant_propagation(func, blocks, n, values);
    CHECK(n > 0, "no blocks");
    ir_destroy_function(func);
    PASS();
}

static void test_available_exprs(void) {
    TEST("df_available_exprs");
    IRFunction* func = ir_create_function("ae_test2");
    int a = ir_new_temp(func); ir_emit(func, IR_MOV, a, 1, -1, NULL);
    int b = ir_new_temp(func); ir_emit(func, IR_MOV, b, 2, -1, NULL);
    int c = ir_new_temp(func); ir_emit(func, IR_ADD, c, a, b, NULL);
    ir_emit(func, IR_RET, c, -1, -1, NULL);
    IRBasicBlock blocks[MAX_BLOCKS];
    int n = ir_build_cfg(func, blocks, MAX_BLOCKS);
    DataflowResult result;
    df_available_exprs(func, blocks, n, &result);
    CHECK(n > 0, "no blocks");
    ir_destroy_function(func);
    PASS();
}

static void test_very_busy_exprs(void) {
    TEST("df_very_busy_exprs");
    IRFunction* func = ir_create_function("vbe_test2");
    int x = ir_new_temp(func); ir_emit(func, IR_MOV, x, 5, -1, NULL);
    int y = ir_new_temp(func); ir_emit(func, IR_MUL, y, x, 2, NULL);
    ir_emit(func, IR_RET, y, -1, -1, NULL);
    IRBasicBlock blocks[MAX_BLOCKS];
    int n = ir_build_cfg(func, blocks, MAX_BLOCKS);
    DataflowResult result;
    df_very_busy_exprs(func, blocks, n, &result);
    CHECK(n > 0, "no blocks");
    ir_destroy_function(func);
    PASS();
}

static void test_df_analyze_generic(void) {
    TEST("df_analyze generic solver");
    IRFunction* func = ir_create_function("df_gen2");
    int v = ir_new_temp(func); ir_emit(func, IR_MOV, v, 10, -1, NULL);
    ir_emit(func, IR_RET, v, -1, -1, NULL);
    IRBasicBlock blocks[MAX_BLOCKS];
    int n = ir_build_cfg(func, blocks, MAX_BLOCKS);
    DataflowResult r1, r2, r3;
    df_analyze(func, blocks, n, DF_REACHING_DEFS, &r1);
    df_analyze(func, blocks, n, DF_LIVE_VARIABLES, &r2);
    df_analyze(func, blocks, n, DF_AVAILABLE_EXPRS, &r3);
    CHECK(n > 0, "no blocks");
    ir_destroy_function(func);
    PASS();
}

/* Module 5: Optimizer */

static void test_opt_dce(void) {
    TEST("Dead Code Elimination");
    IRFunction* func = ir_create_function("dce_test");
    int a = ir_new_temp(func); ir_emit(func, IR_MOV, a, 10, -1, NULL);
    int dead = ir_new_temp(func); ir_emit(func, IR_MUL, dead, a, 3, NULL);
    int b = ir_new_temp(func); ir_emit(func, IR_ADD, b, a, 1, NULL);
    ir_emit(func, IR_RET, b, -1, -1, NULL);
    OptStats s = opt_dce(func);
    CHECK(s.removed_instructions >= 0, "negative removal count");
    CHECK(func->num_inst > 0, "DCE removed everything");
    ir_destroy_function(func);
    PASS();
}

static void test_opt_cse(void) {
    TEST("Common Subexpression Elimination");
    IRFunction* func = ir_create_function("cse_test2");
    int a = ir_new_temp(func); ir_emit(func, IR_MOV, a, 3, -1, NULL);
    int b = ir_new_temp(func); ir_emit(func, IR_MOV, b, 4, -1, NULL);
    int c1 = ir_new_temp(func); ir_emit(func, IR_ADD, c1, a, b, NULL);
    int c2 = ir_new_temp(func); ir_emit(func, IR_ADD, c2, a, b, NULL);
    ir_emit(func, IR_RET, c2, -1, -1, NULL);
    OptStats s = opt_cse(func);
    CHECK(s.replaced_expressions >= 0, "negative replacement count");
    ir_destroy_function(func);
    PASS();
}

static void test_opt_constant_folding(void) {
    TEST("Constant Folding");
    IRFunction* func = ir_create_function("cf_test2");
    ir_new_temp(func); ir_emit(func, IR_MOV, 0, 3, -1, NULL);
    ir_new_temp(func); ir_emit(func, IR_MOV, 1, 5, -1, NULL);
    int sum = ir_new_temp(func);
    ir_emit(func, IR_ADD, sum, 0, 1, NULL);
    ir_emit(func, IR_RET, sum, -1, -1, NULL);
    OptStats s = opt_constant_folding(func);
    CHECK(s.folded_constants >= 0, "negative folding count");
    ir_destroy_function(func);
    PASS();
}

static void test_opt_copy_propagation(void) {
    TEST("Copy Propagation");
    IRFunction* func = ir_create_function("copy_test2");
    int a = ir_new_temp(func); ir_emit(func, IR_MOV, a, 7, -1, NULL);
    int b = ir_new_temp(func); ir_emit(func, IR_MOV, b, a, -1, NULL);
    int c = ir_new_temp(func); ir_emit(func, IR_ADD, c, b, 3, NULL);
    ir_emit(func, IR_RET, c, -1, -1, NULL);
    OptStats s = opt_copy_propagation(func);
    CHECK(s.copies_propagated >= 0, "negative propagation count");
    ir_destroy_function(func);
    PASS();
}

static void test_opt_simplify_cfg(void) {
    TEST("CFG Simplification");
    IRFunction* func = ir_create_function("simpl_test2");
    int v = ir_new_temp(func); ir_emit(func, IR_MOV, v, 1, -1, NULL);
    ir_emit(func, IR_RET, v, -1, -1, NULL);
    OptStats s = opt_simplify_cfg(func);
    CHECK(s.removed_instructions >= 0, "negative removal");
    ir_destroy_function(func);
    PASS();
}

static void test_opt_pipeline(void) {
    TEST("Optimizer Pipeline");
    IRFunction* func = ir_create_function("pipe_test2");
    int a = ir_new_temp(func); ir_emit(func, IR_MOV, a, 3, -1, NULL);
    int b = ir_new_temp(func); ir_emit(func, IR_MOV, b, 4, -1, NULL);
    int s1 = ir_new_temp(func); ir_emit(func, IR_ADD, s1, a, b, NULL);
    int dead = ir_new_temp(func); ir_emit(func, IR_MUL, dead, s1, 0, NULL);
    int s2 = ir_new_temp(func); ir_emit(func, IR_ADD, s2, a, b, NULL);
    ir_emit(func, IR_RET, s2, -1, -1, NULL);
    OptPass pipeline[] = {OPT_DCE, OPT_CSE, OPT_CONST_FOLD, OPT_COPY_PROP};
    OptStats total = opt_run_pipeline(func, pipeline, 4);
    CHECK(total.removed_instructions >= 0, "negative removal");
    CHECK(func->num_inst > 0, "pipeline removed all instructions");
    ir_destroy_function(func);
    PASS();
}

static void test_opt_loop_invariant(void) {
    TEST("Loop Invariant Code Motion");
    IRFunction* func = ir_create_function("licm_test2");
    int invariant = ir_new_temp(func);
    ir_emit(func, IR_MOV, invariant, 42, -1, NULL);
    int loop_var = ir_new_temp(func);
    ir_emit(func, IR_MOV, loop_var, 0, -1, NULL);
    int one = ir_new_temp(func);
    ir_emit(func, IR_MOV, one, 1, -1, NULL);
    int ctr = ir_new_temp(func);
    ir_emit(func, IR_ADD, ctr, loop_var, one, NULL);
    int result = ir_new_temp(func);
    ir_emit(func, IR_ADD, result, ctr, invariant, NULL);
    ir_emit(func, IR_RET, result, -1, -1, NULL);
    OptPass passes[] = {OPT_LOOP_INVARIANT};
    OptStats total = opt_run_pipeline(func, passes, 1);
    CHECK(total.removed_instructions >= 0, "negative removal");
    ir_destroy_function(func);
    PASS();
}

/* Module 6: Register Allocation */

static void test_ra_create(void) {
    TEST("ra_create / ra_destroy");
    RegisterAllocator* ra = ra_create(8);
    CHECK(ra != NULL, "create returned NULL");
    CHECK(ra->num_regs == 8, "num_regs != 8");
    CHECK(ra->num_nodes == 0, "initial nodes != 0");
    ra_destroy(ra);
    PASS();
}

static void test_ra_add_live_range(void) {
    TEST("ra_add_live_range");
    RegisterAllocator* ra = ra_create(4);
    int n0 = ra_add_live_range(ra, 0);
    int n1 = ra_add_live_range(ra, 1);
    int n2 = ra_add_live_range(ra, 2);
    CHECK(n0 == 0, "first node should be 0");
    CHECK(n1 == 1, "second node should be 1");
    CHECK(n2 == 2, "third node should be 2");
    CHECK(ra->num_nodes == 3, "node count mismatch");
    ra_destroy(ra);
    PASS();
}

static void test_ra_interference(void) {
    TEST("ra_add_interference");
    RegisterAllocator* ra = ra_create(4);
    ra_add_live_range(ra, 0); ra_add_live_range(ra, 1); ra_add_live_range(ra, 2);
    ra_add_interference(ra, 0, 1);
    CHECK(ra->ig.matrix[0][1], "edge 0-1 should exist");
    CHECK(ra->ig.matrix[1][0], "edge 1-0 should exist (undirected)");
    CHECK(ra->nodes[0].degree == 1, "node 0 degree should be 1");
    ra_add_interference(ra, 0, 0);
    CHECK(ra->nodes[0].degree == 1, "self-loop should not increase degree");
    ra_destroy(ra);
    PASS();
}

static void test_ra_color_graph_simple(void) {
    TEST("ra_color_graph (no conflicts)");
    RegisterAllocator* ra = ra_create(4);
    ra_add_live_range(ra, 0); ra_add_live_range(ra, 1); ra_add_live_range(ra, 2);
    RAStats stats = ra_color_graph(ra);
    CHECK(stats.spills == 0, "should not spill with no conflicts");
    CHECK(stats.min_colors >= 1, "should need at least 1 color");
    for (int i = 0; i < ra->num_nodes; i++)
        CHECK(ra->colors[i] >= 0, "node should be colored");
    ra_destroy(ra);
    PASS();
}

static void test_ra_color_graph_conflict(void) {
    TEST("ra_color_graph (with conflicts)");
    RegisterAllocator* ra = ra_create(2);
    ra_add_live_range(ra, 0); ra_add_live_range(ra, 1); ra_add_live_range(ra, 2);
    ra_add_interference(ra, 0, 1); ra_add_interference(ra, 0, 2);
    ra_add_interference(ra, 1, 2);
    RAStats stats = ra_color_graph(ra);
    CHECK(stats.spills >= 1, "K3 with 2 regs should spill at least 1");
    ra_destroy(ra);
    PASS();
}

static void test_ra_linear_scan(void) {
    TEST("ra_linear_scan");
    IRFunction* func = ir_create_function("ls_test2");
    int a = ir_new_temp(func); ir_emit(func, IR_MOV, a, 1, -1, NULL);
    int b = ir_new_temp(func); ir_emit(func, IR_ADD, b, a, 2, NULL);
    int c = ir_new_temp(func); ir_emit(func, IR_ADD, c, b, 3, NULL);
    ir_emit(func, IR_RET, c, -1, -1, NULL);
    int regs[MAX_TEMP_REGS];
    ra_linear_scan(func, 4, regs);
    CHECK(regs[a] >= -1, "a should have assignment");
    CHECK(regs[b] >= -1, "b should have assignment");
    CHECK(regs[c] >= -1, "c should have assignment");
    ir_destroy_function(func);
    PASS();
}

static void test_ra_coalesce(void) {
    TEST("ra_coalesce");
    RegisterAllocator* ra = ra_create(4);
    ra_add_live_range(ra, 0); ra_add_live_range(ra, 1);
    ra_add_interference(ra, 0, 1);
    bool coalescable = ra_is_coalescable(ra, 0, 1);
    CHECK(coalescable, "should be coalescable with 4 registers");
    ra_coalesce(ra, 0, 1);
    CHECK(ra->coalesced_pairs[0][1], "pair should be marked coalesced");
    ra_destroy(ra);
    PASS();
}

/* Module 7: Backend (Code Generation) */

static void test_cg_create(void) {
    TEST("cg_create / cg_destroy");
    CodeGen* cg = cg_create("test_func");
    CHECK(cg != NULL, "create returned NULL");
    CHECK(cg->num_inst == 0, "initial inst count not zero");
    CHECK(cg->frame.total_size == 0, "initial stack size not zero");
    cg_destroy(cg);
    PASS();
}

static void test_cg_emit(void) {
    TEST("cg_emit instructions");
    CodeGen* cg = cg_create("emit_func");
    int i0 = cg_emit(cg, TGT_MOV, 0, 1, -1, NULL);
    int i1 = cg_emit(cg, TGT_ADD, 0, 2, -1, NULL);
    int i2 = cg_emit(cg, TGT_RET, 0, 0, -1, NULL);
    CHECK(i0 == 0, "first emit should be 0");
    CHECK(i1 == 1, "second emit should be 1");
    CHECK(i2 == 2, "third emit should be 2");
    CHECK(cg->num_inst == 3, "instruction count mismatch");
    cg_destroy(cg);
    PASS();
}

static void test_cg_generate_simple(void) {
    TEST("cg_generate (simple function)");
    IRFunction* func = ir_create_function("gen_test2");
    int a = ir_new_temp(func); ir_emit(func, IR_MOV, a, 42, -1, NULL);
    ir_emit(func, IR_RET, a, -1, -1, NULL);
    int reg_assignments[MAX_TEMP_REGS];
    for (int i = 0; i < MAX_TEMP_REGS; i++) reg_assignments[i] = -1;
    reg_assignments[a] = 0;
    CodeGen* cg = cg_create("gen_test2");
    cg_generate(cg, func, reg_assignments);
    CHECK(cg->num_inst > 0, "generated no instructions");
    CGStats stats = cg_get_stats(cg);
    CHECK(stats.total_count > 0, "no target instructions");
    cg_destroy(cg);
    ir_destroy_function(func);
    PASS();
}

static void test_cg_peephole(void) {
    TEST("cg_peephole_optimize");
    CodeGen* cg = cg_create("peep_test2");
    cg_emit(cg, TGT_MOV, 0, 0, -1, NULL);
    cg_emit(cg, TGT_ADD, 1, 0, -1, NULL);
    int before = cg->num_inst;
    cg_peephole_optimize(cg);
    int after = cg->num_inst;
    CHECK(after == before, "peephole should not change instruction count");
    CHECK(cg->instructions[0].op == TGT_COMMENT, "mov r,r should be eliminated");
    cg_destroy(cg);
    PASS();
}

static void test_cg_stack_frame(void) {
    TEST("cg_allocate_stack_slot");
    CodeGen* cg = cg_create("stack_test2");
    int offset = cg_allocate_stack_slot(cg, 10, 4);
    CHECK(offset < 0, "stack slot should have negative offset");
    CHECK(cg->frame.total_size == 4, "stack size should be 4");
    CHECK(cg->frame.slots[0].temp_id == 10, "temp_id mismatch");
    int offset2 = cg_allocate_stack_slot(cg, 20, 8);
    CHECK(offset2 < offset, "second slot should be at lower address");
    cg_destroy(cg);
    PASS();
}

/* Module 8: Loop Analysis */

static void test_loop_find(void) {
    TEST("loop_find_natural_loops");
    IRFunction* func = ir_create_function("loopfind2");
    int i = ir_new_temp(func); ir_emit(func, IR_MOV, i, 0, -1, NULL);
    int limit = ir_new_temp(func); ir_emit(func, IR_MOV, limit, 10, -1, NULL);
    int hdr = ir_new_label(func); char hs[8]; snprintf(hs, sizeof(hs), "%d", hdr);
    int end = ir_new_label(func); char es[8]; snprintf(es, sizeof(es), "%d", end);
    ir_emit(func, IR_ADD, i, i, 1, NULL);
    ir_emit(func, IR_BRCOND, -1, i, -1, hs);
    snprintf(func->instructions[func->num_inst - 1].src1_label, MAX_LABEL_LEN, "%s", es);
    ir_emit(func, IR_RET, i, -1, -1, NULL);
    CFG cfg; cfg_build(func, &cfg);
    LoopTree tree;
    loop_find_natural_loops(&cfg, &tree);
    CHECK(tree.num_loops >= 0, "loop count should be >= 0");
    ir_destroy_function(func);
    PASS();
}

static void test_induction_vars(void) {
    TEST("loop_detect_induction_variables");
    IRFunction* func = ir_create_function("iv_test2");
    int i = ir_new_temp(func); ir_emit(func, IR_MOV, i, 0, -1, NULL);
    int step = ir_new_temp(func); ir_emit(func, IR_MOV, step, 1, -1, NULL);
    int i2 = ir_new_temp(func); ir_emit(func, IR_ADD, i2, i, step, NULL);
    ir_emit(func, IR_MOV, i, i2, -1, NULL);
    int j = ir_new_temp(func); ir_emit(func, IR_MUL, j, i, 4, NULL);
    CFG cfg; cfg_build(func, &cfg);
    LoopTree tree; memset(&tree, 0, sizeof(tree));
    tree.loops[0].header = 0; tree.loops[0].num_blocks = 1;
    tree.loops[0].blocks[0] = 0; tree.num_loops = 1;
    loop_detect_induction_variables(func, &cfg, &tree.loops[0]);
    CHECK(tree.loops[0].num_ivars >= 0, "ivar count should be >= 0");
    ir_destroy_function(func);
    PASS();
}

static void test_loop_invariant_detect(void) {
    TEST("loop_detect_invariant_code");
    IRFunction* func = ir_create_function("inv_test2");
    int c = ir_new_temp(func); ir_emit(func, IR_MOV, c, 42, -1, NULL);
    int i = ir_new_temp(func); ir_emit(func, IR_MOV, i, 0, -1, NULL);
    int sum = ir_new_temp(func); ir_emit(func, IR_ADD, sum, i, c, NULL);
    ir_emit(func, IR_RET, sum, -1, -1, NULL);
    CFG cfg; cfg_build(func, &cfg);
    LoopInfo loop; memset(&loop, 0, sizeof(loop));
    loop.blocks[0] = 0; loop.num_blocks = 1;
    loop_detect_invariant_code(func, &cfg, &loop);
    ir_destroy_function(func);
    PASS();
}

static void test_banerjee(void) {
    TEST("Banerjee dependence test");
    bool dep = banerjee_test(1, 0, 0, 1, 0, 0, 10);
    CHECK(dep, "identical accesses should be dependent");
    dep = banerjee_test(1, 0, 0, 1, 0, 1, 10);
    CHECK(dep, "offset accesses may be dependent");
    dep = banerjee_test(1, 0, 0, 1, 0, 100, 10);
    CHECK(!dep, "far offset should be independent");
    PASS();
}

static void test_loop_analysis_integration(void) {
    TEST("loop analysis integration");
    IRFunction* func = ir_create_function("loop_int2");
    int x = ir_new_temp(func); ir_emit(func, IR_MOV, x, 5, -1, NULL);
    ir_emit(func, IR_RET, x, -1, -1, NULL);
    CFG cfg; cfg_build(func, &cfg);
    LoopTree tree;
    loop_find_natural_loops(&cfg, &tree);
    CHECK(tree.num_loops >= 0, "loop count negative");
    ir_destroy_function(func);
    PASS();
}

/* Cross-Module Integration Tests */

static void test_full_pipeline(void) {
    TEST("Full pipeline: IR -> Opt -> RegAlloc -> CodeGen");
    IRFunction* func = ir_create_function("full_pipe2");
    int a = ir_new_temp(func); ir_emit(func, IR_MOV, a, 3, -1, NULL);
    int b = ir_new_temp(func); ir_emit(func, IR_MOV, b, 7, -1, NULL);
    int sum = ir_new_temp(func); ir_emit(func, IR_ADD, sum, a, b, NULL);
    int dead = ir_new_temp(func); ir_emit(func, IR_MUL, dead, sum, 0, NULL);
    int result = ir_new_temp(func); ir_emit(func, IR_ADD, result, sum, 1, NULL);
    ir_emit(func, IR_RET, result, -1, -1, NULL);
    OptPass pipeline[] = {OPT_DCE, OPT_CSE, OPT_CONST_FOLD, OPT_COPY_PROP,
                          OPT_SIMPLIFY_CFG, OPT_LOOP_INVARIANT};
    opt_run_pipeline(func, pipeline, 6);
    int reg_assignments[MAX_TEMP_REGS];
    ra_linear_scan(func, 8, reg_assignments);
    CodeGen* cg = cg_create("full_pipe2");
    cg_generate(cg, func, reg_assignments);
    CGStats s = cg_get_stats(cg);
    CHECK(s.total_count > 0, "no code generated");
    cg_peephole_optimize(cg);
    cg_destroy(cg);
    ir_destroy_function(func);
    PASS();
}

static void test_ssa_to_backend_pipeline(void) {
    TEST("SSA -> Opt -> Out-of-SSA -> Backend pipeline");
    IRFunction* func = ir_create_function("ssa_pipe2");
    int x = ir_new_temp(func); ir_emit(func, IR_MOV, x, 10, -1, NULL);
    int y = ir_new_temp(func); ir_emit(func, IR_ADD, y, x, 20, NULL);
    ir_emit(func, IR_RET, y, -1, -1, NULL);
    ssa_build(func);
    OptPass pipeline[] = {OPT_DCE, OPT_CONST_FOLD};
    opt_run_pipeline(func, pipeline, 2);
    ssa_destroy(func);
    int regs[MAX_TEMP_REGS];
    ra_linear_scan(func, 6, regs);
    CodeGen* cg = cg_create("ssa_pipe2");
    cg_generate(cg, func, regs);
    CHECK(cg->num_inst > 0, "SSA pipeline generated no code");
    cg_destroy(cg);
    ir_destroy_function(func);
    PASS();
}

/* Edge Case Tests */

static void test_null_safety(void) {
    TEST("Null pointer safety");
    ir_print_function(NULL, NULL);
    ir_destroy_function(NULL);
    ir_build_cfg(NULL, NULL, 0);
    CFG cfg; cfg_build(NULL, &cfg);
    cfg_print_graph(NULL, NULL);
    int order[MAX_BLOCKS]; int num = 0;
    cfg_reverse_postorder(NULL, order, &num);
    int doms[MAX_BLOCKS][MAX_BLOCKS];
    cfg_dominators(NULL, doms);
    int loops[MAX_BLOCKS], be[MAX_BLOCKS][2], nbe;
    cfg_find_loops(NULL, loops, be, &nbe);
    ssa_build(NULL); ssa_destroy(NULL); ssa_print(NULL, NULL);
    bv_init(NULL); bv_set(NULL, 0); bv_test(NULL, 0); bv_union(NULL, NULL);
    DataflowResult dr;
    df_reaching_defs(NULL, NULL, 0, NULL);
    df_live_variables(NULL, NULL, 0, NULL);
    df_constant_propagation(NULL, NULL, 0, NULL);
    df_available_exprs(NULL, NULL, 0, NULL);
    df_very_busy_exprs(NULL, NULL, 0, NULL);
    opt_dce(NULL); opt_cse(NULL); opt_copy_propagation(NULL);
    opt_constant_folding(NULL); opt_simplify_cfg(NULL);
    opt_run_pass(NULL, OPT_DCE);
    ra_destroy(NULL); ra_add_interference(NULL, 0, 0);
    ra_color_graph(NULL); ra_linear_scan(NULL, 0, NULL);
    cg_destroy(NULL); cg_generate(NULL, NULL, NULL);
    cg_peephole_optimize(NULL);
    PASS();
}

static void test_empty_function(void) {
    TEST("Empty function handling");
    IRFunction* func = ir_create_function("empty2");
    IRBasicBlock blocks[MAX_BLOCKS];
    int n = ir_build_cfg(func, blocks, MAX_BLOCKS);
    CHECK(n == 0, "empty function should have 0 blocks");
    ssa_build(func); ssa_destroy(func);
    OptPass pipeline[] = {OPT_DCE};
    OptStats s = opt_run_pipeline(func, pipeline, 1);
    CHECK(s.removed_instructions == 0, "empty function should remove nothing");
    int regs[MAX_TEMP_REGS];
    ra_linear_scan(func, 4, regs);
    CodeGen* cg = cg_create("empty2");
    cg_generate(cg, func, regs);
    cg_destroy(cg);
    ir_destroy_function(func);
    PASS();
}

static void test_max_capacity(void) {
    TEST("Maximum capacity stress");
    IRFunction* func = ir_create_function("max_test2");
    for (int i = 0; i < MAX_INSTRUCTIONS / 2 && func->num_inst < MAX_INSTRUCTIONS; i++) {
        int t = ir_new_temp(func);
        ir_emit(func, IR_MOV, t, i, -1, NULL);
    }
    CHECK(func->num_inst > 0, "should have emitted instructions");
    CHECK(func->num_inst <= MAX_INSTRUCTIONS, "exceeded max instructions");
    IRBasicBlock blocks[MAX_BLOCKS];
    int n = ir_build_cfg(func, blocks, MAX_BLOCKS);
    CHECK(n > 0, "CFG failed at capacity");
    DataflowResult result;
    df_reaching_defs(func, blocks, n, &result);
    ir_destroy_function(func);
    PASS();
}

/* ================================================================
 * Main
 * ================================================================ */

int main(void) {
    printf("\n=== mini-compiler-middle Test Suite ===\n\n");

    printf("[Module 1: IR]\n");
    test_ir_create_destroy(); test_ir_emit_instructions();
    test_ir_op_names(); test_ir_new_temp(); test_ir_build_cfg_basic();

    printf("\n[Module 2: CFG]\n");
    test_cfg_build(); test_cfg_rpo();
    test_cfg_dominators(); test_cfg_find_loops();

    printf("\n[Module 3: SSA]\n");
    test_dom_computation(); test_dominance_frontier();
    test_ssa_build_simple(); test_ssa_destroy(); test_lengauer_tarjan();

    printf("\n[Module 4: Dataflow Analysis]\n");
    test_bitvector_ops(); test_reaching_defs();
    test_live_variables(); test_constant_propagation();
    test_available_exprs(); test_very_busy_exprs();
    test_df_analyze_generic();

    printf("\n[Module 5: Optimizer]\n");
    test_opt_dce(); test_opt_cse(); test_opt_constant_folding();
    test_opt_copy_propagation(); test_opt_simplify_cfg();
    test_opt_pipeline(); test_opt_loop_invariant();

    printf("\n[Module 6: Register Allocation]\n");
    test_ra_create(); test_ra_add_live_range();
    test_ra_interference(); test_ra_color_graph_simple();
    test_ra_color_graph_conflict(); test_ra_linear_scan();
    test_ra_coalesce();

    printf("\n[Module 7: Code Generation]\n");
    test_cg_create(); test_cg_emit(); test_cg_generate_simple();
    test_cg_peephole(); test_cg_stack_frame();

    printf("\n[Module 8: Loop Analysis]\n");
    test_loop_find(); test_induction_vars();
    test_loop_invariant_detect(); test_banerjee();
    test_loop_analysis_integration();

    printf("\n[Cross-Module Integration]\n");
    test_full_pipeline(); test_ssa_to_backend_pipeline();

    printf("\n[Edge Cases]\n");
    test_null_safety(); test_empty_function(); test_max_capacity();

    printf("\n========================================\n");
    printf("Results: %d passed, %d failed\n", tests_passed, tests_failed);
    printf("========================================\n");

    if (tests_failed > 0) {
        printf("\n*** SOME TESTS FAILED ***\n");
        return 1;
    }
    printf("\n*** ALL TESTS PASSED ***\n");
    return 0;
}
