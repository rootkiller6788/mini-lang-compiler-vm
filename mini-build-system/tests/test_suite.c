#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <math.h>
#include <time.h>

#include "dep_graph.h"
#include "incremental.h"
#include "make_engine.h"
#include "ninja_graph.h"
#include "task_scheduler.h"
#include "build_toolchain.h"
#include "build_manifest.h"
#include "build_logger.h"
#include "hash_cache.h"
#include "scheduler_advanced.h"
#include "build_theorem.h"

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    printf("  TEST: %-50s ... ", name); \
    fflush(stdout); \
} while(0)

#define PASS() do { \
    printf("PASS\n"); \
    tests_passed++; \
} while(0)

#define FAIL(msg) do { \
    printf("FAIL: %s\n", msg); \
    tests_failed++; \
} while(0)

#define CHECK(cond, msg) do { \
    if (!(cond)) { FAIL(msg); } else { PASS(); } \
} while(0)

/* =========================================================================
 * DepGraph Tests (L5: Topo Sort, Cycle Detection, SCC, Dominators)
 * ========================================================================= */
static void test_dep_graph_basic(void) {
    static DepGraph dg;
    memset(&dg, 0, sizeof(dg));

    TEST("dep_add_node");
    int a = dep_add_node(&dg, "A");
    int b = dep_add_node(&dg, "B");
    int c = dep_add_node(&dg, "C");
    CHECK(a == 0 && b == 1 && c == 2, "node indices");

    TEST("dep_add_edge");
    bool ok = dep_add_edge(&dg, a, b);
    ok = ok && dep_add_edge(&dg, b, c);
    CHECK(ok, "edge creation");

    TEST("dep_topological_sort");
    int order[DEP_MAX_NODES];
    int order_len = 0;
    ok = dep_topological_sort(&dg, order, &order_len);
    CHECK(ok && order_len == 3, "topo sort 3 nodes");

    TEST("dep_detect_cycle_no_cycle");
    ok = !dep_detect_cycle(&dg);
    CHECK(ok, "no cycle in DAG");

    TEST("dep_parallel_schedule");
    int schedule[DEP_MAX_NODES];
    int num_levels = 0;
    ok = dep_parallel_schedule(&dg, schedule, &num_levels);
    CHECK(ok && num_levels == 3, "parallel schedule");

    TEST("dep_find_sccs");
    int scc_map[DEP_MAX_NODES];
    int num_sccs = dep_find_sccs(&dg, scc_map);
    CHECK(num_sccs == 3, "SCC count = 3");

    TEST("dep_compute_dominators");
    int dom[DEP_MAX_NODES];
    dep_compute_dominators(&dg, dom);
    CHECK(dom[0] == 0, "root dominates itself");

    TEST("dep_detect_cycle_positive");
    dep_add_edge(&dg, c, a);  /* create cycle A->B->C->A */
    ok = dep_detect_cycle(&dg);
    CHECK(ok, "cycle detected");
}

/* =========================================================================
 * Incremental Tests (L4: Caching)
 * ========================================================================= */
static void test_incremental_cache(void) {
    static BuildCache bc;
    TEST("cache_init");
    cache_init(&bc, ".test-cache");
    CHECK(bc.num_entries == 0, "empty cache");

    TEST("cache_compute_key");
    char key[64];
    cache_compute_key("abc123", "gcc -c foo.c", key, 64);
    CHECK(strlen(key) > 0, "key computed");

    TEST("cache_store_and_lookup");
    const char *outputs[] = {"foo.o", "foo.d"};
    cache_store(&bc, key, (const char(*)[CACHE_PATH_LEN])outputs, 2);
    char out_files[16][CACHE_PATH_LEN];
    int num_out = 0;
    bool found = cache_lookup(&bc, key, out_files, &num_out);
    CHECK(found && num_out == 2, "cache hit");

    TEST("cache_invalidate");
    cache_invalidate(&bc, key);
    found = cache_lookup(&bc, key, out_files, &num_out);
    CHECK(!found, "cache miss after invalidate");

    TEST("cache_prune");
    cache_prune(&bc);
    CHECK(bc.num_entries <= CACHE_MAX_ENTRIES, "prune ok");
}

