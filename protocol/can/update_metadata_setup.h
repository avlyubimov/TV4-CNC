#ifndef TV4_UPDATE_METADATA_SETUP_H
#define TV4_UPDATE_METADATA_SETUP_H
#include "update_negotiation.h"
#include "update_metadata.h"

/* Opt-in experimental logical setup/complete-message codec, NOT a production
 * CAN ABI, authenticated peer, persistent epoch provider or install authority.
 * Zero-init once, exclusive owner, valid/disjoint objects/spans, no live copy,
 * move, reinit, threads, ISR, heap, callbacks or I/O. Fields read-only to caller.
 * Original CLOSED negotiation, pool and live empty image TX/RX remain alive and
 * unchanged until close/abort. Source container must remain immutable externally.
 * Poll without traffic; every uint32 clock observation gap must be <2^31 ms. */
#define TV4_SETUP_MESSAGE_BYTES 172u
typedef struct {
    tv4_neg_kind kind;
    tv4_update_id request_id;
    tv4_update_binding binding;
    tv4_update_descriptor metadata;
    uint8_t compatibility_tag[32];
    uint16_t block_bytes;
} tv4_metadata_setup_message;
typedef struct {
    uint32_t retry_ms,total_ms,max_bundle_bytes;
    uint8_t send_limit;
} tv4_metadata_setup_limits;
typedef enum {
    TV4_SETUP_IDLE, TV4_SETUP_WAIT_REQUEST, TV4_SETUP_WAIT_OFFER,
    TV4_SETUP_WAIT_ACCEPT, TV4_SETUP_WAIT_CONFIRM, TV4_SETUP_READY,
    TV4_SETUP_TAKEN, TV4_SETUP_CLOSED, TV4_SETUP_ABORTED,
    TV4_SETUP_EXPIRED, TV4_SETUP_FAILED
} tv4_metadata_setup_state;
typedef enum {
    TV4_SETUP_NO_ERROR, TV4_SETUP_CLOCK, TV4_SETUP_CONTEXT, TV4_SETUP_IMAGE,
    TV4_SETUP_TIMEOUT, TV4_SETUP_SENDS, TV4_SETUP_EXHAUSTED, TV4_SETUP_CONFLICT
} tv4_metadata_setup_error;
typedef enum {
    TV4_SETUP_OK, TV4_SETUP_SENT, TV4_SETUP_WAIT, TV4_SETUP_DUPLICATE,
    TV4_SETUP_ARGUMENT, TV4_SETUP_STATE, TV4_SETUP_SESSION, TV4_SETUP_CONTENT,
    TV4_SETUP_LIMIT, TV4_SETUP_ERROR
} tv4_metadata_setup_result;
typedef struct {
    tv4_update_binding binding;
    tv4_update_descriptor metadata;
    uint16_t block_bytes;
} tv4_metadata_parameters;
typedef struct {
    tv4_neg_pool *pool;
    const tv4_negotiation *image;
    tv4_update_tx *image_tx;
    tv4_update_rx *image_rx;
    tv4_metadata_setup_message message;
    tv4_metadata_setup_limits limits;
    tv4_update_limits image_limits;
    uint64_t generation;
    uint32_t local_boot,peer_boot,observed,started,sent;
    uint32_t image_started,image_progress,issued,peer_request,last_image,last_request;
    tv4_neg_role role;
    tv4_neg_kind pending;
    tv4_metadata_setup_state state;
    tv4_metadata_setup_error error;
    uint8_t sends;
    bool configured;
} tv4_metadata_setup;

/* Exact 'MS',draft1,kind1..4,body164 LE16,reserved0,0; request ID8,
 * image binding76, metadata descriptor44, opaque tag32, block LE16,reserved0,0.
 * REQUEST metadata ID zero, other kinds use same epoch / image.transfer+1.
 * No hashing/CRC/signature performed. Encode failure leaves bytes untouched,
 * written=0; decode failure zeroes output. Success is structural only. */
tv4_metadata_setup_result tv4_metadata_setup_encode(const tv4_metadata_setup_message *message,
    uint8_t *bytes,size_t capacity,size_t *written);
tv4_metadata_setup_result tv4_metadata_setup_decode(const uint8_t *bytes,size_t length,
    tv4_metadata_setup_message *out);
/* No defaults: 0<retry<total<=INT32_MAX, send_limit>0, max bundle98..1215. */
bool tv4_metadata_setup_init(tv4_metadata_setup *s,tv4_neg_pool *pool,tv4_neg_role role,
    tv4_metadata_setup_limits limits,uint64_t generation,uint32_t now);
tv4_metadata_setup_result tv4_metadata_setup_request(tv4_metadata_setup *s,
    const tv4_negotiation *closed_image,tv4_update_tx *image_tx,uint32_t bundle_bytes,
    const uint8_t bundle_sha256[32],uint64_t generation,uint32_t now);
tv4_metadata_setup_result tv4_metadata_setup_respond(tv4_metadata_setup *s,
    const tv4_negotiation *closed_image,tv4_update_rx *image_rx,uint64_t generation,uint32_t now);
tv4_metadata_setup_result tv4_metadata_setup_receive(tv4_metadata_setup *s,
    const tv4_metadata_setup_message *message,uint64_t generation,uint32_t now);
tv4_metadata_setup_result tv4_metadata_setup_next(tv4_metadata_setup *s,
    tv4_metadata_setup_message *out,uint64_t generation,uint32_t now);
tv4_metadata_setup_result tv4_metadata_setup_poll(tv4_metadata_setup *s,uint64_t generation,uint32_t now);
/* One owned transport descriptor, NOT VERIFIED/auth/install/live receipt.
 * Responder may continue bounded CONFIRM retransmission after take. */
tv4_metadata_setup_result tv4_metadata_setup_take(tv4_metadata_setup *s,
    tv4_metadata_parameters *out,uint64_t generation,uint32_t now);
/* Release shared pool only against actual completed metadata TX / TAKEN RX;
 * callers still need independent raw digest/signature/image/lease/model checks.
 * Original image deadlines are never reset/extended and image stays empty. */
tv4_metadata_setup_result tv4_metadata_setup_close_tx(tv4_metadata_setup *s,
    tv4_update_tx *metadata_tx,uint64_t generation,uint32_t now);
tv4_metadata_setup_result tv4_metadata_setup_close_rx(tv4_metadata_setup *s,
    tv4_metadata_rx *metadata_rx,uint64_t generation,uint32_t now);
tv4_metadata_setup_result tv4_metadata_setup_abort(tv4_metadata_setup *s,uint64_t generation,uint32_t now);
/* Only terminal -> IDLE; retain IDs/observed clock. CLOCK/CONTEXT are sticky.
 * A new image negotiation is required. Never clear/reinit to reuse old IDs. */
tv4_metadata_setup_result tv4_metadata_setup_clear(tv4_metadata_setup *s,uint64_t generation,uint32_t now);
#endif
