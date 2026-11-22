#include "timer_plan.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>

/* Synthetic inputs, not the timing/rate/clock of any purchased motor/driver. */
static const tv4_stepdir_timing fixture = {true, 100000U, 2000U, 3000U, 5000U, 7000U};
static tv4_stepdir_request request = {16000000U, 1000U, 1U, TV4_STEPDIR_POSITIVE};

static void zero_plan(const tv4_stepdir_plan *p)
{
    assert(p->psc == 0U && p->arr == 0U && p->ccr1 == 0U && p->rcr == 0U);
    assert(p->cr1 == 0U && p->ccmr1 == 0U && p->ccer == 0U);
    assert(!p->main_output_enabled && p->direction == 0);
    assert(p->post_stop_dir_hold_ticks == 0U && p->rate_numerator == 0U);
    assert(p->rate_denominator == 0U);
}

static void invariants(const tv4_stepdir_request *r, const tv4_stepdir_timing *t,
                       const tv4_stepdir_plan *p)
{
    const uint64_t divisor = (uint64_t)p->psc + 1U;
    const uint64_t period = (uint64_t)p->arr + 1U;
    assert(p->ccr1 > 0U && p->ccr1 <= p->arr && p->rcr == r->pulses - 1U);
    assert(p->cr1 == 0x8cU && p->ccmr1 == 0x78U);
    assert(p->ccer == 0U && !p->main_output_enabled && (p->cr1 & 1U) == 0U);
    assert(p->direction == r->direction && p->rate_numerator == r->timer_input_hz);
    assert(p->rate_denominator == divisor * period);
    /* Cross-products independently check actual rate and physical-time bounds. */
    assert((uint64_t)r->timer_input_hz <= p->rate_denominator * r->requested_hz);
    assert((period - 1U) * divisor * r->requested_hz < r->timer_input_hz);
    assert((period - p->ccr1) * divisor * UINT64_C(1000000000) >=
           (uint64_t)r->timer_input_hz * t->min_active_ns);
    assert((uint64_t)p->ccr1 * divisor * UINT64_C(1000000000) >=
           (uint64_t)r->timer_input_hz * t->min_inactive_ns);
    assert((uint64_t)p->ccr1 * divisor * UINT64_C(1000000000) >=
           (uint64_t)r->timer_input_hz * t->dir_setup_ns);
    const long double hold = (long double)p->post_stop_dir_hold_ticks * divisor * 1000000000.0L;
    const long double required = (long double)r->timer_input_hz * t->dir_hold_ns;
    assert(hold >= required);
    if (p->post_stop_dir_hold_ticks != 0U)
        assert(hold - (long double)divisor * 1000000000.0L < required);
}

/* Independent host counter/comparator/repetition/OPM model. A simulated CEN
 * may start this RAM model ONLY; there is no target peripheral or pin API.
 * Physical gate count remains zero even while the OCREF model has pulses.
 */
static void model(const tv4_stepdir_plan *p, bool host_start, uint32_t expected)
{
    uint32_t cnt = 0U, remaining = p->rcr;
    uint32_t rises = 0U, falls = 0U, external_rises = 0U;
    uint64_t ticks = 0U, active_ticks = 0U;
    bool running = host_start, previous = false;
    while (running) {
        bool reference = cnt >= p->ccr1;
        if (reference && !previous) {
            ++rises;
            if ((p->ccer & 1U) != 0U && p->main_output_enabled) ++external_rises;
            assert(cnt == p->ccr1);
        }
        if (reference) ++active_ticks;
        if (!reference && previous) ++falls;
        previous = reference;
        ++ticks;
        assert(ticks <= UINT64_C(16777216));
        if (cnt == p->arr) {
            cnt = 0U;
            if (remaining == 0U) running = false; /* update stops OPM */
            else --remaining;
        } else ++cnt;
    }
    if (previous) ++falls; /* final overflow returns PWM2 inactive at CNT=0 */
    assert(cnt == 0U && rises == expected && falls == expected && external_rises == 0U);
    assert(ticks == ((uint64_t)p->arr + 1U) * expected);
    assert(active_ticks == ((uint64_t)p->arr + 1U - p->ccr1) * expected);
}