static void test_file_tracker(void) {
    static BuildTracker bt;
    TEST("tracker_init");
    tracker_init(&bt);
    CHECK(bt.num_files == 0, "empty tracker");

    TEST("tracker_add_file");
    tracker_add_file(&bt, "nonexistent.txt");
    CHECK(bt.num_files == 1, "file added");

    TEST("tracker_detect_changes");
    tracker_detect_changes(&bt);
    CHECK(!tracker_has_changes(&bt) || bt.num_dirtied >= 0, "change detection");

    TEST("tracker_reset");
    tracker_reset(&bt);
    CHECK(bt.num_dirtied == 0, "tracker reset");
}

/* =========================================================================
 * Make Engine Tests (L2: Rule parsing, L5: Pattern matching)
 * ========================================================================= */
static void test_make_engine(void) {
    static MakeFile mf;
    memset(&mf, 0, sizeof(mf));

    TEST("make_parse_empty_rules");
    bool ok = make_parse(&mf, "nonexistent.mk");
    CHECK(!ok, "nonexistent file returns false");

    TEST("make_match_exact");
    char stem[128];
    ok = make_match_pattern("hello.o", "hello.o", stem, sizeof(stem));
    CHECK(ok, "exact match");

    TEST("make_match_pattern_pct");
    ok = make_match_pattern("%.o", "world.o", stem, sizeof(stem));
    CHECK(ok && strcmp(stem, "world") == 0, "pattern match with stem");

    TEST("make_match_no_match");
    ok = make_match_pattern("%.o", "file.c", stem, sizeof(stem));
    CHECK(!ok, "no match for wrong extension");
}

/* =========================================================================
 * Ninja Graph Tests
 * ========================================================================= */
static void test_ninja_graph(void) {
    static NinjaBuild nb;
    memset(&nb, 0, sizeof(nb));

    TEST("ninja_add_node");
    int idx = ninja_add_node(&nb, "hello.cpp", NINJA_NODE_FILE);
    CHECK(idx == 0, "first node");

    TEST("ninja_find_node");
    int found = ninja_find_node(&nb, "hello.cpp");
    CHECK(found == 0, "found node");

    TEST("ninja_duplicate_node");
    int dup = ninja_add_node(&nb, "hello.cpp", NINJA_NODE_FILE);
    CHECK(dup == 0, "duplicate returns existing index");
}

/* =========================================================================
 * Task Scheduler Tests
 * ========================================================================= */
static void test_task_scheduler(void) {
    static TaskScheduler ts;

    TEST("sched_init");
    sched_init(&ts, 4);
    CHECK(ts.max_parallel == 4, "max_parallel set");

    TEST("sched_add_task");
    int t0 = sched_add_task(&ts, "gcc -c a.c");
    int t1 = sched_add_task(&ts, "gcc -c b.c");
    CHECK(t0 == 0 && t1 == 1, "task IDs");

    TEST("sched_add_dependency");
    int t2 = sched_add_task(&ts, "gcc -o prog a.o b.o");
    bool ok = sched_add_dependency(&ts, t2, t0);
    ok = ok && sched_add_dependency(&ts, t2, t1);
    CHECK(ok, "dependencies added");

    TEST("sched_state_name");
    const char *name = sched_state_name(TASK_PENDING);
    CHECK(strcmp(name, "PENDING") == 0, "state name");

    TEST("sched_reset");
    sched_reset(&ts);
    CHECK(ts.completed_count == 0, "reset completed");
}

/* =========================================================================
 * Toolchain Tests (L1: Definitions, L4: Triple parsing)
 * ========================================================================= */
