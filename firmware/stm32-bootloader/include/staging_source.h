#ifndef TV4_BOOT_STAGING_SOURCE_H
#define TV4_BOOT_STAGING_SOURCE_H
#include "update_handoff.h"

typedef enum {
    TV4_STAGE_EMPTY, TV4_STAGE_RECEIVING, TV4_STAGE_SEALED,
    TV4_STAGE_LEASED, TV4_STAGE_INVALID
} tv4_boot_stage_state;
typedef struct {
    uint8_t *buffer;
    size_t capacity;
    uint32_t boot;
    uint64_t object;
} tv4_boot_stage_config;
/* Zero-init once, one owner, no copy/move/reinit/ISR/threads. Caller must keep
 * buffer exclusively owned and alive, disjoint from RX/core/workspace/outputs.
 * No direct writes/DMA aliases while staged/leased. This API enforces ownership
 * only through its own operations; it is not memory protection or persistence.
 * Boot/object are explicit local identities, not authenticated/default providers.
 * Counters/high-water survive discard, not a real reboot/object destruction.
 * Public storage is for static allocation/inspection, not live mutation. */
typedef struct {
    tv4_boot_stage_state state;
    bool configured;
    uint8_t *buffer;
    size_t capacity;
    uint32_t boot, last_transfer, written, next_sequence, rx_started_ms, rx_progress_ms;
    uint64_t object, generation, token_counter, active_token, released_token;
    tv4_update_descriptor descriptor;
    tv4_update_limits limits;
    tv4_update_rx *rx;
} tv4_boot_stage;

/* Init/begin invalid arguments leave objects untouched. No allocation/clear of
 * the supplied region at init. Fresh receiving RX only; transfer high-water and
 * generation advance on accepted begin, including later failed transfers. */
bool tv4_boot_stage_init(tv4_boot_stage *stage, const tv4_boot_stage_config *config);
bool tv4_boot_stage_begin(tv4_boot_stage *stage, tv4_update_rx *rx);
/* Worker only. Bounded existing pending block -> RAM copy -> consume/ACK.
 * WAIT means no pending block; transport faults invalidate the partial source.
 * Bad arguments/aliases do not change protected objects; deadlines unchanged.
 * Final successful consume seals bytes, detaches RX; still UNVERIFIED. */
tv4_update_result tv4_boot_stage_consume(tv4_boot_stage *stage, tv4_update_ack *ack, uint32_t now_ms);
/* Worker only, O(written bytes) wipe. Denied while leased; no transport abort or
 * implicit RX reset. Preserve configuration/monotonic counters/high-water. */
bool tv4_boot_stage_discard(tv4_boot_stage *stage);
/* O(1) acquire/inspect/release; read <=256 bytes. Failed/aliased calls leave
 * outputs untouched. Exact source binding is opaque until independent crypto.
 * Repeat release succeeds only for the last released token while still SEALED;
 * stale token can never unlock a newer lease. No force release or install. */
bool tv4_boot_stage_acquire(tv4_boot_stage *stage, const tv4_update_descriptor *identity, tv4_boot_source_view *out);
bool tv4_boot_stage_inspect(const tv4_boot_stage *stage, uint64_t token, tv4_boot_source_view *out);
bool tv4_boot_stage_read(const tv4_boot_stage *stage, uint64_t token, uint32_t offset,
    uint8_t *bytes, size_t capacity, size_t *count, bool *eof);
bool tv4_boot_stage_release(tv4_boot_stage *stage, uint64_t token);
#endif
