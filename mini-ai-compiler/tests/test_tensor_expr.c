#include "tensor_expr.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

int main(void)
{
    printf("=== Test: Tensor Expression DSL ===\n");

    /* Test expression creation */
    TensorExpr ctx = tensor_expr_create();
    assert(ctx.node_count == 0);

    ExprNode *input = texpr_placeholder(&ctx, "input", 3, DType_FLOAT32);
    assert(input != NULL);
    assert(input->kind == Expr_PLACEHOLDER);
    printf("Placeholder created: %s\n", input->name);

    ExprNode *var_x = texpr_var(&ctx, "x", 256);
    assert(var_x != NULL);
    printf("Index var: %s\n", var_x->var.name);

    ExprNode *cst = texpr_constant(&ctx, 42.0f);
    assert(cst != NULL);
    printf("Constant: %g\n", cst->const_val.fval);

    /* Test binary expressions */
    ExprNode *add = texpr_add(&ctx, cst, cst);
    assert(add != NULL);
    assert(add->kind == Expr_ADD);
    printf("Add node created\n");

    ExprNode *mul = texpr_mul(&ctx, add, var_x);
    assert(mul != NULL);
    printf("Mul node created\n");

    ExprNode *sub = texpr_sub(&ctx, add, mul);
    ExprNode *div_node = texpr_div(&ctx, sub, cst);
    ExprNode *max_node = texpr_max(&ctx, div_node, cst);
    ExprNode *min_node = texpr_min(&ctx, max_node, cst);
    (void)min_node;
    printf("All binary ops created\n");

    /* Test select */
    ExprNode *cond = texpr_add(&ctx, var_x, var_x);
    ExprNode *sel = texpr_select(&ctx, cond, cst, add);
    assert(sel != NULL);
    assert(sel->kind == Expr_SELECT);
    printf("Select created\n");

    /* Test cast */
    ExprNode *cast = texpr_cast(&ctx, cst, DType_INT32);
    assert(cast != NULL);
    assert(cast->dtype == DType_INT32);
    printf("Cast created\n");

    /* Test load */
    IndexVar indices[2];
    memset(&indices[0], 0, sizeof(IndexVar));
    strcpy(indices[0].name, "i");
    indices[0].domain_extent = 256;
    strcpy(indices[1].name, "j");
    indices[1].domain_extent = 256;

    ExprNode *load = texpr_load(&ctx, "buffer_a", indices, 2);
    assert(load != NULL);
    printf("Load created: %s\n", load->buffer_name);

    /* Test reduce */
    int reduce_axes[1] = {0};
    ExprNode *sum = texpr_reduce_sum(&ctx, load, reduce_axes, 1);
    assert(sum != NULL);
    assert(sum->kind == Expr_REDUCE_SUM);
    printf("Reduce sum created\n");

    /* Test stage creation */
    ComputeStage *stage = texpr_create_stage(&ctx, "conv_stage", add);
    assert(stage != NULL);
    texpr_stage_set_loops(stage, indices, 2);
    printf("Stage created: %s\n", stage->name);

    /* Test schedule transforms */
    texpr_schedule_split(&ctx, 0, 0, 8);
    texpr_schedule_reorder(&ctx, 0, (int[]){1, 0, 2, 3}, 4);
    texpr_schedule_unroll(&ctx, 0, 1);
    texpr_schedule_vectorize(&ctx, 0, 1);
    texpr_schedule_parallel(&ctx, 0, 0);
    texpr_schedule_compute_at(&ctx, 0, 1, 2);
    texpr_schedule_inline(&ctx, 0);
    printf("All schedule transforms applied\n");

    /* Test bounds inference */
    int inferred = texpr_infer_stage_bounds(stage, stage->compute_expr);
    printf("Bounds inferred: %d\n", inferred);

    /* Test strides computation */
    int dims[4] = {2, 3, 4, 5};
    int strides[4];
    texpr_compute_strides(dims, 4, strides);
    assert(strides[3] == 1);
    assert(strides[2] == 5);
    assert(strides[1] == 20);
    assert(strides[0] == 60);
    printf("Strides: [%d,%d,%d,%d]\n", strides[0], strides[1],
           strides[2], strides[3]);

    /* Test flatten indices */
    int flat = texpr_flatten_indices((int[]){1, 2, 3, 4}, strides, 4);
    printf("Flattened offset: %d\n", flat);

    /* Test storage align */
    texpr_storage_align(&ctx, 0, 0, 32);
    printf("Storage aligned\n");

    /* Test lowering to graph */
    ComputeGraph g = graph_create();
    int n_stages = texpr_lower_to_graph(&ctx, &g);
    printf("Lowered to graph: %d stages -> %d nodes\n", n_stages,
           g.node_count);

    /* Print full state */
    texpr_print_full(&ctx);

    /* Test rfactor (on a reduction stage) */
    TensorExpr ctx2 = tensor_expr_create();
    ExprNode *load2 = texpr_load(&ctx2, "buf", indices, 2);
    ExprNode *sum2 = texpr_reduce_sum(&ctx2, load2, (int[]){0}, 1);
    ComputeStage *reduce_stage = texpr_create_stage(&ctx2, "reduce", sum2);
    texpr_stage_set_loops(reduce_stage, indices, 2);
    bool rfactored = texpr_rfactor(&ctx2, 0, 0);
    printf("rfactor applied: %s\n", rfactored ? "yes" : "no");

    printf("\nAll tensor expression tests passed!\n");
    return 0;
}
