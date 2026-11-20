#include "update_outcome.h"
#include <limits.h>
#include <string.h>

_Static_assert(sizeof(tv4_outcome_tx) <= 256, "outcome TX storage regression");
_Static_assert(sizeof(tv4_outcome_rx) <= 384, "outcome RX storage regression");
static bool same_id(tv4_update_id a, tv4_update_id b) { return a.boot == b.boot && a.transfer == b.transfer; }
static bool same_image(const tv4_update_descriptor *a, const tv4_update_descriptor *b)
{
    return same_id(a->id,b->id) && a->image_bytes == b->image_bytes &&
        memcmp(a->image_sha256,b->image_sha256,32) == 0;
}
bool tv4_update_binding_equal(const tv4_update_binding *a, const tv4_update_binding *b)
{
    return a != NULL && b != NULL && same_image(&a->image,&b->image) &&
        memcmp(a->metadata_sha256,b->metadata_sha256,32) == 0;
}
bool tv4_update_outcome_valid(const tv4_update_outcome *r)
{
    if (r == NULL || r->binding.image.id.boot == 0 || r->binding.image.id.transfer == 0 ||
        r->binding.image.image_bytes == 0) return false;
    switch (r->code) {
    case TV4_OUTCOME_CHECKED_CLOSED: return r->reason == TV4_OUTCOME_REASON_NONE;
    case TV4_OUTCOME_REJECTED_CLOSED: return r->reason >= TV4_OUTCOME_SIGNATURE && r->reason <= TV4_OUTCOME_VECTORS;
    case TV4_OUTCOME_CANCELLED_CLOSED: return r->reason == TV4_OUTCOME_CANCEL;
    case TV4_OUTCOME_FAILED_CLOSED: return r->reason >= TV4_OUTCOME_SOURCE && r->reason <= TV4_OUTCOME_INTERNAL && r->reason != TV4_OUTCOME_CANCEL;
    case TV4_OUTCOME_QUARANTINED: return r->reason >= TV4_OUTCOME_SIGNATURE && r->reason <= TV4_OUTCOME_INTERNAL;
    default: return false;
    }
}
bool tv4_update_outcome_equal(const tv4_update_outcome *a, const tv4_update_outcome *b)
{
    return a != NULL && b != NULL && tv4_update_binding_equal(&a->binding,&b->binding) && a->code == b->code && a->reason == b->reason;
}
static tv4_outcome_result fail(tv4_outcome_context *c, tv4_outcome_error error)
{
    c->state = TV4_OUTCOME_FAILED; c->error = error; return TV4_OUTCOME_ERROR;
}
static bool init(tv4_outcome_context *c, tv4_outcome_limits limits, uint32_t now)
{
    *c = (tv4_outcome_context){.limits=limits,.observed_ms=now};
    if (limits.retry_limit == 0 || limits.ack_timeout_ms == 0 || limits.ack_timeout_ms >= limits.linger_ms ||
        limits.linger_ms > limits.total_timeout_ms || limits.total_timeout_ms > INT32_MAX) {
        (void)fail(c,TV4_OUTCOME_ERROR_CONFIG); return false;
    }
    c->configured = true; return true;
}
static tv4_outcome_result poll(tv4_outcome_context *c, uint32_t now)
{
    if (!c->configured || c->state == TV4_OUTCOME_FAILED) return TV4_OUTCOME_ERROR;
    if (c->state == TV4_OUTCOME_ABORTED) return TV4_OUTCOME_REJECT_STATE;
    if (c->state == TV4_OUTCOME_ACKED || c->state == TV4_OUTCOME_FINISHED) return TV4_OUTCOME_DONE;
    if ((uint32_t)(now-c->observed_ms) > INT32_MAX) return fail(c,TV4_OUTCOME_ERROR_CLOCK);
    c->observed_ms = now;
    if (c->state == TV4_OUTCOME_IDLE) return TV4_OUTCOME_OK;
    if ((uint32_t)(now-c->started_ms) >= c->limits.total_timeout_ms) return fail(c,TV4_OUTCOME_ERROR_TIMEOUT);
    if (c->state == TV4_OUTCOME_LINGER && (uint32_t)(now-c->last_ms) >= c->limits.linger_ms) {
        c->state = TV4_OUTCOME_FINISHED; return TV4_OUTCOME_DONE;
    }
    return TV4_OUTCOME_OK;
}
static tv4_outcome_result repeat(tv4_outcome_context *c, tv4_outcome_result r)
{
    if (c->retries == c->limits.retry_limit) return fail(c,TV4_OUTCOME_ERROR_RETRIES);
    ++c->retries; return r;
}
bool tv4_outcome_tx_init(tv4_outcome_tx *tx, tv4_outcome_limits limits, uint32_t boot, uint32_t now)
{
    if (tx == NULL) return false;
    *tx = (tv4_outcome_tx){.producer_boot=boot};
    if (!init(&tx->context,limits,now)) return false;
    if (boot == 0) { tx->context.configured = false; (void)fail(&tx->context,TV4_OUTCOME_ERROR_CONFIG); return false; }
    return true;
}
bool tv4_outcome_rx_init(tv4_outcome_rx *rx, tv4_outcome_limits limits, uint32_t now)
{
    if (rx == NULL) return false;
    *rx = (tv4_outcome_rx){0}; return init(&rx->context,limits,now);
}
tv4_outcome_result tv4_outcome_tx_poll(tv4_outcome_tx *tx, uint32_t now)
{
    return tx == NULL ? TV4_OUTCOME_REJECT_ARGUMENT : poll(&tx->context,now);
}
static bool completed(const tv4_update_tx *t)
{
    if (t == NULL) return false;
    const tv4_update_context *c = &t->context;
    return c->configured && c->error == TV4_UPDATE_ERROR_NONE && c->state == TV4_UPDATE_COMPLETE_UNVERIFIED &&
        c->descriptor.id.boot != 0 && c->local_boot == c->descriptor.id.boot && c->descriptor.id.transfer != 0 &&
        c->descriptor.image_bytes != 0 && c->limits.block_bytes != 0 && c->limits.block_bytes <= TV4_UPDATE_BLOCK_MAX &&
        c->next_offset == c->descriptor.image_bytes && c->next_sequence == (c->descriptor.image_bytes-1u)/c->limits.block_bytes+1u;
}
tv4_outcome_result tv4_outcome_rx_poll(tv4_outcome_rx *rx, uint32_t now)
{
    if (rx == NULL) return TV4_OUTCOME_REJECT_ARGUMENT;
    if ((rx->context.state == TV4_OUTCOME_WAIT_REPORT || rx->context.state == TV4_OUTCOME_LINGER) &&
        (!completed(rx->transfer) || !same_image(&rx->transfer->context.descriptor,&rx->expected.image) ||
        rx->transfer->context.started_ms != rx->transfer_started_ms || rx->transfer->context.progress_ms != rx->transfer_progress_ms ||
        rx->transfer->context.next_sequence != rx->transfer_sequence)) return fail(&rx->context,TV4_OUTCOME_ERROR_TRANSPORT);
    return poll(&rx->context,now);
}
tv4_outcome_result tv4_outcome_tx_begin(tv4_outcome_tx *tx, const tv4_update_outcome *report, uint32_t now)
{
    if (tx == NULL) return TV4_OUTCOME_REJECT_ARGUMENT;
    tv4_outcome_result r = poll(&tx->context,now);
    if (r != TV4_OUTCOME_OK) return r;
    if (tx->context.state != TV4_OUTCOME_IDLE) return TV4_OUTCOME_REJECT_STATE;
    if (!tv4_update_outcome_valid(report)) return TV4_OUTCOME_REJECT_ARGUMENT;
    if (report->binding.image.id.boot != tx->producer_boot) return TV4_OUTCOME_REJECT_BINDING;
    tx->report = *report; tx->context.started_ms = now; tx->context.state = TV4_OUTCOME_TX_READY;
    return TV4_OUTCOME_OK;
}
tv4_outcome_result tv4_outcome_rx_begin(tv4_outcome_rx *rx, tv4_update_tx *transfer, const uint8_t metadata[32], uint32_t now)
{
    if (rx == NULL || metadata == NULL) return TV4_OUTCOME_REJECT_ARGUMENT;
    tv4_outcome_result r = poll(&rx->context,now);
    if (r != TV4_OUTCOME_OK) return r;
    if (rx->context.state != TV4_OUTCOME_IDLE || !completed(transfer)) return TV4_OUTCOME_REJECT_STATE;
    if ((uint32_t)(now-transfer->context.observed_ms) > INT32_MAX) return fail(&rx->context,TV4_OUTCOME_ERROR_CLOCK);
    rx->expected.image = transfer->context.descriptor; memcpy(rx->expected.metadata_sha256,metadata,32);
    rx->transfer = transfer; rx->transfer_started_ms = transfer->context.started_ms;
    rx->transfer_progress_ms = transfer->context.progress_ms; rx->transfer_sequence = transfer->context.next_sequence;
    rx->context.started_ms = transfer->context.progress_ms; rx->context.state = TV4_OUTCOME_WAIT_REPORT;
    return tv4_outcome_rx_poll(rx,now);
}
tv4_outcome_result tv4_outcome_tx_next(tv4_outcome_tx *tx, tv4_update_outcome *out, uint32_t now)
{
    if (tx == NULL || out == NULL) return TV4_OUTCOME_REJECT_ARGUMENT;
    tv4_outcome_result r = poll(&tx->context,now);
    if (r != TV4_OUTCOME_OK) return r;
    if (tx->context.state == TV4_OUTCOME_WAIT_ACK) {
        if ((uint32_t)(now-tx->context.last_ms) < tx->context.limits.ack_timeout_ms) return TV4_OUTCOME_WAIT;
        r = repeat(&tx->context,TV4_OUTCOME_OK); if (r != TV4_OUTCOME_OK) return r;
    } else if (tx->context.state != TV4_OUTCOME_TX_READY) return TV4_OUTCOME_REJECT_STATE;
    *out = tx->report; tx->context.last_ms = now; tx->context.state = TV4_OUTCOME_WAIT_ACK;
    return TV4_OUTCOME_SENT;
}
tv4_outcome_result tv4_outcome_rx_receive(tv4_outcome_rx *rx, const tv4_update_outcome *report,
    tv4_update_outcome *ack, tv4_update_outcome *delivery, uint32_t now)
{
    if (rx == NULL || report == NULL || ack == NULL || delivery == NULL) return TV4_OUTCOME_REJECT_ARGUMENT;
    tv4_outcome_result r = tv4_outcome_rx_poll(rx,now); if (r != TV4_OUTCOME_OK) return r;
    if (rx->context.state != TV4_OUTCOME_WAIT_REPORT && rx->context.state != TV4_OUTCOME_LINGER) return TV4_OUTCOME_REJECT_STATE;
    if (!tv4_update_binding_equal(&report->binding,&rx->expected)) return TV4_OUTCOME_REJECT_BINDING;
    if (!tv4_update_outcome_valid(report)) return repeat(&rx->context,TV4_OUTCOME_REJECT_CONTENT);
    if (rx->context.state == TV4_OUTCOME_LINGER) {
        if (!tv4_update_outcome_equal(report,&rx->report)) return fail(&rx->context,TV4_OUTCOME_ERROR_CONFLICT);
        r = repeat(&rx->context,TV4_OUTCOME_ACK); if (r != TV4_OUTCOME_ACK) return r;
        *ack = rx->report; return r;
    }
    rx->report = *report; *ack = *report; *delivery = *report;
    rx->context.state = TV4_OUTCOME_LINGER; rx->context.last_ms = now; rx->context.retries = 0;
    return TV4_OUTCOME_DELIVERED;
}
tv4_outcome_result tv4_outcome_tx_ack(tv4_outcome_tx *tx, const tv4_update_outcome *ack, uint32_t now)
{
    if (tx == NULL || ack == NULL) return TV4_OUTCOME_REJECT_ARGUMENT;
    tv4_outcome_result r = poll(&tx->context,now);
    if (r != TV4_OUTCOME_OK && r != TV4_OUTCOME_DONE) return r;
    if (tx->context.state != TV4_OUTCOME_WAIT_ACK && tx->context.state != TV4_OUTCOME_ACKED) return TV4_OUTCOME_REJECT_STATE;
    if (!tv4_update_binding_equal(&ack->binding,&tx->report.binding)) return TV4_OUTCOME_REJECT_BINDING;
    if (!tv4_update_outcome_equal(ack,&tx->report))
        return tx->context.state == TV4_OUTCOME_ACKED ? TV4_OUTCOME_REJECT_CONTENT : repeat(&tx->context,TV4_OUTCOME_REJECT_CONTENT);
    if (tx->context.state == TV4_OUTCOME_ACKED) return TV4_OUTCOME_DUPLICATE;
    tx->context.state = TV4_OUTCOME_ACKED; return TV4_OUTCOME_OK;
}
tv4_outcome_result tv4_outcome_tx_abort(tv4_outcome_tx *tx, tv4_update_id id, uint32_t now)
{
    if (tx == NULL) return TV4_OUTCOME_REJECT_ARGUMENT;
    tv4_outcome_result r = poll(&tx->context,now); if (r != TV4_OUTCOME_OK && r != TV4_OUTCOME_DONE) return r;
    if (tx->context.state == TV4_OUTCOME_IDLE) return TV4_OUTCOME_REJECT_STATE;
    if (!same_id(id,tx->report.binding.image.id)) return TV4_OUTCOME_REJECT_BINDING;
    tx->context.state = TV4_OUTCOME_ABORTED; return TV4_OUTCOME_OK;
}
tv4_outcome_result tv4_outcome_rx_abort(tv4_outcome_rx *rx, tv4_update_id id, uint32_t now)
{
    if (rx == NULL) return TV4_OUTCOME_REJECT_ARGUMENT;
    tv4_outcome_result r = tv4_outcome_rx_poll(rx,now); if (r != TV4_OUTCOME_OK && r != TV4_OUTCOME_DONE) return r;
    if (rx->context.state == TV4_OUTCOME_IDLE) return TV4_OUTCOME_REJECT_STATE;
    if (!same_id(id,rx->expected.image.id)) return TV4_OUTCOME_REJECT_BINDING;
    rx->context.state = TV4_OUTCOME_ABORTED; return TV4_OUTCOME_OK;
}
