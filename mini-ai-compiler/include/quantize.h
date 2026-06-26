#ifndef QUANTIZE_H
#define QUANTIZE_H

#include <stdbool.h>
#include <stddef.h>
#include "graph_ir.h"

/*
 * Model Quantization — Module 14.8
 *
 * Integer quantization for efficient inference.
 * References:
 *   - Jacob et al., "Quantization and Training of Neural Networks
 *     for Efficient Integer-Arithmetic-Only Inference" (CVPR 2018)
 *   - Krishnamoorthi, "Quantizing deep convolutional networks
 *     for efficient inference: A whitepaper" (2018)
 *   - Nagel et al., "A White Paper on Neural Network Quantization" (2021)
 *
 * L1: Definitions — scale/zero-point, symmetric/asymmetric
 * L2: Core Concepts — affine mapping float↔int
 * L4: Standards — IEEE 754 float range, INT8 range symmetry
 * L5: Algorithms — calibration, per-channel quantization
 * L8: Advanced Topics — quantization-aware training, dynamic quantization
 */

#define QUANTIZE_MAX_OBSERVERS 64
#define QUANTIZE_INT8_MAX      127.0f
#define QUANTIZE_INT8_MIN     (-128.0f)
#define QUANTIZE_UINT8_MAX     255.0f
#define QUANTIZE_UINT8_MIN     0.0f

/* ---- L1: Definitions — Quantization Scheme ---- */
typedef enum {
    QScheme_SYMMETRIC,       /* scale only, zero_point=0 */
    QScheme_ASYMMETRIC,      /* scale + zero_point */
    QScheme_PER_TENSOR,      /* one scale/zp for whole tensor */
    QScheme_PER_CHANNEL,     /* per-channel scale/zp */
    QScheme_PER_GROUP,       /* per-group (block) quantization */
    QScheme_COUNT
} QuantScheme;

typedef enum {
    QPrecision_INT8,
    QPrecision_UINT8,
    QPrecision_INT4,
    QPrecision_INT16,
    QPrecision_FP16,
    QPrecision_COUNT
} QuantPrecision;

/* ---- L1: Definitions — Quantization Parameters ---- */
typedef struct {
    double scale;
    int    zero_point;
    int    qmin;
    int    qmax;
} QuantParams;

/* ---- L1: Definitions — Calibration Statistics ---- */
typedef struct {
    double min_val;
    double max_val;
    double mean;
    double stddev;
    int    num_samples;
    int    histogram[256];
    bool   is_calibrated;
} CalibrationStats;

/* ---- L1: Definitions — Observer (collects activation ranges) ---- */
typedef struct {
    char name[GRAPH_MAX_STR_LEN];
    CalibrationStats stats;
    QuantParams params;
    QuantScheme scheme;
    QuantPrecision precision;
    bool is_weight;
    bool is_activation;
    int tensor_dims[GRAPH_MAX_SHAPE_DIMS];
    int ndim;
    int channel_axis;
} QuantObserver;

/* ---- L1: Definitions — Quantized Operation ---- */
typedef struct {
    GraphOpType original_op;
    QuantParams input_params;
    QuantParams weight_params;
    QuantParams output_params;
    double requant_scale;
    int    requant_zero_point;
    bool   has_bias;
    QuantParams bias_params;
} QuantizedOp;

/* ---- L1: Definitions — Quantization Context ---- */
typedef struct {
    QuantObserver observers[QUANTIZE_MAX_OBSERVERS];
    int observer_count;
    QuantizedOp ops[QUANTIZE_MAX_OBSERVERS];
    int op_count;
    QuantScheme default_scheme;
    QuantPrecision default_precision;
    bool calibration_done;
    double calibration_mse;
} QuantContext;

/* ---- L1: API Declarations ---- */

/* L1: Core quantization formulas */
double  quantize_float_to_int(double value, double scale, int zero_point,
                               int qmin, int qmax);
double  quantize_int_to_float(int qvalue, double scale, int zero_point);
void    quantize_compute_params(double fmin, double fmax, QuantScheme scheme,
                                QuantPrecision prec, QuantParams *out);

/* L2: Symmetric quantization (scale only, zp=0) */
void quantize_symmetric_params(double absmax, QuantPrecision prec,
                                QuantParams *out);

/* L2: Asymmetric quantization (scale + zp) */
void quantize_asymmetric_params(double fmin, double fmax,
                                 QuantPrecision prec, QuantParams *out);

/* L5: Per-channel quantization parameters */
int  quantize_per_channel_params(const double *values, int channels,
                                  int values_per_channel, QuantScheme scheme,
                                  QuantPrecision prec, QuantParams *out);

/* L5: Calibration — collect activation ranges */
QuantContext quantize_context_create(QuantScheme scheme, QuantPrecision prec);
QuantObserver *quantize_add_observer(QuantContext *ctx, const char *name,
                                      const int *dims, int ndim,
                                      bool is_weight, int channel_axis);
void quantize_calibrate_observer(QuantObserver *obs, const double *data,
                                  int num_elements);
void quantize_calibrate_all(QuantContext *ctx);

/* L5: Quantize graph nodes */
int  quantize_graph(QuantContext *ctx, ComputeGraph *g);

/* L5: Fuse requantization after quantized ops */
int  quantize_fuse_requant(QuantContext *ctx, ComputeGraph *g);

/* L8: Dynamic quantization (calibrate at runtime) */
void quantize_dynamic_params(const double *data, int n, QuantPrecision prec,
                              QuantParams *out);

/* L8: Quantization-aware training simulation */
void quantize_fake_quant_forward(const double *input, int n,
                                  QuantParams *params, double *output);
void quantize_fake_quant_backward(const double *grad_output, int n,
                                   QuantParams *params,
                                   const double *input, double *grad_input);

/* L9: GPTQ-style group quantization */
void quantize_group_params(const double *values, int total_elements,
                            int group_size, QuantPrecision prec,
                            QuantParams *out, int *num_groups);

/* L9: INT4 quantization (4-bit weights) */
void quantize_int4_pack(const int *qvalues, int n, unsigned char *packed,
                         int *packed_bytes);
void quantize_int4_unpack(const unsigned char *packed, int packed_bytes,
                           int *qvalues, int n);

/* Utilities */
void quantize_print_params(QuantParams *p);
void quantize_print_observer(QuantObserver *obs);
void quantize_print_context(QuantContext *ctx);
const char *quantize_scheme_name(QuantScheme s);
const char *quantize_precision_name(QuantPrecision p);
int    quantize_precision_bits(QuantPrecision p);

/* L4: Quantization error metrics */
double quantize_compute_mse(const double *original, const double *reconstructed,
                             int n);
double quantize_compute_snr(const double *original, const double *reconstructed,
                             int n);
double quantize_compute_cosine_sim(const double *a, const double *b, int n);

#endif /* QUANTIZE_H */
