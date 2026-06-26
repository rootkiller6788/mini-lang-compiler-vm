#ifndef CODEGEN_H
#define CODEGEN_H

#include <stdbool.h>
#include <stddef.h>
#include "graph_ir.h"

/*
 * Code Generation — Module 14.7
 *
 * IR-to-C code generation for computation graphs.
 * References:
 *   - TVM Codegen: BYOC infrastructure, C source module
 *   - Halide: lowering pipeline from algorithm to loops
 *   - LLVM: SelectionDAG and instruction selection
 *
 * L1: Definitions — CodeGen structures
 * L2: Core Concepts — target-independent lowering
 * L3: Engineering Structures — loop nest generation, buffer management
 * L6: Canonical Problem — end-to-end graph→compilable C code
 * L8: Advanced Topics — AOT compilation, kernel specialization
 */

#define CODEGEN_MAX_BUFFERS     32
#define CODEGEN_MAX_LOOPS       16
#define CODEGEN_MAX_STMTS       256
#define CODEGEN_MAX_LINE_LEN    256
#define CODEGEN_BUFFER_NAME_LEN 64

/* ---- L1: Definitions — Target Architecture ---- */
typedef enum {
    Target_CPU_x86,
    Target_CPU_ARM,
    Target_GPU_CUDA,
    Target_GPU_OpenCL,
    Target_WASM,
    Target_COUNT
} CodegenTarget;

typedef enum {
    Prec_FP32,
    Prec_FP16,
    Prec_INT32,
    Prec_INT8,
    Prec_COUNT
} CodegenPrecision;

/* ---- L1: Definitions — Buffer Allocation ---- */
typedef struct {
    char name[CODEGEN_BUFFER_NAME_LEN];
    int dims[GRAPH_MAX_SHAPE_DIMS];
    int ndim;
    DataType dtype;
    int size_bytes;
    bool is_input;
    bool is_output;
    bool is_intermediate;
    int node_id;
} CodegenBuffer;

/* ---- L1: Definitions — Loop Nest ---- */
typedef struct {
    char index_var[16];
    int start;
    int extent;
    int step;
    int tile_size;
    bool is_parallel;
    bool is_vectorized;
    int unroll_factor;
} CodegenLoop;

/* ---- L1: Definitions — Generated Statement ---- */
typedef enum {
    Stmt_ALLOC,
    Stmt_FREE,
    Stmt_LOOP_BEGIN,
    Stmt_LOOP_END,
    Stmt_COMPUTE,
    Stmt_LOAD,
    Stmt_STORE,
    Stmt_DECLARE,
    Stmt_COMMENT,
    Stmt_INCLUDE,
    Stmt_KERNEL_BEGIN,
    Stmt_KERNEL_END,
    Stmt_COUNT
} StmtKind;

typedef struct {
    StmtKind kind;
    char text[CODEGEN_MAX_LINE_LEN];
    int indent_level;
} CodegenStmt;

/* ---- L1: Definitions — Code Generator ---- */
typedef struct {
    CodegenTarget target;
    CodegenPrecision precision;
    CodegenBuffer buffers[CODEGEN_MAX_BUFFERS];
    int buffer_count;
    CodegenStmt stmts[CODEGEN_MAX_STMTS];
    int stmt_count;
    const ComputeGraph *graph;
    bool optimize_memory;
    bool inline_small_kernels;
    char module_name[CODEGEN_BUFFER_NAME_LEN];
} CodeGenerator;

/* ---- L1: API Declarations ---- */

/* Create and configure code generator */
CodeGenerator codegen_create(CodegenTarget target, CodegenPrecision prec);
void codegen_set_module_name(CodeGenerator *cg, const char *name);
void codegen_set_optimize_memory(CodeGenerator *cg, bool enable);
void codegen_set_inline_kernels(CodeGenerator *cg, bool enable);

/* L6: Main lowering pipeline */
int codegen_lower_graph(CodeGenerator *cg, const ComputeGraph *g);

/* L3: Buffer allocation with liveness analysis */
int codegen_allocate_buffers(CodeGenerator *cg, const ComputeGraph *g);
int codegen_compute_buffer_size(const GraphNode *node, const TensorNode *tensor);
int codegen_find_buffer(CodeGenerator *cg, int node_id);

/* L5: Loop nest generation for each operation type */
void codegen_emit_conv2d(CodeGenerator *cg, const GraphNode *node,
                          const CodegenBuffer *in, const CodegenBuffer *out,
                          const CodegenBuffer *weight, const CodegenBuffer *bias);
void codegen_emit_matmul(CodeGenerator *cg, const GraphNode *node,
                          const CodegenBuffer *A, const CodegenBuffer *B,
                          const CodegenBuffer *C);
void codegen_emit_relu(CodeGenerator *cg, const GraphNode *node,
                        const CodegenBuffer *in, const CodegenBuffer *out);
void codegen_emit_elemwise(CodeGenerator *cg, const GraphNode *node,
                            const CodegenBuffer *in, const CodegenBuffer *out,
                            const char *op_expr);

/* L6: Generate complete C file */
void codegen_emit_header(CodeGenerator *cg);
void codegen_emit_forward_decl(CodeGenerator *cg);
int  codegen_emit_body(CodeGenerator *cg);

/* Output */
void codegen_print_c(CodeGenerator *cg);
void codegen_print_buffers(CodeGenerator *cg);
void codegen_print_stats(CodeGenerator *cg);

/* L8: Advanced — AOT compilation manifest */
void codegen_emit_aot_manifest(CodeGenerator *cg);

/* L8: Advanced — Kernel specialization for specific shapes */
void codegen_specialize_for_shape(CodeGenerator *cg, int H, int W,
                                   int C_in, int C_out);

/* L9: Industry — MLIR→LLVM progressive lowering sketch */
void codegen_print_lowering_pipeline(void);

/* Constants */
const char *codegen_target_name(CodegenTarget t);
const char *codegen_precision_name(CodegenPrecision p);
const char *codegen_stmt_kind_name(StmtKind k);

#endif /* CODEGEN_H */
