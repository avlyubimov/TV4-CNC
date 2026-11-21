#ifndef TV4_UPDATE_EGRESS_H
#define TV4_UPDATE_EGRESS_H
#include "frame_queue.h"
#include "update_channel.h"

/* Bound local channel TX + ACTUAL owned custody queue. No caller-provided
 * drain/success boolean. Not a driver, remote ACK/authentication/upper-protocol
 * completion/install/physical authority. One owner, zero-init once, never
 * copy/move/reinit or access inner mutable APIs. Valid disjoint exclusive
 * objects/spans, no I/O/ISR/threads/callbacks/heap. Driver copies remain owned
 * externally until exact terminal ticket return; no forced forget on timeout. */
typedef struct {
    tv4_channel_tx channel;
    tv4_frame_queue queue;
    bool configured,failed;
} tv4_update_egress;

bool tv4_update_egress_init(tv4_update_egress *s,const tv4_fragment_config *fragments,
    tv4_frame_queue_limits queue,uint32_t route,uint64_t generation,
    uint16_t last_fragment,uint32_t last_ticket,uint32_t now);
tv4_channel_result tv4_update_egress_begin(tv4_update_egress *s,
    const tv4_channel_message *message,uint64_t generation,uint32_t now);
/* Bounded one-frame pump. Channel commit only after actual queue-owned copy.
 * No real transmit; claim/complete are separate trusted driver-adapter inputs. */
tv4_frame_queue_result tv4_update_egress_pump(tv4_update_egress *s,uint64_t generation,uint32_t now);
tv4_frame_queue_result tv4_update_egress_claim(tv4_update_egress *s,
    tv4_frame_delivery *out,uint64_t generation,uint32_t now);
tv4_frame_queue_result tv4_update_egress_complete(tv4_update_egress *s,
    tv4_frame_ticket ticket,tv4_frame_driver_result result,uint32_t now);
tv4_frame_queue_result tv4_update_egress_poll(tv4_update_egress *s,uint64_t generation,uint32_t now);
/* Requires bound queue empty after correlated local completions AND actual
 * inner channel locally idle. Original upper state/crypto/lease/deadlines and
 * future real driver reset/flush/routing contract remain mandatory. */
tv4_channel_result tv4_update_egress_advance(tv4_update_egress *s,tv4_channel_phase phase,
    tv4_update_id id,uint64_t generation,uint32_t now);
tv4_frame_queue_result tv4_update_egress_cancel(tv4_update_egress *s,uint64_t generation,uint32_t now);
tv4_frame_queue_result tv4_update_egress_clear(tv4_update_egress *s,uint64_t generation,uint32_t now);
#endif
