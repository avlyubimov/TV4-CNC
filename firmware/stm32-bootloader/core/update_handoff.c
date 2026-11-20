#include "update_handoff.h"
#include <limits.h>
#include <string.h>

_Static_assert(sizeof(tv4_boot_handoff) <= 2304, "handoff state budget regression");

static bool separate(const void *a, size_t as, const void *b, size_t bs)
{
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    return a != NULL && b != NULL && as <= UINTPTR_MAX - x && bs <= UINTPTR_MAX - y &&
        (x + as <= y || y + bs <= x);
}
static bool same_id(tv4_update_id a, tv4_update_id b) { return a.boot == b.boot && a.transfer == b.transfer; }
static bool same_limits(tv4_update_limits a, tv4_update_limits b)
{
    return a.max_image_bytes == b.max_image_bytes && a.ack_timeout_ms == b.ack_timeout_ms &&
        a.progress_timeout_ms == b.progress_timeout_ms && a.total_timeout_ms == b.total_timeout_ms &&
        a.block_bytes == b.block_bytes && a.retry_limit == b.retry_limit;
}
static bool complete(const tv4_update_rx *rx)
{
    const tv4_update_context *c = &rx->context;
    if (!c->configured || c->state != TV4_UPDATE_COMPLETE_UNVERIFIED || c->error != TV4_UPDATE_ERROR_NONE ||
        c->descriptor.id.boot == 0 || c->descriptor.id.transfer == 0 || c->local_boot != c->descriptor.id.boot ||
        c->descriptor.image_bytes == 0 || c->limits.block_bytes == 0 || c->limits.block_bytes > TV4_UPDATE_BLOCK_MAX ||
        !rx->retained || c->next_offset != c->descriptor.image_bytes) return false;
    uint32_t sequence = (c->descriptor.image_bytes - 1u) / c->limits.block_bytes;
    uint32_t offset = sequence * c->limits.block_bytes;
    return c->next_sequence == sequence + 1u && rx->block.sequence == sequence && rx->block.offset == offset &&
        rx->block.length == c->descriptor.image_bytes - offset && same_id(rx->block.id, c->descriptor.id);
}
static void fail(tv4_boot_handoff *s, tv4_boot_handoff_error error)
{
    if (s->error == TV4_HANDOFF_ERROR_NONE) s->error = error;
    s->candidate = (tv4_boot_candidate){0};
}
static bool time_valid(tv4_boot_handoff *s)
{
    uint32_t now = s->ops.now_ms(s->ops.context);
    if ((uint32_t)(now - s->observed_ms) > INT32_MAX) fail(s, TV4_HANDOFF_CLOCK);
    s->observed_ms = now;
    if ((uint32_t)(now - s->started_ms) >= s->lifetime_ms) fail(s, TV4_HANDOFF_TIMEOUT);
    return s->error == TV4_HANDOFF_ERROR_NONE;
}
static bool view_valid(const tv4_boot_handoff *s, const tv4_boot_source_view *v)
{
    return v->token != 0 && v->object != 0 && v->generation != 0 &&
        same_id(v->id, s->descriptor.id) && v->image_bytes == s->descriptor.image_bytes;
}
static bool guard(tv4_boot_handoff *s, bool source)
{
    if (s->cancel_requested || s->ops.cancelled(s->ops.context)) fail(s, TV4_HANDOFF_CANCELLED);
    if (s->error != TV4_HANDOFF_ERROR_NONE || !time_valid(s)) return false;
    if (source) {
        tv4_boot_source_view v = {0};
        if (!s->ops.inspect(s->ops.context, s->source.token, &v) || !view_valid(s, &v) ||
            v.token != s->source.token || v.object != s->source.object || v.generation != s->source.generation)
            fail(s, TV4_HANDOFF_SOURCE);
    }
    tv4_boot_owner_stamp stamp = {0};
    if (!s->ops.owner_stamp(s->ops.context, &stamp) || stamp.boot != s->owner.boot ||
        stamp.policy_generation != s->owner.policy_generation) fail(s, TV4_HANDOFF_OWNER);
    if (s->cancel_requested || s->ops.cancelled(s->ops.context)) fail(s, TV4_HANDOFF_CANCELLED);
    if (!time_valid(s)) return false;
    tv4_update_context *c = &s->rx->context;
    if (tv4_update_rx_poll(s->rx, s->observed_ms) != TV4_UPDATE_OK || !complete(s->rx) ||
        !same_id(c->descriptor.id, s->descriptor.id) || c->descriptor.image_bytes != s->descriptor.image_bytes ||
        memcmp(c->descriptor.image_sha256, s->descriptor.image_sha256, TV4_UPDATE_HASH_BYTES) != 0 ||
        !same_limits(c->limits, s->rx_limits) || c->started_ms != s->rx_started_ms || c->progress_ms != s->rx_progress_ms)
        fail(s, TV4_HANDOFF_RECEIVER);
    return s->error == TV4_HANDOFF_ERROR_NONE;
}
static void release_once(tv4_boot_handoff *s)
{
    s->candidate = (tv4_boot_candidate){0};
    memset(s->metadata, 0, sizeof(s->metadata));
    memset(s->signature, 0, sizeof(s->signature));
    memset(s->keys, 0, sizeof(s->keys));
    memset(s->key_id, 0, sizeof(s->key_id));
    if (!s->held) { s->state = TV4_HANDOFF_CLOSED; s->rx = NULL; return; }
    if (s->source.token == 0 || s->release_attempts >= s->release_attempt_limit) {
        s->state = TV4_HANDOFF_QUARANTINED;
        return;
    }
    s->state = TV4_HANDOFF_RELEASING;
    ++s->release_attempts;
    if (s->ops.release(s->ops.context, s->source.token)) {
        s->held = false;
        s->state = TV4_HANDOFF_CLOSED;
        s->rx = NULL;
    } else {
        fail(s, TV4_HANDOFF_RELEASE);
        if (s->release_attempts == s->release_attempt_limit) s->state = TV4_HANDOFF_QUARANTINED;
    }
    if (s->cancel_requested) fail(s, TV4_HANDOFF_CANCELLED);
}
static bool config_valid(const tv4_boot_handoff *s, const tv4_boot_handoff_config *c, const tv4_update_rx *rx)
{
    if (c == NULL || rx == NULL || !separate(s, sizeof(*s), c, sizeof(*c)) ||
        !separate(s, sizeof(*s), rx, sizeof(*rx)) || !complete(rx) ||
        c->metadata == NULL || c->metadata_bytes == 0 || c->metadata_bytes > TV4_BOOT_METADATA_MAX ||
        c->signature == NULL || c->signature_bytes < 8 || c->signature_bytes > TV4_BOOT_SIGNATURE_MAX ||
        c->key_id == NULL || c->key_id_bytes == 0 || c->key_id_bytes >= TV4_BOOT_ID_BYTES ||
        c->policy.keys == NULL || c->policy.key_count == 0 || c->policy.key_count > TV4_BOOT_KEYS_MAX ||
        c->arena_bytes == 0 || c->arena_bytes > TV4_BOOT_ARENA_BYTES ||
        c->lifetime_ms == 0 || c->lifetime_ms > INT32_MAX ||
        c->release_attempt_limit == 0 || c->release_attempt_limit > TV4_BOOT_RELEASE_ATTEMPTS_MAX ||
        c->ops.now_ms == NULL || c->ops.cancelled == NULL || c->ops.owner_stamp == NULL ||
        c->ops.acquire == NULL || c->ops.inspect == NULL || c->ops.read == NULL || c->ops.release == NULL) return false;
    return separate(s, sizeof(*s), c->metadata, c->metadata_bytes) &&
        separate(s, sizeof(*s), c->signature, c->signature_bytes) && separate(s, sizeof(*s), c->key_id, c->key_id_bytes) &&
        separate(s, sizeof(*s), c->policy.keys, c->policy.key_count * sizeof(*c->policy.keys));
}
bool tv4_boot_handoff_begin(tv4_boot_handoff *s, const tv4_boot_handoff_config *c, tv4_update_rx *rx)
{
    if (s == NULL || s->busy || s->held || s->state != TV4_HANDOFF_EMPTY || !config_valid(s, c, rx)) return false;
    s->policy = c->policy;
    memcpy(s->keys, c->policy.keys, c->policy.key_count * sizeof(*s->keys));
    s->policy.keys = s->keys;
    s->ops = c->ops;
    s->metadata_bytes = c->metadata_bytes; s->signature_bytes = c->signature_bytes; s->key_id_bytes = c->key_id_bytes;
    s->arena_bytes = c->arena_bytes; s->lifetime_ms = c->lifetime_ms; s->release_attempt_limit = c->release_attempt_limit;
    memcpy(s->metadata, c->metadata, s->metadata_bytes); memcpy(s->signature, c->signature, s->signature_bytes);
    memcpy(s->key_id, c->key_id, s->key_id_bytes);
    s->rx = rx; s->descriptor = rx->context.descriptor; s->rx_limits = rx->context.limits;
    s->rx_started_ms = rx->context.started_ms; s->rx_progress_ms = rx->context.progress_ms;
    s->busy = true; s->state = TV4_HANDOFF_CLAIMING;
    s->started_ms = s->ops.now_ms(s->ops.context); s->observed_ms = s->started_ms;
    if (!s->ops.owner_stamp(s->ops.context, &s->owner) || s->owner.boot != s->descriptor.id.boot ||
        s->owner.policy_generation == 0) fail(s, TV4_HANDOFF_OWNER);
    if (guard(s, false)) {
        tv4_boot_source_view v = {0};
        if (!s->ops.acquire(s->ops.context, &s->descriptor, &v)) fail(s, TV4_HANDOFF_SOURCE);
        else { s->source = v; s->held = true; if (!view_valid(s, &v)) fail(s, TV4_HANDOFF_SOURCE); }
    }
    if (s->error == TV4_HANDOFF_ERROR_NONE && guard(s, true)) s->state = TV4_HANDOFF_BOUND;
    else release_once(s);
    s->busy = false;
    return s->state == TV4_HANDOFF_BOUND;
}
static uint32_t admission_now(void *context) { return ((tv4_boot_handoff *)context)->observed_ms; }
static bool admission_cancelled(void *context) { return !guard(context, true); }
static bool admission_read(void *context, uint32_t offset, uint8_t *bytes, size_t capacity, size_t *count, bool *eof)
{
    tv4_boot_handoff *s = context;
    if (!guard(s, true)) return false;
    bool ok = s->ops.read(s->ops.context, s->source.token, offset, bytes, capacity, count, eof);
    return guard(s, true) && ok;
}
bool tv4_boot_handoff_verify(tv4_boot_handoff *s, tv4_boot_workspace *w)
{
    if (s == NULL || s->busy || s->state != TV4_HANDOFF_BOUND || w == NULL ||
        !separate(s, sizeof(*s), w, sizeof(*w)) || !separate(s->rx, sizeof(*s->rx), w, sizeof(*w))) return false;
    s->busy = true;
    if (guard(s, true)) {
        s->state = TV4_HANDOFF_VERIFYING;
        tv4_boot_request request = {.context = s, .now_ms = admission_now, .cancelled = admission_cancelled,
            .read = admission_read, .metadata = s->metadata, .metadata_bytes = s->metadata_bytes,
            .signature = s->signature, .signature_bytes = s->signature_bytes, .key_id = s->key_id,
            .key_id_bytes = s->key_id_bytes, .arena_bytes = s->arena_bytes};
        s->admission_attempted = true;
        s->admission_result = tv4_boot_admit(&s->policy, &request, w, &s->candidate);
        if (s->admission_result != TV4_BOOT_VERIFIED_CANDIDATE) fail(s, TV4_HANDOFF_ADMISSION);
        if (guard(s, true) && (s->candidate.image_bytes != s->descriptor.image_bytes ||
            memcmp(s->candidate.image_sha256, s->descriptor.image_sha256, TV4_UPDATE_HASH_BYTES) != 0))
            fail(s, TV4_HANDOFF_BINDING);
    }
    if (s->error == TV4_HANDOFF_ERROR_NONE) s->state = TV4_HANDOFF_VERIFIED;
    else release_once(s);
    s->busy = false;
    return s->state == TV4_HANDOFF_VERIFIED;
}
bool tv4_boot_handoff_poll(tv4_boot_handoff *s)
{
    if (s == NULL || s->busy) return false;
    s->busy = true;
    bool ok = false;
    if (s->state == TV4_HANDOFF_BOUND || s->state == TV4_HANDOFF_VERIFIED) {
        ok = guard(s, true);
        if (!ok) release_once(s);
    } else if (s->state == TV4_HANDOFF_RELEASING) release_once(s);
    s->busy = false;
    return ok;
}
bool tv4_boot_handoff_peek(tv4_boot_handoff *s, tv4_boot_handoff_receipt *out)
{
    if (s == NULL || s->busy || out == NULL || !separate(s, sizeof(*s), out, sizeof(*out)) ||
        (s->rx != NULL && !separate(s->rx, sizeof(*s->rx), out, sizeof(*out)))) return false;
    *out = (tv4_boot_handoff_receipt){0};
    if (!tv4_boot_handoff_poll(s) || s->state != TV4_HANDOFF_VERIFIED) return false;
    *out = (tv4_boot_handoff_receipt){.image = s->candidate, .id = s->descriptor.id,
        .policy_generation = s->owner.policy_generation, .source_object = s->source.object,
        .source_generation = s->source.generation};
    return true;
}
bool tv4_boot_handoff_close(tv4_boot_handoff *s)
{
    if (s == NULL || s->busy || s->state == TV4_HANDOFF_EMPTY || s->state == TV4_HANDOFF_QUARANTINED) return false;
    if (s->state == TV4_HANDOFF_CLOSED) return true;
    s->busy = true; release_once(s); s->busy = false;
    return s->state == TV4_HANDOFF_CLOSED;
}
void tv4_boot_handoff_abort(tv4_boot_handoff *s)
{
    if (s == NULL || s->state == TV4_HANDOFF_EMPTY || s->state == TV4_HANDOFF_CLOSED || s->state == TV4_HANDOFF_QUARANTINED) return;
    s->cancel_requested = true;
    if (s->busy) return;
    s->busy = true; fail(s, TV4_HANDOFF_CANCELLED); release_once(s); s->busy = false;
}
bool tv4_boot_handoff_clear(tv4_boot_handoff *s)
{
    if (s == NULL || s->busy || s->held || (s->state != TV4_HANDOFF_EMPTY && s->state != TV4_HANDOFF_CLOSED)) return false;
    *s = (tv4_boot_handoff){0};
    return true;
}
