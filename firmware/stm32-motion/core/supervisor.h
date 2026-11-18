#ifndef TV4_MOTION_SUPERVISOR_H
#define TV4_MOTION_SUPERVISOR_H
#include <stdbool.h>
#include <stdint.h>
/* Portable bench supervisor, not a production safety controller. Single owner;
 * ISR/transport must deliver validated snapshots through a bounded queue. */
typedef enum { TV4_MOTION_BOOT, TV4_MOTION_DISCONNECTED,
               TV4_MOTION_IDLE, TV4_MOTION_FAULT } tv4_motion_state;
enum {
    TV4_MOTION_INTERLOCK = 1u << 0,
    TV4_MOTION_DRIVE_FAULT = 1u << 1,
    TV4_MOTION_LINK_LOST = 1u << 2,
    TV4_MOTION_DEADLINE = 1u << 3,
    TV4_MOTION_PLATFORM = 1u << 4,
    TV4_MOTION_CONFIG = 1u << 5,
    TV4_MOTION_RESET_WATCHDOG = 1u << 6,
    TV4_MOTION_SELF_TEST = 1u << 7
};
typedef struct {
    void *context;
    /* Must inhibit STEP, axis enables and spindle. true means the adapter
     * completed its inhibit operation, NOT a safety-rated feedback signal.
     * No inverse/enable callback exists in this foundation. */
    bool (*inhibit_outputs)(void *context);
    void (*feed_watchdog)(void *context);
} tv4_motion_platform;
typedef struct {
    /* Both limits must be 1..INT32_MAX milliseconds. Not CAN wire constants. */
    uint32_t heartbeat_timeout_ms;
    uint32_t cycle_timeout_ms;
} tv4_motion_config;
typedef struct { bool interlocks_closed; bool drive_fault; } tv4_motion_inputs;
typedef struct {
    tv4_motion_platform platform;
    tv4_motion_config config;
    tv4_motion_state state;
    uint32_t active_faults, latched_faults;
    uint32_t session, last_heartbeat_ms, last_cycle_ms;
    bool configured, self_test_passed, heartbeat_seen, cycle_seen;
} tv4_motion_supervisor;
/* All APIs require a valid, initialized object; no allocation or blocking I/O.
 * now_ms is monotonic modulo 2^32; poll at intervals <2^31 ms. */
void tv4_motion_init(tv4_motion_supervisor *m, tv4_motion_config config,
                     tv4_motion_platform platform, uint32_t now_ms,
                     bool watchdog_reset);
void tv4_motion_self_test(tv4_motion_supervisor *m, bool passed);
/* Negotiation is external. A new session discards previous heartbeat;
 * reconnect cannot clear a latched fault. Session zero means disconnected. */
void tv4_motion_session(tv4_motion_supervisor *m, uint32_t session);
bool tv4_motion_heartbeat(tv4_motion_supervisor *m, uint32_t session,
                           uint32_t now_ms);
void tv4_motion_cycle(tv4_motion_supervisor *m, uint32_t now_ms,
                      tv4_motion_inputs inputs);
/* Only after a successful fresh cycle with no active cause. Never enables IO. */
bool tv4_motion_ack_faults(tv4_motion_supervisor *m, uint32_t now_ms);
#endif