static void boundaries(void)
{
    tv4_stepdir_plan p;
    for (uint16_t count = 1U; count <= 256U; ++count) {
        tv4_stepdir_request r = request; r.pulses = count;
        assert(tv4_stepdir_compile(&r, &fixture, &p) == TV4_STEPDIR_PLAN_OK);
        invariants(&r, &fixture, &p); model(&p, false, 0U);
        if (count == 1U || count == 2U || count == 256U) model(&p, true, count);
    }
    tv4_stepdir_request r = {65536U, 1U, 2U, TV4_STEPDIR_NEGATIVE};
    tv4_stepdir_timing t = {true, 1000000U, 1U, 1U, 0U, 0U};
    assert(tv4_stepdir_compile(&r, &t, &p) == TV4_STEPDIR_PLAN_OK);
    assert(p.psc == 0U && p.arr == 65535U); model(&p, true, 2U);
    r.timer_input_hz = 65537U;
    assert(tv4_stepdir_compile(&r, &t, &p) == TV4_STEPDIR_PLAN_OK);
    assert(p.psc == 1U); invariants(&r, &t, &p); model(&p, true, 2U);
    r.timer_input_hz = 168000000U; r.requested_hz = 1U;
    assert(tv4_stepdir_compile(&r, &t, &p) == TV4_STEPDIR_PLAN_OK);
    invariants(&r, &t, &p);
    r.timer_input_hz = 2U; r.requested_hz = 1U;
    assert(tv4_stepdir_compile(&r, &t, &p) == TV4_STEPDIR_PLAN_OK);
    assert(p.arr == 1U && p.ccr1 == 1U); model(&p, true, 2U);
    r.timer_input_hz = 16000001U; r.requested_hz = 9999U;
    assert(tv4_stepdir_compile(&r, &t, &p) == TV4_STEPDIR_PLAN_OK);
    invariants(&r, &t, &p); model(&p, true, 2U);
    t.dir_hold_ns = UINT32_MAX;
    assert(tv4_stepdir_compile(&r, &t, &p) == TV4_STEPDIR_PLAN_OK);
    invariants(&r, &t, &p);
}

static void refusals(void)
{
    tv4_stepdir_plan p;
    assert(tv4_stepdir_compile(&request, &fixture, NULL) == TV4_STEPDIR_PLAN_INVALID);
    assert(tv4_stepdir_compile(NULL, &fixture, &p) == TV4_STEPDIR_PLAN_INVALID); zero_plan(&p);
    assert(tv4_stepdir_compile(&request, NULL, &p) == TV4_STEPDIR_PLAN_INVALID); zero_plan(&p);
    for (unsigned i = 0U; i < 14U; ++i) {
        tv4_stepdir_request r = request; tv4_stepdir_timing t = fixture;
        switch (i) {
        case 0: t.reviewed = false; break;
        case 1: r.timer_input_hz = 0U; break;
        case 2: r.timer_input_hz = 168000001U; break;
        case 3: r.requested_hz = 0U; break;
        case 4: r.requested_hz = t.max_step_hz + 1U; break;
        case 5: t.max_step_hz = 0U; break;
        case 6: r.pulses = 0U; break;
        case 7: r.pulses = 257U; break;
        case 8: r.direction = (tv4_stepdir_direction)0; break;
        case 9: t.min_active_ns = 0U; break;
        case 10: t.min_inactive_ns = 0U; break;
        case 11: t.min_active_ns = UINT32_MAX; break;
        case 12: t.dir_setup_ns = UINT32_MAX; break;
        default: r.timer_input_hz = 1U; r.requested_hz = UINT32_MAX;
                 t.max_step_hz = UINT32_MAX; break;
        }
        assert(tv4_stepdir_compile(&r, &t, &p) != TV4_STEPDIR_PLAN_OK); zero_plan(&p);
    }
    /* A failed request overwrites a previously successful plan, never stale gates. */
    assert(tv4_stepdir_compile(&request, &fixture, &p) == TV4_STEPDIR_PLAN_OK);
    tv4_stepdir_timing t = fixture; t.reviewed = false;
    assert(tv4_stepdir_compile(&request, &t, &p) == TV4_STEPDIR_PLAN_UNREVIEWED); zero_plan(&p);
}

static uint32_t random_state = UINT32_C(0xf40723);
static uint32_t next_random(void)
{
    random_state = random_state * UINT32_C(1664525) + UINT32_C(1013904223);
    return random_state;
}

static void property_cases(void)
{
    unsigned passed = 0U, refused = 0U;
    for (unsigned i = 0U; i < 20000U; ++i) {
        tv4_stepdir_request r = {1U + next_random() % 168000000U,
            1U + next_random() % 100000U, (uint16_t)(1U + next_random() % 256U),
            (next_random() & 1U) != 0U ? TV4_STEPDIR_POSITIVE : TV4_STEPDIR_NEGATIVE};
        tv4_stepdir_timing t = {true,100000U,1U + next_random() % 20000U,
            1U + next_random() % 20000U,next_random() % 20000U,next_random()};
        tv4_stepdir_plan p;
        if (tv4_stepdir_compile(&r, &t, &p) == TV4_STEPDIR_PLAN_OK) {
            ++passed; invariants(&r, &t, &p);
        } else { ++refused; zero_plan(&p); }
    }
    assert(passed > 1000U && refused > 1000U);
    printf("20000 deterministic timing cases: %u accepted, %u refused\n", passed, refused);
}

int main(void)
{
    boundaries(); refusals(); property_cases();
    puts("PASS timer register plan, OPM/RCR model, disabled outputs; no hardware");
    return 0;
}
