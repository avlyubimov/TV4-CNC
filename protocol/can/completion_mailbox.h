#ifndef TV4_COMPLETION_MAILBOX_H
#define TV4_COMPLETION_MAILBOX_H
#include "update_driver_events.h"
#include <stdatomic.h>

/* C11 SPSC ONLY: exactly one producer calls post; exactly one consumer calls
 * take/empty. Not MPSC, not an approved MCU ISR/RTOS/cache/DMA adapter.
 * Zero-init once, stable address/lifetime; publish completed init externally
 * before either actor starts; stop/join both before destruction. No concurrent
 * init/reinit/copy/move/flush/reset or direct field access. No heap/I/O/spin/locks.
 * Queue binding is immutable. Producer NEVER reads mutable protocol state.
 * All objects/spans valid, disjoint and exclusive to their documented actor. */
#define TV4_COMPLETION_MAILBOX_MAX 4u
typedef struct {
    tv4_driver_event entries[TV4_COMPLETION_MAILBOX_MAX+1u];
    atomic_uint head,tail;
    const tv4_frame_queue *queue;
    uint64_t generation;
    uint32_t route;
    unsigned slots;
    bool configured;
} tv4_completion_mailbox;
bool tv4_completion_mailbox_init(tv4_completion_mailbox *m,unsigned capacity,const tv4_frame_queue *queue);
/* Producer only. ADMITTED=owned event copy; BUSY requires retained external
 * report/retry. No timestamp, driver/remote completion or claim release here. */
tv4_frame_queue_result tv4_completion_mailbox_post(tv4_completion_mailbox *m,const tv4_driver_event *event);
/* Consumer only. FRAME=owned report, not ledger completion; other output zero.
 * Slots become reusable only after owned copy/wipe. No deadline observations. */
tv4_frame_queue_result tv4_completion_mailbox_take(tv4_completion_mailbox *m,tv4_driver_event *out);
/* Consumer observation only, NOT proof producer stopped or remote/hardware drain.
 * A later duplicate may still arrive; exact tickets/retained IDs remain needed. */
tv4_frame_queue_result tv4_completion_mailbox_empty(const tv4_completion_mailbox *m);

typedef struct {
    tv4_driver_events events;
    tv4_completion_mailbox mailbox;
    tv4_driver_event pending;
    bool has_pending,configured;
} tv4_completion_owner;
/* Consumer init before actors start. Inner mutable APIs prohibited; producer
 * uses ONLY owner_post, all other APIs are exclusive to the consumer. */
bool tv4_completion_owner_init(tv4_completion_owner *s,const tv4_fragment_config *fragments,
    tv4_frame_queue_limits custody,unsigned capacity,uint32_t route,uint64_t generation,
    uint16_t last_fragment,uint32_t last_ticket,uint32_t now);
tv4_frame_queue_result tv4_completion_owner_post(tv4_completion_owner *s,const tv4_driver_event *event);
tv4_frame_queue_result tv4_completion_owner_poll(tv4_completion_owner *s,uint64_t generation,uint32_t now);
tv4_frame_queue_result tv4_completion_owner_service(tv4_completion_owner *s,uint64_t generation,uint32_t now);
tv4_channel_result tv4_completion_owner_begin(tv4_completion_owner *s,const tv4_channel_message *message,uint64_t generation,uint32_t now);
tv4_frame_queue_result tv4_completion_owner_pump(tv4_completion_owner *s,uint64_t generation,uint32_t now);
tv4_frame_queue_result tv4_completion_owner_claim(tv4_completion_owner *s,tv4_frame_delivery *out,uint64_t generation,uint32_t now);
tv4_channel_result tv4_completion_owner_advance(tv4_completion_owner *s,tv4_channel_phase phase,tv4_update_id id,uint64_t generation,uint32_t now);
/* Never discard mailbox/pending/inner events/actual held claim. Terminal reports
 * can still reclaim after cancellation/sticky faults; upper state never resumes.
 * Clear retains original tickets/phase/time AND atomic ring positions. */
tv4_frame_queue_result tv4_completion_owner_cancel(tv4_completion_owner *s,uint64_t generation,uint32_t now);
tv4_frame_queue_result tv4_completion_owner_clear(tv4_completion_owner *s,uint64_t generation,uint32_t now);
#endif
