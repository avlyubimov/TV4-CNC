#ifndef TV4_UPDATE_CHANNEL_H
#define TV4_UPDATE_CHANNEL_H
#include "update_fragments.h"
#include "update_metadata_setup.h"

/* Opt-in local traffic filter + owned queue, NOT a driver/production CAN ABI,
 * authentication, boot epoch provider or upper protocol/install authority.
 * Zero-init once, one owner, valid exclusive/disjoint objects/spans. No copy,
 * move/reinit/threads/ISR/callbacks/I/O/heap. Public storage read-only to caller.
 * TX and RX are independent local owners. Their peers must select compatible
 * policies externally. Local route/generation/phase are not serialized.
 * Poll without traffic; every uint32 observation gap <2^31 ms. */
typedef enum {
    TV4_CHANNEL_NEGOTIATION, TV4_CHANNEL_SETUP, TV4_CHANNEL_METADATA,
    TV4_CHANNEL_IMAGE, TV4_CHANNEL_OUTCOME
} tv4_channel_phase;
typedef enum {TV4_CHANNEL_NO_MESSAGE,TV4_CHANNEL_WIRE,TV4_CHANNEL_MS} tv4_channel_kind;
typedef struct {
    tv4_channel_kind kind;
    union {tv4_wire_message wire;tv4_metadata_setup_message setup;} body;
} tv4_channel_message;
typedef struct {
    tv4_channel_phase phase;
    tv4_update_id metadata,image;
} tv4_channel_policy;
typedef enum {
    TV4_CHANNEL_OK,TV4_CHANNEL_TAKEN_UNVERIFIED,TV4_CHANNEL_ARGUMENT,
    TV4_CHANNEL_STATE,TV4_CHANNEL_IDENTITY,TV4_CHANNEL_CONTENT,
    TV4_CHANNEL_BUSY,TV4_CHANNEL_ERROR
} tv4_channel_result;
typedef enum {
    TV4_CHANNEL_NO_ERROR,TV4_CHANNEL_FRAGMENT,TV4_CHANNEL_CODEC,
    TV4_CHANNEL_QUEUE_PROGRESS,TV4_CHANNEL_QUEUE_TOTAL
} tv4_channel_error;
typedef struct {
    tv4_fragment_tx fragments;
    tv4_channel_policy policy;
    tv4_fragment_frame outbox;
    uint8_t scratch[TV4_WIRE_MESSAGE_MAX];
    uint32_t started,progress;
    tv4_channel_error error;
    bool active,queued;
} tv4_channel_tx;
typedef struct {
    tv4_fragment_rx fragments;
    tv4_channel_policy policy;
    uint8_t scratch[TV4_WIRE_MESSAGE_MAX];
    tv4_channel_error error;
} tv4_channel_rx;

bool tv4_channel_tx_init(tv4_channel_tx *tx,const tv4_fragment_config *config,
    uint32_t route,uint64_t generation,uint16_t last_issued,uint32_t now);
bool tv4_channel_rx_init(tv4_channel_rx *rx,const tv4_fragment_config *config,
    uint32_t route,uint64_t generation,uint16_t last_seen,uint32_t now);
tv4_channel_result tv4_channel_tx_begin(tv4_channel_tx *tx,
    const tv4_channel_message *message,uint64_t generation,uint32_t now);
/* Lazy one-frame owned outbox. Exact commit only when the next LOCAL queue
 * accepts that frame. This is not remote delivery/ACK/flush proof. Stalling
 * even the final queued frame remains bounded by original TX message windows. */
tv4_fragment_result tv4_channel_tx_peek(tv4_channel_tx *tx,
    tv4_fragment_frame *out,uint64_t generation,uint32_t now);
tv4_fragment_result tv4_channel_tx_commit(tv4_channel_tx *tx,
    const tv4_fragment_frame *frame,uint64_t generation,uint32_t now);
tv4_fragment_result tv4_channel_rx_receive(tv4_channel_rx *rx,uint32_t route,
    const tv4_fragment_frame *frame,uint64_t generation,uint32_t now);
/* Exact TU/MS decoder chosen only by prefix; no fallback on codec failure.
 * Output zero on every non-take result. Wrong phase/ID consumes that bounded
 * message without dispatch. Caller MUST apply original upper session/CRC/
 * crypto/live lease/model/deadline gates. Opaque tags/hashes remain unverified. */
tv4_channel_result tv4_channel_rx_take(tv4_channel_rx *rx,
    tv4_channel_message *out,uint64_t generation,uint32_t now);
tv4_fragment_result tv4_channel_tx_poll(tv4_channel_tx *tx,uint64_t generation,uint32_t now);
tv4_fragment_result tv4_channel_rx_poll(tv4_channel_rx *rx,uint64_t generation,uint32_t now);
/* Local queue/reassembly must be idle. Strict sequential phases; OUTCOME may
 * explicitly recycle to NEGOTIATION, retaining clocks/fragment counters.
 * id nonzero only for METADATA/IMAGE; metadata=image+1/same boot is checked.
 * This traffic policy does NOT prove upper negotiation/transfer/verify complete
 * or that external driver queues drained. Higher owner must establish those. */
tv4_channel_result tv4_channel_tx_advance(tv4_channel_tx *tx,tv4_channel_phase phase,
    tv4_update_id id,uint64_t generation,uint32_t now);
tv4_channel_result tv4_channel_rx_advance(tv4_channel_rx *rx,tv4_channel_phase phase,
    tv4_update_id id,uint64_t generation,uint32_t now);
tv4_fragment_result tv4_channel_tx_cancel(tv4_channel_tx *tx,uint64_t generation,uint32_t now);
tv4_fragment_result tv4_channel_rx_cancel(tv4_channel_rx *rx,uint64_t generation,uint32_t now);
/* Recoverable failure/cancel only; retains phase/IDs/counters/observed time.
 * Lower CLOCK/CONTEXT/EXHAUSTED remain sticky. Never clear inside fragments. */
tv4_fragment_result tv4_channel_tx_clear(tv4_channel_tx *tx,uint64_t generation,uint32_t now);
tv4_fragment_result tv4_channel_rx_clear(tv4_channel_rx *rx,uint64_t generation,uint32_t now);
#endif
