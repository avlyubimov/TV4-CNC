#ifndef TV4_UPDATE_WIRE_H
#define TV4_UPDATE_WIRE_H
#include "update_negotiation.h"
#include "update_outcome.h"

/* EXPERIMENTAL local byte format, not a production CAN protocol/frame.
 * One owner, valid disjoint objects/spans, no threads/ISR/callback/I/O/heap.
 * No handler dispatch, CRC/authentication, session/time/crypto/physical checks.
 * Feed decoded messages to the existing negotiation/RX/outcome APIs; their
 * validation/deadlines remain mandatory. No install/commit/jump/ARM operation.
 * Header: 'T','U', draft=1, kind, body length LE16, reserved=0,0.
 * Exact complete message only; no streaming reassembly, CAN IDs or fragments. */
#define TV4_WIRE_HEADER_BYTES 8u
#define TV4_WIRE_MESSAGE_MAX (TV4_WIRE_HEADER_BYTES + 22u + TV4_UPDATE_BLOCK_MAX)
typedef enum {
    TV4_WIRE_NONE = 0,
    TV4_WIRE_REQUEST = 1, TV4_WIRE_OFFER = 2,
    TV4_WIRE_ACCEPT = 3, TV4_WIRE_CONFIRM = 4,
    TV4_WIRE_DATA = 5, TV4_WIRE_DATA_ACK = 6,
    TV4_WIRE_OUTCOME = 7, TV4_WIRE_OUTCOME_ACK = 8
} tv4_wire_kind;
typedef struct {
    tv4_wire_kind kind;
    union {
        tv4_neg_message negotiation;
        tv4_update_block data;
        tv4_update_ack ack;
        tv4_update_outcome outcome;
    } body;
} tv4_wire_message;
typedef enum {
    TV4_WIRE_OK, TV4_WIRE_ARGUMENT, TV4_WIRE_HEADER,
    TV4_WIRE_LENGTH, TV4_WIRE_CONTENT, TV4_WIRE_CAPACITY
} tv4_wire_result;

/* Encoder: failure leaves bytes untouched and *written=0 when non-NULL.
 * Decoder: failure zeroes the entire output when non-NULL. Output is valid
 * only on TV4_WIRE_OK. Encoded DATA retains the caller's CRC verbatim; RX must
 * verify it. Opaque hashes/tags and peer outcomes are NOT trusted by this API.
 * Caller retains input storage unchanged and exclusive throughout the call. */
tv4_wire_result tv4_update_wire_encode(const tv4_wire_message *message,
    uint8_t *bytes, size_t capacity, size_t *written);
tv4_wire_result tv4_update_wire_decode(const uint8_t *bytes, size_t length,
    tv4_wire_message *message);
#endif
