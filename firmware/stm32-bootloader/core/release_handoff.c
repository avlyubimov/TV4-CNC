#include "release_handoff.h"
#include "release_metadata.h"
#include "update_handoff_internal.h"
#include <string.h>

_Static_assert(sizeof(tv4_boot_release_handoff) <= 544, "release adapter storage regression");
static bool separate(const void *a, size_t as, const void *b, size_t bs)
{
    uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
    return a!=NULL && b!=NULL && as<=UINTPTR_MAX-x && bs<=UINTPTR_MAX-y && (x+as<=y || y+bs<=x);
}
static bool attached(const tv4_boot_release_handoff *a)
{
    return a!=NULL && a->handoff!=NULL && a->handoff->release_document;
}
static bool busy(const tv4_boot_release_handoff *a)
{
    return a!=NULL && (a->busy || (a->handoff!=NULL && a->handoff->busy));
}
void tv4_release_handoff_refresh(tv4_boot_release_handoff *a)
{
    const tv4_boot_handoff *s=a->handoff;
    if (s->state!=TV4_HANDOFF_VERIFIED) a->candidate=(tv4_boot_release_candidate){0};
    if (s->state==TV4_HANDOFF_RELEASING || s->state==TV4_HANDOFF_CLOSED || s->state==TV4_HANDOFF_QUARANTINED)
        a->policy=(tv4_boot_release_policy){0};
}
bool tv4_boot_release_handoff_begin(tv4_boot_release_handoff *a, tv4_boot_handoff *s,
    const tv4_boot_release_handoff_config *c, tv4_update_rx *rx)
{
    if (a==NULL || s==NULL || c==NULL || rx==NULL || busy(a) || a->handoff!=NULL ||
        !separate(a,sizeof(*a),s,sizeof(*s)) || !separate(a,sizeof(*a),c,sizeof(*c)) ||
        !separate(a,sizeof(*a),rx,sizeof(*rx)) || !separate(s,sizeof(*s),c,sizeof(*c)) ||
        !tv4_boot_release_policy_valid(&c->policy)) return false;
    tv4_boot_handoff_config base={.policy=c->policy.image,.ops=c->ops,
        .metadata=c->metadata,.signature=c->signature,.key_id=c->key_id,
        .metadata_bytes=c->metadata_bytes,.signature_bytes=c->signature_bytes,.key_id_bytes=c->key_id_bytes,
        .arena_bytes=c->arena_bytes,.lifetime_ms=c->lifetime_ms,.release_attempt_limit=c->release_attempt_limit};
    if (!tv4_handoff_can_begin(s,&base,rx) ||
        !separate(a,sizeof(*a),c->metadata,c->metadata_bytes) ||
        !separate(a,sizeof(*a),c->signature,c->signature_bytes) ||
        !separate(a,sizeof(*a),c->key_id,c->key_id_bytes) ||
        !separate(a,sizeof(*a),c->policy.image.keys,c->policy.image.key_count*sizeof(*c->policy.image.keys))) return false;
    a->policy=c->policy; a->candidate=(tv4_boot_release_candidate){0}; a->handoff=s; a->busy=true;
    bool ok=tv4_handoff_begin_mode(s,&base,rx,true);
    a->policy.image=s->policy; /* includes copied local keys, never caller storage */
    tv4_release_handoff_refresh(a); a->busy=false; return ok;
}
bool tv4_boot_release_handoff_verify(tv4_boot_release_handoff *a, tv4_boot_workspace *w)
{
    if (!attached(a) || busy(a) || w==NULL || !separate(a,sizeof(*a),w,sizeof(*w))) return false;
    a->busy=true;
    bool ok=tv4_handoff_verify_mode(a->handoff,w,&a->policy,&a->candidate);
    tv4_release_handoff_refresh(a); a->busy=false; return ok;
}
bool tv4_boot_release_handoff_poll(tv4_boot_release_handoff *a)
{
    if (!attached(a) || busy(a)) return false;
    a->busy=true; bool ok=tv4_boot_handoff_poll(a->handoff); tv4_release_handoff_refresh(a); a->busy=false; return ok;
}
bool tv4_boot_release_handoff_peek(tv4_boot_release_handoff *a, tv4_boot_release_handoff_receipt *out)
{
    if (!attached(a) || busy(a) || out==NULL || !separate(a,sizeof(*a),out,sizeof(*out)) ||
        !separate(a->handoff,sizeof(*a->handoff),out,sizeof(*out)) ||
        (a->handoff->rx!=NULL && !separate(a->handoff->rx,sizeof(*a->handoff->rx),out,sizeof(*out)))) return false;
    *out=(tv4_boot_release_handoff_receipt){0}; a->busy=true;
    bool ok=tv4_handoff_peek_mode(a->handoff,&out->source,true);
    if (ok) out->release=a->candidate;
    tv4_release_handoff_refresh(a); a->busy=false; return ok;
}
tv4_boot_image_result tv4_boot_release_handoff_preflight(tv4_boot_release_handoff *a,
    const tv4_boot_image_profile *p, tv4_boot_workspace *w, tv4_boot_handoff_image_report *out)
{
    if (busy(a)) return TV4_IMAGE_BUSY;
    if (!attached(a) || p==NULL || w==NULL || out==NULL ||
        !separate(a,sizeof(*a),p,sizeof(*p)) || !separate(a,sizeof(*a),w,sizeof(*w)) ||
        !separate(a,sizeof(*a),out,sizeof(*out))) return TV4_IMAGE_ARGUMENT;
    a->busy=true;
    tv4_boot_image_result result=tv4_handoff_preflight_mode(a->handoff,p,w,out,true);
    tv4_release_handoff_refresh(a); a->busy=false; return result;
}
bool tv4_boot_release_handoff_close(tv4_boot_release_handoff *a)
{
    if (!attached(a) || busy(a)) return false;
    a->busy=true; bool ok=tv4_boot_handoff_close(a->handoff); tv4_release_handoff_refresh(a); a->busy=false; return ok;
}
void tv4_boot_release_handoff_abort(tv4_boot_release_handoff *a)
{
    if (!attached(a)) return;
    if (busy(a)) { tv4_boot_handoff_abort(a->handoff); return; } /* only latches while core is busy */
    a->busy=true; tv4_boot_handoff_abort(a->handoff); tv4_release_handoff_refresh(a); a->busy=false;
}
bool tv4_boot_release_handoff_clear(tv4_boot_release_handoff *a)
{
    if (a==NULL || busy(a)) return false;
    if (a->handoff==NULL) { *a=(tv4_boot_release_handoff){0}; return true; }
    if (!attached(a) || !tv4_handoff_clear_mode(a->handoff,true)) return false;
    *a=(tv4_boot_release_handoff){0}; return true;
}
