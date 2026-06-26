#include "ir_passes.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

int main(void)
{
    ComputeGraph g = graph_create();
    int a = graph_add_node(&g, GOp_ADD, NULL, 0, "a");
    int b = graph_add_node(&g, GOp_ADD, (int[]){a}, 1, "b");
    int c = graph_add_node(&g, GOp_ADD, (int[]){a}, 1, "c");
    (void)c; /* Used to create duplicate for CSE test */
    graph_set_output(&g, b);

    printf("=== Test: IR Passes ===\n");

    /* Test CSE */
    PassContext ctx = pass_context_create(&g);
    int removed = pass_run_cse(&ctx);
    printf("CSE removed: %d nodes\n", removed);

    /* Test DCE */
    removed = pass_run_dce(&ctx);
    printf("DCE removed: %d nodes\n", removed);

    /* Test algebraic rule lookup */
    const AlgebraicRule *rule = pass_get_rule(GOp_ADD);
    assert(rule != NULL);
    assert(rule->has_identity);
    printf("Rule identity check: pass\n");

    /* Test identity/annihilator */
    assert(pass_is_identity_op(GOp_ADD, 0) == true);
    assert(pass_is_annihilator_op(GOp_MUL, 0) == true);
    printf("Identity/annihilator check: pass\n");

    /* Test hash function */
    unsigned long h = pass_op_hash(&g, 0);
    assert(h > 0);
    printf("Op hash: %lu\n", h);

    /* Test pipeline */
    PassContext ctx2 = pass_context_create(&g);
    int total = pass_run_pipeline(&ctx2);
    printf("Pipeline made %d changes\n", total);
    pass_print_stats(&ctx2);

    /* Test LICM */
    removed = pass_run_licm(&ctx2);
    printf("LICM hoisted: %d\n", removed);

    /* Test strength reduction */
    removed = pass_run_strength_reduction(&ctx2);
    printf("Strength reduction: %d\n", removed);

    pass_print_algebraic_rules();

    printf("\nAll IR passes tests passed!\n");
    return 0;
}
