#ifndef TV4_BOOT_RELEASE_HANDOFF_H
#define TV4_BOOT_RELEASE_HANDOFF_H
#include "release_admission.h"
#include "update_handoff.h"

typedef struct {
    tv4_boot_release_policy policy;
    tv4_boot_handoff_ops ops;
    const uint8_t *metadata, *signature, *key_id;
    size_t metadata_bytes, signature_bytes, key_id_bytes, arena_bytes;
    uint32_t lifetime_ms;
    uint8_t release_attempt_limit;
} tv4_boot_release_handoff_config;
typedef struct {
    tv4_boot_handoff_receipt source;
    tv4_boot_release_candidate release;
} tv4_boot_release_handoff_receipt;

/* Zero-init, one owner, no copy/move/threads/ISR. Adapter AND borrowed handoff,
 * RX and observer context must remain disjoint/alive until explicit clear;
 * QUARANTINED retains ownership, never force-clears. Core uses the exact same
 * lease/guards/retry/deadlines as legacy, with explicit local release mode.
 * Change owner policy_generation for ANY release selection/version/key/model
 * policy change; numbers themselves do not provide locking or provenance. */
typedef struct tv4_boot_release_handoff {
    tv4_boot_handoff *handoff;
    tv4_boot_release_policy policy;
    tv4_boot_release_candidate candidate;
    bool busy;
} tv4_boot_release_handoff;

bool tv4_boot_release_handoff_begin(tv4_boot_release_handoff *adapter,
    tv4_boot_handoff *handoff, const tv4_boot_release_handoff_config *config, tv4_update_rx *rx);
bool tv4_boot_release_handoff_verify(tv4_boot_release_handoff *adapter, tv4_boot_workspace *workspace);
bool tv4_boot_release_handoff_poll(tv4_boot_release_handoff *adapter);
/* Live diagnostic receipt only: independently verified release + same source
 * identity/generation. With an attached adapter, failed peek clears disjoint
 * out; invalid arguments, BUSY and aliases leave it untouched.
 * Copies cannot be revoked; no install/COMMIT/jump/physical/ARM authority. */
bool tv4_boot_release_handoff_peek(tv4_boot_release_handoff *adapter, tv4_boot_release_handoff_receipt *out);
/* Model report covers image/source; full release fields are available by live
 * peek only. No default board map/vector contract or hardware readiness. */
tv4_boot_image_result tv4_boot_release_handoff_preflight(tv4_boot_release_handoff *adapter,
    const tv4_boot_image_profile *profile, tv4_boot_workspace *workspace,
    tv4_boot_handoff_image_report *out);
bool tv4_boot_release_handoff_close(tv4_boot_release_handoff *adapter);
void tv4_boot_release_handoff_abort(tv4_boot_release_handoff *adapter);
bool tv4_boot_release_handoff_clear(tv4_boot_release_handoff *adapter);
#endif
