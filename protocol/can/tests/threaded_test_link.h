#ifndef TV4_THREADED_TEST_LINK_H
#define TV4_THREADED_TEST_LINK_H
#include "fragment_test_link.h"
#include <pthread.h>

/* HOST TEST ONLY. One calling protocol/crypto actor + one pthread driver.
 * No device, runtime adapter, real clock, physical CAN/backpressure/drain proof.
 * init objects before start; stable addresses until stop/join. Never copy live.
 * The mutex/conditions schedule bounded logical ticks, NOT target API access.
 * now is frozen during each tick, read locally AFTER take/stage. Crypto CPU
 * duration is not charged. At most one backend frame/capture per direction.
 */
typedef struct {
    tv4_dispatch_delivery job;
    tv4_fragment_frame captured;
    tv4_ingress_permit permit;
    uint32_t due,captured_at;
    uint16_t index;
    unsigned kind;
    bool occupied,repeated,drop_message,drop_current,has_capture,pending_report;
} tv4_threaded_direction;
typedef struct {
    unsigned claims,posts,ingress_posts,rejected_callbacks,drops,held_ticks;
    unsigned captures,retained_captures,pause_acks,report_retries;
    uint16_t dropped_kinds;
} tv4_threaded_statistics;
typedef struct tv4_threaded_link {
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t work,done;
    /* Shared only under lock; no target API is called while holding lock. */
    uint32_t now;
    bool requested,finished,stopping,hold_backend,active,started;
    /* Immutable bindings/config published by pthread_create. */
    tv4_driver_dispatch *dispatch[2];
    tv4_ingress_owner *owner[2];
    uint64_t generation[2];
    tv4_test_link_config config;
    /* Driver-private until tick completion/join. No protocol state aliases. */
    tv4_threaded_direction direction[2];
    tv4_threaded_statistics stats;
} tv4_threaded_link;

void tv4_threaded_link_start(tv4_threaded_link *s,tv4_test_link *link);
void tv4_threaded_link_begin(tv4_threaded_link *s,uint32_t now);
void tv4_threaded_link_wait(tv4_threaded_link *s,tv4_test_link *link);
/* A bounded driver turn at the SAME logical time, for pause ACK/backpressure.
 * Never advances clock or bypasses actual consumer drain/phase/upper guards. */
void tv4_threaded_link_pulse(tv4_threaded_link *s,tv4_test_link *link);
/* Test scheduling only. Held actual driver job remains owned even if expired. */
void tv4_threaded_link_hold(tv4_threaded_link *s,tv4_test_link *link,bool hold);
/* Joins only. Does not flush, synthesize completion, reset or clear ownership. */
void tv4_threaded_link_stop(tv4_threaded_link *s,tv4_test_link *link);
#endif
