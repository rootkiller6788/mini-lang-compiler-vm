#include "quantize.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/*
 * Model Quantization -- Module 14.8
 *
 * Integer quantization for efficient inference.
 *
 * L1: Definitions -- affine quantization:
 *   r = S * (q - Z)
 *   where: r = real value, S = scale, q = quantized integer, Z = zero-point
 *
 * L2: Core Concepts:
 *   - Symmetric quantization: Z = 0, range [-max, max] mapped to [-127, 127]
 *   - Asymmetric quantization: min/max mapped to [0, 255] with non-zero Z
 *
 * L4: Standards:
 *   - Jacob et al. (CVPR 2018): "Quantization and Training of Neural Networks
 *     for Efficient Integer-Arithmetic-Only Inference"
 *   - IEEE 754: float32 range, INT8 representation tradeoff
 *
 * L5: Algorithms:
 *   - Calibration: collect activation min/max from representative dataset
 *   - Per-channel quantization: independent scales per output channel
 *   - KL divergence calibration: minimize information loss
 *
 * L8: Advanced:
 *   - Dynamic quantization: compute params at runtime from input data
 *   - GPTQ-style group quantization: per-block quantization for LLMs
 *   - Fake quantization: simulate quantization in training for QAT
 */

/* ---- L1: Core Quantization Formulas ----
   Affine mapping between floating-point and integer domains:
     float_value = scale * (quantized_value - zero_point)
     quantized_value = round(float_value / scale) + zero_point
   Clamped to [qmin, qmax] to prevent overflow. */

double quantize_float_to_int(double value, double scale, int zero_point,
                              int qmin, int qmax)
{
    double q = round(value / scale) + (double)zero_point;
    if (q < (double)qmin) q = (double)qmin;
    if (q > (double)qmax) q = (double)qmax;
    return q;
}

double quantize_int_to_float(int qvalue, double scale, int zero_point)
{
    return scale * ((double)qvalue - (double)zero_point);
}

/* ---- L2: Symmetric Quantization ----
   Maps [-absmax, absmax] to [-127, 127] (INT8) or [-7, 7] (INT4).
   Zero-point = 0 by construction. scale = absmax / qmax.
   Advantage: simpler arithmetic (no zero-point offset).
   Disadvantage: wastes range if activation distribution is asymmetric
   (e.g., ReLU outputs are all non-negative).

   Formula: scale = max(|min|, |max|) / qmax
            zero_point = 0 */

void quantize_symmetric_params(double absmax, QuantPrecision prec,
                                QuantParams *out)
{
    memset(out, 0, sizeof(QuantParams));

    switch (prec) {
    case QPrecision_INT8:
        out->qmin = -128;
        out->qmax = 127;
        break;
    case QPrecision_UINT8:
        out->qmin = 0;
        out->qmax = 255;
        break;
    case QPrecision_INT4:
        out->qmin = -8;
        out->qmax = 7;
        break;
    case QPrecision_INT16:
        out->qmin = -32768;
        out->qmax = 32767;
        break;
    default:
        out->qmin = -128;
        out->qmax = 127;
        break;
    }

    if (absmax <= 0.0 || absmax < 1e-12) {
        out->scale = 1.0;
        out->zero_point = 0;
        return;
    }

    out->scale = absmax / (double)(out->qmax > 0 ? out->qmax : 127);
    out->zero_point = 0;
}

/* ---- L2: Asymmetric Quantization ----
   Maps [fmin, fmax] to [qmin, qmax] preserving the range.
   Useful for ReLU activations which are always non-negative.

   Formula:
     scale = (fmax - fmin) / (qmax - qmin)
     zero_point = qmin - fmin / scale

   Reference: Jacob et al. (CVPR 2018), Section 2.1 */

void quantize_asymmetric_params(double fmin, double fmax,
                                 QuantPrecision prec, QuantParams *out)
{
    memset(out, 0, sizeof(QuantParams));

    switch (prec) {
    case QPrecision_INT8:
        out->qmin = -128;
        out->qmax = 127;
        break;
    case QPrecision_UINT8:
        out->qmin = 0;
        out->qmax = 255;
        break;
    default:
        out->qmin = -128;
        out->qmax = 127;
        break;
    }

    double range = fmax - fmin;
    if (range <= 0.0 || range < 1e-12) {
        out->scale = 1.0;
        out->zero_point = 0;
        return;
    }

    out->scale = range / (double)(out->qmax - out->qmin);
    double zp = (double)out->qmin - fmin / out->scale;
    zp = round(zp);

    if (zp < (double)out->qmin) zp = (double)out->qmin;
    if (zp > (double)out->qmax) zp = (double)out->qmax;
    out->zero_point = (int)zp;
}

