#ifndef AUTO_SCHEDULE_H
#define AUTO_SCHEDULE_H

#include <stdbool.h>
#include <stddef.h>

#define SCHEDULE_MAX_FACTORS     8
#define SCHEDULE_MAX_CANDIDATES 64
#define SCHEDULE_MAX_DIMS        4
#define SCHEDULE_MAX_STR_LEN    64

typedef enum {
    DimOrder_INPUTS,
    DimOrder_WEIGHTS,
    DimOrder_OUTPUTS,
    DimOrder_COUNT
} DimOrderType;

typedef struct {
    int factors[SCHEDULE_MAX_FACTORS];
    int factor_count;
} SplitFactors;

typedef struct {
    int dim_order[SCHEDULE_MAX_DIMS];
    int dim_count;
    SplitFactors splits[SCHEDULE_MAX_DIMS];
    int unroll_factor;
    int vectorize_width;
    bool use_tensorization;
    char name[SCHEDULE_MAX_STR_LEN];
} ScheduleSpace;

typedef struct {
    char workload_name[SCHEDULE_MAX_STR_LEN];
    int M;
    int N;
    int K;
    int batch_size;
    int in_channels;
    int out_channels;
    int kernel_h;
    int kernel_w;
    char op_type[SCHEDULE_MAX_STR_LEN];
} TuneTask;

typedef struct {
    double flops;
    double gflops;
    double memory_bytes;
    double arithmetic_intensity;
    double estimated_us;
    double peak_util_pct;
} TunerMetrics;

typedef struct {
    TuneTask task;
    ScheduleSpace candidates[SCHEDULE_MAX_CANDIDATES];
    int candidate_count;
    TunerMetrics metrics[SCHEDULE_MAX_CANDIDATES];
    int best_idx;
    int population_size;
    int generations;
    double mutation_rate;
    double crossover_rate;
} AutoTuner;

TuneTask tuner_task_create(const char *name, const char *op_type,
                           int M, int N, int K);
TuneTask tuner_task_conv2d(int N, int C, int K, int H, int W,
                           int R, int S, int stride, int pad);

ScheduleSpace tuner_schedule_create(const char *name);
void tuner_schedule_add_split(ScheduleSpace *s, int factors[], int count);
void tuner_schedule_set_order(ScheduleSpace *s, int order[], int count);
void tuner_schedule_set_unroll(ScheduleSpace *s, int factor);
void tuner_schedule_set_vectorize(ScheduleSpace *s, int width);

AutoTuner tuner_init(TuneTask task, int pop_size, int generations,
                     double mutation_rate, double crossover_rate);
void tuner_generate_candidates(AutoTuner *tuner);
ScheduleSpace tuner_random_sketch(TuneTask *task);
ScheduleSpace tuner_mutate_schedule(ScheduleSpace *parent, double rate);
ScheduleSpace tuner_crossover_schedule(ScheduleSpace *a, ScheduleSpace *b);

TunerMetrics tuner_evaluate_candidate(TuneTask *task, ScheduleSpace *schedule);
double tuner_estimate_matmul_cost(int M, int N, int K, ScheduleSpace *s);
double tuner_estimate_conv2d_cost(TuneTask *task, ScheduleSpace *s);
double tuner_estimate_memory_time(TunerMetrics *m, double bandwidth_gb_s);
double tuner_estimate_compute_time(TunerMetrics *m, double peak_gflops);

void tuner_evolutionary_search(AutoTuner *tuner);
void tuner_select_top_k(AutoTuner *tuner, int k);
void tuner_sort_candidates(AutoTuner *tuner);

void tuner_print_best(AutoTuner *tuner);
void tuner_print_schedule(ScheduleSpace *s);
void tuner_print_metrics(TunerMetrics *m);
void tuner_print_task(TuneTask *task);

#endif
