#include "auto_schedule.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    TuneTask task;
    AutoTuner tuner;
    ScheduleSpace manual;
    TunerMetrics manual_metrics;

    printf("===== Auto-Scheduling (Ansor-style) Demo =====\n\n");

    task = tuner_task_create("matmul_1024x1024x1024", "matmul",
                             1024, 1024, 1024);
    tuner_print_task(&task);
    printf("\n");

    printf("--- Manual Schedule ---\n");
    manual = tuner_schedule_create("naive_matmul");
    tuner_schedule_set_order(&manual, (int[]){0, 1, 2}, 3);
    manual.unroll_factor = 1;
    manual.vectorize_width = 1;
    tuner_print_schedule(&manual);
    manual_metrics = tuner_evaluate_candidate(&task, &manual);
    tuner_print_metrics(&manual_metrics);
    printf("\n");

    printf("--- Random Sketch Generation ---\n");
    int i;
    for (i = 0; i < 5; i++) {
        ScheduleSpace sketch = tuner_random_sketch(&task);
        TunerMetrics m = tuner_evaluate_candidate(&task, &sketch);
        printf("  Sketch %d: unroll=%d, vec=%d, est=%.2f us\n",
               i + 1, sketch.unroll_factor, sketch.vectorize_width,
               m.estimated_us);
    }
    printf("\n");

    printf("--- Evolutionary Search (pop=16, gen=10) ---\n");
    tuner = tuner_init(task, 16, 10, 0.3, 0.6);
    tuner_evolutionary_search(&tuner);
    tuner_print_best(&tuner);
    printf("\n");

    printf("--- Schedule Mutation Demo ---\n");
    ScheduleSpace parent = tuner_schedule_create("parent");
    tuner_schedule_set_order(&parent, (int[]){0, 1, 2}, 3);
    parent.unroll_factor = 4;
    parent.vectorize_width = 4;
    int split_factors[] = {16, 16, 4};
    tuner_schedule_add_split(&parent, split_factors, 3);

    printf("Parent:\n");
    tuner_print_schedule(&parent);

    for (i = 0; i < 3; i++) {
        ScheduleSpace mutated = tuner_mutate_schedule(&parent, 0.5);
        printf("Mutated #%d:\n", i + 1);
        printf("  unroll=%d, vec=%d\n",
               mutated.unroll_factor, mutated.vectorize_width);
    }
    printf("\n");

    printf("--- Conv2D Auto-Schedule ---\n");
    TuneTask conv_task = tuner_task_conv2d(1, 64, 128, 56, 56, 3, 3, 1, 1);
    AutoTuner conv_tuner = tuner_init(conv_task, 8, 5, 0.3, 0.5);
    tuner_generate_candidates(&conv_tuner);
    tuner_evolutionary_search(&conv_tuner);
    tuner_print_best(&conv_tuner);
    printf("\n");

    printf("Auto-scheduling complete.\n");

    return 0;
}
