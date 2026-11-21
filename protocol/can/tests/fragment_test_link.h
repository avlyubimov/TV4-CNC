#ifndef TV4_FRAGMENT_TEST_LINK_H
#define TV4_FRAGMENT_TEST_LINK_H
#include "update_fragments.h"
#include "update_metadata_setup.h"

/* HOST TEST FIXTURE ONLY. Not a driver, CAN schedule, identity provider or
 * production owner. One exclusive caller clock shared with all upper layers;
 * step advances simulated milliseconds, never sleeps. No implicit dispatch.
 * All timings are explicit synthetic inputs, not hardware/default budgets. */
typedef struct {
    tv4_fragment_config fragments;
    uint32_t delivery_ms, held_delivery_ms;
    unsigned hold_every, duplicate_every;
    uint16_t drop_kinds, drop_index;
    bool drop_every_message, recover_fragment_timeouts, raw_setup;
} tv4_test_link_config;
typedef struct {
    tv4_fragment_tx tx;
    tv4_fragment_rx rx;
    tv4_fragment_frame queued;
    uint32_t due;
    uint16_t index;
    unsigned kind;
    bool occupied, repeated, drop_message;
} tv4_test_link_direction;
typedef struct {
    tv4_test_link_direction direction[2];
    tv4_test_link_config config;
    uint32_t *clock;
    uint64_t generation[2];
    unsigned frames, delivered_frames, duplicate_frames, dropped_frames;
    unsigned queue_busy, repeated_peeks, order_rejects, stale_rejects, ready_busy;
    unsigned timeouts, sticky_failures, takes, attempts[9], deliveries[9];
    uint16_t dropped_kinds;
    bool configured;
} tv4_test_link;
void tv4_test_link_init(tv4_test_link *link, uint32_t *clock,
    const tv4_test_link_config *config);
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
