#ifndef TV4_UPDATE_OUTCOME_H
#define TV4_UPDATE_OUTCOME_H

#include "update_session.h"

/* Logical LOCAL structs, not wire/CAN frames or production protocol version.
 * Peer reports and echoed ACKs are NOT authenticated. Never install, jump,
 * ARM/START or infer physical readiness from them. All success is HISTORY:
 * the source lease has closed; a future action requires fresh full checks. */
typedef struct {
    tv4_update_descriptor image;
    uint8_t metadata_sha256[32];
} tv4_update_binding;
typedef enum {
    TV4_OUTCOME_CHECKED_CLOSED = 1, TV4_OUTCOME_REJECTED_CLOSED,
    TV4_OUTCOME_CANCELLED_CLOSED, TV4_OUTCOME_FAILED_CLOSED,
    TV4_OUTCOME_QUARANTINED
} tv4_outcome_code;
typedef enum {
    TV4_OUTCOME_REASON_NONE, TV4_OUTCOME_SIGNATURE, TV4_OUTCOME_KEY,
    TV4_OUTCOME_METADATA, TV4_OUTCOME_COMPATIBILITY, TV4_OUTCOME_ROLLBACK,
    TV4_OUTCOME_LENGTH, TV4_OUTCOME_HASH, TV4_OUTCOME_PROFILE, TV4_OUTCOME_VECTORS,
    TV4_OUTCOME_SOURCE, TV4_OUTCOME_CONTEXT, TV4_OUTCOME_CLOCK, TV4_OUTCOME_TIMEOUT,
    TV4_OUTCOME_CANCEL, TV4_OUTCOME_RELEASE, TV4_OUTCOME_BINDING, TV4_OUTCOME_INTERNAL
} tv4_outcome_reason;
typedef struct {
    tv4_update_binding binding;
    tv4_outcome_code code;
    tv4_outcome_reason reason;
} tv4_update_outcome;
typedef struct {
    uint32_t ack_timeout_ms, linger_ms, total_timeout_ms;
    uint8_t retry_limit;
} tv4_outcome_limits;
typedef enum {
    TV4_OUTCOME_IDLE, TV4_OUTCOME_TX_READY, TV4_OUTCOME_WAIT_ACK, TV4_OUTCOME_ACKED,
    TV4_OUTCOME_WAIT_REPORT, TV4_OUTCOME_LINGER, TV4_OUTCOME_FINISHED,
    TV4_OUTCOME_ABORTED, TV4_OUTCOME_FAILED
} tv4_outcome_state;
typedef enum {
    TV4_OUTCOME_ERROR_NONE, TV4_OUTCOME_ERROR_CONFIG, TV4_OUTCOME_ERROR_CLOCK,
    TV4_OUTCOME_ERROR_TIMEOUT, TV4_OUTCOME_ERROR_RETRIES,
    TV4_OUTCOME_ERROR_TRANSPORT, TV4_OUTCOME_ERROR_CONFLICT
} tv4_outcome_error;
typedef enum {
    TV4_OUTCOME_OK, TV4_OUTCOME_SENT, TV4_OUTCOME_DELIVERED, TV4_OUTCOME_ACK,
    TV4_OUTCOME_DUPLICATE, TV4_OUTCOME_WAIT, TV4_OUTCOME_DONE,
    TV4_OUTCOME_REJECT_ARGUMENT, TV4_OUTCOME_REJECT_STATE,
    TV4_OUTCOME_REJECT_BINDING, TV4_OUTCOME_REJECT_CONTENT, TV4_OUTCOME_ERROR
} tv4_outcome_result;
typedef struct {
    tv4_outcome_limits limits;
    tv4_outcome_state state;
    tv4_outcome_error error;
    uint32_t started_ms, observed_ms, last_ms;
    uint8_t retries;
    bool configured;
} tv4_outcome_context;
typedef struct {
    tv4_outcome_context context;
    tv4_update_outcome report;
    uint32_t producer_boot;
} tv4_outcome_tx;
typedef struct {
    tv4_outcome_context context;
    tv4_update_outcome report;
    tv4_update_binding expected;
    tv4_update_tx *transfer;
    uint32_t transfer_started_ms, transfer_progress_ms, transfer_sequence;
} tv4_outcome_rx;

bool tv4_update_binding_equal(const tv4_update_binding *a, const tv4_update_binding *b);
bool tv4_update_outcome_valid(const tv4_update_outcome *report);
bool tv4_update_outcome_equal(const tv4_update_outcome *a, const tv4_update_outcome *b);
/* One initialized owner, disjoint valid C objects, no ISR/threads/callbacks/heap.
 * Limits have no default: 0<ack<linger<=total<=INT32_MAX; retry nonzero.
 * Poll regularly, even without packets. Identity uniqueness after reboot is an
 * external obligation; never reinit to resume/reuse an old negotiation. */
bool tv4_outcome_tx_init(tv4_outcome_tx *tx, tv4_outcome_limits limits, uint32_t producer_boot, uint32_t now_ms);
bool tv4_outcome_rx_init(tv4_outcome_rx *rx, tv4_outcome_limits limits, uint32_t now_ms);
tv4_outcome_result tv4_outcome_tx_begin(tv4_outcome_tx *tx, const tv4_update_outcome *report, uint32_t now_ms);
/* Only after actual DATA TX completion. Borrows transfer until terminal RX
 * state; reinit/abort/change invalidates report channel. Deadline starts at
 * the last DATA ACK (not late begin). Metadata SHA comes from the local request,
 * never from the received report. No candidate/lease is held by this channel. */
tv4_outcome_result tv4_outcome_rx_begin(tv4_outcome_rx *rx, tv4_update_tx *completed_transfer,
    const uint8_t expected_metadata_sha256[32], uint32_t now_ms);
tv4_outcome_result tv4_outcome_tx_poll(tv4_outcome_tx *tx, uint32_t now_ms);
tv4_outcome_result tv4_outcome_rx_poll(tv4_outcome_rx *rx, uint32_t now_ms);
tv4_outcome_result tv4_outcome_tx_next(tv4_outcome_tx *tx, tv4_update_outcome *report, uint32_t now_ms);
/* ACK echoes every semantic field, not struct padding; accidental mismatch is
 * rejected, not authenticated. First receive: DELIVERED writes delivery+ack;
 * exact duplicates: ACK writes ack only. No repeated delivery/deadline extension.
 * A different report for the same binding causes a sticky conflict. */
tv4_outcome_result tv4_outcome_rx_receive(tv4_outcome_rx *rx, const tv4_update_outcome *report,
    tv4_update_outcome *ack, tv4_update_outcome *delivery, uint32_t now_ms);
tv4_outcome_result tv4_outcome_tx_ack(tv4_outcome_tx *tx, const tv4_update_outcome *ack, uint32_t now_ms);
tv4_outcome_result tv4_outcome_tx_abort(tv4_outcome_tx *tx, tv4_update_id id, uint32_t now_ms);
tv4_outcome_result tv4_outcome_rx_abort(tv4_outcome_rx *rx, tv4_update_id id, uint32_t now_ms);

#endif