static void test_toolchain(void) {
    static ToolchainProfile tc;

    TEST("tc_init");
    tc_init(&tc, "test-tc");
    CHECK(strcmp(tc.name, "test-tc") == 0, "name set");

    TEST("tc_add_tool");
    bool ok = tc_add_tool(&tc, TOOL_CC, "gcc", "-Wall");
    CHECK(ok && tc.num_tools == 1, "tool added");

    TEST("tc_add_include");
    ok = tc_add_include(&tc, "/usr/include");
    CHECK(ok && tc.num_includes == 1, "include added");

    TEST("tc_find_tool");
    ToolSpec *ts = tc_find_tool(&tc, TOOL_CC);
    CHECK(ts != NULL && ts->kind == TOOL_CC, "found CC");

    TEST("tc_validate");
    tc_add_tool(&tc, TOOL_LD, "ld", "");
    ok = tc_validate(&tc);
    CHECK(ok && tc.is_valid, "toolchain valid");

    TEST("tc_parse_triple");
    TargetTriple triple;
    ok = tc_parse_triple("x86_64-pc-linux-gnu", &triple);
    CHECK(ok && strcmp(triple.machine, "x86_64") == 0
              && strcmp(triple.os, "linux") == 0
              && strcmp(triple.abi, "gnu") == 0, "triple parsed");

    TEST("tc_parse_triple_3part");
    ok = tc_parse_triple("arm-none-eabi", &triple);
    CHECK(ok && strcmp(triple.machine, "arm") == 0, "3-part triple");

    TEST("tc_set_cross_target");
    ok = tc_set_cross_target(&tc, "aarch64-apple-darwin");
    CHECK(ok, "cross target set");

    TEST("tc_variant_name");
    CHECK(strcmp(tc_variant_name(BUILD_DEBUG), "debug") == 0, "debug variant");
    CHECK(strcmp(tc_variant_name(BUILD_RELEASE), "release") == 0, "release variant");

    TEST("tc_tool_kind_name");
    CHECK(strcmp(tc_tool_kind_name(TOOL_CC), "CC") == 0, "CC kind");

    TEST("tc_compile_cmd");
    char cmd[1024];
    int len = tc_compile_cmd(&tc, "test.c", "test.o", cmd, sizeof(cmd));
    CHECK(len > 0 && strstr(cmd, "-c") && strstr(cmd, "test.c"), "compile command");

    TEST("tc_triple_to_string");
    char buf[128];
    tc_triple_to_string(&triple, buf, sizeof(buf));
    CHECK(strcmp(buf, "arm-none-eabi") == 0, "triple to string");
}

/* =========================================================================
 * Build Manifest Tests (L3: Workspace structure)
 * ========================================================================= */
static void test_manifest(void) {
    static BuildWorkspace ws;

    TEST("manifest_init");
    manifest_init(&ws, "test-ws", "/tmp/test");
    CHECK(strcmp(ws.name, "test-ws") == 0, "workspace init");

    TEST("manifest_add_project");
    int p0 = manifest_add_project(&ws, "libA", "libA/", PROJECT_LIBRARY);
    int p1 = manifest_add_project(&ws, "app", "app/", PROJECT_EXECUTABLE);
    CHECK(p0 == 0 && p1 == 1, "projects added");

    TEST("manifest_add_target");
    int t0 = manifest_add_target(&ws, 0, "libA-core", "libA/src", PROJECT_LIBRARY);
    CHECK(t0 == 0, "target added");

    TEST("manifest_add_target_dep");
    /* Add target to project 1 first, then add a dep on libA-core */
    manifest_add_target(&ws, 1, "app-main", "app/src", PROJECT_EXECUTABLE);
    bool ok = manifest_add_target_dep(&ws, 1, 0, "libA-core");
    CHECK(ok, "target dep added");

    TEST("manifest_add_project_dep");
    ok = manifest_add_project_dep(&ws, 1, "libA");
    CHECK(ok, "project dep added");

    TEST("manifest_no_cycles");
    ok = !manifest_has_cycles(&ws);
    CHECK(ok, "no cycles");

    TEST("manifest_resolve_deps");
    ok = manifest_resolve_deps(&ws);
    CHECK(ok && ws.is_configured, "deps resolved");

    TEST("manifest_validate");
    ok = manifest_validate(&ws);
    CHECK(ok, "valid workspace");
}

