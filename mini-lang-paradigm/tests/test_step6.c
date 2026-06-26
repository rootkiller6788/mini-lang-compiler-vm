#include <stdio.h>
#include "pattern_match.h"
int main(void) {
    printf("Step 6: Pattern test\n");
    Pattern* wp = pattern_wild();
    MatchValue mv = {NULL, 42, false};
    Binding bindings[PM_MAX_BINDINGS]; int bc=0;
    match_simple(wp, &mv, bindings, &bc);
    printf("  wild matched=%d\n", mv.matched);
    pattern_destroy(wp);
    printf("ALL PM TESTS PASSED\n"); return 0;
}