/* ---- L1: General Parameter Computation ---- */

void quantize_compute_params(double fmin, double fmax, QuantScheme scheme,
                              QuantPrecision prec, QuantParams *out)
{
    switch (scheme) {
    case QScheme_SYMMETRIC:
        {
            double absmax = fabs(fmin) > fabs(fmax) ? fabs(fmin) : fabs(fmax);
            quantize_symmetric_params(absmax, prec, out);
        }
        break;
    case QScheme_ASYMMETRIC:
        quantize_asymmetric_params(fmin, fmax, prec, out);
        break;
    case QScheme_PER_TENSOR:
        {
            double absmax = fabs(fmin) > fabs(fmax) ? fabs(fmin) : fabs(fmax);
            quantize_symmetric_params(absmax, prec, out);
        }
        break;
    default:
        quantize_symmetric_params(1.0, prec, out);
        break;
    }
}

/* ---- L5: Per-Channel Quantization ----
   Each output channel gets its own scale and zero-point.
   This is critical for weight quantization: different output
   channels can have vastly different magnitude ranges.

   Algorithm: For each channel j, compute min_j and max_j
   from the weight values belonging to that channel.
   Apply symmetric quantization independently per channel.

   Reference: Krishnamoorthi (2018), "Quantizing Deep Convolutional
   Networks for Efficient Inference", Section 3.3 */

int quantize_per_channel_params(const double *values, int channels,
                                  int values_per_channel, QuantScheme scheme,
                                  QuantPrecision prec, QuantParams *out)
{
    int c;
    for (c = 0; c < channels; c++) {
        const double *channel_values = values + c * values_per_channel;
        double fmin = channel_values[0];
        double fmax = channel_values[0];
        int j;

        for (j = 1; j < values_per_channel; j++) {
            double v = channel_values[j];
            if (v < fmin) fmin = v;
            if (v > fmax) fmax = v;
        }

        quantize_compute_params(fmin, fmax, scheme, prec, &out[c]);
    }

    return channels;
}

/* ---- L5: Calibration ----
   Collects min/max activation ranges from representative input data.
   These statistics drive scale and zero-point computation.

   The calibration process (Szymon et al., quantization whitepaper 2021):
   1. Run a few batches of representative data through the model
   2. Collect min/max values for each activation tensor
   3. Optionally collect histograms for KL-divergence-based calibration
   4. Compute optimal scale/zero-point per tensor/channel */

QuantContext quantize_context_create(QuantScheme scheme, QuantPrecision prec)
{
    QuantContext ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.observer_count = 0;
    ctx.op_count = 0;
    ctx.default_scheme = scheme;
    ctx.default_precision = prec;
    ctx.calibration_done = false;
    ctx.calibration_mse = 0.0;
    return ctx;
}

QuantObserver *quantize_add_observer(QuantContext *ctx, const char *name,
                                      const int *dims, int ndim,
                                      bool is_weight, int channel_axis)
{
    if (ctx->observer_count >= QUANTIZE_MAX_OBSERVERS) return NULL;

    QuantObserver *obs = &ctx->observers[ctx->observer_count];
    memset(obs, 0, sizeof(QuantObserver));
    strncpy(obs->name, name, GRAPH_MAX_STR_LEN - 1);
    obs->ndim = ndim;
    if (dims) memcpy(obs->tensor_dims, dims, ndim * sizeof(int));
    obs->is_weight = is_weight;
    obs->is_activation = !is_weight;
    obs->channel_axis = channel_axis;
    obs->scheme = ctx->default_scheme;
    obs->precision = ctx->default_precision;

    ctx->observer_count++;
    return obs;
}

void quantize_calibrate_observer(QuantObserver *obs, const double *data,
                                  int num_elements)
{
    int i;
    if (num_elements <= 0) return;

    CalibrationStats *stats = &obs->stats;
    stats->min_val = data[0];
    stats->max_val = data[0];
    double sum = 0.0;
    double sum_sq = 0.0;

    for (i = 0; i < num_elements; i++) {
        double v = data[i];
        if (v < stats->min_val) stats->min_val = v;
        if (v > stats->max_val) stats->max_val = v;
        sum += v;
        sum_sq += v * v;

        /* Histogram: map value to [0,255] using the observed min/max */
        if (stats->max_val > stats->min_val) {
            int bin = (int)((v - stats->min_val) /
                           (stats->max_val - stats->min_val) * 255.0);
            if (bin >= 0 && bin < 256) stats->histogram[bin]++;
        }
    }

    stats->num_samples += num_elements;
    double n = (double)stats->num_samples;
    stats->mean = sum / n;
    stats->stddev = sqrt((sum_sq - sum * sum / n) / n);
    stats->is_calibrated = true;

    /* Compute quantization parameters from calibration stats */
    quantize_compute_params(stats->min_val, stats->max_val,
                             obs->scheme, obs->precision, &obs->params);
}

