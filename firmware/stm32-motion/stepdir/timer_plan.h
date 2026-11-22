#ifndef TV4_STEPDIR_TIMER_PLAN_H
#define TV4_STEPDIR_TIMER_PLAN_H

#include <stdbool.h>
#include <stdint.h>

/* Design only: TIM1/TIM8, edge-aligned upcounter, CH1 PWM2 + OPM + RCR.
 * No peripheral address, pin mapping, physical polarity or start/enable API.
 * These inputs are caller claims, never hardware approval or safety authority.
 * All timing values must come from a reviewed driver/interface specification.
 */
typedef enum {
    TV4_STEPDIR_POSITIVE = 1,
    TV4_STEPDIR_NEGATIVE = 2
} tv4_stepdir_direction;

typedef struct {
    bool reviewed;
    uint32_t max_step_hz;
    uint32_t min_active_ns;
    uint32_t min_inactive_ns;
    uint32_t dir_setup_ns;
    uint32_t dir_hold_ns;
} tv4_stepdir_timing;

typedef struct {
    uint32_t timer_input_hz; /* Explicit TIM clock, not APB clock or measured HSI. */
    uint32_t requested_hz;   /* Integer Hz; rounding may only reduce this rate. */
    uint16_t pulses;        /* 1..256; no automatic chaining or acceleration. */
    tv4_stepdir_direction direction; /* Logical only; no DIR electrical level. */
} tv4_stepdir_request;

typedef struct {
    uint16_t psc;
    uint16_t arr;
    uint16_t ccr1;
    uint16_t rcr;
    uint16_t cr1;           /* OPM/ARPE/URS, CEN=0; UDIS/DIR/CMS=0. */
    uint16_t ccmr1;         /* PWM2 and preload; CC1S=output. */
    uint16_t ccer;          /* Always zero: all channel gates disabled. */
    bool main_output_enabled; /* Always false; no complete BDTR/polarity config. */
    tv4_stepdir_direction direction;
    uint32_t post_stop_dir_hold_ticks; /* Contract for a future adapter, not executed. */
    uint32_t rate_numerator;          /* Exact OCREF frequency = numerator/denominator. */
    uint64_t rate_denominator;
} tv4_stepdir_plan;

typedef enum {
    TV4_STEPDIR_PLAN_OK = 0,
    TV4_STEPDIR_PLAN_INVALID,
    TV4_STEPDIR_PLAN_UNREVIEWED,
    TV4_STEPDIR_PLAN_UNREPRESENTABLE
} tv4_stepdir_result;

/* Input/output objects must not overlap. Failure clears *out, when non-NULL.
 * OK is arithmetic only, not permission to apply the plan. Before a future
 * start: disable gates/counter, load PSC/ARR/CCR/RCR, UG while stopped (loads
 * shadow repetition count), clear pending status, then independently reviewed
 * DIR/setup/hold, clock, GPIO, break/inhibit and explicit start sequencing.
 */
tv4_stepdir_result tv4_stepdir_compile(const tv4_stepdir_request *request,
                                     const tv4_stepdir_timing *timing,
                                     tv4_stepdir_plan *out);

#endif
