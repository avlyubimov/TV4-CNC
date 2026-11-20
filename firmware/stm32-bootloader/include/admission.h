#ifndef TV4_BOOT_ADMISSION_H
#define TV4_BOOT_ADMISSION_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>

#define TV4_BOOT_METADATA_MAX 1024u
#define TV4_BOOT_SIGNATURE_MAX 72u
#define TV4_BOOT_KEY_BYTES 91u
#define TV4_BOOT_ID_BYTES 32u
#define TV4_BOOT_KEYS_MAX 4u
#define TV4_BOOT_CHUNK_BYTES 256u
#define TV4_BOOT_ARENA_BYTES 16384u
#define TV4_BOOT_DRAFT_SCHEMA "tv4-motion-admission-draft-1"

/* Offline foundation, not an installer/board image. One owner; no ISR/threads.
 * Callbacks must be bounded, synchronous, and conform to the read capacity.
 * Caller owns valid disjoint policy/request/input/output/workspace storage. */
typedef struct {
    char id[TV4_BOOT_ID_BYTES];
    uint8_t spki[TV4_BOOT_KEY_BYTES]; /* local canonical DER P-256 SPKI only */
} tv4_boot_key;
typedef struct {
    char hardware_id[TV4_BOOT_ID_BYTES];
    char hardware_revision[TV4_BOOT_ID_BYTES];
    char memory_profile[TV4_BOOT_ID_BYTES];
    uint32_t installed_image_counter, minimum_image_counter;
    uint32_t bootloader_counter, protocol, max_image_bytes, timeout_ms;
    const tv4_boot_key *keys;
    size_t key_count;
} tv4_boot_policy;
typedef struct {
    void *context;
    uint32_t (*now_ms)(void *context);
    bool (*cancelled)(void *context);
    /* offset is a stream position, NOT an MCU address. Exact EOF is required.
     * false, zero without EOF, early EOF or count>capacity always rejects. */
    bool (*read)(void *context, uint32_t offset, uint8_t *bytes,
                 size_t capacity, size_t *count, bool *eof);
    const uint8_t *metadata, *signature, *key_id;
    size_t metadata_bytes, signature_bytes, key_id_bytes;
    /* Tests may reduce arena capacity to inject OOM; no heap fallback. */
    size_t arena_bytes;
} tv4_boot_request;
typedef struct {
    alignas(max_align_t) uint8_t arena[TV4_BOOT_ARENA_BYTES];
    uint8_t metadata[TV4_BOOT_METADATA_MAX];
    uint8_t chunk[TV4_BOOT_CHUNK_BYTES];
} tv4_boot_workspace;
typedef struct {
    uint32_t image_bytes, image_counter;
    uint8_t image_sha256[32], metadata_sha256[32];
    char key_id[TV4_BOOT_ID_BYTES];
} tv4_boot_candidate;
typedef enum {
    TV4_BOOT_VERIFIED_CANDIDATE, TV4_BOOT_ARGUMENT, TV4_BOOT_POLICY,
    TV4_BOOT_BUSY, TV4_BOOT_KEY, TV4_BOOT_SIGNATURE, TV4_BOOT_METADATA,
    TV4_BOOT_COMPATIBILITY, TV4_BOOT_ROLLBACK, TV4_BOOT_RESOURCE,
    TV4_BOOT_SOURCE, TV4_BOOT_LENGTH, TV4_BOOT_HASH,
    TV4_BOOT_CANCELLED, TV4_BOOT_TIMEOUT, TV4_BOOT_CLOCK, TV4_BOOT_CRYPTO
} tv4_boot_result;

/* There is no default policy/trust store, no persistent receipt or permission
 * to write Flash. Counters/profiles are explicitly local, reviewed inputs.
 * Unknown/zero context rejects. now_ms monotonic modulo 2^32, interval <2^31.
 * Input bytes are snapshotted before callbacks. Workspace is wiped on exit
 * after acquisition; failures clear output except invalid aliases/BUSY, which
 * leave all caller storage untouched. BUSY never touches active workspace.
 * Output remains only a candidate; storage mutation/reset requires revalidation. */
tv4_boot_result tv4_boot_admit(const tv4_boot_policy *policy,
    const tv4_boot_request *request, tv4_boot_workspace *workspace,
    tv4_boot_candidate *candidate);
#endif
