#ifndef TV4_FRAME_INGRESS_H
#define TV4_FRAME_INGRESS_H
#include "update_fragments.h"
#include <stdatomic.h>

/* SPSC ONLY. Driver actor: offer/retry/lost. Protocol actor: stage/check/empty.
 * faults is safe for either. Init externally published before both start;
 * stable address, stop/join BOTH before destruction. No live copy/move/reinit/
 * reset/flush/direct field access. Valid disjoint actor-exclusive spans. No
 * target ISR/RTOS/cache/DMA/clock/routing/authentication/driver truth approval.
 * Binding never changes; producer NEVER reads mutable protocol state. */
#define TV4_FRAME_INGRESS_MAX 4u
typedef enum {
    TV4_INGRESS_ADMITTED,TV4_INGRESS_HELD,TV4_INGRESS_BUSY,TV4_INGRESS_WAIT,
    TV4_INGRESS_UNCHECKED,TV4_INGRESS_FRAME,TV4_INGRESS_STALE,
    TV4_INGRESS_ARGUMENT,TV4_INGRESS_IDENTITY,TV4_INGRESS_FAULT
} tv4_ingress_result;
typedef enum {
    TV4_INGRESS_NO_FAULT=0,TV4_INGRESS_CLOCK=1,TV4_INGRESS_EXPIRED=2,
    TV4_INGRESS_OVERRUN=4,TV4_INGRESS_EXHAUSTED=8
} tv4_ingress_fault;
typedef struct {tv4_fragment_frame frame;uint32_t captured,serial;} tv4_ingress_entry;
struct tv4_frame_ingress;
typedef struct {
    const struct tv4_frame_ingress *owner;
    uint64_t generation;
    uint32_t route,serial,captured;
} tv4_ingress_token; /* UNCHECKED local identity, contains NO frame payload */
typedef struct tv4_frame_ingress {
    tv4_ingress_entry entries[TV4_FRAME_INGRESS_MAX+1u];
    atomic_uint head,tail,held,fault;
    uint64_t generation;
    uint32_t route,residency_ms;
    unsigned slots;
    bool configured; /* immutable binding after init */
    /* Private to driver actor; NEVER read by protocol actor while live. */
    tv4_ingress_entry producer_pending;
    uint32_t issued,producer_observed;
    bool producer_has_pending;
    /* Private to protocol actor; NEVER read by driver actor while live. */
    tv4_ingress_entry consumer_staged;
    uint32_t consumed,consumer_observed;
    bool consumer_has_staged;
} tv4_frame_ingress;
bool tv4_frame_ingress_init(tv4_frame_ingress *m,unsigned capacity,uint32_t residency_ms,
    uint32_t route,uint64_t generation,uint32_t last_serial,uint32_t now);
/* ADMITTED or HELD owns a COPY: caller may wipe input immediately. Full retains
 * the first frame/time/serial privately; retry it before offering another frame.
 * BUSY does NOT copy new input. Caller must retain it AND its original captured
 * time externally, or call lost if hardware/unretainable input was lost. Never
 * silently drop/overwrite or refresh capture time on a later offer. captured is
 * the actual driver capture time; now is a fresh current clock observation. */
tv4_ingress_result tv4_frame_ingress_offer(tv4_frame_ingress *m,uint32_t route,
    uint64_t generation,const tv4_fragment_frame *frame,uint32_t captured,uint32_t now);
tv4_ingress_result tv4_frame_ingress_retry(tv4_frame_ingress *m,uint32_t now);
/* Report any actual loss/overflow; sticky even when queue is empty. */
tv4_ingress_result tv4_frame_ingress_lost(tv4_frame_ingress *m);
/* Stage copies/wipes/releases one ring slot, keeps actual frame private, and
 * exposes only a token. Read current time AFTER successful stage, then check.
 * One staged frame max. Non-UNCHECKED token output completely zero. */
tv4_ingress_result tv4_frame_ingress_stage(tv4_frame_ingress *m,tv4_ingress_token *out);
/* Fresh common uint32 monotonic ms clock, observation gaps <2^31. Exact token
 * only. FRAME returns an owned copy then retires staged entry. Other frame
 * output zero. Expiry is original capture age, never retry/stage time. Any
 * sticky fault denies exposure. Caller still polls original protocol deadlines
 * at CURRENT time; never dispatch with captured time or treat FRAME as verified.
 * No live recovery: stop/join/reconcile under a separate reviewed lifetime. */
tv4_ingress_result tv4_frame_ingress_check(tv4_frame_ingress *m,
    tv4_ingress_token token,tv4_fragment_frame *out,uint32_t now);
/* Consumer snapshot only, includes published producer-held and staged entry.
 * NOT evidence producer stopped, phase cutover, backend/remote drain or safety.
 * No actual channel binding/phase change authority supplied by this library. */
tv4_ingress_result tv4_frame_ingress_empty(const tv4_frame_ingress *m);
unsigned tv4_frame_ingress_faults(const tv4_frame_ingress *m); /* invalid object => UINT_MAX */
#endif
