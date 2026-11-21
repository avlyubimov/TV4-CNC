#ifndef TV4_UPDATE_DRIVER_EVENTS_H
#define TV4_UPDATE_DRIVER_EVENTS_H
#include "update_egress.h"

/* Local serialized handoff only, NOT a real driver or concurrent/ISR-safe queue.
 * Zero-init once; one exclusive owner, valid/disjoint objects/spans, stable
 * address, no copy/move/reinit/heap/I/O/ISR/threads. Future RTOS/driver adapters
 * must supply synchronization and truthful correlated local completion reports.
 * Inner objects are inspection-only; never call their mutable APIs externally. */
#define TV4_DRIVER_EVENTS_MAX 4u
typedef struct {tv4_frame_ticket ticket;tv4_frame_driver_result result;} tv4_driver_event;
typedef struct {
    tv4_update_egress egress;
    tv4_driver_event entries[TV4_DRIVER_EVENTS_MAX];
    uint8_t capacity,head,count;
    bool configured;
} tv4_driver_events;

bool tv4_driver_events_init(tv4_driver_events *s,const tv4_fragment_config *fragments,
    tv4_frame_queue_limits custody,uint8_t event_capacity,uint32_t route,uint64_t generation,
    uint16_t last_fragment,uint32_t last_ticket,uint32_t now);
tv4_channel_result tv4_driver_events_begin(tv4_driver_events *s,
    const tv4_channel_message *message,uint64_t generation,uint32_t now);
tv4_frame_queue_result tv4_driver_events_pump(tv4_driver_events *s,uint64_t generation,uint32_t now);
tv4_frame_queue_result tv4_driver_events_claim(tv4_driver_events *s,
    tv4_frame_delivery *out,uint64_t generation,uint32_t now);
/* ADMITTED means only an owned event copy, NOT ledger completion. No timestamp
 * is accepted. BUSY leaves inbox/egress untouched: adapter must retain/retry its
 * report; never lose an actual terminal return. Posting does not poll time.
 * UNCERTAIN does not end driver frame custody. Exact owner/route/generation are
 * checked; stale serials/duplicates are consumed by service, never reused. */
tv4_frame_queue_result tv4_driver_events_post(tv4_driver_events *s,const tv4_driver_event *event);
/* Observe original deadlines without consuming events, e.g. while the worker's
 * explicit event budget is exhausted. Does not grant phase/clear authority. */
tv4_frame_queue_result tv4_driver_events_poll(tv4_driver_events *s,uint64_t generation,uint32_t now);
/* Mandatory even without traffic. Poll original egress first at CURRENT owner
 * time, then process at most one event (including cleanup after sticky faults).
 * Dequeue only the processed event; terminal late return cannot revive upper
 * update. STALE/HELD can coexist with failed egress; inspect/poll via service.
 * No caller success/drained boolean or external queue is accepted. */
tv4_frame_queue_result tv4_driver_events_service(tv4_driver_events *s,uint64_t generation,uint32_t now);
tv4_channel_result tv4_driver_events_advance(tv4_driver_events *s,tv4_channel_phase phase,
    tv4_update_id id,uint64_t generation,uint32_t now);
/* Cancel preserves the entire inbox and actual driver claim. Clear requires
 * actual inbox empty AND original egress cleanup; IDs/phase/time retained. */
tv4_frame_queue_result tv4_driver_events_cancel(tv4_driver_events *s,uint64_t generation,uint32_t now);
tv4_frame_queue_result tv4_driver_events_clear(tv4_driver_events *s,uint64_t generation,uint32_t now);
#endif
