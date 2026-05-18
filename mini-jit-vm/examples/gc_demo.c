#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "gc.h"

int main(void) {
    printf("=== Garbage Collection Demo ===\n\n");

    GCHeap heap;
    gc_init(&heap);

    GCObject* roots[GC_MAX_ROOTS];
    int32_t num_roots = 0;

    printf("--- Phase 1: Allocating Objects ---\n");

    for (int32_t i = 0; i < 10; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "string_%d", i);
        GCObject* str = gc_alloc_string(&heap, buf);
        roots[num_roots++] = str;
    }

    for (int32_t i = 0; i < 10; i++) {
        GCObject* num = gc_alloc_int(&heap, i * 100);
        roots[num_roots++] = num;
    }

    printf("After allocation (20 objects):\n");
    gc_print_stats(&heap);

    printf("\n--- Phase 2: Minor GC (nursery sweep) ---\n");
    heap.nursery_objects = heap.objects;
    heap.objects = NULL;

    for (int32_t i = 0; i < 10; i++) {
        char buf[32];
        snprintf(buf, sizeof(buf), "nursery_str_%d", i);
        GCObject* ns = gc_alloc_string(&heap, buf);
        ns->next = heap.nursery_objects;
        heap.nursery_objects = ns;
        heap.nursery_allocated += ns->size;
        roots[num_roots++] = ns;
    }

    gc_print_stats(&heap);

    printf("\nPerforming minor collection...\n");
    gc_minor_collect(&heap, roots, num_roots);
    printf("After minor collection:\n");
    gc_print_stats(&heap);

    printf("\n--- Phase 3: Mark-Sweep Full GC ---\n");
    for (int32_t i = 0; i < 5; i++) {
        GCObject* extra = gc_alloc_int(&heap, i * 1000);
    }
    printf("Before major GC:\n");
    gc_print_stats(&heap);

    printf("\nPerforming major collection (mark-sweep)...\n");
    gc_major_collect(&heap, roots, num_roots);
    printf("After major GC:\n");
    gc_print_stats(&heap);

    printf("\n--- Phase 4: Write Barrier Test ---\n");
    GCObject* parent = gc_alloc_int(&heap, -1);
    GCObject* child  = gc_alloc_int(&heap, -2);
    printf("Before write barrier:\n");
    gc_print_stats(&heap);
    gc_write_barrier(&heap, parent, child);
    printf("Write barrier executed (stub).\n");

    printf("\n--- Phase 5: Cleanup ---\n");
    gc_free_all(&heap);
    printf("All objects freed.\n");

    printf("\n=== Demo Complete ===\n");
    return 0;
}
