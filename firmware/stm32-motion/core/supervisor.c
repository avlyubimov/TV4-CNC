#include "supervisor.h"
#include <limits.h>
#include <stddef.h>

static bool valid_timeout(uint32_t timeout)
{
    return timeout > 0 && timeout <= INT32_MAX;
}
static bool inhibit(tv4_motion_supervisor *m)
{
    bool ok = m->platform.inhibit_outputs != NULL &&
              m->platform.inhibit_outputs(m->platform.context);
    if (!ok) {
        m->active_faults |= TV4_MOTION_PLATFORM;
        m->latched_faults |= TV4_MOTION_PLATFORM;
    }
    return ok;
}
static void refresh_state(tv4_motion_supervisor *m)
{
    if (m->latched_faults) m->state = TV4_MOTION_FAULT;
    else if (!m->self_test_passed) m->state = TV4_MOTION_BOOT;
    else if (!m->heartbeat_seen) m->state = TV4_MOTION_DISCONNECTED;
    else m->state = TV4_MOTION_IDLE;
}
void tv4_motion_init(tv4_motion_supervisor *m, tv4_motion_config config,
                     tv4_motion_platform platform, uint32_t now_ms,
                     bool watchdog_reset)
{
    *m = (tv4_motion_supervisor){.config = config, .platform = platform,
                                .last_cycle_ms = now_ms};
    m->configured = valid_timeout(config.heartbeat_timeout_ms) &&
                    valid_timeout(config.cycle_timeout_ms) &&
                    platform.inhibit_outputs != NULL &&
                    platform.feed_watchdog != NULL;
    if (!m->configured) m->active_faults |= TV4_MOTION_CONFIG;
    if (watchdog_reset) m->latched_faults |= TV4_MOTION_RESET_WATCHDOG;
    m->latched_faults |= m->active_faults;
    (void)inhibit(m);
    refresh_state(m);
}
void tv4_motion_self_test(tv4_motion_supervisor *m, bool passed)
{
    m->self_test_passed = passed;
    if (passed) m->active_faults &= ~(uint32_t)TV4_MOTION_SELF_TEST;
    else {
        m->active_faults |= TV4_MOTION_SELF_TEST;
        m->latched_faults |= TV4_MOTION_SELF_TEST;
    }
    m->cycle_seen = false;
    (void)inhibit(m);
    refresh_state(m);
}
void tv4_motion_session(tv4_motion_supervisor *m, uint32_t session)
{
    if (session == m->session) return;
    if (m->heartbeat_seen) {
        m->active_faults |= TV4_MOTION_LINK_LOST;
        m->latched_faults |= TV4_MOTION_LINK_LOST;
    }
    m->session = session;
    m->heartbeat_seen = false;
    m->cycle_seen = false;
    (void)inhibit(m);
    refresh_state(m);
}
bool tv4_motion_heartbeat(tv4_motion_supervisor *m, uint32_t session,
                           uint32_t now_ms)
{
    if (!m->configured || session == 0 || session != m->session) return false;
    /* A late packet cannot erase a missed deadline before cycle() sees it. */
    if (m->heartbeat_seen &&
        (uint32_t)(now_ms - m->last_heartbeat_ms) >= m->config.heartbeat_timeout_ms) {
        m->active_faults |= TV4_MOTION_LINK_LOST;
        m->latched_faults |= TV4_MOTION_LINK_LOST;
    }
    m->last_heartbeat_ms = now_ms;
    m->heartbeat_seen = true;
    m->cycle_seen = false;
    (void)inhibit(m);
    refresh_state(m);
    return true;
}
void tv4_motion_cycle(tv4_motion_supervisor *m, uint32_t now_ms,
                      tv4_motion_inputs inputs)
{
    uint32_t sticky = m->active_faults & (TV4_MOTION_CONFIG | TV4_MOTION_SELF_TEST);
    m->active_faults = sticky;
    if (!inputs.interlocks_closed) m->active_faults |= TV4_MOTION_INTERLOCK;
    if (inputs.drive_fault) m->active_faults |= TV4_MOTION_DRIVE_FAULT;
    if (m->configured) {
        if ((uint32_t)(now_ms - m->last_cycle_ms) >= m->config.cycle_timeout_ms)
            m->active_faults |= TV4_MOTION_DEADLINE;
        if (!m->heartbeat_seen ||
            (uint32_t)(now_ms - m->last_heartbeat_ms) >= m->config.heartbeat_timeout_ms)
            m->active_faults |= TV4_MOTION_LINK_LOST;
    }
    bool progressed = now_ms != m->last_cycle_ms;
    m->last_cycle_ms = now_ms;
    m->cycle_seen = true;
    bool inhibited = inhibit(m);
    m->latched_faults |= m->active_faults;
    refresh_state(m);
    /* Feed only after a fresh, timely control cycle and successful inhibit.
     * Ordinary interlock/link faults can remain diagnosable; deadline/platform
     * failures must expire the real watchdog once a board adapter exists. */
    if (m->configured && m->self_test_passed && inhibited && progressed &&
        !(m->latched_faults & (TV4_MOTION_DEADLINE | TV4_MOTION_PLATFORM)))
        m->platform.feed_watchdog(m->platform.context);
}
bool tv4_motion_ack_faults(tv4_motion_supervisor *m, uint32_t now_ms)
{
    if (!m->configured || !m->self_test_passed || !m->cycle_seen || m->active_faults)
        return false;
    if ((uint32_t)(now_ms - m->last_cycle_ms) >= m->config.cycle_timeout_ms ||
        !m->heartbeat_seen ||
        (uint32_t)(now_ms - m->last_heartbeat_ms) >= m->config.heartbeat_timeout_ms)
        return false;
    /* Deadline/platform recovery requires reinitialization, not an ACK. */
    if (m->latched_faults & (TV4_MOTION_DEADLINE | TV4_MOTION_PLATFORM)) return false;
    if (!inhibit(m)) { refresh_state(m); return false; }
    m->latched_faults = 0;
    refresh_state(m);
    return true;
}
