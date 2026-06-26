#ifndef HASH_CACHE_H
#define HASH_CACHE_H

#include <stdbool.h>
#include <stdint.h>

/* ============================================================================
 * L4: Standards - Content-Addressed Storage (CAS)
 * L5: Algorithms - Merkle Tree Construction
 * L8: Advanced Topic - Incremental Hashing with Rolling Hash
 *
 * Content-addressed storage identifies artifacts by their content hash
 * rather than by name/path. This is the foundation of:
 *   - Bazel's action cache
 *   - Git's object store
 *   - IPFS (InterPlanetary File System)
 *   - Nix package manager's /nix/store
 *
 * Reference: Merkle, R.C. "A Digital Signature Based on a Conventional
 *            Encryption Function" (CRYPTO '87)
 * Course: CMU 15-445/15-721 (Database Systems), Stanford CS 245
 * ========================================================================= */

#define HASH_MAX_ENTRIES   512
#define HASH_MAX_CHILDREN  64
#define HASH_STRING_LEN    32   /* 16 hex chars + null for 64-bit hash */
#define HASH_PATH_LEN      256

/* 64-bit hash type (simplified; production uses SHA-256) */
typedef uint64_t HashValue;

/* L1: ContentHash - a content-addressed cache entry */
typedef struct {
    HashValue   hash;
    char        file_path[HASH_PATH_LEN];
    uint64_t    file_size;
    bool        is_valid;
} ContentHash;

/* L1: HashCache - content-addressed storage */
typedef struct {
    ContentHash entries[HASH_MAX_ENTRIES];
    int         num_entries;
    char        cache_root[HASH_PATH_LEN];
} HashCache;

/* L1: MerkleNode - node in a Merkle tree */
typedef struct {
    HashValue   hash;             /* combined hash of children */
    char        label[HASH_PATH_LEN];
    int         children[HASH_MAX_CHILDREN];
    int         num_children;
    bool        is_leaf;
} MerkleNode;

/* L1: MerkleTree - hash tree for integrity verification */
typedef struct {
    MerkleNode nodes[HASH_MAX_ENTRIES];
    int        num_nodes;
    HashValue  root_hash;
} MerkleTree;

/* L5: Hash functions (djb2 algorithm, widely used in practice) */
HashValue hash_string(const char *str);
HashValue hash_buffer(const uint8_t *data, size_t len);
HashValue hash_file(const char *filepath);
HashValue hash_combine(HashValue h1, HashValue h2);
void     hash_to_hex(HashValue hv, char *buf, size_t buf_size);

/* L4: Content-addressed cache operations */
void hc_init(HashCache *hc, const char *cache_root);
int  hc_lookup(const HashCache *hc, HashValue hv);
bool hc_insert(HashCache *hc, const char *file_path, HashValue hv);
bool hc_contains(const HashCache *hc, HashValue hv);
void hc_invalidate(HashCache *hc, HashValue hv);
void hc_print(const HashCache *hc);

/* L5: Merkle tree construction and verification
 *
 * A Merkle tree enables efficient integrity verification of
 * large datasets. Each leaf stores the hash of a data block;
 * internal nodes store hashes of their children.
 *
 * Verification complexity: O(log n) for single-element proofs. */
void merkle_init(MerkleTree *mt);
int  merkle_add_leaf(MerkleTree *mt, const char *label, HashValue hv);
HashValue merkle_compute_root(MerkleTree *mt);
bool merkle_verify(const MerkleTree *mt);
void merkle_print(const MerkleTree *mt, int node_idx, int depth);

/* L8: Rolling hash (Rabin-Karp fingerprint) for incremental hashing.
 * Reference: Rabin, M.O. "Fingerprinting by Random Polynomials" (1981) */
typedef struct {
    HashValue hash;
    uint64_t  window_size;
    uint64_t  pos;
    uint8_t   window[256];
} RollingHash;

void   rh_init(RollingHash *rh);
void   rh_update(RollingHash *rh, uint8_t byte);
HashValue rh_digest(const RollingHash *rh);

/* L8: Recursive directory hashing - computes hash of entire directory trees.
 * Used by Bazel, Buck, and similar systems for build artifact fingerprinting. */
int hash_dir_recursive(const char *dir_path, ContentHash *hashes, int max_hashes);

#endif
