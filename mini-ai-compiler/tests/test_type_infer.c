#include "type_infer.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

int main(void)
{
    printf("=== Test: Type Inference ===\n");

    /* Test type variable creation */
    TypeNode tv = type_new_var(0);
    assert(tv.is_free);
    assert(tv.var_id == 0);
    printf("Type var created: %s\n", tv.name);

    /* Test tensor type creation */
    int dims[4] = {1, 3, 224, 224};
    TypeNode elem = type_new_concrete(TypeCon_FLOAT32);
    TypeNode tensor = type_new_tensor(elem, 4, dims);
    assert(tensor.con == TypeCon_TENSOR);
    assert(tensor.ndim == 4);
    printf("Tensor type: [%d,%d,%d,%d]\n",
           tensor.shape_dims[0], tensor.shape_dims[1],
           tensor.shape_dims[2], tensor.shape_dims[3]);

    /* Test broadcast */
    int shape_a[3] = {1, 3, 1};
    int shape_b[3] = {4, 1, 5};
    int result[3];
    int result_ndim;
    bool ok = type_broadcast_shapes(shape_a, 3, shape_b, 3,
                                     result, &result_ndim);
    assert(ok);
    assert(result_ndim == 3);
    assert(result[0] == 4);
    assert(result[1] == 3);
    assert(result[2] == 5);
    printf("Broadcast: [%d,%d,%d]\n", result[0], result[1], result[2]);

    /* Test shape function dispatch */
    const ShapeFunction *sf = type_get_shape_function(GOp_CONV2D);
    assert(sf != NULL);
    printf("Shape func: %s\n", sf->description);

    /* Test shape inference for Conv2D */
    int in_dims[4] = {1, 3, 224, 224};
    GraphAttrs attrs;
    memset(&attrs, 0, sizeof(attrs));
    attrs.kernel_size[0] = 3;
    attrs.kernel_size[1] = 3;
    attrs.stride[0] = 1;
    attrs.stride[1] = 1;
    attrs.padding[0] = 1;
    attrs.padding[1] = 1;
    int out_dims[4];
    int out_ndim;
    int ret = type_invoke_shape_function(GOp_CONV2D, in_dims, 4,
                                          &attrs, out_dims, &out_ndim);
    assert(ret == 0);
    assert(out_ndim == 4);
    printf("Conv2D shape: [%d,%d,%d,%d] -> [%d,%d,%d,%d]\n",
           in_dims[0], in_dims[1], in_dims[2], in_dims[3],
           out_dims[0], out_dims[1], out_dims[2], out_dims[3]);

    /* Test type inference on a graph */
    ComputeGraph g = graph_create();
    graph_add_node(&g, GOp_CONV2D, NULL, 0, "input");
    graph_add_node(&g, GOp_RELU, (int[]){0}, 1, "relu");
    graph_add_node(&g, GOp_ADD, (int[]){1}, 1, "add");
    graph_set_output(&g, 2);

    TypeEnv env;
    memset(&env, 0, sizeof(env));
    int n = type_infer_graph(&env, &g);
    assert(n == 3);
    assert(env.solved == false); /* Not solved yet without constraints */
    printf("Inferred types for %d nodes\n", n);

    /* Test type checking */
    bool all_ok = type_check_graph(&env, &g);
    assert(all_ok);
    printf("Type check: passed\n");

    /* Test fusion compatibility */
    assert(type_can_fuse(GOp_CONV2D, GOp_BATCH_NORM));
    assert(type_can_fuse(GOp_BATCH_NORM, GOp_RELU));
    assert(!type_can_fuse(GOp_CONV2D, GOp_RELU));
    printf("Fusion type check: pass\n");

    /* Test shape specialization */
    int fixed[4] = {1, 64, 56, 56};
    int n_spec = type_specialize_for_shape(&g, fixed, 4);
    printf("Specialized: %d nodes\n", n_spec);

    /* Test dependent shape verification */
    TypeNode check_type = type_new_tensor(elem, 4, dims);
    int rt_shape[4] = {1, 3, 224, 224};
    assert(type_verify_shape_dependent(&check_type, rt_shape));
    rt_shape[0] = 2;
    assert(!type_verify_shape_dependent(&check_type, rt_shape));
    printf("Dependent shape check: pass\n");

    type_print_env(&env);

    printf("\nAll type inference tests passed!\n");
    return 0;
}