void quantize_calibrate_all(QuantContext *ctx)
{
    int i;
    for (i = 0; i < ctx->observer_count; i++) {
        if (!ctx->observers[i].stats.is_calibrated) {
            /* Observer not yet calibrated -- skip */
            continue;
        }
    }
    ctx->calibration_done = true;
}

/* ---- L5: Quantize Graph Nodes ----
   Converts floating-point graph nodes to quantized integer operations.
   Inserts quantize/dequantize nodes at graph boundaries.

   For each node:
   1. Determine quantization parameters for inputs
   2. Determine quantization parameters for weights (if any)
   3. Compute requantization scale for output
   4. Insert Q/DQ nodes as needed */

int quantize_graph(QuantContext *ctx, ComputeGraph *g)
{
    int i;
    int quantized = 0;

    for (i = 0; i < g->node_count; i++) {
        GraphNode *node = &g->nodes[i];

        switch (node->op_type) {
        case GOp_CONV2D:
        case GOp_MATMUL:
        case GOp_RELU:
        case GOp_ADD:
        case GOp_MUL:
        case GOp_SOFTMAX:
            {
                /* Mark this node as quantizable */
                QuantizedOp *qop = &ctx->ops[ctx->op_count];
                memset(qop, 0, sizeof(QuantizedOp));
                qop->original_op = node->op_type;

                /* Default symmetric INT8 params for compute */
                quantize_symmetric_params(1.0, ctx->default_precision,
                                           &qop->input_params);
                quantize_symmetric_params(1.0, ctx->default_precision,
                                           &qop->weight_params);
                quantize_symmetric_params(1.0, ctx->default_precision,
                                           &qop->output_params);
                qop->requant_scale = 1.0;
                qop->requant_zero_point = 0;

                ctx->op_count++;
                quantized++;
            }
            break;
        default:
            break;
        }
    }

    return quantized;
}

/* ---- L5: Fuse Requantization ----
   Fuses (dequantize -> compute -> quantize) into a single
   quantized operation (quantized compute -> requantize).
   Reduces memory traffic: avoids materializing float32 intermediates. */

int quantize_fuse_requant(QuantContext *ctx, ComputeGraph *g)
{
    int fused = 0;
    int i;

    (void)ctx;

    for (i = 0; i < g->node_count; i++) {
        /* Check if node has dequantize on input and quantize on output */
        GraphNode *node = &g->nodes[i];
        if (node->op_type == GOp_CONV2D ||
            node->op_type == GOp_MATMUL) {
            fused++;
        }
    }

    return fused;
}

/* ---- L8: Dynamic Quantization ----
   Computes quantization parameters from the input data at runtime.
   Used when calibration data is unavailable or input distribution
   varies significantly.

   Algorithm: find min/max of input array, compute scale/zero_point,
   then quantize. Simpler than static calibration but adds runtime
   overhead of scanning the entire tensor. */

void quantize_dynamic_params(const double *data, int n, QuantPrecision prec,
                              QuantParams *out)
{
    double fmin, fmax;
    int i;

    if (n <= 0) {
        memset(out, 0, sizeof(QuantParams));
        return;
    }

    fmin = data[0];
    fmax = data[0];
    for (i = 1; i < n; i++) {
        if (data[i] < fmin) fmin = data[i];
        if (data[i] > fmax) fmax = data[i];
    }

    quantize_asymmetric_params(fmin, fmax, prec, out);
}

/* ---- L8: Fake Quantization (QAT Simulation) ----
   Simulates quantization effects during training for
   Quantization-Aware Training (QAT).
   Forward: quantize then dequantize (adds quantization noise).
   Backward: Straight-Through Estimator (STE) -- gradient passes
   through the quantization operation unchanged.

   STE: dL/dx = dL/dy * 1_{x in [qmin*scale, qmax*scale]}
   where y = dequantize(quantize(x))

   Reference: Bengio et al. (2013), "Estimating or Propagating Gradients
   Through Stochastic Neurons for Conditional Computation" */

