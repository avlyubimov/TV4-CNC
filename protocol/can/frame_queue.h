#ifndef TV4_FRAME_QUEUE_H
#define TV4_FRAME_QUEUE_H
#include "update_fragments.h"

/* Local software custody ledger, NOT CAN hardware/authentication/remote ACK.
 * Zero-init once, exclusive owner, no copy/move/reinit/ISR/threads/callbacks/
 * I/O/heap. Valid disjoint spans/objects; public fields read-only to caller.
 * Tickets/snapshots must not outlive their stable-address queue owner.
 * Driver receives an OWNED frame copy and must retain/correlate its exact ticket
 * until a truthful local terminal completion. No DMA pointer into this ledger.
 * generation is explicit external local lifetime, not a serialized boot epoch.
 * Poll without traffic; uint32 monotonic ms, every observation gap <2^31. */
#define TV4_FRAME_QUEUE_MAX 4u
/* MAX is a software ceiling, not a peripheral FIFO depth or MCU RAM approval. */
typedef struct {uint32_t residency_ms;uint8_t capacity;} tv4_frame_queue_limits;
struct tv4_frame_queue;
typedef struct {
    uint64_t generation;
    uint32_t serial,route;
    const struct tv4_frame_queue *owner; /* local correlation only; never serialize */
} tv4_frame_ticket;
typedef struct {
    tv4_frame_ticket ticket;
    uint32_t route;
    tv4_fragment_frame frame;
} tv4_frame_delivery;
typedef enum {
    TV4_FRAME_SENT_LOCAL=1,TV4_FRAME_FAILED_LOCAL,TV4_FRAME_CANCELLED_LOCAL,
    TV4_FRAME_OWNERSHIP_UNCERTAIN
} tv4_frame_driver_result;
typedef enum {
    TV4_QUEUE_NO_ERROR,TV4_QUEUE_TIMEOUT,TV4_QUEUE_DRIVER,TV4_QUEUE_CANCELLED,
    TV4_QUEUE_UNCERTAIN,TV4_QUEUE_CONTEXT,TV4_QUEUE_CLOCK,TV4_QUEUE_EXHAUSTED
} tv4_frame_queue_error;
typedef enum {
    TV4_QUEUE_OK,TV4_QUEUE_ADMITTED,TV4_QUEUE_FRAME,TV4_QUEUE_LOCAL_COMPLETE_UNVERIFIED,
    TV4_QUEUE_RECLAIMED_FAILED,TV4_QUEUE_WAIT,TV4_QUEUE_BUSY,TV4_QUEUE_STALE,
    TV4_QUEUE_ARGUMENT,TV4_QUEUE_STATE,TV4_QUEUE_HELD,TV4_QUEUE_ERROR
} tv4_frame_queue_result;
typedef struct {
    tv4_fragment_frame frame;
    uint32_t serial,admitted;
} tv4_frame_queue_entry;
typedef struct tv4_frame_queue {
    tv4_frame_queue_entry entries[TV4_FRAME_QUEUE_MAX];
    tv4_frame_queue_limits limits;
    uint64_t generation;
    uint32_t route,observed,last_issued,flight;
    uint8_t head,count;
    tv4_frame_queue_error error;
    bool configured,stopped;
} tv4_frame_queue;

bool tv4_frame_queue_init(tv4_frame_queue *q,tv4_frame_queue_limits limits,
    uint32_t route,uint64_t generation,uint32_t last_issued,uint32_t now);
/* Fragment frame length5..8 only; no codec/CRC/physical validation. Failed
 * admission zeroes ticket, never consumes serial or extends entry residency. */
tv4_frame_queue_result tv4_frame_queue_admit(tv4_frame_queue *q,
    const tv4_fragment_frame *frame,tv4_frame_ticket *ticket,uint64_t generation,uint32_t now);
/* At most one outstanding claim. Non-FRAME output is entirely zero. Claim is
 * NOT completion, does not reset residency and cannot be forgotten by cancel. */
tv4_frame_queue_result tv4_frame_queue_claim(tv4_frame_queue *q,
    tv4_frame_delivery *out,uint64_t generation,uint32_t now);
/* Exact original queue address/route/generation/serial ticket only. Terminal late return can reclaim held ownership
 * even after context/clock failure; result remains RECLAIMED_FAILED, not success.
 * UNCERTAIN retains claim. now is current owner time when processing the
 * event, NOT a stale timestamp carried by the driver completion snapshot.
 * No generic flush/success bool or force-unlock API. */
tv4_frame_queue_result tv4_frame_queue_complete(tv4_frame_queue *q,tv4_frame_ticket ticket,
    tv4_frame_driver_result result,uint32_t now);
tv4_frame_queue_result tv4_frame_queue_poll(tv4_frame_queue *q,uint64_t generation,uint32_t now);
/* Stops admissions/claims; wipes unclaimed entries only, preserves actual claim.
 * Caller must arrange a future real driver return externally; this API does no I/O. */
tv4_frame_queue_result tv4_frame_queue_cancel(tv4_frame_queue *q,uint64_t generation,uint32_t now);
/* Stopped + no pending/claimed ownership only. Retains serial/time/context;
 * CLOCK/CONTEXT/EXHAUSTED sticky. Never revives an expired upper protocol. */
tv4_frame_queue_result tv4_frame_queue_clear(tv4_frame_queue *q,uint64_t generation,uint32_t now);
/* Local ledger empty after correlated reports, not remote/physical drain proof. */
tv4_frame_queue_result tv4_frame_queue_quiescent(tv4_frame_queue *q,uint64_t generation,uint32_t now);
#endif
