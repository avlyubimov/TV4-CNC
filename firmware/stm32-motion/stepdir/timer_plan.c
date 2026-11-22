#include "timer_plan.h"

#define NS_PER_SECOND UINT64_C(1000000000)
#define COUNTER_VALUES UINT64_C(65536)
#define MAX_TIMER_HZ UINT32_C(168000000)

static uint64_t ceil_div(uint64_t numerator, uint64_t denominator)
{
    return numerator / denominator + (uint64_t)(numerator % denominator != 0U);
}

static uint64_t ticks_for_ns(uint32_t hz, uint32_t ns, uint64_t divisor)
{
    /* 168e6 * UINT32_MAX fits uint64_t; divisor <=65536. */
    return ceil_div((uint64_t)hz * ns, NS_PER_SECOND * divisor);
}

tv4_stepdir_result tv4_stepdir_compile(const tv4_stepdir_request *request,
                                     const tv4_stepdir_timing *timing,
                                     tv4_stepdir_plan *out)
{
    if (out == 0) return TV4_STEPDIR_PLAN_INVALID;
    *out = (tv4_stepdir_plan){0};
    if (request == 0 || timing == 0) return TV4_STEPDIR_PLAN_INVALID;
    if (!timing->reviewed) return TV4_STEPDIR_PLAN_UNREVIEWED;
    if (request->timer_input_hz == 0U || request->timer_input_hz > MAX_TIMER_HZ ||
        request->requested_hz == 0U || timing->max_step_hz == 0U ||
        request->requested_hz > timing->max_step_hz ||
        request->pulses == 0U || request->pulses > 256U ||
        (request->direction != TV4_STEPDIR_POSITIVE &&
         request->direction != TV4_STEPDIR_NEGATIVE) ||
        timing->min_active_ns == 0U || timing->min_inactive_ns == 0U)
        return TV4_STEPDIR_PLAN_INVALID;

    const uint64_t clock = request->timer_input_hz;
    const uint64_t rate = request->requested_hz;
    if (rate * 2U > clock) return TV4_STEPDIR_PLAN_UNREPRESENTABLE;
    /* Smallest prescaler that can fit a rounded-up period. No slow search. */
    uint64_t divisor = ceil_div(clock, rate * COUNTER_VALUES);
    if (divisor == 0U) divisor = 1U;
    if (divisor > COUNTER_VALUES) return TV4_STEPDIR_PLAN_UNREPRESENTABLE;
    const uint64_t period = ceil_div(clock, rate * divisor);
    const uint64_t active = ticks_for_ns(request->timer_input_hz,
                                         timing->min_active_ns, divisor);
    uint64_t inactive = ticks_for_ns(request->timer_input_hz,
                                    timing->min_inactive_ns, divisor);
    const uint64_t setup = ticks_for_ns(request->timer_input_hz,
                                       timing->dir_setup_ns, divisor);
    if (setup > inactive) inactive = setup;
    if (period < 2U || period > COUNTER_VALUES || active == 0U ||
        inactive == 0U || active + inactive > period)
        return TV4_STEPDIR_PLAN_UNREPRESENTABLE;

    const uint64_t hold = ticks_for_ns(request->timer_input_hz,
                                      timing->dir_hold_ns, divisor);
    if (hold > UINT32_MAX) return TV4_STEPDIR_PLAN_UNREPRESENTABLE;
    out->psc = (uint16_t)(divisor - 1U);
    out->arr = (uint16_t)(period - 1U);
    /* PWM2 starts inactive at CNT=0 and returns inactive on final overflow. */
    out->ccr1 = (uint16_t)(period - active);
    out->rcr = (uint16_t)(request->pulses - 1U);
    out->cr1 = UINT16_C(0x008c); /* ARPE|OPM|URS, never CEN. */
    out->ccmr1 = UINT16_C(0x0078); /* OC1M=111 (PWM2), OC1PE=1. */
    out->direction = request->direction;
    out->post_stop_dir_hold_ticks = (uint32_t)hold;
    out->rate_numerator = request->timer_input_hz;
    out->rate_denominator = divisor * period;
    return TV4_STEPDIR_PLAN_OK;
}
