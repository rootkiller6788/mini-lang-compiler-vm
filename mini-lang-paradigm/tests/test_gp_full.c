#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "generic_prog.h"

static int int_cmp(const void* a, const void* b) {
    int ia = *(const int*)a, ib = *(const int*)b;
    return (ia > ib) - (ia < ib);
}

int main(void) {
    printf("GP full test\n"); fflush(stdout);

    printf("A"); fflush(stdout);
    GVector* v = gvec_create(sizeof(int), free);
    int* p1 = malloc(sizeof(int)); *p1 = 42;
    int* p2 = malloc(sizeof(int)); *p2 = -7;
    int* p3 = malloc(sizeof(int)); *p3 = 99;
    gvec_push(v, p1); gvec_push(v, p2); gvec_push(v, p3);
    printf(" len=%zu", gvec_len(v)); fflush(stdout);

    printf("B"); fflush(stdout);
    gvec_sort(v, int_cmp);
    int* first = (int*)gvec_get(v, 0);
    int* mid = (int*)gvec_get(v, 1);
    int* last = (int*)gvec_get(v, 2);
    printf(" sorted=%d,%d,%d", *first, *mid, *last); fflush(stdout);

    printf("C"); fflush(stdout);
    GBST* t = gbst_create(int_cmp, free, free);
    int* k1 = malloc(sizeof(int)); *k1 = 10;
    int* v1 = malloc(sizeof(int)); *v1 = 100;
    int* k2 = malloc(sizeof(int)); *k2 = 20;
    int* v2 = malloc(sizeof(int)); *v2 = 200;
    gbst_insert(t, k1, v1);
    gbst_insert(t, k2, v2);
    printf(" contains=%d,%d", gbst_contains(t, k1), gbst_contains(t, k2)); fflush(stdout);

    printf("D"); fflush(stdout);
    int search_key = 10;
    int* found = (int*)gbst_search(t, &search_key);
    printf(" search=%d", found ? *found : -1); fflush(stdout);

    printf("E"); fflush(stdout);
    gbst_destroy(t);
    printf(" bst_destroyed"); fflush(stdout);
    
    printf("F"); fflush(stdout);
    gvec_destroy(v);
    printf(" vec_destroyed"); fflush(stdout);

    printf("\nGP DONE\n");
    return 0;
}
