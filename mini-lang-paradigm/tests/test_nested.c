#include <stdio.h>
#include <stdlib.h>
#include "fp_closure.h"

static int my_add(void* a, void* b) { return *(int*)a + *(int*)b; }

int main(void) {
    printf("Testing without nested functions\n");

    /* Use static function pointer via wrapper instead */
    typedef void* (*binop_fn)(void*,void*);
    static void* fold_add_wrapper(void* acc, void* val) {
        *(int*)acc += *(int*)val;
        return acc;
    }

    int vals[] = {1, 2, 3, 4, 5};
    FPList* list = NULL;
    for (int i = 4; i >= 0; i--) {
        list = fp_cons(&vals[i], list);
    }

    int zero = 0;
    fp_foldl(fold_add_wrapper, &zero, list);
    printf("sum=%d\n", zero);

    fp_list_destroy(list);
    printf("OK: no nested functions\n");
    return 0;
}
