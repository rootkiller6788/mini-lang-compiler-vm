#include "codegen.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

int main(void)
{
    printf("=== Test: Code Generation ===\n");

    /* Test codegen creation */
    CodeGenerator cg = codegen_create(Target_CPU_x86, Prec_FP32);
    assert(cg.target == Target_CPU_x86);
    assert(cg.precision == Prec_FP32);
    printf("Target: %s\n", codegen_target_name(cg.target));

    /* Test name tables */
    assert(strcmp(codegen_target_name(Target_CPU_ARM), "aarch64") == 0);
    assert(strcmp(codegen_precision_name(Prec_INT32), "int32_t") == 0);
    printf("Name tables: pass\n");

    /* Test configuration */
    codegen_set_module_name(&cg, "test_model");
    codegen_set_optimize_memory(&cg, true);
    codegen_set_inline_kernels(&cg, false);
    assert(strcmp(cg.module_name, "test_model") == 0);

    /* Test lowering */
    ComputeGraph g = graph_create();
    graph_add_node(&g, GOp_CONV2D, NULL, 0, "conv1");
    graph_add_node(&g, GOp_RELU, (int[]){0}, 1, "relu1");
    graph_add_node(&g, GOp_ADD, (int[]){1}, 1, "add1");
    graph_set_output(&g, 2);

    int stmts = codegen_lower_graph(&cg, &g);
    printf("Lowered graph: %d statements\n", stmts);
    assert(cg.buffer_count == 3);
    assert(cg.stmt_count > 0);

    /* Test buffer allocation */
    CodeGenerator cg2 = codegen_create(Target_GPU_CUDA, Prec_FP16);
    int n_bufs = codegen_allocate_buffers(&cg2, &g);
    assert(n_bufs == 3);
    printf("Buffers allocated: %d\n", n_bufs);

    /* Test buffer find */
    assert(codegen_find_buffer(&cg2, 0) >= 0);
    assert(codegen_find_buffer(&cg2, 999) == -1);
    printf("Buffer lookup: pass\n");

    /* Test individual emitter functions */
    CodeGenerator cg3 = codegen_create(Target_CPU_x86, Prec_FP32);
    codegen_allocate_buffers(&cg3, &g);

    /* Test specialization */
    codegen_specialize_for_shape(&cg3, 224, 224, 3, 64);

    /* Test forward declaration */
    codegen_emit_forward_decl(&cg3);
    printf("Forward decl emitted\n");

    /* Test body emission */
    int body_stmts = codegen_emit_body(&cg3);
    printf("Body statements: %d\n", body_stmts);

    /* Test AOT manifest */
    codegen_emit_aot_manifest(&cg3);

    /* Test lowering pipeline doc */
    codegen_print_lowering_pipeline();

    /* Test stats */
    codegen_print_stats(&cg3);
    codegen_print_buffers(&cg3);

    /* Test stmt kind names */
    assert(strcmp(codegen_stmt_kind_name(Stmt_LOOP_BEGIN), "loop_begin") == 0);
    assert(strcmp(codegen_stmt_kind_name(Stmt_ALLOC), "alloc") == 0);
    printf("Stmt kind names: pass\n");

    printf("\nAll codegen tests passed!\n");
    return 0;
}
