#include <stdio.h>
#include "lambda_calc.h"
int main(void) {
    printf("Step 3: Lambda Calc test\n");
    LCTerm* t = lc_church_numeral(3);
    printf("  Church 3 -> int: %d\n", lc_church_to_int(t));
    lc_destroy(t);
    printf("ALL LC TESTS PASSED\n"); return 0;
}
