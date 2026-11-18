#include "supervisor.h"
#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>

typedef struct { unsigned inhibit_calls, feeds; bool inhibit_ok; } fake_board;
static bool inhibit(void *context)
{
    fake_board *b = context;
    ++b->inhibit_calls;
    return b->inhibit_ok;
}
static void feed(void *context) { ++((fake_board *)context)->feeds; }
static tv4_motion_inputs healthy(void)
{
    return (tv4_motion_inputs){.interlocks_closed = true};
}
static void setup(tv4_motion_supervisor *m, fake_board *b, uint32_t now)
{
    *b = (fake_board){.inhibit_ok = true};
    tv4_motion_init(m, (tv4_motion_config){100, 10},
                    (tv4_motion_platform){b, inhibit, feed}, now, false);
}
static void connect(tv4_motion_supervisor *m, uint32_t now)
{
    tv4_motion_self_test(m, true);
    tv4_motion_session(m, 42);
    assert(tv4_motion_heartbeat(m, 42, now));
    tv4_motion_cycle(m, now, healthy());
    assert(tv4_motion_ack_faults(m, now));
    assert(m->state == TV4_MOTION_IDLE);
}
static void boot_and_reset(void)
{
    tv4_motion_supervisor m; fake_board b;
    setup(&m, &b, 0);
    assert(m.state == TV4_MOTION_BOOT && b.inhibit_calls == 1 && b.feeds == 0);
    assert(!tv4_motion_ack_faults(&m, 0));
    tv4_motion_self_test(&m, true);
    assert(m.state == TV4_MOTION_DISCONNECTED);
    connect(&m, 0);
    tv4_motion_init(&m, m.config, m.platform, 1, true);
    assert(m.state == TV4_MOTION_FAULT && m.session == 0 && !m.heartbeat_seen);
    assert(m.latched_faults & TV4_MOTION_RESET_WATCHDOG);
    assert(!tv4_motion_ack_faults(&m, 1));
    connect(&m, 1);
    assert(m.latched_faults == 0);
}
static void interlocks_and_ack(void)
{
    tv4_motion_supervisor m; fake_board b;
    setup(&m, &b, 0); connect(&m, 0);
    tv4_motion_cycle(&m, 1, (tv4_motion_inputs){false, true});
    assert(m.state == TV4_MOTION_FAULT);
    assert(m.active_faults == (TV4_MOTION_INTERLOCK | TV4_MOTION_DRIVE_FAULT));
    assert(!tv4_motion_ack_faults(&m, 1));
    tv4_motion_cycle(&m, 2, healthy());
    assert(m.active_faults == 0 && m.state == TV4_MOTION_FAULT);
    assert(!tv4_motion_ack_faults(&m, 12)); /* stale control snapshot */
    assert(tv4_motion_ack_faults(&m, 2));
    assert(m.state == TV4_MOTION_IDLE);
    tv4_motion_self_test(&m, false);
    tv4_motion_cycle(&m, 3, healthy());
    assert(!tv4_motion_ack_faults(&m, 3));
    tv4_motion_self_test(&m, true);
    assert(!tv4_motion_ack_faults(&m, 3)); /* requires new cycle */
    tv4_motion_cycle(&m, 4, healthy());
    assert(tv4_motion_ack_faults(&m, 4));
}
static void link_loss_and_sessions(void)
{
    tv4_motion_supervisor m; fake_board b;
    setup(&m, &b, 0); connect(&m, 0);
    assert(!tv4_motion_heartbeat(&m, 0, 1));
    assert(!tv4_motion_heartbeat(&m, 43, 1));
    for (uint32_t t = 1; t <= 100; ++t) tv4_motion_cycle(&m, t, healthy());
    assert(m.active_faults == TV4_MOTION_LINK_LOST); /* exact boundary */
    assert(!tv4_motion_ack_faults(&m, 100));
    assert(tv4_motion_heartbeat(&m, 42, 100));
    assert(!tv4_motion_ack_faults(&m, 100));
    tv4_motion_cycle(&m, 101, healthy());
    assert(m.state == TV4_MOTION_FAULT); /* reconnect never clears latch */
    assert(tv4_motion_ack_faults(&m, 101));
    tv4_motion_session(&m, 43);
    assert(!m.heartbeat_seen && m.state == TV4_MOTION_FAULT);
    assert(!tv4_motion_heartbeat(&m, 42, 102));
    assert(tv4_motion_heartbeat(&m, 43, 102));
    tv4_motion_cycle(&m, 102, healthy());
    assert(tv4_motion_ack_faults(&m, 102));
    tv4_motion_session(&m, 0);
    assert(!tv4_motion_heartbeat(&m, 0, 103));
}
static void late_packet_cannot_hide_loss(void)
{
    tv4_motion_supervisor m; fake_board b;
    setup(&m, &b, 0); connect(&m, 0);
    for (uint32_t t = 1; t < 100; ++t) tv4_motion_cycle(&m, t, healthy());
    assert(tv4_motion_heartbeat(&m, 42, 100));
    tv4_motion_cycle(&m, 100, healthy());
    assert(m.active_faults == 0 && m.latched_faults == TV4_MOTION_LINK_LOST);
    assert(tv4_motion_ack_faults(&m, 100));
}
static void watchdog_and_platform_failure(void)
{
    tv4_motion_supervisor m; fake_board b;
    setup(&m, &b, 0); connect(&m, 0);
    tv4_motion_cycle(&m, 1, healthy());
    assert(b.feeds == 1);
    tv4_motion_cycle(&m, 1, healthy()); /* repeated poll is not progress */
    assert(b.feeds == 1);
    tv4_motion_cycle(&m, 11, healthy());
    assert(m.latched_faults & TV4_MOTION_DEADLINE);
    tv4_motion_cycle(&m, 12, healthy());
    assert(!tv4_motion_ack_faults(&m, 12) && b.feeds == 1);
    setup(&m, &b, 0); connect(&m, 0);
    b.inhibit_ok = false;
    tv4_motion_cycle(&m, 1, healthy());
    assert(m.latched_faults & TV4_MOTION_PLATFORM);
    b.inhibit_ok = true;
    tv4_motion_cycle(&m, 2, healthy());
    assert(!tv4_motion_ack_faults(&m, 2) && b.feeds == 0);
}
static void wraparound_and_bad_config(void)
{
    tv4_motion_supervisor m; fake_board b;
    setup(&m, &b, UINT32_MAX - 2); connect(&m, UINT32_MAX - 2);
    tv4_motion_cycle(&m, 1, healthy());
    assert(m.active_faults == 0 && b.feeds == 1);
    tv4_motion_config configs[] = {{0, 10}, {100, 0}, {UINT32_MAX, 10},
                                  {100, UINT32_MAX}, {100, INT32_MAX}};
    for (unsigned i = 0; i < sizeof configs / sizeof configs[0] - 1; ++i) {
        tv4_motion_init(&m, configs[i], m.platform, 0, false);
        tv4_motion_self_test(&m, true);
        tv4_motion_cycle(&m, 1, healthy());
        assert(m.active_faults & TV4_MOTION_CONFIG);
        assert(!tv4_motion_heartbeat(&m, 42, 1));
        assert(!tv4_motion_ack_faults(&m, 1));
    }
    tv4_motion_init(&m, configs[4], (tv4_motion_platform){0}, 0, false);
    assert(m.latched_faults & TV4_MOTION_PLATFORM);
    assert(m.latched_faults & TV4_MOTION_CONFIG);
}
static void adversarial_sequence(void)
{
    tv4_motion_supervisor m; fake_board b;
    setup(&m, &b, 0); connect(&m, 0);
    uint32_t rng = 1;
    for (uint32_t t = 1; t < 100000; ++t) {
        rng = rng * 1664525u + 1013904223u;
        if ((rng & 15u) == 0) (void)tv4_motion_heartbeat(&m, 42, t);
        tv4_motion_cycle(&m, t, (tv4_motion_inputs){(rng & 7u) != 0, (rng & 31u) == 1});
        if ((rng & 3u) == 0) (void)tv4_motion_ack_faults(&m, t);
        assert(m.state == TV4_MOTION_IDLE || m.state == TV4_MOTION_FAULT);
        if (m.active_faults) assert(m.state == TV4_MOTION_FAULT);
        assert(b.inhibit_calls >= t);
    }
}
int main(void)
{
    boot_and_reset(); interlocks_and_ack(); link_loss_and_sessions();
    late_packet_cannot_hide_loss(); watchdog_and_platform_failure();
    wraparound_and_bad_config(); adversarial_sequence();
    puts("7 motion supervisor scenarios PASS (including 99,999 fault-injection cycles)");
    return 0;
}
