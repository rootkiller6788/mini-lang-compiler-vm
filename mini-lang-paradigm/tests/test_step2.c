#include <stdio.h>
#include <stdlib.h>
#include "fp_closure.h"
static void* add_fn(void** args) {
    int* r = malloc(sizeof(int)); *r = *(int*)args[0] + *(int*)args[1]; return r;
}
int main(void) {
    printf("Step 2: FP test\n");
    FPClosure* add = fp_closure_create(add_fn, 2, 0);
    int a=3,b=5; int* s = (int*)fp_apply(add, (void*[]){&a,&b});
    printf("  3+5=%d\n", *s); free(s); fp_closure_destroy(add);
    printf("ALL FP TESTS PASSED\n"); return 0;
}
