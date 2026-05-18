#include "graph_ir.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *graph_op_type_name(GraphOpType t)
{
    switch (t) {
    case GOp_CONV2D:        return "conv2d";
    case GOp_RELU:          return "relu";
    case GOp_BATCH_NORM:    return "batch_norm";
    case GOp_ADD:           return "add";
    case GOp_MUL:           return "mul";
    case GOp_MATMUL:        return "matmul";
    case GOp_SOFTMAX:       return "softmax";
    case GOp_RESHAPE:       return "reshape";
    case GOp_CONCAT:        return "concat";
    case GOp_MAXPOOL2D:     return "maxpool2d";
    case GOp_AVGPOOL2D:     return "avgpool2d";
    case GOp_TRANSPOSE:     return "transpose";
    case GOp_FUSED_CONV_BN_RELU:     return "fused_conv_bn_relu";
    case GOp_FUSED_MATMUL_BIAS_RELU:  return "fused_matmul_bias_relu";
    case GOp_FUSED_ELEMWISE_CHAIN:    return "fused_elemwise_chain";
    default: return "unknown";
    }
}

const char *data_type_name(DataType t)
{
    switch (t) {
    case DType_FLOAT32: return "float32";
    case DType_INT32:   return "int32";
    case DType_FLOAT16: return "float16";
    case DType_INT8:    return "int8";
    default: return "unknown";
    }
}

ComputeGraph graph_create(void)
{
    ComputeGraph g;
    memset(&g, 0, sizeof(g));
    g.node_count = 0;
    g.input_count = 0;
    g.output_id = -1;
    g.next_id = 0;
    return g;
}

int graph_add_node(ComputeGraph *g, GraphOpType op_type, const int *inputs,
                   int input_count, const char *name)
{
    int i;
    if (g->node_count >= GRAPH_MAX_NODES) return -1;

    GraphNode *node = &g->nodes[g->node_count];
    memset(node, 0, sizeof(GraphNode));
    node->id = g->next_id++;
    node->op_type = op_type;
    node->input_count = (input_count > GRAPH_MAX_INPUTS) ? GRAPH_MAX_INPUTS : input_count;
    for (i = 0; i < node->input_count; i++) {
        node->inputs[i] = inputs[i];
    }
    node->output_count = 1;
    node->outputs[0] = -1;
    if (name) {
        strncpy(node->name, name, GRAPH_MAX_STR_LEN - 1);
    }
    graph_set_attr_defaults(&node->attrs);
    g->node_count++;
    return node->id;
}

void graph_node_set_kernel(GraphNode *node, int kh, int kw, int sh, int sw,
                           int ph, int pw)
{
    node->attrs.kernel_size[0] = kh;
    node->attrs.kernel_size[1] = kw;
    node->attrs.stride[0] = sh;
    node->attrs.stride[1] = sw;
    node->attrs.padding[0] = ph;
    node->attrs.padding[1] = pw;
}

void graph_node_set_attr_float(GraphAttrs *attrs, double val)
{
    attrs->epsilon = val;
}

void graph_set_input(ComputeGraph *g, int dims[4], DataType dtype,
                     const char *name)
{
    int id = graph_add_node(g, GOp_CONV2D, NULL, 0, name);
    if (g->input_count < GRAPH_MAX_NODES && id >= 0) {
        g->input_ids[g->input_count] = id;
        g->nodes[g->node_count - 1].op_type = GOp_CONV2D;
        g->input_count++;
    }
}

void graph_set_output(ComputeGraph *g, int node_id)
{
    g->output_id = node_id;
}

void graph_set_attr_defaults(GraphAttrs *attrs)
{
    attrs->kernel_size[0] = 1;
    attrs->kernel_size[1] = 1;
    attrs->stride[0] = 1;
    attrs->stride[1] = 1;
    attrs->padding[0] = 0;
    attrs->padding[1] = 0;
    attrs->dilation[0] = 1;
    attrs->dilation[1] = 1;
    attrs->groups = 1;
    attrs->epsilon = 1e-5;
    attrs->axis = 0;
}

static bool graph_has_cycle_util(ComputeGraph *g, int node_idx, int *visited,
                                 int *rec_stack)
{
    int i;
    if (!visited[node_idx]) {
        visited[node_idx] = 1;
        rec_stack[node_idx] = 1;
        GraphNode *node = &g->nodes[node_idx];
        for (i = 0; i < node->input_count; i++) {
            int in_id = node->inputs[i];
            int j;
            for (j = 0; j < g->node_count; j++) {
                if (g->nodes[j].id == in_id) {
                    if (!visited[j] && graph_has_cycle_util(g, j, visited, rec_stack))
                        return true;
                    else if (rec_stack[j])
                        return true;
                    break;
                }
            }
        }
    }
    rec_stack[node_idx] = 0;
    return false;
}

