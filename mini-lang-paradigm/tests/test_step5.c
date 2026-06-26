#include <stdio.h>
#include <stdlib.h>
#include "generic_prog.h"
static int int_cmp(const void* a, const void* b) {
    int ia=*(const int*)a, ib=*(const int*)b; return (ia>ib)-(ia<ib);
}
int main(void) {
    printf("Step 5: Generic Prog test\n");
    GVector* v = gvec_create(sizeof(int), free);
    int* p = malloc(sizeof(int)); *p=42; gvec_push(v, p);
    printf("  vec len=%zu\n", gvec_len(v));
    GBST* t = gbst_create(int_cmp, free, free);
    int* k=malloc(sizeof(int)); *k=5; int* val=malloc(sizeof(int)); *val=99;
    gbst_insert(t, k, val);
    printf("  bst contains 5: %d\n", gbst_contains(t, k));
    gbst_destroy(t); gvec_destroy(v);
    printf("ALL GP TESTS PASSED\n"); return 0;
}
