#include "generic_prog.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* L3: Generic vector ? amortized O(1) push with growth factor 2.
 * The doubling strategy ensures amortized constant-time append: if we
 * allocate 2^k space and fill it, the total copies across all expansions
 * sum to O(n), giving O(1) amortized per push. */

GVector* gvec_create(size_t elem_size, FreeFn ff) {
    GVector* v = malloc(sizeof(GVector));
    if (!v) return NULL;
    v->data = NULL;
    v->capacity = 0;
    v->length = 0;
    v->elem_size = elem_size;
    v->free_elem = ff;
    return v;
}

void gvec_push(GVector* v, void* elem) {
    if (!v) return;
    if (v->length >= v->capacity) {
        size_t new_cap = v->capacity == 0 ? 4 : v->capacity * 2;
        void** new_data = realloc(v->data, new_cap * sizeof(void*));
        if (!new_data) return;
        v->data = new_data;
        v->capacity = new_cap;
    }
    v->data[v->length++] = elem;
}

void* gvec_get(GVector* v, size_t index) {
    if (!v || index >= v->length) return NULL;
    return v->data[index];
}

size_t gvec_len(GVector* v) { return v ? v->length : 0; }

/* L5: QuickSort on generic vector ? O(n log n) average, O(n^2) worst.
 * Uses the standard in-place partitioning scheme (Hoare 1961). */

static void gvec_qsort_range(GVector* v, CmpFn cmp, int lo, int hi) {
    if (lo >= hi) return;
    void* pivot = v->data[hi];
    int i = lo - 1;
    for (int j = lo; j < hi; j++) {
        if (cmp(v->data[j], pivot) <= 0) {
            i++;
            void* tmp = v->data[i];
            v->data[i] = v->data[j];
            v->data[j] = tmp;
        }
    }
    void* tmp = v->data[i + 1];
    v->data[i + 1] = v->data[hi];
    v->data[hi] = tmp;
    int p = i + 1;
    gvec_qsort_range(v, cmp, lo, p - 1);
    gvec_qsort_range(v, cmp, p + 1, hi);
}

void gvec_sort(GVector* v, CmpFn cmp) {
    if (!v || v->length <= 1) return;
    gvec_qsort_range(v, cmp, 0, (int)(v->length - 1));
}

void gvec_destroy(GVector* v) {
    if (!v) return;
    if (v->free_elem) {
        for (size_t i = 0; i < v->length; i++) {
            v->free_elem(v->data[i]);
        }
    }
    free(v->data);
    free(v);
}

/* L5: Generic BST implementation.
 * BST property: for any node with key k, all keys in left subtree < k,
 * all keys in right subtree > k. Search complexity: O(h) where h is
 * the height of the tree (O(log n) average, O(n) worst case).
 *
 * L4: The BST ordering invariant is a manifestation of the
 * total order property required on the key type. */

GBST* gbst_create(CmpFn cmp_key, FreeFn fk, FreeFn fv) {
    GBST* t = malloc(sizeof(GBST));
    if (!t) return NULL;
    t->root = NULL;
    t->cmp_key = cmp_key;
    t->free_key = fk;
    t->free_val = fv;
    t->size = 0;
    return t;
}

static GBSTNode* gbst_insert_rec(GBSTNode* node, void* key, void* value,
                                  CmpFn cmp, FreeFn fv, bool* inserted) {
    if (!node) {
        GBSTNode* n = malloc(sizeof(GBSTNode));
        if (!n) return NULL;
        n->key = key;
        n->value = value;
        n->left = n->right = NULL;
        *inserted = true;
        return n;
    }
    int c = cmp(key, node->key);
    if (c < 0) {
        node->left = gbst_insert_rec(node->left, key, value, cmp, fv, inserted);
    } else if (c > 0) {
        node->right = gbst_insert_rec(node->right, key, value, cmp, fv, inserted);
    } else {
        if (fv) fv(node->value);
        node->value = value;
    }
    return node;
}

void gbst_insert(GBST* t, void* key, void* value) {
    if (!t) return;
    bool inserted = false;
    t->root = gbst_insert_rec(t->root, key, value, t->cmp_key, t->free_val, &inserted);
    if (inserted) t->size++;
}

void* gbst_search(GBST* t, void* key) {
    if (!t) return NULL;
    GBSTNode* cur = t->root;
    while (cur) {
        int c = t->cmp_key(key, cur->key);
        if (c < 0) cur = cur->left;
        else if (c > 0) cur = cur->right;
        else return cur->value;
    }
    return NULL;
}

bool gbst_contains(GBST* t, void* key) {
    return gbst_search(t, key) != NULL;
}

static void gbst_destroy_rec(GBSTNode* node, FreeFn fk, FreeFn fv) {
    if (!node) return;
    gbst_destroy_rec(node->left, fk, fv);
    gbst_destroy_rec(node->right, fk, fv);
    if (fk) fk(node->key);
    if (fv) fv(node->value);
    free(node);
}

void gbst_destroy(GBST* t) {
    if (!t) return;
    gbst_destroy_rec(t->root, t->free_key, t->free_val);
    free(t);
}

/* L7: Generic linked list with iterator pattern */

GLinkedList* glist_create(FreeFn ff) {
    GLinkedList* list = malloc(sizeof(GLinkedList));
    if (!list) return NULL;
    list->head = NULL;
    list->tail = NULL;
    list->length = 0;
    list->free_data = ff;
    return list;
}

void glist_append(GLinkedList* list, void* data) {
    if (!list) return;
    GListNode* node = malloc(sizeof(GListNode));
    if (!node) return;
    node->data = data;
    node->next = NULL;
    if (list->tail) {
        list->tail->next = node;
    } else {
        list->head = node;
    }
    list->tail = node;
    list->length++;
}

void* glist_find(GLinkedList* list, const void* target, CmpFn cmp) {
    if (!list) return NULL;
    GListNode* cur = list->head;
    while (cur) {
        if (cmp(cur->data, target) == 0) return cur->data;
        cur = cur->next;
    }
    return NULL;
}

void glist_foreach(GLinkedList* list, void (*fn)(void*)) {
    if (!list || !fn) return;
    GListNode* cur = list->head;
    while (cur) {
        fn(cur->data);
        cur = cur->next;
    }
}

void glist_destroy(GLinkedList* list) {
    if (!list) return;
    GListNode* cur = list->head;
    while (cur) {
        GListNode* next = cur->next;
        if (list->free_data) list->free_data(cur->data);
        free(cur);
        cur = next;
    }
    free(list);
}
