#include "update_completion.h"
#include "update_handoff_internal.h"
#include <limits.h>
#include <string.h>

_Static_assert(sizeof(tv4_boot_completion) <= 768, "completion storage regression");
static bool separate(const void *a, size_t as, const void *b, size_t bs)
{
    uintptr_t x=(uintptr_t)a, y=(uintptr_t)b;
    return a != NULL && b != NULL && as <= UINTPTR_MAX-x && bs <= UINTPTR_MAX-y && (x+as <= y || y+bs <= x);
}
static bool same_image(const tv4_update_descriptor *a, const tv4_update_descriptor *b)
{
    return a->id.boot == b->id.boot && a->id.transfer == b->id.transfer && a->image_bytes == b->image_bytes &&
        memcmp(a->image_sha256,b->image_sha256,32) == 0;
}
static void remember(tv4_boot_completion *c, tv4_outcome_reason reason)
{
    if (c->reason == TV4_OUTCOME_REASON_NONE) c->reason=reason;
}
static tv4_outcome_reason admission_reason(tv4_boot_result r)
{
    switch (r) {
    case TV4_BOOT_SIGNATURE: return TV4_OUTCOME_SIGNATURE;
    case TV4_BOOT_KEY: return TV4_OUTCOME_KEY;
    case TV4_BOOT_METADATA: return TV4_OUTCOME_METADATA;
    case TV4_BOOT_COMPATIBILITY: return TV4_OUTCOME_COMPATIBILITY;
    case TV4_BOOT_ROLLBACK: return TV4_OUTCOME_ROLLBACK;
    case TV4_BOOT_LENGTH: return TV4_OUTCOME_LENGTH;
    case TV4_BOOT_HASH: return TV4_OUTCOME_HASH;
    case TV4_BOOT_SOURCE: return TV4_OUTCOME_SOURCE;
    case TV4_BOOT_CANCELLED: return TV4_OUTCOME_CANCEL;
    case TV4_BOOT_TIMEOUT: return TV4_OUTCOME_TIMEOUT;
    case TV4_BOOT_CLOCK: return TV4_OUTCOME_CLOCK;
    default: return TV4_OUTCOME_INTERNAL;
    }
}
static tv4_outcome_reason transport_reason(const tv4_update_rx *rx)
{
    switch (rx->context.error) {
    case TV4_UPDATE_ERROR_CLOCK: return TV4_OUTCOME_CLOCK;
    case TV4_UPDATE_ERROR_PROGRESS_TIMEOUT: case TV4_UPDATE_ERROR_TOTAL_TIMEOUT: return TV4_OUTCOME_TIMEOUT;
    default: return TV4_OUTCOME_CONTEXT;
    }
}
static void handoff_reason(tv4_boot_completion *c)
{
    const tv4_boot_handoff *s=c->handoff;
    switch (s->error) {
    case TV4_HANDOFF_ERROR_NONE: break;
    case TV4_HANDOFF_CANCELLED: remember(c,TV4_OUTCOME_CANCEL); break;
    case TV4_HANDOFF_CLOCK: remember(c,TV4_OUTCOME_CLOCK); break;
    case TV4_HANDOFF_TIMEOUT: remember(c,TV4_OUTCOME_TIMEOUT); break;
    case TV4_HANDOFF_OWNER: remember(c,TV4_OUTCOME_CONTEXT); break;
    case TV4_HANDOFF_RECEIVER: remember(c,transport_reason(c->transfer)); break;
    case TV4_HANDOFF_SOURCE: remember(c,TV4_OUTCOME_SOURCE); break;
    case TV4_HANDOFF_ADMISSION: remember(c,admission_reason(s->admission_result)); break;
    case TV4_HANDOFF_BINDING: remember(c,TV4_OUTCOME_BINDING); break;
    case TV4_HANDOFF_RELEASE: remember(c,TV4_OUTCOME_RELEASE); break;
    case TV4_HANDOFF_PREFLIGHT:
        switch (s->preflight_result) {
        case TV4_IMAGE_PROFILE: remember(c,TV4_OUTCOME_PROFILE); break;
        case TV4_IMAGE_LENGTH: remember(c,TV4_OUTCOME_LENGTH); break;
        case TV4_IMAGE_STACK: case TV4_IMAGE_RESERVED_VECTOR: case TV4_IMAGE_HANDLER: remember(c,TV4_OUTCOME_VECTORS); break;
        case TV4_IMAGE_SOURCE: remember(c,TV4_OUTCOME_SOURCE); break;
        case TV4_IMAGE_CONTEXT: remember(c,TV4_OUTCOME_CONTEXT); break;
        default: remember(c,TV4_OUTCOME_INTERNAL); break;
        }
        break;
    }
}
static bool observe(tv4_boot_completion *c)
{
    if (c->cancel_requested || c->observer.cancelled(c->observer.context)) remember(c,TV4_OUTCOME_CANCEL);
    tv4_boot_owner_stamp stamp={0};
    if (!c->observer.owner_stamp(c->observer.context,&stamp) || stamp.boot != c->owner.boot ||
        stamp.policy_generation != c->owner.policy_generation) remember(c,TV4_OUTCOME_CONTEXT);
    uint32_t now=c->observer.now_ms(c->observer.context);
    if ((uint32_t)(now-c->observed_ms) > INT32_MAX) remember(c,TV4_OUTCOME_CLOCK);
    c->observed_ms=now;
    if ((uint32_t)(now-c->started_ms) >= c->lifetime_ms) remember(c,TV4_OUTCOME_TIMEOUT);
    const tv4_update_context *t=&c->transfer->context;
    if (tv4_update_rx_poll(c->transfer,now) != TV4_UPDATE_OK || t->state != TV4_UPDATE_COMPLETE_UNVERIFIED ||
        t->error != TV4_UPDATE_ERROR_NONE || !same_image(&t->descriptor,&c->expected.image) ||
        t->started_ms != c->transfer_started_ms || t->progress_ms != c->transfer_progress_ms)
        remember(c,transport_reason(c->transfer));
    if (c->cancel_requested) remember(c,TV4_OUTCOME_CANCEL);
    return c->reason == TV4_OUTCOME_REASON_NONE;
}
static bool matches(const tv4_boot_completion *c)
{
    const tv4_boot_handoff_receipt *r=&c->inspection.receipt;
    return r->id.boot == c->expected.image.id.boot && r->id.transfer == c->expected.image.id.transfer &&
        r->image.image_bytes == c->expected.image.image_bytes && memcmp(r->image.image_sha256,c->expected.image.image_sha256,32) == 0 &&
        memcmp(r->image.metadata_sha256,c->expected.metadata_sha256,32) == 0;
}
static void settle(tv4_boot_completion *c)
{
    handoff_reason(c);
    (void)observe(c); /* including close callbacks, before ending borrowed lifetime */
    tv4_boot_handoff *s=c->handoff;
    if (s->state == TV4_HANDOFF_RELEASING) c->state=TV4_COMPLETION_CLOSING;
    else if (s->state == TV4_HANDOFF_CLOSED && !s->held) {
        c->report.binding=c->expected; c->report.reason=c->reason;
        c->report.code=c->reason == TV4_OUTCOME_REASON_NONE ? TV4_OUTCOME_CHECKED_CLOSED :
            c->reason == TV4_OUTCOME_CANCEL ? TV4_OUTCOME_CANCELLED_CLOSED :
            c->reason <= TV4_OUTCOME_VECTORS ? TV4_OUTCOME_REJECTED_CLOSED : TV4_OUTCOME_FAILED_CLOSED;
        c->state=TV4_COMPLETION_READY; c->handoff=NULL; c->transfer=NULL;
        c->observer=(tv4_boot_completion_observer){0};
    } else {
        remember(c,TV4_OUTCOME_RELEASE);
        c->report=(tv4_update_outcome){.binding=c->expected,.code=TV4_OUTCOME_QUARANTINED,.reason=c->reason};
        c->state=TV4_COMPLETION_QUARANTINED; /* retain handoff/owner, never force close */
    }
    memset(&c->profile,0,sizeof(c->profile)); memset(&c->inspection,0,sizeof(c->inspection));
}
static bool start(tv4_boot_completion *c, tv4_boot_handoff *s,
    const tv4_update_binding *expected, const tv4_boot_image_profile *profile, tv4_boot_workspace *w,
    tv4_boot_release_handoff *release)
{
    if (c == NULL || s == NULL || expected == NULL || profile == NULL || w == NULL || c->busy ||
        c->state != TV4_COMPLETION_EMPTY || s->busy || s->release_document != (release != NULL) ||
        s->state != TV4_HANDOFF_BOUND || !s->held || s->rx == NULL ||
        !separate(c,sizeof(*c),s,sizeof(*s)) || !separate(c,sizeof(*c),w,sizeof(*w)) ||
        !separate(c,sizeof(*c),expected,sizeof(*expected)) || !separate(c,sizeof(*c),profile,sizeof(*profile)) ||
        !separate(c,sizeof(*c),s->rx,sizeof(*s->rx)) || !separate(w,sizeof(*w),s,sizeof(*s)) ||
        !separate(w,sizeof(*w),s->rx,sizeof(*s->rx)) || !separate(w,sizeof(*w),expected,sizeof(*expected)) ||
        !separate(w,sizeof(*w),profile,sizeof(*profile)) || !same_image(&s->descriptor,&expected->image) ||
        s->ops.now_ms == NULL || s->ops.cancelled == NULL || s->ops.owner_stamp == NULL) return false;
    c->handoff=s; c->transfer=s->rx; c->expected=*expected; c->profile=*profile;
    c->observer=(tv4_boot_completion_observer){.context=s->ops.context,.now_ms=s->ops.now_ms,
        .cancelled=s->ops.cancelled,.owner_stamp=s->ops.owner_stamp};
    c->owner=s->owner; c->started_ms=s->started_ms; c->observed_ms=s->observed_ms; c->lifetime_ms=s->lifetime_ms;
    c->transfer_started_ms=s->rx_started_ms; c->transfer_progress_ms=s->rx_progress_ms;
    c->state=TV4_COMPLETION_WORKING; c->busy=true;
    if (!observe(c)) { tv4_boot_handoff_abort(s); goto complete; }
    if (!tv4_handoff_verify_mode(s,w,release==NULL ? NULL : &release->policy,release==NULL ? NULL : &release->candidate)) goto complete;
    if (!tv4_handoff_peek_mode(s,&c->inspection.receipt,release!=NULL)) goto complete;
    if (!matches(c)) { remember(c,TV4_OUTCOME_BINDING); tv4_boot_handoff_abort(s); goto complete; }
    if (tv4_handoff_preflight_mode(s,&c->profile,w,&c->inspection,release!=NULL) != TV4_IMAGE_PROFILE_MODEL_MATCH) goto complete;
    if (!matches(c)) { remember(c,TV4_OUTCOME_BINDING); tv4_boot_handoff_abort(s); goto complete; }
    (void)tv4_boot_handoff_close(s); /* exactly one initial attempt */
complete:
    settle(c); c->busy=false; return true;
}
bool tv4_boot_completion_start(tv4_boot_completion *c, tv4_boot_handoff *s,
    const tv4_update_binding *expected, const tv4_boot_image_profile *profile, tv4_boot_workspace *w)
{
    return start(c,s,expected,profile,w,NULL);
}
bool tv4_boot_completion_start_release(tv4_boot_completion *c, tv4_boot_release_handoff *a,
    const tv4_update_binding *expected, const tv4_boot_image_profile *profile, tv4_boot_workspace *w)
{
    if (a==NULL || a->busy || a->handoff==NULL || a->handoff->busy || !a->handoff->release_document ||
        !separate(a,sizeof(*a),c,sizeof(*c)) || !separate(a,sizeof(*a),expected,sizeof(*expected)) ||
        !separate(a,sizeof(*a),profile,sizeof(*profile)) || !separate(a,sizeof(*a),w,sizeof(*w))) return false;
    a->busy=true;
    bool accepted=start(c,a->handoff,expected,profile,w,a);
    if (accepted) tv4_release_handoff_refresh(a);
    a->busy=false; return accepted;
}
bool tv4_boot_completion_poll(tv4_boot_completion *c)
{
    if (c == NULL || c->busy) return false;
    if (c->state == TV4_COMPLETION_READY) return true;
    if (c->state != TV4_COMPLETION_CLOSING) return false;
    c->busy=true; (void)observe(c); (void)tv4_boot_handoff_close(c->handoff);
    settle(c); c->busy=false; return c->state == TV4_COMPLETION_READY;
}
void tv4_boot_completion_abort(tv4_boot_completion *c)
{
    if (c == NULL || c->state == TV4_COMPLETION_EMPTY || c->state == TV4_COMPLETION_READY || c->state == TV4_COMPLETION_QUARANTINED) return;
    c->cancel_requested=true;
    if (c->busy) {
        if (c->handoff != NULL && c->handoff->busy) tv4_boot_handoff_abort(c->handoff); /* latches only */
    } else (void)tv4_boot_completion_poll(c);
}
bool tv4_boot_completion_snapshot(const tv4_boot_completion *c, tv4_update_outcome *out)
{
    if (c == NULL || out == NULL || c->busy || !separate(c,sizeof(*c),out,sizeof(*out)) ||
        (c->handoff != NULL && !separate(c->handoff,sizeof(*c->handoff),out,sizeof(*out))) ||
        (c->transfer != NULL && !separate(c->transfer,sizeof(*c->transfer),out,sizeof(*out)))) return false;
    *out=(tv4_update_outcome){0};
    if ((c->state != TV4_COMPLETION_READY && c->state != TV4_COMPLETION_QUARANTINED) || !tv4_update_outcome_valid(&c->report)) return false;
    *out=c->report; return true;
}
bool tv4_boot_completion_clear(tv4_boot_completion *c)
{
    if (c == NULL || c->busy || c->handoff != NULL ||
        (c->state != TV4_COMPLETION_EMPTY && c->state != TV4_COMPLETION_READY)) return false;
    *c=(tv4_boot_completion){0}; return true;
}
