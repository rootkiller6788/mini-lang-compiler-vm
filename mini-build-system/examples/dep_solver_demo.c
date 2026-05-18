#define _CRT_SECURE_NO_WARNINGS
#include "dep_graph.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    DepGraph dg;
    memset(&dg, 0, sizeof(dg));

    printf("=== Dependency Graph Solver Demo ===\n\n");

    /* Build a 10-node dependency graph representing a build pipeline:
     *
     * source -> compile_A -> link
     * source -> compile_B -> link
     * source -> format
     * link -> test
     * link -> package
     * package -> deploy
     * format -> lint (order-only)
     */

    const char *names[] = {
        "source", "compile_A", "compile_B", "format",
        "lint", "link", "test", "package", "deploy", "clean"
    };

    printf("Adding 10 nodes:\n");
    for (int i = 0; i < 10; i++) {
        int idx = dep_add_node(&dg, names[i]);
        printf("  node %d: %s\n", idx, names[i]);
    }

    printf("\nAdding edges:\n");
    dep_add_edge_by_name(&dg, "source", "compile_A");
    printf("  source -> compile_A\n");
    dep_add_edge_by_name(&dg, "source", "compile_B");
    printf("  source -> compile_B\n");
    dep_add_edge_by_name(&dg, "source", "format");
    printf("  source -> format\n");
    dep_add_edge_by_name(&dg, "format", "lint");
    printf("  format -> lint\n");
    dep_add_edge_by_name(&dg, "compile_A", "link");
    printf("  compile_A -> link\n");
    dep_add_edge_by_name(&dg, "compile_B", "link");
    printf("  compile_B -> link\n");
    dep_add_edge_by_name(&dg, "link", "test");
    printf("  link -> test\n");
    dep_add_edge_by_name(&dg, "link", "package");
    printf("  link -> package\n");
    dep_add_edge_by_name(&dg, "package", "deploy");
    printf("  package -> deploy\n");

    printf("\n=== Topological sort ===\n");
    int order[DEP_MAX_NODES];
    int order_len = 0;

    if (dep_topological_sort(&dg, order, &order_len)) {
        dep_print_order(&dg, order, order_len);
    } else {
        printf("  ERROR: Graph has a cycle!\n");
    }

    /* Compute critical path (before cycle is added) */
    dep_compute_critical_path(&dg);
    printf("\n=== Critical path analysis ===\n");
    for (int i = 0; i < dg.num_nodes; i++) {
        printf("  %s: critical_path_length = %d\n",
               dg.nodes[i].name, dg.nodes[i].critical_length);
    }

    /* Parallel schedule */
    printf("\n=== Parallel build schedule ===\n");
    int schedule[DEP_MAX_NODES];
    int num_levels = 0;

    if (dep_parallel_schedule(&dg, schedule, &num_levels)) {
        printf("  Parallel schedule computed (%d levels):\n", num_levels);
        dep_print_schedule(&dg, schedule, num_levels);
    } else {
        printf("  Cannot schedule (cycle present).\n");
    }

    /* Check for cycles */
    printf("\n=== Cycle detection ===\n");
    if (dep_detect_cycle(&dg)) {
        printf("  Cycle detected!\n");
    } else {
        printf("  No cycles found. Graph is a DAG.\n");
    }

    /* Demonstrate cycle detection by adding one */
    printf("\nAdding a cycle (deploy -> source) to test detection:\n");
    dep_add_edge_by_name(&dg, "deploy", "source");
    if (dep_detect_cycle(&dg)) {
        printf("  Cycle detected after adding deploy -> source!\n");
    }

    /* Summary statistics */
    printf("\n=== Summary ===\n");
    printf("  Total nodes: %d\n", dg.num_nodes);
    printf("  Topological order length: %d\n", order_len);

    int total_edges = 0;
    for (int i = 0; i < dg.num_nodes; i++)
        total_edges += dg.nodes[i].num_deps;
    printf("  Total edges: %d\n", total_edges);

    int max_indeg = 0;
    for (int i = 0; i < dg.num_nodes; i++) {
        if (dg.nodes[i].indegree > max_indeg)
            max_indeg = dg.nodes[i].indegree;
    }
    printf("  Max indegree: %d\n", max_indeg);
    printf("  Max parallelism width: %d nodes at deepest level\n", max_indeg + 1);

    printf("\n=== Demo complete ===\n");
    return 0;
}
