#include "auto_schedule.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

TuneTask tuner_task_create(const char *name, const char *op_type,
                           int M, int N, int K)
{
    TuneTask task;
    memset(&task, 0, sizeof(task));
    strncpy(task.workload_name, name, SCHEDULE_MAX_STR_LEN - 1);
    strncpy(task.op_type, op_type, SCHEDULE_MAX_STR_LEN - 1);
    task.M = M;
    task.N = N;
    task.K = K;
    task.batch_size = 1;
    task.in_channels = 0;
    task.out_channels = 0;
    task.kernel_h = 0;
    task.kernel_w = 0;
    return task;
}

TuneTask tuner_task_conv2d(int N, int C, int K, int H, int W,
                           int R, int S, int stride, int pad)
{
    TuneTask task;
    memset(&task, 0, sizeof(task));
    strncpy(task.workload_name, "conv2d", SCHEDULE_MAX_STR_LEN - 1);
    strncpy(task.op_type, "conv2d", SCHEDULE_MAX_STR_LEN - 1);
    task.batch_size = N;
    task.in_channels = C;
    task.out_channels = K;
    task.M = H;
    task.N = W;
    task.K = K;
    task.kernel_h = R;
    task.kernel_w = S;
    (void)stride;
    (void)pad;
    return task;
}

ScheduleSpace tuner_schedule_create(const char *name)
{
    ScheduleSpace s;
    memset(&s, 0, sizeof(s));
    strncpy(s.name, name, SCHEDULE_MAX_STR_LEN - 1);
    s.dim_count = 0;
    s.unroll_factor = 1;
    s.vectorize_width = 1;
    s.use_tensorization = false;
    return s;
}

void tuner_schedule_add_split(ScheduleSpace *s, int factors[], int count)
{
    int i;
    if (s->dim_count >= SCHEDULE_MAX_DIMS) return;
    for (i = 0; i < count && i < SCHEDULE_MAX_FACTORS; i++) {
        s->splits[s->dim_count].factors[i] = factors[i];
    }
    s->splits[s->dim_count].factor_count = count > SCHEDULE_MAX_FACTORS
                                           ? SCHEDULE_MAX_FACTORS : count;
    s->dim_count++;
}

void tuner_schedule_set_order(ScheduleSpace *s, int order[], int count)
{
    int i;
    for (i = 0; i < count && i < SCHEDULE_MAX_DIMS; i++) {
        s->dim_order[i] = order[i];
    }
    s->dim_count = count;
}

void tuner_schedule_set_unroll(ScheduleSpace *s, int factor)
{
    if (factor > 0) {
        s->unroll_factor = factor;
    }
}

void tuner_schedule_set_vectorize(ScheduleSpace *s, int width)
{
    if (width >= 1 && width <= 16) {
        s->vectorize_width = width;
    }
}

AutoTuner tuner_init(TuneTask task, int pop_size, int generations,
                     double mutation_rate, double crossover_rate)
{
    AutoTuner tuner;
    memset(&tuner, 0, sizeof(tuner));
    tuner.task = task;
    tuner.candidate_count = 0;
    tuner.best_idx = -1;
    tuner.population_size = pop_size;
    tuner.generations = generations;
    tuner.mutation_rate = mutation_rate;
    tuner.crossover_rate = crossover_rate;
    srand((unsigned int)time(NULL));
    return tuner;
}

ScheduleSpace tuner_random_sketch(TuneTask *task)
{
    ScheduleSpace s = tuner_schedule_create("sketch");
    int order[] = {0, 1, 2};
    static int split_factors[8];
    int num_factors;
    int i;

    tuner_schedule_set_order(&s, order, 3);

    num_factors = 2 + (rand() % 3);
    for (i = 0; i < num_factors; i++) {
        split_factors[i] = 4 << (rand() % 5);
    }
    tuner_schedule_add_split(&s, split_factors, num_factors);

    s.unroll_factor = 1 << (rand() % 4);
    s.vectorize_width = (rand() % 2) ? 4 : 8;

    (void)task;
    return s;
}

ScheduleSpace tuner_mutate_schedule(ScheduleSpace *parent, double rate)
{
    ScheduleSpace child = *parent;
    int i;

    if ((double)rand() / RAND_MAX < rate) {
        child.unroll_factor = 1 << (rand() % 4);
    }

    if ((double)rand() / RAND_MAX < rate) {
        child.vectorize_width = (rand() % 2) ? 4 : 8;
    }

    if ((double)rand() / RAND_MAX < rate && child.dim_count > 0) {
        int dim_idx = rand() % child.dim_count;
        for (i = 0; i < child.splits[dim_idx].factor_count; i++) {
            if ((double)rand() / RAND_MAX < rate) {
                child.splits[dim_idx].factors[i] = 4 << (rand() % 5);
            }
        }
    }

    if ((double)rand() / RAND_MAX < rate && child.dim_count > 1) {
        int a = rand() % child.dim_count;
        int b = rand() % child.dim_count;
        int tmp = child.dim_order[a];
        child.dim_order[a] = child.dim_order[b];
        child.dim_order[b] = tmp;
    }

    return child;
}

