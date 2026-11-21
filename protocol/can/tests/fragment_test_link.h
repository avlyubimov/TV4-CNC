#ifndef TV4_FRAGMENT_TEST_LINK_H
#define TV4_FRAGMENT_TEST_LINK_H
#include "update_fragments.h"
#include "update_metadata_setup.h"
#include "update_channel.h"
#include "update_egress.h"
#include "update_driver_events.h"
#include "completion_mailbox.h"
#include "driver_dispatch.h"
#include "frame_ingress.h"
#include "ingress_owner.h"

/* HOST TEST FIXTURE ONLY. Not a driver, CAN schedule, identity provider or
 * production owner. One exclusive caller clock shared with all upper layers;
 * step advances simulated milliseconds, never sleeps. No implicit dispatch.
 * All timings are explicit synthetic inputs, not hardware/default budgets. */
/* Host-only statistics mask: TU1..8, MS kinds mapped to slots9..12.
 * No new wire tokens/IDs or CAN routing policy. */
#define TV4_TEST_LINK_MS_DROP_MASK UINT16_C(0x1e00)
typedef struct {
    tv4_fragment_config fragments;
    uint32_t delivery_ms, held_delivery_ms;
    unsigned hold_every, duplicate_every;
    uint16_t drop_kinds, drop_index;
    bool drop_every_message, recover_fragment_timeouts, raw_setup, multiplexed, egress, driver_events, completion_mailbox, driver_dispatch, frame_ingress, ingress_owner;
    tv4_frame_queue_limits custody;
    uint32_t event_service_ms;
    uint8_t event_capacity;
    unsigned ingress_capacity;
    uint32_t ingress_residency_ms,ingress_service_ms;
} tv4_test_link_config;
typedef struct {
    tv4_fragment_tx tx;
    tv4_fragment_rx rx;
    tv4_channel_tx channel_tx;
    tv4_channel_rx channel_rx;
    tv4_update_egress egress;
    tv4_driver_events events;
    tv4_completion_owner completion;
    tv4_driver_dispatch dispatch;
    tv4_frame_ingress ingress;
    tv4_ingress_owner owner;
    tv4_frame_ticket ticket;
    tv4_fragment_frame queued;
    uint32_t due,event_due,ingress_due;
    uint16_t index;
    unsigned kind;
    bool occupied, repeated, drop_message, drop_current;
} tv4_test_link_direction;
typedef struct {
    tv4_test_link_direction direction[2];
    tv4_test_link_config config;
    uint32_t *clock;
    uint64_t generation[2];
    unsigned frames, delivered_frames, duplicate_frames, dropped_frames;
    unsigned queue_busy, repeated_peeks, order_rejects, stale_rejects, ready_busy;
    unsigned timeouts, sticky_failures, takes, attempts[13], deliveries[13];
    unsigned driver_admissions,driver_claims,driver_returns,driver_held_ticks,driver_max_pending;
    unsigned event_posts,event_services,event_stales,event_held_ticks,event_max_pending;
    unsigned dispatch_publications,dispatch_rejected_callbacks;
    unsigned ingress_posts,ingress_checks;
    unsigned owner_pauses,owner_advances,owner_recovers,owner_retirements;
    uint16_t dropped_kinds;
    bool configured;
} tv4_test_link;
void tv4_test_link_init(tv4_test_link *link, uint32_t *clock,
    const tv4_test_link_config *config);
bool tv4_test_link_idle(const tv4_test_link *link);
void tv4_test_link_advance(tv4_test_link *link,tv4_channel_phase phase,tv4_update_id id);
bool tv4_test_link_can_send(const tv4_test_link *link, unsigned direction);
tv4_fragment_result tv4_test_link_send(tv4_test_link *link, unsigned direction,
    const tv4_wire_message *message);
tv4_fragment_result tv4_test_link_send_setup(tv4_test_link *link,unsigned direction,
    const tv4_metadata_setup_message *message);
tv4_fragment_result tv4_test_link_take_setup(tv4_test_link *link,unsigned direction,
    tv4_metadata_setup_message *out);
void tv4_test_link_step(tv4_test_link *link);
void tv4_test_link_poll(tv4_test_link *link);
tv4_fragment_result tv4_test_link_take(tv4_test_link *link, unsigned direction,
    tv4_wire_message *out);
#endif
