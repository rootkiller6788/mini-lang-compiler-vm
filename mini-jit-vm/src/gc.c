#include "gc.h"
#include <stdlib.h>
#include <string.h>

void gc_init(GCHeap* heap) {
    heap->objects           = NULL;
    heap->nursery_objects   = NULL;
    heap->total_allocated   = 0;
    heap->nursery_allocated = 0;
    heap->threshold         = GC_HEAP_THRESHOLD;
    heap->gc_count          = 0;
    heap->minor_gc_count    = 0;
    heap->major_gc_count    = 0;
}

bool gc_should_collect(const GCHeap* heap) {
    return heap->total_allocated > heap->threshold;
}

GCObject* gc_alloc(GCHeap* heap, GCObjectType type, size_t size) {
    GCObject* obj = (GCObject*)calloc(1, sizeof(GCObject) + size);
    if (!obj) {
        fprintf(stderr, "gc_alloc: out of memory\n");
        return NULL;
    }
    obj->marked   = false;
    obj->size     = sizeof(GCObject) + size;
    obj->type_tag = type;
    obj->next     = heap->objects;
    heap->objects = obj;
    heap->total_allocated += obj->size;
    return obj;
}

GCObject* gc_alloc_int(GCHeap* heap, int64_t value) {
    GCObject* obj = gc_alloc(heap, GC_INT_OBJ, 0);
    if (obj) {
        obj->data.int_value = value;
    }
    return obj;
}

GCObject* gc_alloc_string(GCHeap* heap, const char* str) {
    size_t len = strlen(str) + 1;
    GCObject* obj = gc_alloc(heap, GC_STRING_OBJ, len);
    if (obj) {
        obj->data.str_value = (char*)(obj + 1);
        memcpy(obj->data.str_value, str, len);
    }
    return obj;
}

void gc_mark(GCObject* obj) {
    if (!obj || obj->marked) return;
    obj->marked = true;

    switch (obj->type_tag) {
        case GC_ARRAY_OBJ:
            if (obj->data.array.elements) {
                for (int32_t i = 0; i < obj->data.array.length; i++) {
                    gc_mark(obj->data.array.elements[i]);
                }
            }
            break;
        case GC_CLOSURE_OBJ:
            if (obj->data.closure.captures) {
                for (int32_t i = 0; i < obj->data.closure.num_captures; i++) {
                    gc_mark(obj->data.closure.captures[i]);
                }
            }
            break;
        default:
            break;
    }
}

void gc_mark_roots(GCHeap* heap, GCObject** roots, int32_t num_roots) {
    (void)heap;
    for (int32_t i = 0; i < num_roots; i++) {
        gc_mark(roots[i]);
    }
}

void gc_sweep(GCHeap* heap) {
    GCObject** prev = &heap->objects;
    GCObject* obj   = heap->objects;

    while (obj) {
        if (!obj->marked) {
            GCObject* unreached = obj;
            *prev = obj->next;
            obj   = obj->next;
            heap->total_allocated -= unreached->size;
            if (unreached->type_tag == GC_STRING_OBJ) {
            }
            free(unreached);
        } else {
            obj->marked = false;
            prev = &obj->next;
            obj  = obj->next;
        }
    }
}

void gc_collect(GCHeap* heap, GCObject** roots, int32_t num_roots) {
    heap->major_gc_count++;
    heap->gc_count++;
#ifdef GC_TRACE
    printf("[GC] Major collection #%d starting...\n", heap->major_gc_count);
    gc_print_stats(heap);
#endif
    gc_mark_roots(heap, roots, num_roots);
    gc_sweep(heap);
#ifdef GC_TRACE
    printf("[GC] Major collection complete.\n");
    gc_print_stats(heap);
#endif
}

void gc_minor_collect(GCHeap* heap, GCObject** roots, int32_t num_roots) {
    heap->minor_gc_count++;
    heap->gc_count++;
#ifdef GC_TRACE
    printf("[GC] Minor collection #%d starting...\n", heap->minor_gc_count);
#endif
    gc_mark_roots(heap, roots, num_roots);

    GCObject** prev = &heap->nursery_objects;
    GCObject* obj   = heap->nursery_objects;

    while (obj) {
        if (obj->marked) {
            obj->marked = false;
            GCObject* promoted = obj;
            *prev = obj->next;
            promoted->next = heap->objects;
            heap->objects = promoted;
            obj = *prev;
        } else {
            GCObject* dead = obj;
            *prev = obj->next;
            obj = obj->next;
            heap->total_allocated -= dead->size;
            free(dead);
        }
    }
    heap->nursery_allocated = 0;
#ifdef GC_TRACE
    printf("[GC] Minor collection complete.\n");
#endif
}

void gc_major_collect(GCHeap* heap, GCObject** roots, int32_t num_roots) {
    gc_collect(heap, roots, num_roots);
}

void gc_write_barrier(GCHeap* heap, GCObject* parent, GCObject* child) {
    (void)heap;
    (void)parent;
    (void)child;
}

void gc_print_stats(const GCHeap* heap) {
    int32_t obj_count = 0;
    size_t live_bytes = 0;
    for (GCObject* o = heap->objects; o; o = o->next) {
        obj_count++;
        live_bytes += o->size;
    }
    printf("GC Heap: %d objects, %zu bytes allocated, %zu threshold\n",
           obj_count, heap->total_allocated, heap->threshold);
    printf("  Major GCs: %d, Minor GCs: %d, Total GCs: %d\n",
           heap->major_gc_count, heap->minor_gc_count, heap->gc_count);
}

void gc_free_all(GCHeap* heap) {
    GCObject* obj = heap->objects;
    while (obj) {
        GCObject* next = obj->next;
        free(obj);
        obj = next;
    }
    obj = heap->nursery_objects;
    while (obj) {
        GCObject* next = obj->next;
        free(obj);
        obj = next;
    }
    heap->objects         = NULL;
    heap->nursery_objects = NULL;
    heap->total_allocated = 0;
}
