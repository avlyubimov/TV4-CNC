#ifndef TV4_UPDATE_NEGOTIATION_H
#define TV4_UPDATE_NEGOTIATION_H
#include "update_session.h"

/* Logical draft, NOT a CAN frame/codec or production protocol version.
 * One owner, valid disjoint C objects, no ISR/threads/callbacks/heap/I/O.
 * All objects zero-initialized before their first init; never copy a live pool
 * or coordinator. Boot epochs must be nonzero/distinct and globally fresh;
 * obtaining/persisting them is EXTERNAL, not implemented by these counters.
 * No authentication, signature verification, safe-state/install/ARM permission. */
typedef enum {
    TV4_NEG_NO_MESSAGE, TV4_NEG_REQUEST, TV4_NEG_OFFER, TV4_NEG_ACCEPT, TV4_NEG_CONFIRM
} tv4_neg_kind;
typedef enum { TV4_NEG_REQUESTER, TV4_NEG_RESPONDER } tv4_neg_role;
typedef enum {
    TV4_NEG_IDLE, TV4_NEG_WAIT_OFFER, TV4_NEG_WAIT_ACCEPT, TV4_NEG_WAIT_CONFIRM,
    TV4_NEG_READY, TV4_NEG_TAKEN, TV4_NEG_CLOSED, TV4_NEG_ABORTED,
    TV4_NEG_EXPIRED, TV4_NEG_FAILED
} tv4_neg_state;
typedef enum {
    TV4_NEG_ERROR_NONE, TV4_NEG_ERROR_CLOCK, TV4_NEG_ERROR_CONTEXT,
    TV4_NEG_ERROR_OWNER, TV4_NEG_ERROR_TIMEOUT, TV4_NEG_ERROR_SENDS,
    TV4_NEG_ERROR_EXHAUSTED, TV4_NEG_ERROR_CONFLICT
} tv4_neg_error;
typedef enum {
    TV4_NEG_OK, TV4_NEG_SENT, TV4_NEG_WAIT, TV4_NEG_DUPLICATE,
    TV4_NEG_REJECT_ARGUMENT, TV4_NEG_REJECT_STATE, TV4_NEG_REJECT_SESSION,
    TV4_NEG_REJECT_CONTENT, TV4_NEG_BUSY, TV4_NEG_ERROR
} tv4_neg_result;
typedef struct {
    uint32_t max_image_bytes,retry_ms,total_ms;
    uint16_t block_ceiling;
    uint8_t send_limit; /* initial transmission + repeats, per phase */
} tv4_neg_limits;
typedef struct {
    uint32_t image_bytes;
    uint8_t image_sha256[32],metadata_sha256[32]; /* opaque caller binding */
} tv4_neg_image;
typedef struct {
    tv4_neg_kind kind;
    tv4_update_id request_id;
    tv4_update_descriptor descriptor; /* REQUEST has id={0,0}; OFFER assigns it */
    uint32_t responder_boot;
    uint8_t metadata_sha256[32],compatibility_tag[32];
    uint16_t block_bytes;
} tv4_neg_message;
typedef struct {
    uint32_t local_boot,peer_boot,last_issued,last_peer_request;
    const void *owner;
    bool configured;
} tv4_neg_pool;
typedef struct {
    tv4_update_descriptor descriptor;
    uint8_t metadata_sha256[32];
    uint16_t block_bytes;
} tv4_negotiated;
typedef struct {
    tv4_neg_pool *pool;
    tv4_neg_limits limits;
    uint64_t policy_generation;
    uint32_t local_boot,peer_boot,observed_ms,started_ms,sent_ms;
    uint8_t compatibility_tag[32],sends;
    tv4_neg_message request,offer;
    tv4_neg_kind pending;
    tv4_neg_role role;
    tv4_neg_state state;
    tv4_neg_error error;
    bool configured;
} tv4_negotiation;

/* No defaults: 0<retry<total<=INT32_MAX, block1..256, size/send_limit>0.
 * last_issued is explicit trusted local state for a NEW boot pair; peer high
 * water starts0. Re-init fails untouched; clear/close never reset counters.
 * A fixed boot pair has one active coordinator per pool. Rebinding/reboot/entropy
 * and persistent replay resistance need a future reviewed identity provider. */
bool tv4_neg_pool_init(tv4_neg_pool *pool,uint32_t local_boot,uint32_t peer_boot,uint32_t last_issued);
bool tv4_neg_init(tv4_negotiation *s,tv4_neg_pool *pool,tv4_neg_role role,
    tv4_neg_limits limits,const uint8_t compatibility_tag[32],uint64_t policy_generation,uint32_t now_ms);
tv4_neg_result tv4_neg_begin(tv4_negotiation *s,const tv4_neg_image *image,uint64_t generation,uint32_t now_ms);
tv4_neg_result tv4_neg_receive(tv4_negotiation *s,const tv4_neg_message *message,uint64_t generation,uint32_t now_ms);
tv4_neg_result tv4_neg_next(tv4_negotiation *s,tv4_neg_message *out,uint64_t generation,uint32_t now_ms);
tv4_neg_result tv4_neg_poll(tv4_negotiation *s,uint64_t generation,uint32_t now_ms);
/* Consume READY exactly once within original total deadline, before DATA begin.
 * Valid output is only transport setup. Existing DATA APIs use responder_boot
 * as their selected update epoch on BOTH sides (their legacy local_boot arg).
 * No DATA init/begin, Flash or driver action occurs in this module. */
tv4_neg_result tv4_neg_take(tv4_negotiation *s,tv4_negotiated *out,uint64_t generation,uint32_t now_ms);
/* Close only after take, abort explicitly otherwise. Release only this pool
 * owner, retain replay counters. Clear terminal state for a NEW request; live
 * coordinators cannot be reinitialized/cleared to resume/reuse old messages.
 * Poll regularly. uint32 clock intervals must remain<2^31; no duplicate extends
 * total deadline. Already-started DATA has its own unchanged lifetime/gates. */
tv4_neg_result tv4_neg_close(tv4_negotiation *s,uint64_t generation,uint32_t now_ms);
tv4_neg_result tv4_neg_abort(tv4_negotiation *s,uint64_t generation,uint32_t now_ms);
bool tv4_neg_clear(tv4_negotiation *s);
#endif
