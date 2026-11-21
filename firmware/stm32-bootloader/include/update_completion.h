#ifndef TV4_BOOT_UPDATE_COMPLETION_H
#define TV4_BOOT_UPDATE_COMPLETION_H

#include "update_handoff.h"
#include "release_handoff.h"
#include "update_outcome.h"

typedef enum {
    TV4_COMPLETION_EMPTY, TV4_COMPLETION_WORKING, TV4_COMPLETION_CLOSING,
    TV4_COMPLETION_READY, TV4_COMPLETION_QUARANTINED
} tv4_boot_completion_state;
typedef struct {
    void *context;
    uint32_t (*now_ms)(void *context);
    bool (*cancelled)(void *context);
    bool (*owner_stamp)(void *context, tv4_boot_owner_stamp *stamp);
} tv4_boot_completion_observer;
/* Zero-initialized, single owner, bounded static storage. No copy/move/ISR.
 * Borrowed handoff/RX/observer context remain valid until READY or unresolved
 * QUARANTINED recovery. Scratch is borrowed only during synchronous start.
 * Inputs/outputs/context/scratch are valid mutually disjoint owned C objects.
 * Callbacks obey the handoff contract. Never forget a held lease on error. */
typedef struct {
    tv4_boot_completion_state state;
    tv4_outcome_reason reason;
    bool busy, cancel_requested;
    tv4_boot_handoff *handoff;
    tv4_update_rx *transfer;
    tv4_boot_completion_observer observer;
    tv4_boot_owner_stamp owner;
    uint32_t started_ms, observed_ms, lifetime_ms, transfer_started_ms, transfer_progress_ms;
    tv4_update_binding expected;
    tv4_boot_image_profile profile;
    tv4_boot_handoff_image_report inspection;
    tv4_update_outcome report;
} tv4_boot_completion;

/* Accept only an EMPTY completion and BOUND handoff. Performs actual full
 * signature/SHA -> local model check -> close, never a caller-supplied success
 * boolean or old receipt. Copies expected binding/profile before callbacks.
 * Expected metadata SHA is request identity, not authentication; a positive
 * report requires equality with the independently verified raw metadata SHA.
 * Return true means the operation was accepted, NOT that checks passed.
 * READY report may be negative; CLOSING needs regular poll; QUARANTINED keeps
 * ownership. At most one backend release attempt per start/poll invocation. */
bool tv4_boot_completion_start(tv4_boot_completion *completion, tv4_boot_handoff *handoff,
    const tv4_update_binding *expected, const tv4_boot_image_profile *profile,
    tv4_boot_workspace *scratch);
/* Explicit release-draft path: uses the adapter's captured selection/policy,
 * same raw negotiated metadata binding, live guards/model/close algorithm.
 * Historical report is unchanged and carries no install/ARM authority. Adapter
 * and core remain under this one owner's control until READY/quarantine;
 * do not clear or independently operate them while completion is active. */
bool tv4_boot_completion_start_release(tv4_boot_completion *completion, tv4_boot_release_handoff *adapter,
    const tv4_update_binding *expected, const tv4_boot_image_profile *profile, tv4_boot_workspace *scratch);
bool tv4_boot_completion_poll(tv4_boot_completion *completion);
void tv4_boot_completion_abort(tv4_boot_completion *completion);
/* Pure copy of a HISTORICAL report; no lease/candidate/physical/install token.
 * READY no longer borrows context; QUARANTINED still does and cannot clear.
 * Before a future action re-negotiate identity and fully verify saved bytes.
 * Peer delivery and ACK provide no authentication/installation permission.
 * Invalid aliases/BUSY leave caller storage untouched; otherwise false clears
 * disjoint out. No callback is made by snapshot/clear. */
bool tv4_boot_completion_snapshot(const tv4_boot_completion *completion, tv4_update_outcome *out);
bool tv4_boot_completion_clear(tv4_boot_completion *completion);

#endif
