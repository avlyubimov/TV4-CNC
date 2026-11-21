#ifndef TV4_BOOT_UPDATE_HANDOFF_H
#define TV4_BOOT_UPDATE_HANDOFF_H

#include "admission.h"
#include "image_profile.h"
#include "update_session.h"

#define TV4_BOOT_RELEASE_ATTEMPTS_MAX 8u

/* Trusted LOCAL context, not fields received over CAN or physical proof.
 * Boot must equal the receiver's boot; change policy_generation whenever any
 * key/compatibility/counter policy changes. Never reuse identities/generations. */
typedef struct { uint32_t boot; uint64_t policy_generation; } tv4_boot_owner_stamp;
typedef struct {
    uint64_t token, object, generation;
    tv4_update_id id;
    uint32_t image_bytes;
} tv4_boot_source_view;
typedef struct {
    void *context;
    uint32_t (*now_ms)(void *context);
    bool (*cancelled)(void *context);
    bool (*owner_stamp)(void *context, tv4_boot_owner_stamp *stamp);
    /* Acquire failure is atomic (no lease). Success MUST supply a nonzero
     * token and an exclusive immutable read snapshot until release succeeds.
     * Inspect/read/release use that same token; release must be idempotent.
     * No storage writer, address, persistent recovery or physical I/O here. */
    bool (*acquire)(void *context, const tv4_update_descriptor *identity, tv4_boot_source_view *view);
    bool (*inspect)(void *context, uint64_t token, tv4_boot_source_view *view);
    bool (*read)(void *context, uint64_t token, uint32_t offset,
                 uint8_t *bytes, size_t capacity, size_t *count, bool *eof);
    bool (*release)(void *context, uint64_t token);
} tv4_boot_handoff_ops;
typedef struct {
    tv4_boot_policy policy;
    tv4_boot_handoff_ops ops;
    const uint8_t *metadata, *signature, *key_id;
    size_t metadata_bytes, signature_bytes, key_id_bytes, arena_bytes;
    uint32_t lifetime_ms;
    uint8_t release_attempt_limit;
} tv4_boot_handoff_config;
typedef enum {
    TV4_HANDOFF_EMPTY, TV4_HANDOFF_CLAIMING, TV4_HANDOFF_BOUND,
    TV4_HANDOFF_VERIFYING, TV4_HANDOFF_VERIFIED, TV4_HANDOFF_RELEASING,
    TV4_HANDOFF_CLOSED, TV4_HANDOFF_QUARANTINED
} tv4_boot_handoff_state;
typedef enum {
    TV4_HANDOFF_ERROR_NONE, TV4_HANDOFF_CANCELLED, TV4_HANDOFF_CLOCK,
    TV4_HANDOFF_TIMEOUT, TV4_HANDOFF_OWNER, TV4_HANDOFF_RECEIVER,
    TV4_HANDOFF_SOURCE, TV4_HANDOFF_ADMISSION, TV4_HANDOFF_BINDING,
    TV4_HANDOFF_RELEASE, TV4_HANDOFF_PREFLIGHT
} tv4_boot_handoff_error;
typedef struct {
    tv4_boot_candidate image;
    tv4_update_id id;
    uint64_t policy_generation, source_object, source_generation;
} tv4_boot_handoff_receipt;
typedef struct {
    tv4_boot_handoff_receipt receipt;
    tv4_boot_vector_inspection vectors;
} tv4_boot_handoff_image_report;

/* Zero-initialize once, then only the API may change live state. Static bounded
 * storage, one owner, no ISR/threads. Do not copy/move a live session. RX and callback context must remain valid
 * until CLOSED; context must be outside all wiped/borrowed workspaces. Inputs,
 * outputs, RX and workspace are valid, mutually disjoint owned C objects.
 * Callbacks are bounded and truthful; clock/stamp/inspect are observations,
 * never mutate source/RX/policy. Generation checks cannot implement locking.
 * Ordinary reentry returns false untouched. Reentrant abort only latches cancel.
 */
typedef struct {
    tv4_boot_handoff_state state;
    tv4_boot_handoff_error error;
    tv4_boot_result admission_result;
    tv4_boot_image_result preflight_result;
    bool busy, held, cancel_requested, admission_attempted, preflight_attempted;
    uint8_t release_attempts, release_attempt_limit;
    bool release_document; /* explicit local mode, uses former padding */
    uint32_t started_ms, observed_ms, lifetime_ms, rx_started_ms, rx_progress_ms;
    tv4_update_rx *rx;
    tv4_update_descriptor descriptor;
    tv4_update_limits rx_limits;
    tv4_boot_owner_stamp owner;
    tv4_boot_source_view source;
    tv4_boot_policy policy;
    tv4_boot_handoff_ops ops;
    size_t metadata_bytes, signature_bytes, key_id_bytes, arena_bytes;
    tv4_boot_key keys[TV4_BOOT_KEYS_MAX];
    uint8_t metadata[TV4_BOOT_METADATA_MAX], signature[TV4_BOOT_SIGNATURE_MAX], key_id[TV4_BOOT_ID_BYTES];
    tv4_boot_candidate candidate;
} tv4_boot_handoff;

/* Begin only on EMPTY and a live COMPLETE_UNVERIFIED receiver. All policy/raw
 * inputs are copied before callbacks. No default timeout/keys/context. Both RX
 * deadlines and local lifetime still apply; poll regularly even without data. */
bool tv4_boot_handoff_begin(tv4_boot_handoff *session, const tv4_boot_handoff_config *config, tv4_update_rx *rx);
/* Synchronous worker-only one-shot admission using borrowed scratch. Success
 * keeps the read lease. No callback/allocator is executed on other states. */
bool tv4_boot_handoff_verify(tv4_boot_handoff *session, tv4_boot_workspace *workspace);
bool tv4_boot_handoff_poll(tv4_boot_handoff *session);
/* Always check return. Failed peek clears a disjoint output, except BUSY/alias
 * leaves storage untouched. A copied receipt is temporary diagnostic evidence,
 * not an install/commit/jump/ARM token. Revalidate on each use; never persist it.
 * It cannot revoke copies already held by a caller. */
bool tv4_boot_handoff_peek(tv4_boot_handoff *session, tv4_boot_handoff_receipt *out);
/* Only after VERIFIED; checks the SAME held immutable source and snapshots a
 * trusted local profile whose ID matches signed memory_profile. Profile changes
 * must also change owner policy generation. Unknown profile never defaults.
 * Reuses/wipes scratch; no retained vector buffer/global/heap. A recognized
 * preflight failure revokes candidate and releases (or quarantines) the lease.
 * Success only PROFILE_MODEL_MATCH, not hardware/bootability/install permission.
 * Report is temporary like peek: revalidate live context on each use, do not
 * persist it or use it across callbacks. BUSY/aliases leave all storage intact.
 */
tv4_boot_image_result tv4_boot_handoff_preflight(tv4_boot_handoff *session,
    const tv4_boot_image_profile *profile, tv4_boot_workspace *workspace,
    tv4_boot_handoff_image_report *out);
/* Close discards candidate and tries release once. Poll/close may retry once per
 * call up to the configured limit. Failure retains token and rejects clear/begin;
 * exhaustion (or an invalid zero success token) quarantines for owner recovery.
 * No automatic force-close, backend reset or hardware action. */
bool tv4_boot_handoff_close(tv4_boot_handoff *session);
void tv4_boot_handoff_abort(tv4_boot_handoff *session);
bool tv4_boot_handoff_clear(tv4_boot_handoff *session);

#endif
