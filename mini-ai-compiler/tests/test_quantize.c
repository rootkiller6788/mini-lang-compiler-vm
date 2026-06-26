#include "quantize.h"
#include <stdio.h>
#include <assert.h>
#include <math.h>

int main(void)
{
    printf("=== Test: Quantization ===\n");

    /* Test symmetric quantization params */
    QuantParams sym_params;
    quantize_symmetric_params(1.0, QPrecision_INT8, &sym_params);
    assert(sym_params.zero_point == 0);
    assert(sym_params.scale > 0.0);
    printf("Symmetric: scale=%.6e, zp=%d\n", sym_params.scale,
           sym_params.zero_point);

    /* Test asymmetric quantization */
    QuantParams asym_params;
    quantize_asymmetric_params(-3.0, 7.0, QPrecision_INT8, &asym_params);
    printf("Asymmetric: scale=%.6e, zp=%d\n", asym_params.scale,
           asym_params.zero_point);

    /* Test float-int conversion roundtrip */
    double original = 0.5;
    double q = quantize_float_to_int(original, 0.01, 0, -128, 127);
    double restored = quantize_int_to_float((int)q, 0.01, 0);
    double error = fabs(original - restored);
    assert(error < 0.02);
    printf("Roundtrip error: %.6e\n", error);

    /* Test per-channel quantization */
    double values[6] = {-1.0, 0.0, 1.0, -2.0, 0.0, 2.0};
    QuantParams per_channel[2];
    int n = quantize_per_channel_params(values, 2, 3,
                                         QScheme_SYMMETRIC,
                                         QPrecision_INT8, per_channel);
    assert(n == 2);
    printf("Per-channel: %d channels computed\n", n);
    printf("  Ch0: scale=%.6e, zp=%d\n", per_channel[0].scale,
           per_channel[0].zero_point);
    printf("  Ch1: scale=%.6e, zp=%d\n", per_channel[1].scale,
           per_channel[1].zero_point);

    /* Test calibration */
    QuantContext qctx = quantize_context_create(QScheme_SYMMETRIC,
                                                 QPrecision_INT8);
    int dims[4] = {1, 3, 224, 224};
    QuantObserver *obs = quantize_add_observer(&qctx, "conv1_out",
                                                dims, 4, false, 0);
    assert(obs != NULL);

    double calib_data[100];
    int i;
    for (i = 0; i < 100; i++) calib_data[i] = (double)i / 100.0;
    quantize_calibrate_observer(obs, calib_data, 100);
    assert(obs->stats.is_calibrated);
    printf("Calibrated: range=[%.3f, %.3f]\n",
           obs->stats.min_val, obs->stats.max_val);

    /* Test dynamic quantization */
    QuantParams dyn_params;
    quantize_dynamic_params(calib_data, 100, QPrecision_INT8, &dyn_params);
    printf("Dynamic: scale=%.6e, zp=%d\n", dyn_params.scale,
           dyn_params.zero_point);

    /* Test fake quantization */
    double fake_out[4];
    QuantParams fake_params;
    quantize_symmetric_params(1.0, QPrecision_INT8, &fake_params);
    quantize_fake_quant_forward((double[]){-1.5, -0.5, 0.5, 1.5},
                                 4, &fake_params, fake_out);
    printf("FakeQuant: [%.3f, %.3f, %.3f, %.3f]\n",
           fake_out[0], fake_out[1], fake_out[2], fake_out[3]);

    /* Test group quantization */
    double group_data[16];
    for (i = 0; i < 16; i++) group_data[i] = (double)i - 8.0;
    QuantParams group_params[4];
    int num_groups;
    quantize_group_params(group_data, 16, 4, QPrecision_INT8,
                           group_params, &num_groups);
    assert(num_groups == 4);
    printf("Group quant: %d groups\n", num_groups);

    /* Test INT4 packing */
    int q4_values[8] = {-1, 0, 1, 2, -3, 4, -5, 6};
    unsigned char packed[4];
    int packed_bytes;
    quantize_int4_pack(q4_values, 8, packed, &packed_bytes);
    assert(packed_bytes == 4);
    int unpacked[8];
    quantize_int4_unpack(packed, 4, unpacked, 8);
    printf("INT4 pack/unpack: %d bytes\n", packed_bytes);
    for (i = 0; i < 8; i++) assert(q4_values[i] == unpacked[i]);

    /* Test error metrics */
    double orig[5] = {1.0, 2.0, 3.0, 4.0, 5.0};
    double recon[5] = {0.9, 2.1, 3.0, 3.9, 5.1};
    double mse = quantize_compute_mse(orig, recon, 5);
    double snr = quantize_compute_snr(orig, recon, 5);
    double cos_sim = quantize_compute_cosine_sim(orig, recon, 5);
    printf("MSE=%.6f, SNR=%.2f dB, CosSim=%.4f\n", mse, snr, cos_sim);

    quantize_print_context(&qctx);

    printf("\nAll quantization tests passed!\n");
    return 0;
}
