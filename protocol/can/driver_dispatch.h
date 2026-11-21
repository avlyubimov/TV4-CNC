#ifndef TV4_DRIVER_DISPATCH_H
#define TV4_DRIVER_DISPATCH_H
#include "completion_mailbox.h"

/* Two actors ONLY. Protocol actor calls init/begin/pump/publish/poll/service/
 * advance/cancel/clear; driver actor calls take/check/report/retry. Externally
 * publish completed init before actors start; stable lifetime, stop/join both
 * before destruction. No live copy/move/reinit/flush or inner mutable APIs.
 * Input/output spans valid, disjoint and exclusive to the calling actor.
 * No direct mutable field access while actors run.
 * No real transmit/ISR/RTOS/cache/DMA/driver truth/physical inhibit approval. */
typedef enum {TV4_DISPATCH_EMPTY,TV4_DISPATCH_READY,TV4_DISPATCH_TAKEN,TV4_DISPATCH_RETURNED} tv4_dispatch_slot_state;
typedef enum {TV4_DISPATCH_UNCHECKED,TV4_DISPATCH_SENDABLE,TV4_DISPATCH_WAIT,TV4_DISPATCH_HELD,
    TV4_DISPATCH_REVOKED,TV4_DISPATCH_EXPIRED,TV4_DISPATCH_CLOCK,
    TV4_DISPATCH_ARGUMENT,TV4_DISPATCH_STALE} tv4_dispatch_status;
typedef struct {tv4_frame_delivery delivery;uint32_t admitted,residency_ms;} tv4_dispatch_delivery;
typedef struct {
    tv4_completion_owner completion;
    tv4_dispatch_delivery published;
    atomic_uint state,revoked;
    /* Driver-private fields: protocol actor must NEVER inspect while active. */
    tv4_dispatch_delivery driver_current;
    tv4_driver_event driver_pending;
    uint32_t driver_observed;
    bool driver_has_current,driver_has_pending,driver_exposed,driver_clock_failed;
    bool configured; /* immutable after externally published init */
} tv4_driver_dispatch;

bool tv4_driver_dispatch_init(tv4_driver_dispatch *s,const tv4_fragment_config *fragments,
    tv4_frame_queue_limits custody,unsigned report_capacity,uint32_t route,uint64_t generation,
    uint16_t last_fragment,uint32_t last_ticket,uint32_t now);
tv4_channel_result tv4_driver_dispatch_begin(tv4_driver_dispatch *s,const tv4_channel_message *message,uint64_t generation,uint32_t now);
tv4_frame_queue_result tv4_driver_dispatch_pump(tv4_driver_dispatch *s,uint64_t generation,uint32_t now);
/* One actual claim -> owned slot -> release publication. BUSY never takes a
 * second claim. Published but untaken job still requires driver terminal return. */
tv4_frame_queue_result tv4_driver_dispatch_publish(tv4_driver_dispatch *s,uint64_t generation,uint32_t now);
tv4_frame_queue_result tv4_driver_dispatch_poll(tv4_driver_dispatch *s,uint64_t generation,uint32_t now);
tv4_frame_queue_result tv4_driver_dispatch_service(tv4_driver_dispatch *s,uint64_t generation,uint32_t now);
tv4_channel_result tv4_driver_dispatch_advance(tv4_driver_dispatch *s,tv4_channel_phase phase,tv4_update_id id,uint64_t generation,uint32_t now);
tv4_frame_queue_result tv4_driver_dispatch_cancel(tv4_driver_dispatch *s,uint64_t generation,uint32_t now);
tv4_frame_queue_result tv4_driver_dispatch_clear(tv4_driver_dispatch *s,uint64_t generation,uint32_t now);
/* Driver actor ONLY. UNCHECKED returns ticket/deadline with ZERO payload; actual
 * owned job is private. Read current time AFTER take, then check before using
 * frame. This avoids a clock snapshot preceding concurrent job publication.
 * No claim is forgotten on any refusal: explicitly return CANCELLED/FAILED
 * (or uncertain). Owner/driver use one monotonic ms domain, each gap <2^31. */
tv4_dispatch_status tv4_driver_dispatch_take(tv4_driver_dispatch *s,tv4_dispatch_delivery *out);
/* SENDABLE exposes an owned frame copy; every other output entirely zero.
 * Check before backend submission. This software observation is NOT an atomic
 * hardware cancel/inhibit: real backend must resolve already-submitted work and
 * return truthful ownership. A later SENT completion may legitimately be late. */
tv4_dispatch_status tv4_driver_dispatch_check(tv4_driver_dispatch *s,tv4_frame_ticket ticket,tv4_dispatch_delivery *out,uint32_t now);
/* Exact current driver ticket only; never an implicit 'current frame' callback.
 * BUSY retains the FIRST event and actual job. Call retry, do not replace it.
 * UNCERTAIN retains job even when admitted. Terminal admission publishes RETURNED;
 * consumer must also process actual correlated return before slot reuse. */
tv4_frame_queue_result tv4_driver_dispatch_report(tv4_driver_dispatch *s,const tv4_driver_event *event);
tv4_frame_queue_result tv4_driver_dispatch_retry(tv4_driver_dispatch *s);
#endif
