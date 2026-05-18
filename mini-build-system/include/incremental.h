#ifndef INCREMENTAL_H
#define INCREMENTAL_H

#include <stdbool.h>
#include <time.h>

#define CACHE_MAX_ENTRIES   512
#define CACHE_MAX_OUTPUTS    16
#define CACHE_KEY_LEN        64
#define CACHE_PATH_LEN       256
#define TRACKER_MAX_FILES   256

typedef struct {
    char   key[CACHE_KEY_LEN];
    char   output_files[CACHE_MAX_OUTPUTS][CACHE_PATH_LEN];
    int    num_outputs;
    time_t timestamp;
    bool   valid;
} CacheEntry;

typedef struct {
    CacheEntry entries[CACHE_MAX_ENTRIES];
    int        num_entries;
    char       cache_dir[CACHE_PATH_LEN];
} BuildCache;

typedef struct {
    char   filepath[CACHE_PATH_LEN];
    time_t last_modified;
    bool   is_dirty;
} TrackedFile;

typedef struct {
    TrackedFile files[TRACKER_MAX_FILES];
    int         num_files;
    char        dirtied_files[TRACKER_MAX_FILES][CACHE_PATH_LEN];
    int         num_dirtied;
} BuildTracker;

void cache_init(BuildCache *bc, const char *cache_dir);
bool cache_lookup(BuildCache *bc, const char *key, char (*out_files)[CACHE_PATH_LEN], int *num_out);
void cache_store(BuildCache *bc, const char *key, const char (*out_files)[CACHE_PATH_LEN], int num_out);
void cache_invalidate(BuildCache *bc, const char *key);
void cache_prune(BuildCache *bc);
void cache_print(const BuildCache *bc);
void cache_compute_key(const char *input_hash, const char *command, char *key_out, size_t key_size);

void tracker_init(BuildTracker *bt);
void tracker_add_file(BuildTracker *bt, const char *filepath);
void tracker_detect_changes(BuildTracker *bt);
void tracker_print_changes(const BuildTracker *bt);
void tracker_reset(BuildTracker *bt);
bool tracker_has_changes(const BuildTracker *bt);
time_t tracker_get_mtime(const char *filepath);

#endif
