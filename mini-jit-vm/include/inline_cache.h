#ifndef INLINE_CACHE_H
#define INLINE_CACHE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define PIC_MAX_ENTRIES 4

typedef void* ClassPtr;

typedef int64_t (*MethodPtr)(void* receiver, int64_t* args, int32_t argc);

typedef struct {
    ClassPtr   cached_class;
    MethodPtr  cached_method_ptr;
    int32_t    hit_count;
    int32_t    miss_count;
} ICsCache;

typedef struct {
    ClassPtr   cls;
    MethodPtr  method;
} PICEntry;

typedef struct {
    PICEntry   entries[PIC_MAX_ENTRIES];
    int32_t    num_entries;
    int32_t    total_hits;
    int32_t    total_misses;
} PolymorphicICS;

void     ic_init(ICsCache* ic);
bool     ic_is_cached(const ICsCache* ic, ClassPtr target_class);
MethodPtr ic_lookup(ICsCache* ic, ClassPtr target_class,
                    const char* method_name);
void     ic_update_monomorphic(ICsCache* ic, ClassPtr cls, MethodPtr method);
void     ic_hit(ICsCache* ic);
void     ic_miss(ICsCache* ic);

void     pic_init(PolymorphicICS* pic);
MethodPtr pic_lookup(PolymorphicICS* pic, ClassPtr target_class,
                     const char* method_name);
void     pic_update(PolymorphicICS* pic, ClassPtr cls, MethodPtr method);
bool     pic_is_full(const PolymorphicICS* pic);
void     pic_print_stats(const PolymorphicICS* pic);

int64_t  ic_dispatch(ICsCache* ic, ClassPtr receiver_class,
                     const char* method_name, void* receiver,
                     int64_t* args, int32_t argc);

#endif