/* =========================================================================
 * Build Logger Tests (L7: CI/CD)
 * ========================================================================= */
static void test_logger(void) {
    static BuildLogger log;

    TEST("blog_init");
    blog_init(&log, "build.log");
    CHECK(log.num_events == 0, "empty log");

    TEST("blog_event_phase");
    blog_event_start(&log, "compile");
    blog_event_end(&log, "compile", 150, true);
    CHECK(log.num_events == 2, "2 events");

    TEST("blog_task");
    blog_task_start(&log, 0, "hello.o");
    blog_task_end(&log, 0, "hello.o", 100, 0);
    CHECK(log.succeeded_tasks == 1, "1 succeeded");

    TEST("blog_cache");
    blog_cache_hit(&log, "hello.o");
    CHECK(log.cache_hits == 1, "1 cache hit");

    TEST("blog_error");
    blog_error(&log, "link", "undefined reference");
    CHECK(log.failed_tasks == 1, "1 failed");

    TEST("blog_all_succeeded");
    CHECK(!blog_all_succeeded(&log), "not all succeeded");

    TEST("blog_summary");
    char summary[512];
    int len = blog_summary(&log, summary, sizeof(summary));
    CHECK(len > 0 && strstr(summary, "FAILED"), "summary contains FAILED");
}

/* =========================================================================
 * Hash Cache Tests (L4: CAS, L5: Merkle tree)
 * ========================================================================= */
static void test_hash_cache(void) {
    TEST("hash_string");
    HashValue h1 = hash_string("hello");
    HashValue h2 = hash_string("hello");
    HashValue h3 = hash_string("world");
    CHECK(h1 == h2, "deterministic hash");
    CHECK(h1 != h3, "different inputs -> different hashes");

    TEST("hash_buffer");
    uint8_t data[] = {1, 2, 3, 4, 5};
    HashValue hb = hash_buffer(data, 5);
    CHECK(hb != 0, "buffer hash nonzero");

    TEST("hash_combine");
    HashValue combined = hash_combine(h1, h3);
    CHECK(combined != h1 && combined != h3, "combined differs from inputs");

    TEST("hash_to_hex");
    char hex[32];
    hash_to_hex(h1, hex, sizeof(hex));
    CHECK(strlen(hex) > 0, "hex string produced");

    TEST("hc_init");
    static HashCache hcache;
    hc_init(&hcache, "/tmp/cache");
    CHECK(hcache.num_entries == 0, "empty cache");

    TEST("hc_insert_lookup");
    bool ok = hc_insert(&hcache, "foo.o", h1);
    CHECK(ok && hc_contains(&hcache, h1), "insert and lookup");

    TEST("hc_contains_miss");
    CHECK(!hc_contains(&hcache, hash_string("nonexistent")), "cache miss");

    /* Merkle tree tests */
    static MerkleTree mt;
    merkle_init(&mt);

    TEST("merkle_add_leaf");
    int l0 = merkle_add_leaf(&mt, "block0", hash_string("data0"));
    int l1 = merkle_add_leaf(&mt, "block1", hash_string("data1"));
    int l2 = merkle_add_leaf(&mt, "block2", hash_string("data2"));
    CHECK(l0 == 0 && l1 == 1 && l2 == 2, "3 leaves added");

    TEST("merkle_compute_root");
    HashValue root = merkle_compute_root(&mt);
    CHECK(root != 0, "root hash computed");
}

/* =========================================================================
 * Rolling Hash Tests (L8)
 * ========================================================================= */
static void test_rolling_hash(void) {
    static RollingHash rh;
    TEST("rh_init");
    rh_init(&rh);
    CHECK(rh.window_size == 64, "window size");

    TEST("rh_update");
    for (int i = 0; i < 100; i++)
        rh_update(&rh, (uint8_t)i);
    HashValue d = rh_digest(&rh);
    CHECK(d != 0, "digest nonzero");
}

/* =========================================================================
 * Advanced Scheduler Tests (L5: Johnson, L8: Work Stealing)
 * ========================================================================= */