bool graph_topological_sort(ComputeGraph *g)
{
    int visited[GRAPH_MAX_NODES] = {0};
    int rec_stack[GRAPH_MAX_NODES] = {0};
    int i;
    for (i = 0; i < g->node_count; i++) {
        if (graph_has_cycle_util(g, i, visited, rec_stack))
            return false;
    }
    return true;
}

int *graph_topological_order(ComputeGraph *g, int *out_count)
{
    int *order = (int *)malloc(g->node_count * sizeof(int));
    int in_degree[GRAPH_MAX_NODES] = {0};
    int queue[GRAPH_MAX_NODES];
    int q_head = 0, q_tail = 0;
    int i, j;

    for (i = 0; i < g->node_count; i++) {
        for (j = 0; j < g->nodes[i].input_count; j++) {
            int in_id = g->nodes[i].inputs[j];
            int k;
            for (k = 0; k < g->node_count; k++) {
                if (g->nodes[k].id == in_id) {
                    in_degree[i]++;
                    break;
                }
            }
        }
    }

    for (i = 0; i < g->node_count; i++) {
        if (in_degree[i] == 0) {
            queue[q_tail++] = i;
        }
    }

    *out_count = 0;
    while (q_head < q_tail) {
        int u = queue[q_head++];
        order[(*out_count)++] = u;
        for (i = 0; i < g->node_count; i++) {
            for (j = 0; j < g->nodes[i].input_count; j++) {
                if (g->nodes[i].inputs[j] == g->nodes[u].id) {
                    in_degree[i]--;
                    if (in_degree[i] == 0) {
                        queue[q_tail++] = i;
                    }
                }
            }
        }
    }
    return order;
}

TensorNode graph_create_tensor(const int *dims, int ndim, DataType dtype,
                               int producer)
{
    TensorNode t;
    memset(&t, 0, sizeof(t));
    t.ndim = ndim > GRAPH_MAX_SHAPE_DIMS ? GRAPH_MAX_SHAPE_DIMS : ndim;
    memcpy(t.dims, dims, t.ndim * sizeof(int));
    t.dtype = dtype;
    t.producer_node_id = producer;
    return t;
}

void graph_tensor_set_shape(TensorNode *t, int d0, int d1, int d2, int d3,
                            int ndim)
{
    t->ndim = ndim;
    t->dims[0] = d0;
    t->dims[1] = d1;
    t->dims[2] = d2;
    t->dims[3] = d3;
}

static void graph_infer_conv2d_shape(TensorNode *input, GraphAttrs *attrs,
                                     TensorNode *out, int out_channels)
{
    int H = input->dims[2];
    int W = input->dims[3];
    int kernel_h = attrs->kernel_size[0];
    int kernel_w = attrs->kernel_size[1];
    int stride_h = attrs->stride[0];
    int stride_w = attrs->stride[1];
    int pad_h = attrs->padding[0];
    int pad_w = attrs->padding[1];
    int out_h = (H + 2 * pad_h - kernel_h) / stride_h + 1;
    int out_w = (W + 2 * pad_w - kernel_w) / stride_w + 1;
    graph_tensor_set_shape(out, input->dims[0], out_channels, out_h, out_w, 4);
}

static void graph_infer_pool_shape(TensorNode *input, GraphAttrs *attrs,
                                   TensorNode *out)
{
    int H = input->dims[2];
    int W = input->dims[3];
    int kernel_h = attrs->kernel_size[0];
    int kernel_w = attrs->kernel_size[1];
    int stride_h = attrs->stride[0];
    int stride_w = attrs->stride[1];
    int out_h = (H - kernel_h) / stride_h + 1;
    int out_w = (W - kernel_w) / stride_w + 1;
    graph_tensor_set_shape(out, input->dims[0], input->dims[1], out_h, out_w, 4);
}

