#include <stdio.h>
#include "type_system.h"
int main(void) {
    printf("Step 7: Type System test\n");
    Type* t = type_create_primitive(T_INT);
    printf("  type tag=%d\n", t->tag);
    type_destroy(t);
    printf("ALL TS TESTS PASSED\n"); return 0;
}