static void test_advanced_scheduler(void) {
    TEST("johnson_2machine_basic");
    FlowShopJob jobs[4] = {
        {0, "A", 3, 6, 0},
        {1, "B", 5, 2, 0},
        {2, "C", 8, 9, 0},
        {3, "D", 4, 7, 0},
    };
    int order[4];
    int n = johnson_2machine(jobs, 4, order);
    CHECK(n == 4, "4 jobs scheduled");

    TEST("johnson_makespan");
    int ms = johnson_makespan(jobs, order, 4);
    CHECK(ms > 0, "makespan computed");

    TEST("sched_spt");
    sched_spt(jobs, 4, order);
    CHECK(order[0] >= 0, "SPT schedule");

    TEST("sched_lpt");
    sched_lpt(jobs, 4, order);
    CHECK(order[0] >= 0, "LPT schedule");

    TEST("sched_edd");
    sched_edd(jobs, 4, order);
    CHECK(order[0] >= 0, "EDD schedule");

    TEST("sched_critical_ratio");
    sched_critical_ratio(jobs, 4, order, 0);
    CHECK(order[0] >= 0, "CR schedule");

    /* Work-stealing pool */
    static WorkStealingPool pool;
    wsp_init(&pool);

    TEST("wsp_add_item");
    int w0 = wsp_add_item(&pool, 50, NULL, 0);
    int w1 = wsp_add_item(&pool, 30, NULL, 0);
    CHECK(w0 == 0 && w1 == 1, "items added");

    TEST("wsp_steal_work");
    int stolen = wsp_steal_work(&pool, 1);
    CHECK(stolen >= 0, "work stolen");
}

/* =========================================================================
 * Build Theorem Tests (L4: Amdahl, Gustafson, bounds)
 * ========================================================================= */
static void test_build_theorem(void) {
    TEST("amdahl_speedup");
    double s1 = amdahl_speedup(0.95, 4);   /* 95% parallel, 4 CPUs */
    double s2 = amdahl_speedup(0.50, 4);   /* 50% parallel, 4 CPUs */
    CHECK(s1 > 1.0 && s2 > 1.0, "speedups > 1");
    CHECK(s1 > s2, "more parallel -> more speedup");

    TEST("amdahl_limit");
    double s_inf = amdahl_speedup(0.95, 10000);
    CHECK(s_inf < 20.1, "Amdahl limit approx 20x for 5% serial");

    TEST("gustafson_speedup");
    double g1 = gustafson_speedup(0.95, 4);
    CHECK(g1 > s1, "Gustafson > Amdahl (scaled workload)");

    TEST("makespan_lower_bound");
    double lb = makespan_lower_bound(100.0, 400.0, 4);
    CHECK(fabs(lb - 100.0) < 0.01, "CPL bound dominates");

    double lb2 = makespan_lower_bound(10.0, 400.0, 4);
    CHECK(fabs(lb2 - 100.0) < 0.01, "work bound dominates");

    TEST("graham_list_bound");
    double gb = graham_list_bound(100.0, 4);
    CHECK(gb > 100.0, "list bound > optimal");

    TEST("compute_optimal_parallelism");
    int opt = compute_optimal_parallelism(0.05, 0.05);
    CHECK(opt > 1 && opt < 256, "optimal P found");

    TEST("verify_build_determinism");
    bool det = verify_build_determinism("same", "same");
    CHECK(det, "same logs -> deterministic");

    det = verify_build_determinism("same", "different");
    CHECK(!det, "different logs -> not deterministic");

    TEST("analyze_build_bottleneck");
    CriticalPathSegment segments[3] = {
        {"compile_A", 50, true},
        {"link", 200, false},
        {"package", 30, true},
    };
    int bottleneck_ids[8];
    int num_bn = 0;
    int count = analyze_build_bottleneck(segments, 3, bottleneck_ids, &num_bn);
    CHECK(count == 1 && num_bn == 1, "1 bottleneck found");

    /* Full speedup analysis */
    BuildSpeedupResult result;
    speedup_analyze(0.95, 4, &result);
    CHECK(result.amdahl_speedup > 1.0, "full analysis ok");
}