ScheduleSpace tuner_crossover_schedule(ScheduleSpace *a, ScheduleSpace *b)
{
    ScheduleSpace child = *a;
    int crossover_point = rand() % 3;
    int i;

    if (crossover_point == 0) {
        child.unroll_factor = b->unroll_factor;
    } else if (crossover_point == 1) {
        child.vectorize_width = b->vectorize_width;
    } else {
        for (i = 0; i < child.dim_count && i < SCHEDULE_MAX_DIMS; i++) {
            child.dim_order[i] = b->dim_order[i];
        }
    }

    return child;
}

double tuner_estimate_matmul_cost(int M, int N, int K, ScheduleSpace *s)
{
    double total_flops = 2.0 * M * N * K;
    double unroll_benefit = 1.0;
    double vec_benefit = 1.0;
    double reorder_benefit = 1.0;

    if (s->unroll_factor > 1) {
        unroll_benefit = 1.0 + 0.15 * log2((double)s->unroll_factor);
    }
    if (s->vectorize_width > 1) {
        vec_benefit = 1.0 + 0.3 * log2((double)s->vectorize_width);
    }
    if (s->dim_order[0] == 0 && s->dim_order[1] == 1 && s->dim_order[2] == 2) {
        reorder_benefit = 1.0;
    } else {
        reorder_benefit = 0.85;
    }

    return total_flops / (unroll_benefit * vec_benefit * reorder_benefit);
}

double tuner_estimate_conv2d_cost(TuneTask *task, ScheduleSpace *s)
{
    int H_out = task->M;
    int W_out = task->N;
    int K = task->out_channels;
    int C = task->in_channels;
    int R = task->kernel_h;
    int S = task->kernel_w;

    double total_flops = 2.0 * H_out * W_out * K * C * R * S;
    double unroll_benefit = 1.0 + 0.2 * log2((double)(s->unroll_factor > 0 ? s->unroll_factor : 1));
    double vec_benefit = 1.0 + 0.35 * log2((double)(s->vectorize_width > 0 ? s->vectorize_width : 1));

    return total_flops / (unroll_benefit * vec_benefit);
}

double tuner_estimate_memory_time(TunerMetrics *m, double bandwidth_gb_s)
{
    if (bandwidth_gb_s <= 0.0) return 1e9;
    return (m->memory_bytes / (bandwidth_gb_s * 1e9)) * 1e6;
}

double tuner_estimate_compute_time(TunerMetrics *m, double peak_gflops)
{
    if (peak_gflops <= 0.0) return 1e9;
    return (m->flops / (peak_gflops * 1e9)) * 1e6;
}

TunerMetrics tuner_evaluate_candidate(TuneTask *task, ScheduleSpace *schedule)
{
    TunerMetrics m;
    memset(&m, 0, sizeof(m));

    if (strcmp(task->op_type, "matmul") == 0) {
        m.flops = tuner_estimate_matmul_cost(task->M, task->N, task->K, schedule);
    } else if (strcmp(task->op_type, "conv2d") == 0) {
        m.flops = tuner_estimate_conv2d_cost(task, schedule);
    } else {
        m.flops = 1e9;
    }

    m.gflops = m.flops / 1e9;
    m.memory_bytes = (double)(task->M * task->N + task->N * task->K +
                               task->M * task->K) * 4.0;
    m.arithmetic_intensity = m.memory_bytes > 0 ? m.flops / m.memory_bytes : 0.0;

    double compute_us = tuner_estimate_compute_time(&m, 1000.0);
    double memory_us = tuner_estimate_memory_time(&m, 900.0);
    m.estimated_us = compute_us > memory_us ? compute_us : memory_us;
    m.peak_util_pct = 50.0 + 40.0 * ((double)rand() / RAND_MAX);

    return m;
}

static int tuner_compare_metrics(const void *a, const void *b)
{
    double us_a = *(const double *)a;
    double us_b = *(const double *)b;
    if (us_a < us_b) return -1;
    if (us_a > us_b) return 1;
    return 0;
}

void tuner_sort_candidates(AutoTuner *tuner)
{
    int i, j;
    for (i = 0; i < tuner->candidate_count - 1; i++) {
        for (j = 0; j < tuner->candidate_count - i - 1; j++) {
            if (tuner->metrics[j].estimated_us >
                tuner->metrics[j + 1].estimated_us) {
                TunerMetrics tmp_m = tuner->metrics[j];
                tuner->metrics[j] = tuner->metrics[j + 1];
                tuner->metrics[j + 1] = tmp_m;

                ScheduleSpace tmp_s = tuner->candidates[j];
                tuner->candidates[j] = tuner->candidates[j + 1];
                tuner->candidates[j + 1] = tmp_s;
            }
        }
    }
    (void)tuner_compare_metrics;
}