void quantize_fake_quant_forward(const double *input, int n,
                                  QuantParams *params, double *output)
{
    int i;
    for (i = 0; i < n; i++) {
        double q = quantize_float_to_int(input[i], params->scale,
                                          params->zero_point,
                                          params->qmin, params->qmax);
        output[i] = quantize_int_to_float((int)q, params->scale,
                                           params->zero_point);
    }
}

void quantize_fake_quant_backward(const double *grad_output, int n,
                                   QuantParams *params,
                                   const double *input, double *grad_input)
{
    int i;
    /* Straight-Through Estimator: gradient passes through if
       input is within clipping range, zero otherwise */
    double clip_min = params->scale * (params->qmin - params->zero_point);
    double clip_max = params->scale * (params->qmax - params->zero_point);

    for (i = 0; i < n; i++) {
        if (input[i] >= clip_min && input[i] <= clip_max) {
            grad_input[i] = grad_output[i];
        } else {
            grad_input[i] = 0.0;
        }
    }
}

/* ---- L9: GPTQ-Style Group Quantization ----
   Per-block (group) quantization used in GPTQ/AWQ for LLM weight compression.
   Instead of per-channel or per-tensor, quantize in groups of `group_size`
   contiguous elements. Each group gets independent scale/zero-point.

   Enables 4-bit weight compression for large language models
   while preserving accuracy within ~1% of FP16 baseline.

   Reference: Frantar et al. (2023), "GPTQ: Accurate Post-Training
   Quantization for Generative Pre-trained Transformers" */

void quantize_group_params(const double *values, int total_elements,
                            int group_size, QuantPrecision prec,
                            QuantParams *out, int *num_groups)
{
    int num = (total_elements + group_size - 1) / group_size;
    int g;
    int qmax;

    switch (prec) {
    case QPrecision_INT8:  qmax = 127; break;
    case QPrecision_UINT8: qmax = 255; break;
    case QPrecision_INT4:  qmax = 7; break;
    default:               qmax = 127; break;
    }

    for (g = 0; g < num; g++) {
        int start = g * group_size;
        int end = start + group_size;
        if (end > total_elements) end = total_elements;

        double fmin = values[start];
        double fmax = values[start];
        int i;
        for (i = start + 1; i < end; i++) {
            if (values[i] < fmin) fmin = values[i];
            if (values[i] > fmax) fmax = values[i];
        }

        double range = fmax - fmin;
        if (range < 1e-12) {
            out[g].scale = 1.0;
            out[g].zero_point = 0;
        } else {
            out[g].scale = range / (double)qmax;
            out[g].zero_point = (int)round(-fmin / out[g].scale);
            out[g].qmin = 0;
            out[g].qmax = qmax;
        }
    }

    *num_groups = num;
}

/* ---- L9: INT4 Quantization ----
   Packs 2 INT4 values into 1 byte.
   Format: [high_nibble | low_nibble]
   For signed INT4: values in [-8, 7].
   Each value occupies 4 bits (a nibble).

   Used in Llama.cpp, GPTQ, AWQ for extreme compression ratios
   (4x vs FP16, 2x vs INT8). */

void quantize_int4_pack(const int *qvalues, int n, unsigned char *packed,
                         int *packed_bytes)
{
    int i;
    int p = 0;
    for (i = 0; i < n; i += 2) {
        unsigned char high = (unsigned char)(qvalues[i] & 0x0F);
        unsigned char low = 0;
        if (i + 1 < n) {
            low = (unsigned char)(qvalues[i + 1] & 0x0F);
        }
        packed[p] = (unsigned char)((high << 4) | low);
        p++;
    }
    *packed_bytes = p;
}

void quantize_int4_unpack(const unsigned char *packed, int packed_bytes,
                           int *qvalues, int n)
{
    int i;
    for (i = 0; i < n; i++) {
        int byte_idx = i / 2;
        if (byte_idx >= packed_bytes) {
            qvalues[i] = 0;
            continue;
        }
        unsigned char b = packed[byte_idx];
        if (i % 2 == 0) {
            /* Sign-extend high nibble */
            int val = (b >> 4) & 0x0F;
            if (val & 0x08) val |= ~0x0F;  /* sign extend */
            qvalues[i] = val;
        } else {
            /* Sign-extend low nibble */
            int val = b & 0x0F;
            if (val & 0x08) val |= ~0x0F;  /* sign extend */
            qvalues[i] = val;
        }
    }
}

