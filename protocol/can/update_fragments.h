#ifndef TV4_UPDATE_FRAGMENTS_H
#define TV4_UPDATE_FRAGMENTS_H
#include "update_wire.h"

/* EXPERIMENTAL envelope v1, explicitly selected by the local owner, NOT a
 * production CAN protocol. No IDs, driver, ISR, I/O, callbacks or allocation.
 * One owner; zero-init once, never copy/move/reinit a live object. All objects
 * and input/output spans valid, exclusive, disjoint throughout each call.
 * Local route/generation are not serialized or authenticated. Reboot/replay
 * isolation needs an external reviewed identity/routing/queue-flush provider.
 * now is monotonic ms modulo 2^32, every observation gap <2^31. Poll without
 * traffic too; rejected/duplicate frames and peek never extend deadlines.
 * READY_BYTES_UNCHECKED is not codec/CRC/auth/session/install/ARM authority. */
#define TV4_FRAGMENT_VERSION 1u
#define TV4_FRAGMENT_BYTES 8u
#define TV4_FRAGMENT_HEADER 4u
#define TV4_FRAGMENT_CHUNK 4u
#define TV4_FRAGMENT_START 0x8000u
typedef struct { uint8_t bytes[TV4_FRAGMENT_BYTES]; uint8_t length; } tv4_fragment_frame;
typedef struct {
    uint32_t progress_timeout_ms, total_timeout_ms;
    uint16_t max_frames;
    uint8_t version;
} tv4_fragment_config;
typedef enum {
    TV4_FRAG_IDLE, TV4_FRAG_SENDING, TV4_FRAG_RECEIVING,
    TV4_FRAG_READY_BYTES_UNCHECKED, TV4_FRAG_FAILED, TV4_FRAG_CANCELLED
} tv4_fragment_state;
typedef enum {
    TV4_FRAG_NO_ERROR, TV4_FRAG_CONTEXT, TV4_FRAG_CLOCK,
    TV4_FRAG_PROGRESS_TIMEOUT, TV4_FRAG_TOTAL_TIMEOUT, TV4_FRAG_EXHAUSTED,
    TV4_FRAG_FRAME_BUDGET, TV4_FRAG_CONFLICT, TV4_FRAG_CODEC
} tv4_fragment_error;
typedef enum {
    TV4_FRAG_OK, TV4_FRAG_FRAME, TV4_FRAG_PENDING, TV4_FRAG_READY,
    TV4_FRAG_TAKEN, TV4_FRAG_COMMITTED, TV4_FRAG_FINISHED, TV4_FRAG_DUPLICATE,
    TV4_FRAG_BUSY, TV4_FRAG_STALE, TV4_FRAG_ARGUMENT, TV4_FRAG_BAD_FRAME,
    TV4_FRAG_ORDER, TV4_FRAG_ROUTE, TV4_FRAG_LIMIT, TV4_FRAG_STATE, TV4_FRAG_ERROR
} tv4_fragment_result;
/* Public storage is caller-owned, not a serialized layout. Fields are read-only
 * to the caller after init. counter is last-issued TX / high-water RX; clear,
 * cancellation, completion and failures retain it and the observed clock. */
typedef struct {
    uint8_t bytes[TV4_WIRE_MESSAGE_MAX];
    tv4_fragment_config config;
    uint64_t generation;
    uint32_t route, observed, started, progress;
    uint16_t counter, total, cursor, attempts;
    tv4_fragment_state state;
    tv4_fragment_error error;
    bool configured;
} tv4_fragment_storage;
typedef struct { tv4_fragment_storage storage; bool peeked; } tv4_fragment_tx;
typedef struct { tv4_fragment_storage storage; } tv4_fragment_rx;

tv4_fragment_result tv4_fragment_tx_init(tv4_fragment_tx *tx,
    const tv4_fragment_config *config, uint32_t route, uint64_t generation,
    uint16_t last_issued, uint32_t now);
tv4_fragment_result tv4_fragment_rx_init(tv4_fragment_rx *rx,
    const tv4_fragment_config *config, uint32_t route, uint64_t generation,
    uint16_t last_seen, uint32_t now);
tv4_fragment_result tv4_fragment_tx_begin(tv4_fragment_tx *tx,
    const tv4_wire_message *message, uint64_t generation, uint32_t now);
/* Peek can repeat without advancing. Commit ONLY after a future local queue
 * accepts exactly that frame; local commit is NOT a remote ACK/delivery proof. */
tv4_fragment_result tv4_fragment_tx_peek(tv4_fragment_tx *tx,
    tv4_fragment_frame *out, uint64_t generation, uint32_t now);
tv4_fragment_result tv4_fragment_tx_commit(tv4_fragment_tx *tx,
    const tv4_fragment_frame *submitted, uint64_t generation, uint32_t now);
tv4_fragment_result tv4_fragment_rx_receive(tv4_fragment_rx *rx, uint32_t route,
    const tv4_fragment_frame *frame, uint64_t generation, uint32_t now);
/* Single delivery; exact codec decode required, failure zeroes output.
 * Caller must still invoke the unchanged logical RX/crypto/lease/time gates. */
tv4_fragment_result tv4_fragment_rx_take(tv4_fragment_rx *rx,
    tv4_wire_message *out, uint64_t generation, uint32_t now);
tv4_fragment_result tv4_fragment_tx_poll(tv4_fragment_tx *tx, uint64_t generation, uint32_t now);
tv4_fragment_result tv4_fragment_rx_poll(tv4_fragment_rx *rx, uint64_t generation, uint32_t now);
tv4_fragment_result tv4_fragment_tx_cancel(tv4_fragment_tx *tx, uint64_t generation, uint32_t now);
tv4_fragment_result tv4_fragment_rx_cancel(tv4_fragment_rx *rx, uint64_t generation, uint32_t now);
/* Clears recoverable failures/cancellation to idle; never resets counter/time.
 * CLOCK, CONTEXT and EXHAUSTED are sticky: a genuinely new external lifetime
 * needs a new object and reviewed identity, not reinit/clear to reuse IDs. */
tv4_fragment_result tv4_fragment_tx_clear(tv4_fragment_tx *tx, uint64_t generation, uint32_t now);
tv4_fragment_result tv4_fragment_rx_clear(tv4_fragment_rx *rx, uint64_t generation, uint32_t now);
#endif