void graph_infer_node_shape(GraphNode *node, TensorNode *tensors, int tensor_count)
{
    (void)tensors;
    (void)tensor_count;
    switch (node->op_type) {
    case GOp_CONV2D:
    case GOp_BATCH_NORM:
    case GOp_RELU:
    case GOp_ADD:
    case GOp_MUL:
    case GOp_FUSED_CONV_BN_RELU:
        break;
    case GOp_SOFTMAX:
    case GOp_RESHAPE:
    case GOp_MATMUL:
    case GOp_CONCAT:
    case GOp_MAXPOOL2D:
    case GOp_AVGPOOL2D:
    case GOp_TRANSPOSE:
    case GOp_FUSED_MATMUL_BIAS_RELU:
    case GOp_FUSED_ELEMWISE_CHAIN:
    default:
        break;
    }
    (void)graph_infer_conv2d_shape;
    (void)graph_infer_pool_shape;
}

bool graph_infer_shapes(ComputeGraph *g)
{
    TensorNode tensors[GRAPH_MAX_NODES];
    int tensor_count = 0;
    int *order;
    int count;
    int i;

    if (!graph_topological_sort(g)) return false;
    order = graph_topological_order(g, &count);
    if (!order) return false;

    for (i = 0; i < count; i++) {
        GraphNode *node = &g->nodes[order[i]];
        if (node->input_count == 0) {
            TensorNode t;
            memset(&t, 0, sizeof(t));
            t.ndim = 4;
            t.dims[0] = 1;
            t.dims[1] = 3;
            t.dims[2] = 224;
            t.dims[3] = 224;
            t.dtype = DType_FLOAT32;
            t.producer_node_id = node->id;
            tensors[tensor_count++] = t;
        } else {
            TensorNode in_shape = tensors[0];
            TensorNode out_shape;
            memset(&out_shape, 0, sizeof(out_shape));
            out_shape.ndim = in_shape.ndim;
            memcpy(out_shape.dims, in_shape.dims, in_shape.ndim * sizeof(int));
            out_shape.dtype = in_shape.dtype;
            out_shape.producer_node_id = node->id;
            tensors[tensor_count++] = out_shape;
        }
        graph_infer_node_shape(node, tensors, tensor_count);
    }

    free(order);
    return true;
}

void graph_print_node(GraphNode *node)
{
    int i;
    printf("  Node[%d] \"%s\" : %s(", node->id, node->name,
           graph_op_type_name(node->op_type));
    for (i = 0; i < node->input_count; i++) {
        if (i > 0) printf(", ");
        printf("%d", node->inputs[i]);
    }
    printf(")");
    if (node->op_type == GOp_CONV2D || node->op_type == GOp_FUSED_CONV_BN_RELU) {
        printf(" kernel=%dx%d stride=%dx%d pad=%dx%d",
               node->attrs.kernel_size[0], node->attrs.kernel_size[1],
               node->attrs.stride[0], node->attrs.stride[1],
               node->attrs.padding[0], node->attrs.padding[1]);
    }
    printf("\n");
}

void graph_print_tensor(TensorNode *t)
{
    int i;
    printf("  Tensor[");
    for (i = 0; i < t->ndim; i++) {
        if (i > 0) printf("x");
        printf("%d", t->dims[i]);
    }
    printf("] %s prod=%d\n", data_type_name(t->dtype), t->producer_node_id);
}

void graph_print(ComputeGraph *g)
{
    int i;
    printf("ComputeGraph (nodes=%d, inputs=%d, output=%d)\n",
           g->node_count, g->input_count, g->output_id);
    for (i = 0; i < g->node_count; i++) {
        graph_print_node(&g->nodes[i]);
    }
}

int graph_find_node_by_name(ComputeGraph *g, const char *name)
{
    int i;
    for (i = 0; i < g->node_count; i++) {
        if (strcmp(g->nodes[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

bool graph_replace_node(ComputeGraph *g, int old_id, GraphNode new_node)
{
    int i;
    for (i = 0; i < g->node_count; i++) {
        if (g->nodes[i].id == old_id) {
            int old_input_count = g->nodes[i].input_count;
            int old_outputs[GRAPH_MAX_OUTPUTS];
            int old_out_count = g->nodes[i].output_count;
            memcpy(old_outputs, g->nodes[i].outputs,
                   old_out_count * sizeof(int));

            g->nodes[i] = new_node;
            g->nodes[i].id = old_id;

            if (new_node.input_count == 0) {
                g->nodes[i].input_count = old_input_count;
            }
            if (new_node.output_count == 0) {
                g->nodes[i].output_count = old_out_count;
                memcpy(g->nodes[i].outputs, old_outputs,
                       old_out_count * sizeof(int));
            }
            return true;
        }
    }
    return false;
}