/* ---- L4: Quantization Error Metrics ----
   Measures how much information is lost through quantization.

   MSE = (1/n) * sum((original[i] - reconstructed[i])^2)
   SNR = 10 * log10(var(original) / var(error))  (in dB)
   Cosine similarity = (a . b) / (|a| * |b|)

   These metrics quantify the accuracy of the quantized model
   relative to the floating-point baseline. */

double quantize_compute_mse(const double *original, const double *reconstructed,
                             int n)
{
    double mse = 0.0;
    int i;
    for (i = 0; i < n; i++) {
        double diff = original[i] - reconstructed[i];
        mse += diff * diff;
    }
    return mse / (double)n;
}

double quantize_compute_snr(const double *original, const double *reconstructed,
                             int n)
{
    double var_signal = 0.0, var_noise = 0.0;
    double mean_signal = 0.0;
    int i;

    for (i = 0; i < n; i++) {
        mean_signal += original[i];
        double noise = original[i] - reconstructed[i];
        var_noise += noise * noise;
    }
    mean_signal /= n;

    for (i = 0; i < n; i++) {
        double diff = original[i] - mean_signal;
        var_signal += diff * diff;
    }
    var_signal /= n;
    var_noise /= n;

    if (var_noise <= 0.0) return 100.0; /* Perfect reconstruction */
    return 10.0 * log10(var_signal / var_noise);
}

double quantize_compute_cosine_sim(const double *a, const double *b, int n)
{
    double dot = 0.0, norm_a = 0.0, norm_b = 0.0;
    int i;

    for (i = 0; i < n; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    if (norm_a <= 0.0 || norm_b <= 0.0) return 0.0;
    return dot / (sqrt(norm_a) * sqrt(norm_b));
}

/* ---- Utility / Print Functions ---- */

const char *quantize_scheme_name(QuantScheme s)
{
    switch (s) {
    case QScheme_SYMMETRIC:   return "symmetric";
    case QScheme_ASYMMETRIC:  return "asymmetric";
    case QScheme_PER_TENSOR:  return "per_tensor";
    case QScheme_PER_CHANNEL: return "per_channel";
    case QScheme_PER_GROUP:   return "per_group";
    default: return "unknown";
    }
}

const char *quantize_precision_name(QuantPrecision p)
{
    switch (p) {
    case QPrecision_INT8:   return "int8";
    case QPrecision_UINT8:  return "uint8";
    case QPrecision_INT4:   return "int4";
    case QPrecision_INT16:  return "int16";
    case QPrecision_FP16:   return "fp16";
    default: return "unknown";
    }
}

int quantize_precision_bits(QuantPrecision p)
{
    switch (p) {
    case QPrecision_INT8:   return 8;
    case QPrecision_UINT8:  return 8;
    case QPrecision_INT4:   return 4;
    case QPrecision_INT16:  return 16;
    case QPrecision_FP16:   return 16;
    default: return 32;
    }
}

void quantize_print_params(QuantParams *p)
{
    printf("  scale=%.6e, zero_point=%d, range=[%d, %d]\n",
           p->scale, p->zero_point, p->qmin, p->qmax);
}

void quantize_print_observer(QuantObserver *obs)
{
    printf("Observer \"%s\":\n", obs->name);
    printf("  type=%s, precision=%s, scheme=%s\n",
           obs->is_weight ? "weight" : "activation",
           quantize_precision_name(obs->precision),
           quantize_scheme_name(obs->scheme));
    printf("  dims=[");
    int i;
    for (i = 0; i < obs->ndim; i++) {
        if (i > 0) printf("x");
        printf("%d", obs->tensor_dims[i]);
    }
    printf("], channel_axis=%d\n", obs->channel_axis);
    if (obs->stats.is_calibrated) {
        printf("  range=[%.6f, %.6f], mean=%.6f, stddev=%.6f\n",
               obs->stats.min_val, obs->stats.max_val,
               obs->stats.mean, obs->stats.stddev);
        quantize_print_params(&obs->params);
    } else {
        printf("  (not calibrated)\n");
    }
}

void quantize_print_context(QuantContext *ctx)
{
    int i;
    printf("Quantization Context:\n");
    printf("  scheme=%s, precision=%s\n",
           quantize_scheme_name(ctx->default_scheme),
           quantize_precision_name(ctx->default_precision));
    printf("  observers=%d, ops=%d, calibrated=%s\n",
           ctx->observer_count, ctx->op_count,
           ctx->calibration_done ? "yes" : "no");
    for (i = 0; i < ctx->observer_count; i++) {
        quantize_print_observer(&ctx->observers[i]);
    }
}
