#define _CRT_SECURE_NO_WARNINGS
#include "incremental.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static unsigned long djb2_hash(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = (unsigned char)*str++))
        hash = ((hash << 5) + hash) + c;
    return hash;
}

void cache_init(BuildCache *bc, const char *cache_dir) {
    memset(bc, 0, sizeof(*bc));
    strncpy(bc->cache_dir, cache_dir, CACHE_PATH_LEN - 1);
}

void cache_compute_key(const char *input_hash, const char *command,
                       char *key_out, size_t key_size) {
    char combined[1024];
    snprintf(combined, sizeof(combined), "%s|%s", input_hash, command);
    unsigned long hash = djb2_hash(combined);
    snprintf(key_out, key_size, "%016lx", (unsigned long)hash);
}

bool cache_lookup(BuildCache *bc, const char *key,
                  char (*out_files)[CACHE_PATH_LEN], int *num_out) {
    for (int i = 0; i < bc->num_entries; i++) {
        if (bc->entries[i].valid && strcmp(bc->entries[i].key, key) == 0) {
            *num_out = bc->entries[i].num_outputs;
            for (int j = 0; j < bc->entries[i].num_outputs; j++) {
                strncpy(out_files[j], bc->entries[i].output_files[j],
                        CACHE_PATH_LEN - 1);
            }
            return true;
        }
    }
    *num_out = 0;
    return false;
}

void cache_store(BuildCache *bc, const char *key,
                 const char (*out_files)[CACHE_PATH_LEN], int num_out) {
    if (bc->num_entries >= CACHE_MAX_ENTRIES) {
        cache_prune(bc);
    }

    int idx = bc->num_entries;
    for (int i = 0; i < bc->num_entries; i++) {
        if (bc->entries[i].valid && strcmp(bc->entries[i].key, key) == 0) {
            idx = i;
            break;
        }
    }

    strncpy(bc->entries[idx].key, key, CACHE_KEY_LEN - 1);
    bc->entries[idx].num_outputs = num_out;
    for (int j = 0; j < num_out && j < CACHE_MAX_OUTPUTS; j++) {
        strncpy(bc->entries[idx].output_files[j], out_files[j],
                CACHE_PATH_LEN - 1);
    }
    bc->entries[idx].timestamp = time(NULL);
    bc->entries[idx].valid = true;

    if (idx == bc->num_entries)
        bc->num_entries++;
}

void cache_invalidate(BuildCache *bc, const char *key) {
    for (int i = 0; i < bc->num_entries; i++) {
        if (strcmp(bc->entries[i].key, key) == 0) {
            bc->entries[i].valid = false;
            return;
        }
    }
}

void cache_prune(BuildCache *bc) {
    time_t oldest = time(NULL);
    int oldest_idx = 0;

    for (int i = 0; i < bc->num_entries; i++) {
        if (bc->entries[i].valid && bc->entries[i].timestamp < oldest) {
            oldest = bc->entries[i].timestamp;
            oldest_idx = i;
        }
    }
    bc->entries[oldest_idx].valid = false;
}

void cache_print(const BuildCache *bc) {
    printf("\n=== Build Cache (%s) ===\n", bc->cache_dir);
    printf("Entries: %d\n", bc->num_entries);
    for (int i = 0; i < bc->num_entries; i++) {
        const CacheEntry *e = &bc->entries[i];
        if (!e->valid) continue;
        printf("  key=%s outputs=[", e->key);
        for (int j = 0; j < e->num_outputs; j++)
            printf("%s%s", e->output_files[j],
                   j < e->num_outputs - 1 ? ", " : "");
        printf("]\n");
    }
    printf("=========================\n");
}

void tracker_init(BuildTracker *bt) {
    memset(bt, 0, sizeof(*bt));
}

void tracker_add_file(BuildTracker *bt, const char *filepath) {
    if (bt->num_files >= TRACKER_MAX_FILES) return;
    strncpy(bt->files[bt->num_files].filepath, filepath, CACHE_PATH_LEN - 1);
    bt->files[bt->num_files].last_modified = tracker_get_mtime(filepath);
    bt->files[bt->num_files].is_dirty = false;
    bt->num_files++;
}

time_t tracker_get_mtime(const char *filepath) {
    struct stat st;
    if (stat(filepath, &st) == 0)
        return st.st_mtime;
    return 0;
}

void tracker_detect_changes(BuildTracker *bt) {
    bt->num_dirtied = 0;

    for (int i = 0; i < bt->num_files; i++) {
        TrackedFile *f = &bt->files[i];
        time_t current_mtime = tracker_get_mtime(f->filepath);

        if (current_mtime == 0) {
            if (f->last_modified != 0) {
                f->is_dirty = true;
                strncpy(bt->dirtied_files[bt->num_dirtied++], f->filepath,
                        CACHE_PATH_LEN - 1);
            }
        } else if (current_mtime != f->last_modified) {
            f->is_dirty = true;
            strncpy(bt->dirtied_files[bt->num_dirtied++], f->filepath,
                    CACHE_PATH_LEN - 1);
            f->last_modified = current_mtime;
        } else {
            f->is_dirty = false;
        }
    }
}

void tracker_print_changes(const BuildTracker *bt) {
    printf("\n=== File Changes Detected ===\n");
    if (bt->num_dirtied == 0) {
        printf("  No changes detected.\n");
    } else {
        printf("  %d changed file(s):\n", bt->num_dirtied);
        for (int i = 0; i < bt->num_dirtied; i++) {
            printf("    [MODIFIED] %s\n", bt->dirtied_files[i]);
        }
    }
    printf("==============================\n");
}

void tracker_reset(BuildTracker *bt) {
    for (int i = 0; i < bt->num_files; i++) {
        bt->files[i].is_dirty = false;
    }
    bt->num_dirtied = 0;
}

bool tracker_has_changes(const BuildTracker *bt) {
    return bt->num_dirtied > 0;
}
