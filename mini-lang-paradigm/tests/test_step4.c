#include <stdio.h>
#include "continuation.h"
static int r=0; static void cap(int v, void* c) { (void)c; r=v; }
int main(void) {
    printf("Step 4: CPS test\n");
    cps_add(3,7,cap,NULL); printf("  3+7=%d\n", r);
    cexpr_cps_eval(cexpr_mul(cexpr_add(cexpr_int(2), cexpr_int(3)), cexpr_int(4)), cap, NULL);
    printf("  (2+3)*4=%d\n", r);
    printf("ALL CPS TESTS PASSED\n"); return 0;
}