void tuner_select_top_k(AutoTuner *tuner, int k)
{
    tuner_sort_candidates(tuner);
    if (tuner->candidate_count > k) {
        tuner->candidate_count = k;
    }
}

void tuner_generate_candidates(AutoTuner *tuner)
{
    int i;
    for (i = 0; i < tuner->population_size && i < SCHEDULE_MAX_CANDIDATES; i++) {
        tuner->candidates[i] = tuner_random_sketch(&tuner->task);
        tuner->metrics[i] = tuner_evaluate_candidate(&tuner->task,
                                                     &tuner->candidates[i]);
    }
    tuner->candidate_count = tuner->population_size > SCHEDULE_MAX_CANDIDATES
                             ? SCHEDULE_MAX_CANDIDATES : tuner->population_size;
}

void tuner_evolutionary_search(AutoTuner *tuner)
{
    int gen, i;
    int best_schedules_count;

    tuner_generate_candidates(tuner);

    for (gen = 0; gen < tuner->generations; gen++) {
        tuner_sort_candidates(tuner);
        best_schedules_count = tuner->candidate_count / 2;
        if (best_schedules_count < 2) best_schedules_count = 2;

        int new_count = best_schedules_count;
        for (i = 0; i < best_schedules_count && new_count < SCHEDULE_MAX_CANDIDATES; i++) {
            int parent_a = rand() % best_schedules_count;
            int parent_b = rand() % best_schedules_count;

            if ((double)rand() / RAND_MAX < tuner->crossover_rate) {
                tuner->candidates[new_count] = tuner_crossover_schedule(
                    &tuner->candidates[parent_a],
                    &tuner->candidates[parent_b]);
            } else {
                tuner->candidates[new_count] = tuner_mutate_schedule(
                    &tuner->candidates[parent_a], tuner->mutation_rate);
            }
            tuner->metrics[new_count] = tuner_evaluate_candidate(
                &tuner->task, &tuner->candidates[new_count]);
            new_count++;
        }
        tuner->candidate_count = new_count;
    }

    tuner_sort_candidates(tuner);
    tuner->best_idx = 0;
}

void tuner_print_schedule(ScheduleSpace *s)
{
    int i, j;
    printf("  Schedule: %s\n", s->name);
    printf("    dim_order: [");
    for (i = 0; i < s->dim_count; i++) {
        if (i > 0) printf(", ");
        printf("%d", s->dim_order[i]);
    }
    printf("]\n");
    printf("    splits: [");
    for (i = 0; i < s->dim_count; i++) {
        if (i > 0) printf(" | ");
        for (j = 0; j < s->splits[i].factor_count; j++) {
            if (j > 0) printf(" * ");
            printf("%d", s->splits[i].factors[j]);
        }
    }
    printf("]\n");
    printf("    unroll=%d, vectorize=%d, tensorize=%s\n",
           s->unroll_factor, s->vectorize_width,
           s->use_tensorization ? "yes" : "no");
}

void tuner_print_metrics(TunerMetrics *m)
{
    printf("    FLOPS=%.2e, GFLOPS=%.2f\n", m->flops, m->gflops);
    printf("    memory=%.2e bytes, arithmetic_intensity=%.2f\n",
           m->memory_bytes, m->arithmetic_intensity);
    printf("    estimated=%.2f us, peak_util=%.1f%%\n",
           m->estimated_us, m->peak_util_pct);
}

void tuner_print_task(TuneTask *task)
{
    printf("Task: %s (%s)\n", task->workload_name, task->op_type);
    printf("  M=%d N=%d K=%d\n", task->M, task->N, task->K);
    if (task->in_channels > 0) {
        printf("  N=%d C=%d K=%d H=%d W=%d R=%d S=%d\n",
               task->batch_size, task->in_channels, task->out_channels,
               task->M, task->N, task->kernel_h, task->kernel_w);
    }
}

void tuner_print_best(AutoTuner *tuner)
{
    printf("===== Auto Tuning Results =====\n");
    tuner_print_task(&tuner->task);
    printf("Population: %d, Generations: %d\n",
           tuner->population_size, tuner->generations);
    printf("Candidates evaluated: %d\n", tuner->candidate_count);

    if (tuner->candidate_count > 0) {
        int best = tuner->best_idx >= 0 ? tuner->best_idx : 0;
        printf("\nBest schedule (#%d):\n", best);
        tuner_print_schedule(&tuner->candidates[best]);
        tuner_print_metrics(&tuner->metrics[best]);
    }

    printf("\nTop 5 candidates:\n");
    int print_count = tuner->candidate_count < 5 ? tuner->candidate_count : 5;
    int i;
    for (i = 0; i < print_count; i++) {
        printf("  #%d: %.2f us ", i, tuner->metrics[i].estimated_us);
        printf("(unroll=%d, vec=%d)\n",
               tuner->candidates[i].unroll_factor,
               tuner->candidates[i].vectorize_width);
    }
}
