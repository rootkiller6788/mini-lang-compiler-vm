#ifndef GC_H
#define GC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define GC_NURSERY_SIZE     (64 * 1024)
#define GC_HEAP_THRESHOLD   (256 * 1024)
#define GC_MAX_ROOTS        256

typedef enum {
    GC_INT_OBJ     = 0,
    GC_STRING_OBJ  = 1,
    GC_ARRAY_OBJ   = 2,
    GC_CLOSURE_OBJ = 3
} GCObjectType;

typedef struct GCObject {
    bool            marked;
    size_t          size;
    GCObjectType    type_tag;
    struct GCObject* next;
    union {
        int64_t  int_value;
        char*    str_value;
        struct {
            struct GCObject** elements;
            int32_t           length;
        } array;
        struct {
            void*   func_ptr;
            int32_t num_captures;
            struct GCObject** captures;
        } closure;
    } data;
} GCObject;

typedef struct {
    GCObject*   objects;
    GCObject*   nursery_objects;
    size_t      total_allocated;
    size_t      nursery_allocated;
    size_t      threshold;
    int32_t     gc_count;
    int32_t     minor_gc_count;
    int32_t     major_gc_count;
} GCHeap;

void      gc_init(GCHeap* heap);
GCObject* gc_alloc(GCHeap* heap, GCObjectType type, size_t size);
void      gc_mark_roots(GCHeap* heap, GCObject** roots, int32_t num_roots);
void      gc_mark(GCObject* obj);
void      gc_sweep(GCHeap* heap);
void      gc_collect(GCHeap* heap, GCObject** roots, int32_t num_roots);
void      gc_minor_collect(GCHeap* heap, GCObject** roots, int32_t num_roots);
void      gc_major_collect(GCHeap* heap, GCObject** roots, int32_t num_roots);
void      gc_write_barrier(GCHeap* heap, GCObject* parent, GCObject* child);
void      gc_print_stats(const GCHeap* heap);
bool      gc_should_collect(const GCHeap* heap);
void      gc_free_all(GCHeap* heap);
GCObject* gc_alloc_int(GCHeap* heap, int64_t value);
GCObject* gc_alloc_string(GCHeap* heap, const char* str);

#endif
