#define _CRT_SECURE_NO_WARNINGS
#include "ninja_graph.h"
#include <stdio.h>
#include <string.h>

int main(void) {
    static NinjaBuild nb;
    memset(&nb, 0, sizeof(nb));

    printf("=== mini-ninja build demo ===\n\n");

    /* Manually construct a build graph mimicking build.ninja */

    /* Add source file nodes */
    ninja_add_node(&nb, "hello.cpp", NINJA_NODE_FILE);
    ninja_add_node(&nb, "main.cpp", NINJA_NODE_FILE);
    ninja_add_node(&nb, "hello.h", NINJA_NODE_FILE);

    /* Add edge: build hello.obj: cxx hello.cpp | hello.h */
    nb.edges[0].num_inputs = 1;
    strcpy(nb.edges[0].rule_name, "cxx");
    strcpy(nb.edges[0].outputs[0], "hello.obj");
    strcpy(nb.edges[0].inputs[0], "hello.cpp");
    nb.edges[0].num_implicit = 1;
    strcpy(nb.edges[0].implicit_inputs[0], "hello.h");
    ninja_add_node(&nb, "hello.obj", NINJA_NODE_FILE);
    nb.num_edges++;

    /* Add edge: build main.obj: cxx main.cpp */
    nb.edges[1].num_inputs = 1;
    strcpy(nb.edges[1].rule_name, "cxx");
    strcpy(nb.edges[1].outputs[0], "main.obj");
    strcpy(nb.edges[1].inputs[0], "main.cpp");
    ninja_add_node(&nb, "main.obj", NINJA_NODE_FILE);
    nb.num_edges++;

    /* Add edge: build hello: link hello.obj main.obj */
    nb.edges[2].num_inputs = 2;
    strcpy(nb.edges[2].rule_name, "link");
    strcpy(nb.edges[2].outputs[0], "hello");
    strcpy(nb.edges[2].inputs[0], "hello.obj");
    strcpy(nb.edges[2].inputs[1], "main.obj");
    ninja_add_node(&nb, "hello", NINJA_NODE_FILE);
    nb.num_edges++;

    /* Set default target */
    strcpy(nb.default_targets[0], "hello");
    nb.num_default_targets = 1;

    printf("Parsed build graph with %d nodes, %d edges\n",
           nb.num_nodes, nb.num_edges);

    /* Print the graph */
    ninja_print_graph(&nb);

    /* Compute dirty nodes */
    printf("\n=== Dirty computation ===\n");
    ninja_compute_dirty(&nb);

    for (int i = 0; i < nb.num_nodes; i++) {
        const char *d = nb.nodes[i].dirty == NINJA_NODE_CLEAN ? "clean" : "dirty";
        printf("  %s -> %s\n", nb.nodes[i].path, d);
    }

    /* Compute schedule (critical path) */
    printf("\n=== Build schedule ===\n");
    ninja_schedule(&nb);

    /* Simulate building the default target */
    printf("\n=== Simulating build ===\n");
    ninja_execute(&nb, "hello");

    /* Lookup a variable (demo) */
    const char *val = ninja_lookup_var(&nb, "cxx");
    printf("\nVariable lookup 'cxx': %s\n", val ? val : "(not set)");

    printf("\n=== Demo complete ===\n");
    return 0;
}
