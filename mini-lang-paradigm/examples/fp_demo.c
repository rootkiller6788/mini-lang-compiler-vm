#include "fp_closure.h"

#include <stdio.h>
#include <stdlib.h>

static void* safe_sum_fn(void** args) {
    int sum = 0;
    for (int i = 0; i < 2; i++) {
        if (args[i] != NULL) {
            sum += *(int*)args[i];
        }
    }
    int* result = malloc(sizeof(int));
    *result = sum;
    return result;
}

static void* add_ten_fn(void** args) {
    int* val = (int*)args[0];
    int* result = malloc(sizeof(int));
    *result = *val + 10;
    return result;
}

static void* double_fn(void** args) {
    int* val = (int*)args[0];
    int* result = malloc(sizeof(int));
    *result = *val * 2;
    return result;
}

static void* sum_fold_fn(void* a, void* b) {
    int* ia = (int*)a;
    int* ib = (int*)b;
    int* result = malloc(sizeof(int));
    *result = *ia + *ib;
    return result;
}

static void* product_fold_fn(void* a, void* b) {
    int* ia = (int*)a;
    int* ib = (int*)b;
    int* result = malloc(sizeof(int));
    *result = *ia * *ib;
    return result;
}

int main(void) {
    printf("=== FP Closure & Curry Demo ===\n\n");

    printf("--- Closures ---\n");
    FPClosure* add_closure = fp_closure_create(safe_sum_fn, 2, 0);
    int a = 3, b = 5;
    void* sum_args[] = { &a, &b };
    int* sum_result = (int*)fp_apply(add_closure, sum_args);
    printf("add(3, 5) = %d\n", *sum_result);
    free(sum_result);
    fp_closure_destroy(add_closure);

    printf("\n--- Curried add ---\n");
    FPClosure* curried_add = fp_curry(safe_sum_fn, 2);
    printf("Curried add created (arity=%d)\n", curried_add->arity);
    int  x = 7, y = 8;
    void* curry_args[] = { &x, &y };
    int* curry_result = (int*)fp_apply(curried_add, curry_args);
    printf("curried_add(7)(8) = %d\n", *curry_result);
    free(curry_result);
    fp_closure_destroy(curried_add);

    printf("\n--- Function composition ---\n");
    FPClosure* add10 = fp_closure_create(add_ten_fn, 1, 0);
    FPClosure* dub   = fp_closure_create(double_fn, 1, 0);
    FPClosure* composed = fp_compose(dub, add10);
    int val = 5;
    void* comp_args[] = { &val };
    int* comp_result = (int*)fp_apply(composed, comp_args);
    printf("double(add10(5)) = double(15) = %d\n", *comp_result);
    free(comp_result);
    fp_closure_destroy(add10);
    fp_closure_destroy(dub);
    fp_closure_destroy(composed);

    printf("\n--- FP List operations ---\n");
    FPList* list = NULL;
    for (int i = 10; i >= 1; i--) {
        int* num = malloc(sizeof(int));
        *num = i;
        list = fp_cons(num, list);
    }

    printf("List: ");
    for (FPList* cur = list; cur; cur = cur->tail) {
        printf("%d ", *(int*)cur->value);
    }
    printf("\nLength: %d\n", fp_list_length(list));

    printf("\n--- map (double) over list ---\n");
    FPClosure* map_dub = fp_closure_create(double_fn, 1, 0);
    FPList* mapped = fp_map(map_dub, list);
    printf("Mapped: ");
    for (FPList* cur = mapped; cur; cur = cur->tail) {
        printf("%d ", *(int*)cur->value);
    }
    printf("\n");
    fp_closure_destroy(map_dub);

    printf("\n--- foldl (sum) ---\n");
    int init_val = 0;
    int* sum_all = (int*)fp_foldl(sum_fold_fn, &init_val, list);
    printf("Sum: %d\n", *sum_all);
    free(sum_all);

    printf("\n--- foldr (product) ---\n");
    int init_prod = 1;
    int* prod_all = (int*)fp_foldr(product_fold_fn, &init_prod, list);
    printf("Product: %d\n", *prod_all);
    free(prod_all);

    printf("\n--- reverse ---\n");
    FPList* reversed = fp_list_reverse(list);
    printf("Reversed: ");
    for (FPList* cur = reversed; cur; cur = cur->tail) {
        printf("%d ", *(int*)cur->value);
    }
    printf("\n");

    printf("\n--- range ---\n");
    FPList* r = fp_range(0, 5);
    printf("range(0,5): ");
    for (FPList* cur = r; cur; cur = cur->tail) {
        printf("%d ", *(int*)cur->value);
    }
    printf("\n");

    fp_list_destroy(list);
    fp_list_destroy(mapped);
    fp_list_destroy(reversed);
    fp_list_destroy(r);

    printf("\nDone.\n");
    return 0;
}
