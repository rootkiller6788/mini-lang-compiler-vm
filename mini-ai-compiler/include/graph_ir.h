#ifndef GRAPH_IR_H
#define GRAPH_IR_H

#include <stdbool.h>
#include <stddef.h>

#define GRAPH_MAX_SHAPE_DIMS    4
#define GRAPH_MAX_INPUTS        4
#define GRAPH_MAX_OUTPUTS       4
#define GRAPH_MAX_NODES         64
#define GRAPH_MAX_STR_LEN       64

typedef enum {
    DType_FLOAT32,
    DType_INT32,
    DType_FLOAT16,
    DType_INT8,
    DType_COUNT
} DataType;

typedef enum {
    GOp_CONV2D,
    GOp_RELU,
    GOp_BATCH_NORM,
    GOp_ADD,
    GOp_MUL,
    GOp_MATMUL,
    GOp_SOFTMAX,
    GOp_RESHAPE,
    GOp_CONCAT,
    GOp_MAXPOOL2D,
    GOp_AVGPOOL2D,
    GOp_TRANSPOSE,
    GOp_FUSED_CONV_BN_RELU,
    GOp_FUSED_MATMUL_BIAS_RELU,
    GOp_FUSED_ELEMWISE_CHAIN,
    GOp_COUNT
} GraphOpType;

typedef struct {
    int dims[GRAPH_MAX_SHAPE_DIMS];
    int ndim;
    DataType dtype;
    int producer_node_id;
    char name[GRAPH_MAX_STR_LEN];
} TensorNode;

typedef struct {
    int kernel_size[2];
    int stride[2];
    int padding[2];
    int dilation[2];
    int groups;
    double epsilon;
    int axis;
} GraphAttrs;

typedef struct {
    int id;
    GraphOpType op_type;
    int inputs[GRAPH_MAX_INPUTS];
    int input_count;
    int outputs[GRAPH_MAX_OUTPUTS];
    int output_count;
    GraphAttrs attrs;
    char name[GRAPH_MAX_STR_LEN];
} GraphNode;

typedef struct {
    GraphNode nodes[GRAPH_MAX_NODES];
    int node_count;
    int input_ids[GRAPH_MAX_NODES];
    int input_count;
    int output_id;
    int next_id;
} ComputeGraph;

const char *graph_op_type_name(GraphOpType t);
const char *data_type_name(DataType t);

ComputeGraph graph_create(void);
int graph_add_node(ComputeGraph *g, GraphOpType op_type, const int *inputs,
                   int input_count, const char *name);
void graph_node_set_kernel(GraphNode *node, int kh, int kw, int sh, int sw,
                           int ph, int pw);
void graph_node_set_attr_float(GraphAttrs *attrs, double val);
void graph_set_input(ComputeGraph *g, int dims[4], DataType dtype, const char *name);
void graph_set_output(ComputeGraph *g, int node_id);
void graph_set_attr_defaults(GraphAttrs *attrs);

bool graph_topological_sort(ComputeGraph *g);
int *graph_topological_order(ComputeGraph *g, int *out_count);

TensorNode graph_create_tensor(const int *dims, int ndim, DataType dtype, int producer);
bool graph_infer_shapes(ComputeGraph *g);
void graph_infer_node_shape(GraphNode *node, TensorNode *tensors, int tensor_count);
void graph_tensor_set_shape(TensorNode *t, int d0, int d1, int d2, int d3, int ndim);

void graph_print(ComputeGraph *g);
void graph_print_node(GraphNode *node);
void graph_print_tensor(TensorNode *t);

int graph_find_node_by_name(ComputeGraph *g, const char *name);
bool graph_replace_node(ComputeGraph *g, int old_id, GraphNode new_node);

#endif