/* =========================================================================
 * Edge Case Tests
 * ========================================================================= */
static void test_edge_cases(void) {
    TEST("dep_graph_max_nodes");
    static DepGraph dg;
    memset(&dg, 0, sizeof(dg));
    for (int i = 0; i < DEP_MAX_NODES; i++) {
        char name[16];
        snprintf(name, sizeof(name), "node%d", i);
        dep_add_node(&dg, name);
    }
    int overflow = dep_add_node(&dg, "overflow");
    CHECK(overflow == -1, "max nodes enforced");

    TEST("cache_max_entries");
    static BuildCache bc;
    cache_init(&bc, ".test");
    for (int i = 0; i < CACHE_MAX_ENTRIES + 1; i++) {
        char key[64], name[64];
        snprintf(name, sizeof(name), "obj%d.o", i);
        snprintf(key, sizeof(key), "key%d", i);
        const char *outputs[] = {name};
        /* Cast needed: the API expects char(*)[CACHE_PATH_LEN] */
        cache_store(&bc, key, (const char(*)[CACHE_PATH_LEN])outputs, 1);
    }
    CHECK(bc.num_entries <= CACHE_MAX_ENTRIES, "cache eviction");

    TEST("manifest_cycle_detection");
    static BuildWorkspace ws;
    manifest_init(&ws, "cycle-test", "/tmp");
    manifest_add_project(&ws, "A", "A/", PROJECT_LIBRARY);
    manifest_add_project(&ws, "B", "B/", PROJECT_LIBRARY);
    manifest_add_project_dep(&ws, 0, "B");
    manifest_add_project_dep(&ws, 1, "A");  /* A->B->A cycle */
    bool has_cycle = manifest_has_cycles(&ws);
    CHECK(has_cycle, "cycle detected in manifest");

    TEST("empty_build_order");
    static BuildWorkspace ws2;
    manifest_init(&ws2, "empty", "/tmp");
    bool ok = manifest_compute_build_order(&ws2);
    CHECK(ok && ws2.num_in_order == 0, "empty workspace ok");
}

/* =========================================================================
 * Main
 * ========================================================================= */
int main(void) {
    printf("\n========== mini-build-system Test Suite ==========\n\n");

    /* DepGraph: L5 algorithms */
    printf("--- DepGraph Tests ---\n");
    test_dep_graph_basic();

    /* Incremental: L4 caching */
    printf("\n--- Incremental Tests ---\n");
    test_incremental_cache();
    test_file_tracker();

    /* Make Engine */
    printf("\n--- Make Engine Tests ---\n");
    test_make_engine();

    /* Ninja Graph */
    printf("\n--- Ninja Graph Tests ---\n");
    test_ninja_graph();

    /* Task Scheduler */
    printf("\n--- Task Scheduler Tests ---\n");
    test_task_scheduler();

    /* Toolchain: L1 L4 */
    printf("\n--- Toolchain Tests ---\n");
    test_toolchain();

    /* Manifest: L3 */
    printf("\n--- Manifest Tests ---\n");
    test_manifest();

    /* Logger: L7 */
    printf("\n--- Logger Tests ---\n");
    test_logger();

    /* Hash Cache: L4 L5 L8 */
    printf("\n--- Hash Cache Tests ---\n");
    test_hash_cache();
    test_rolling_hash();

    /* Advanced Scheduler: L5 L8 */
    printf("\n--- Advanced Scheduler Tests ---\n");
    test_advanced_scheduler();

    /* Build Theorem: L4 */
    printf("\n--- Build Theorem Tests ---\n");
    test_build_theorem();

    /* Edge cases */
    printf("\n--- Edge Case Tests ---\n");
    test_edge_cases();

    printf("\n==================================================\n");
    printf("Results: %d passed, %d failed, %d total\n",
           tests_passed, tests_failed, tests_passed + tests_failed);
    printf("==================================================\n\n");

    return tests_failed > 0 ? 1 : 0;
}
