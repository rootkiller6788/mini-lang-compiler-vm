#ifndef GENERIC_PROG_H
#define GENERIC_PROG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* L1: Generics via type erasure ? simulating parametric polymorphism in C.
 *
 * C does not have native generics. This module implements two approaches:
 * 1. Type-erased containers (void* + function pointers) ? dynamic dispatch
 * 2. Macro-based code generation (compile-time monomorphization)
 *
 * Reference: Milner (1978) "A Theory of Type Polymorphism in Programming";
 * Stroustroup (1994) "The Design and Evolution of C++"
 */

typedef int (*CmpFn)(const void* a, const void* b);
typedef void (*FreeFn)(void* elem);
typedef void* (*CopyFn)(const void* elem);
typedef void (*PrintFn)(const void* elem);

/* L1: Generic vector (dynamic array) with type erasure.
 * L3: Engineering structure ? growth factor of 2 amortizes to O(1) push. */
typedef struct {
    void** data;
    size_t capacity;
    size_t length;
    size_t elem_size;
    FreeFn free_elem;
} GVector;

GVector* gvec_create(size_t elem_size, FreeFn ff);
void     gvec_push(GVector* v, void* elem);
void*    gvec_get(GVector* v, size_t index);
size_t   gvec_len(GVector* v);
void     gvec_sort(GVector* v, CmpFn cmp);
void     gvec_destroy(GVector* v);

/* L1: Generic binary search tree with type erasure.
 * L5: BST operations ? insert/search/delete with O(h) complexity. */
typedef struct GBSTNode GBSTNode;
struct GBSTNode {
    void*      key;
    void*      value;
    GBSTNode*  left;
    GBSTNode*  right;
};

typedef struct {
    GBSTNode* root;
    CmpFn     cmp_key;
    FreeFn    free_key;
    FreeFn    free_val;
    size_t    size;
} GBST;

GBST*    gbst_create(CmpFn cmp_key, FreeFn fk, FreeFn fv);
void     gbst_insert(GBST* t, void* key, void* value);
void*    gbst_search(GBST* t, void* key);
bool     gbst_contains(GBST* t, void* key);
void     gbst_destroy(GBST* t);

/* L7: Application ? generic linked list iterator pattern */
typedef struct GListNode GListNode;
struct GListNode {
    void*      data;
    GListNode* next;
};

typedef struct {
    GListNode* head;
    GListNode* tail;
    size_t     length;
    FreeFn     free_data;
} GLinkedList;

GLinkedList* glist_create(FreeFn ff);
void         glist_append(GLinkedList* list, void* data);
void*        glist_find(GLinkedList* list, const void* target, CmpFn cmp);
void         glist_foreach(GLinkedList* list, void (*fn)(void*));
void         glist_destroy(GLinkedList* list);

#endif
