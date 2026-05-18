#include "inline_cache.h"
#include <string.h>
#include <stdio.h>

void ic_init(ICsCache* ic) {
    ic->cached_class     = NULL;
    ic->cached_method_ptr = NULL;
    ic->hit_count        = 0;
    ic->miss_count       = 0;
}

bool ic_is_cached(const ICsCache* ic, ClassPtr target_class) {
    return ic->cached_class == target_class && ic->cached_method_ptr != NULL;
}

MethodPtr ic_lookup(ICsCache* ic, ClassPtr target_class, const char* method_name) {
    (void)method_name;

    if (ic_is_cached(ic, target_class)) {
        ic->hit_count++;
        return ic->cached_method_ptr;
    }

    ic->miss_count++;
    return NULL;
}

void ic_update_monomorphic(ICsCache* ic, ClassPtr cls, MethodPtr method) {
    ic->cached_class     = cls;
    ic->cached_method_ptr = method;
}

void ic_hit(ICsCache* ic) {
    ic->hit_count++;
}

void ic_miss(ICsCache* ic) {
    ic->miss_count++;
}

void pic_init(PolymorphicICS* pic) {
    for (int32_t i = 0; i < PIC_MAX_ENTRIES; i++) {
        pic->entries[i].cls    = NULL;
        pic->entries[i].method = NULL;
    }
    pic->num_entries  = 0;
    pic->total_hits   = 0;
    pic->total_misses = 0;
}

MethodPtr pic_lookup(PolymorphicICS* pic, ClassPtr target_class,
                     const char* method_name) {
    (void)method_name;

    for (int32_t i = 0; i < pic->num_entries; i++) {
        if (pic->entries[i].cls == target_class) {
            pic->total_hits++;
            return pic->entries[i].method;
        }
    }
    pic->total_misses++;
    return NULL;
}

void pic_update(PolymorphicICS* pic, ClassPtr cls, MethodPtr method) {
    for (int32_t i = 0; i < pic->num_entries; i++) {
        if (pic->entries[i].cls == cls) {
            pic->entries[i].method = method;
            return;
        }
    }

    if (pic->num_entries < PIC_MAX_ENTRIES) {
        pic->entries[pic->num_entries].cls    = cls;
        pic->entries[pic->num_entries].method = method;
        pic->num_entries++;
    } else {
        for (int32_t i = 0; i < PIC_MAX_ENTRIES - 1; i++) {
            pic->entries[i] = pic->entries[i + 1];
        }
        pic->entries[PIC_MAX_ENTRIES - 1].cls    = cls;
        pic->entries[PIC_MAX_ENTRIES - 1].method = method;
    }
}

bool pic_is_full(const PolymorphicICS* pic) {
    return pic->num_entries >= PIC_MAX_ENTRIES;
}

void pic_print_stats(const PolymorphicICS* pic) {
    printf("PIC stats: entries=%d, hits=%d, misses=%d (%.1f%% hit rate)\n",
           pic->num_entries, pic->total_hits, pic->total_misses,
           (pic->total_hits + pic->total_misses) > 0
               ? 100.0 * pic->total_hits / (pic->total_hits + pic->total_misses)
               : 0.0);
    for (int32_t i = 0; i < pic->num_entries; i++) {
        printf("  [%d] class=%p method=%p\n",
               i, (void*)pic->entries[i].cls, (void*)pic->entries[i].method);
    }
}

int64_t ic_dispatch(ICsCache* ic, ClassPtr receiver_class,
                    const char* method_name, void* receiver,
                    int64_t* args, int32_t argc) {
    MethodPtr method = ic_lookup(ic, receiver_class, method_name);
    if (method) {
        return method(receiver, args, argc);
    }

    fprintf(stderr, "ic_dispatch: method '%s' cache miss for class %p\n",
            method_name, (void*)receiver_class);
    return 0;
}
