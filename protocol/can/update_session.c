#include "update_session.h"

#include <limits.h>
#include <string.h>

_Static_assert(sizeof(tv4_update_rx) <= 512, "receiver storage regression");
_Static_assert(sizeof(tv4_update_tx) <= 512, "sender storage regression");

static bool same_id(tv4_update_id a, tv4_update_id b)
{
    return a.boot == b.boot && a.transfer == b.transfer;
}

static tv4_update_result fail(tv4_update_context *c, tv4_update_error error)
{
    c->state = TV4_UPDATE_FAILED;
    c->error = error;
    return TV4_UPDATE_ERROR;
}

static bool init(tv4_update_context *c, tv4_update_limits limits,
                 uint32_t boot, uint32_t now)
{
    *c = (tv4_update_context){.limits = limits, .local_boot = boot,
                              .observed_ms = now};
    if (boot == 0 || limits.max_image_bytes == 0 || limits.block_bytes == 0 ||
        limits.block_bytes > TV4_UPDATE_BLOCK_MAX || limits.retry_limit == 0 ||
        limits.ack_timeout_ms == 0 ||
        limits.ack_timeout_ms >= limits.progress_timeout_ms ||
        limits.progress_timeout_ms > limits.total_timeout_ms ||
        limits.total_timeout_ms > INT32_MAX) {
        (void)fail(c, TV4_UPDATE_ERROR_CONFIG);
        return false;
    }
    c->configured = true;
    return true;
}

static tv4_update_result poll(tv4_update_context *c, uint32_t now, bool receiver)
{
    if (!c->configured || c->state == TV4_UPDATE_FAILED)
        return TV4_UPDATE_ERROR;
    if (c->state == TV4_UPDATE_ABORTED)
        return TV4_UPDATE_REJECT_STATE;
    /* TX completion is terminal. RX retains the final ACK for bounded retries
     * only; its deadlines still apply while that receipt remains available. */
    if (!receiver && c->state == TV4_UPDATE_COMPLETE_UNVERIFIED)
        return TV4_UPDATE_OK;
    if ((uint32_t)(now - c->observed_ms) > INT32_MAX)
        return fail(c, TV4_UPDATE_ERROR_CLOCK);
    c->observed_ms = now;
    if (c->state == TV4_UPDATE_IDLE)
        return TV4_UPDATE_OK;
    if ((uint32_t)(now - c->started_ms) >= c->limits.total_timeout_ms)
        return fail(c, TV4_UPDATE_ERROR_TOTAL_TIMEOUT);
    if ((uint32_t)(now - c->progress_ms) >= c->limits.progress_timeout_ms)
        return fail(c, TV4_UPDATE_ERROR_PROGRESS_TIMEOUT);
    return TV4_UPDATE_OK;
}

static tv4_update_result begin(tv4_update_context *c,
    const tv4_update_descriptor *descriptor, uint32_t now, bool receiver)
{
    tv4_update_result result = poll(c, now, receiver);
    if (result != TV4_UPDATE_OK) return result;
    if (c->state != TV4_UPDATE_IDLE) return TV4_UPDATE_REJECT_STATE;
    if (descriptor == NULL) return TV4_UPDATE_REJECT_ARGUMENT;
    if (descriptor->id.boot != c->local_boot || descriptor->id.transfer == 0)
        return TV4_UPDATE_REJECT_SESSION;
    if (descriptor->image_bytes == 0 || descriptor->image_bytes > c->limits.max_image_bytes)
        return TV4_UPDATE_REJECT_SIZE;
    c->descriptor = *descriptor;
    c->started_ms = now;
    c->progress_ms = now;
    c->state = receiver ? TV4_UPDATE_RECEIVING : TV4_UPDATE_NEED_DATA;
    return TV4_UPDATE_OK;
}

bool tv4_update_rx_init(tv4_update_rx *rx, tv4_update_limits limits,
                        uint32_t local_boot, uint32_t now_ms)
{
    if (rx == NULL) return false;
    *rx = (tv4_update_rx){0};
    return init(&rx->context, limits, local_boot, now_ms);
}

bool tv4_update_tx_init(tv4_update_tx *tx, tv4_update_limits limits,
                        uint32_t local_boot, uint32_t now_ms)
{
    if (tx == NULL) return false;
    *tx = (tv4_update_tx){0};
    return init(&tx->context, limits, local_boot, now_ms);
}

tv4_update_result tv4_update_rx_begin(tv4_update_rx *rx,
    const tv4_update_descriptor *descriptor, uint32_t now_ms)
{
    return rx == NULL ? TV4_UPDATE_REJECT_ARGUMENT : begin(&rx->context, descriptor, now_ms, true);
}

tv4_update_result tv4_update_tx_begin(tv4_update_tx *tx,
    const tv4_update_descriptor *descriptor, uint32_t now_ms)
{
    return tx == NULL ? TV4_UPDATE_REJECT_ARGUMENT : begin(&tx->context, descriptor, now_ms, false);
}

tv4_update_result tv4_update_rx_poll(tv4_update_rx *rx, uint32_t now_ms)
{
    return rx == NULL ? TV4_UPDATE_REJECT_ARGUMENT : poll(&rx->context, now_ms, true);
}

tv4_update_result tv4_update_tx_poll(tv4_update_tx *tx, uint32_t now_ms)
{
    return tx == NULL ? TV4_UPDATE_REJECT_ARGUMENT : poll(&tx->context, now_ms, false);
}

static uint32_t crc_byte(uint32_t crc, uint8_t byte)
{
    crc ^= byte;
    for (unsigned bit = 0; bit < 8; ++bit)
        crc = (crc >> 1) ^ ((0u - (crc & 1u)) & UINT32_C(0x82f63b78));
    return crc;
}

bool tv4_update_crc32c(const uint8_t *bytes, size_t length, uint32_t *checksum)
{
    if (checksum == NULL || length > TV4_UPDATE_BLOCK_MAX || (length != 0 && bytes == NULL))
        return false;
    uint32_t crc = UINT32_MAX;
    for (size_t i = 0; i < length; ++i) crc = crc_byte(crc, bytes[i]);
    *checksum = ~crc;
    return true;
}

static uint32_t crc_word(uint32_t crc, uint32_t word, unsigned bytes)
{
    for (unsigned i = 0; i < bytes; ++i) {
        crc = crc_byte(crc, (uint8_t)(word & 0xffu));
        word >>= 8;
    }
    return crc;
}

bool tv4_update_block_checksum(const tv4_update_block *block, uint32_t *checksum)
{
    if (block == NULL || checksum == NULL || block->length == 0 ||
        block->length > TV4_UPDATE_BLOCK_MAX) return false;
    uint32_t crc = crc_word(UINT32_MAX, block->id.boot, 4);
    crc = crc_word(crc, block->id.transfer, 4);
    crc = crc_word(crc, block->sequence, 4);
    crc = crc_word(crc, block->offset, 4);
    crc = crc_word(crc, block->length, 2);
    for (uint16_t i = 0; i < block->length; ++i) crc = crc_byte(crc, block->bytes[i]);
    *checksum = ~crc;
    return true;
}

static tv4_update_result repeat(tv4_update_context *c, tv4_update_result result)
{
    if (c->retries >= c->limits.retry_limit) return fail(c, TV4_UPDATE_ERROR_RETRIES);
    ++c->retries;
    return result;
}

static void ack_cursor(const tv4_update_context *c, tv4_update_ack *ack)
{
    *ack = (tv4_update_ack){.id = c->descriptor.id,
        .next_sequence = c->next_sequence, .next_offset = c->next_offset};
}

static uint16_t expected_length(const tv4_update_context *c)
{
    uint32_t remaining = c->descriptor.image_bytes - c->next_offset;
    return remaining < c->limits.block_bytes ? (uint16_t)remaining : c->limits.block_bytes;
}

static bool same_block(const tv4_update_block *a, const tv4_update_block *b)
{
    return same_id(a->id, b->id) && a->sequence == b->sequence &&
        a->offset == b->offset && a->length == b->length && a->crc32c == b->crc32c &&
        memcmp(a->bytes, b->bytes, a->length) == 0;
}

tv4_update_result tv4_update_rx_receive(tv4_update_rx *rx,
    const tv4_update_block *block, tv4_update_ack *ack, uint32_t now_ms)
{
    if (rx == NULL) return TV4_UPDATE_REJECT_ARGUMENT;
    tv4_update_context *c = &rx->context;
    tv4_update_result result = poll(c, now_ms, true);
    if (result != TV4_UPDATE_OK) return result;
    if (block == NULL || ack == NULL) return TV4_UPDATE_REJECT_ARGUMENT;
    if (c->state != TV4_UPDATE_RECEIVING && c->state != TV4_UPDATE_BLOCK_PENDING &&
        c->state != TV4_UPDATE_COMPLETE_UNVERIFIED) return TV4_UPDATE_REJECT_STATE;
    if (!same_id(block->id, c->descriptor.id)) return TV4_UPDATE_REJECT_SESSION;
    uint32_t crc;
    if (block->length == 0 || block->length > c->limits.block_bytes)
        return repeat(c, TV4_UPDATE_REJECT_SIZE);
    if (!tv4_update_block_checksum(block, &crc) || crc != block->crc32c)
        return repeat(c, TV4_UPDATE_REJECT_CHECKSUM);
    if (rx->retained && block->sequence == rx->block.sequence) {
        if (!same_block(block, &rx->block)) return repeat(c, TV4_UPDATE_REJECT_CONTENT);
        result = repeat(c, c->state == TV4_UPDATE_BLOCK_PENDING ? TV4_UPDATE_PENDING : TV4_UPDATE_ACK);
        if (result == TV4_UPDATE_ACK) ack_cursor(c, ack);
        return result;
    }
    if (c->state != TV4_UPDATE_RECEIVING || block->sequence != c->next_sequence ||
        block->offset != c->next_offset) return repeat(c, TV4_UPDATE_REJECT_ORDER);
    if (block->length != expected_length(c)) return repeat(c, TV4_UPDATE_REJECT_SIZE);
    memmove(rx->block.bytes, block->bytes, block->length);
    memset(rx->block.bytes + block->length, 0, TV4_UPDATE_BLOCK_MAX - block->length);
    rx->block.id = block->id;
    rx->block.sequence = block->sequence;
    rx->block.offset = block->offset;
    rx->block.length = block->length;
    rx->block.crc32c = block->crc32c;
    rx->retained = true;
    c->state = TV4_UPDATE_BLOCK_PENDING;
    return TV4_UPDATE_PENDING;
}

tv4_update_result tv4_update_rx_pending(tv4_update_rx *rx,
    tv4_update_block *out, uint32_t now_ms)
{
    if (rx == NULL) return TV4_UPDATE_REJECT_ARGUMENT;
    tv4_update_result result = poll(&rx->context, now_ms, true);
    if (result != TV4_UPDATE_OK) return result;
    if (out == NULL) return TV4_UPDATE_REJECT_ARGUMENT;
    if (rx->context.state != TV4_UPDATE_BLOCK_PENDING) return TV4_UPDATE_REJECT_STATE;
    *out = rx->block;
    return TV4_UPDATE_PENDING;
}

tv4_update_result tv4_update_rx_consume(tv4_update_rx *rx, tv4_update_id id,
    uint32_t sequence, bool consumer_ok, tv4_update_ack *ack, uint32_t now_ms)
{
    if (rx == NULL) return TV4_UPDATE_REJECT_ARGUMENT;
    tv4_update_context *c = &rx->context;
    tv4_update_result result = poll(c, now_ms, true);
    if (result != TV4_UPDATE_OK) return result;
    if (ack == NULL) return TV4_UPDATE_REJECT_ARGUMENT;
    if (!same_id(id, c->descriptor.id)) return TV4_UPDATE_REJECT_SESSION;
    if (c->state != TV4_UPDATE_BLOCK_PENDING) return TV4_UPDATE_REJECT_STATE;
    if (sequence != rx->block.sequence) return TV4_UPDATE_REJECT_ORDER;
    if (!consumer_ok) return fail(c, TV4_UPDATE_ERROR_CONSUMER);
    c->next_offset += rx->block.length;
    ++c->next_sequence;
    c->progress_ms = now_ms;
    c->retries = 0;
    c->state = c->next_offset == c->descriptor.image_bytes ?
        TV4_UPDATE_COMPLETE_UNVERIFIED : TV4_UPDATE_RECEIVING;
    ack_cursor(c, ack);
    return TV4_UPDATE_ACK;
}

tv4_update_result tv4_update_tx_load(tv4_update_tx *tx,
    const uint8_t *bytes, size_t length, uint32_t now_ms)
{
    if (tx == NULL) return TV4_UPDATE_REJECT_ARGUMENT;
    tv4_update_context *c = &tx->context;
    tv4_update_result result = poll(c, now_ms, false);
    if (result != TV4_UPDATE_OK) return result;
    if (c->state != TV4_UPDATE_NEED_DATA) return TV4_UPDATE_REJECT_STATE;
    if (bytes == NULL) return TV4_UPDATE_REJECT_ARGUMENT;
    if (length != expected_length(c)) return TV4_UPDATE_REJECT_SIZE;
    memmove(tx->block.bytes, bytes, length);
    memset(tx->block.bytes + length, 0, TV4_UPDATE_BLOCK_MAX - length);
    tx->block.id = c->descriptor.id;
    tx->block.sequence = c->next_sequence;
    tx->block.offset = c->next_offset;
    tx->block.length = (uint16_t)length;
    (void)tv4_update_block_checksum(&tx->block, &tx->block.crc32c);
    c->state = TV4_UPDATE_TX_READY;
    return TV4_UPDATE_OK;
}

tv4_update_result tv4_update_tx_next(tv4_update_tx *tx,
    tv4_update_block *out, uint32_t now_ms)
{
    if (tx == NULL) return TV4_UPDATE_REJECT_ARGUMENT;
    tv4_update_context *c = &tx->context;
    tv4_update_result result = poll(c, now_ms, false);
    if (result != TV4_UPDATE_OK) return result;
    if (out == NULL) return TV4_UPDATE_REJECT_ARGUMENT;
    if (c->state == TV4_UPDATE_WAIT_ACK) {
        if ((uint32_t)(now_ms - tx->sent_ms) < c->limits.ack_timeout_ms) return TV4_UPDATE_WAIT;
        result = repeat(c, TV4_UPDATE_OK);
        if (result != TV4_UPDATE_OK) return result;
    } else if (c->state != TV4_UPDATE_TX_READY) {
        return TV4_UPDATE_WAIT;
    }
    *out = tx->block;
    tx->sent_ms = now_ms;
    c->state = TV4_UPDATE_WAIT_ACK;
    return TV4_UPDATE_OK;
}

tv4_update_result tv4_update_tx_ack(tv4_update_tx *tx,
    const tv4_update_ack *ack, uint32_t now_ms)
{
    if (tx == NULL) return TV4_UPDATE_REJECT_ARGUMENT;
    tv4_update_context *c = &tx->context;
    tv4_update_result result = poll(c, now_ms, false);
    if (result != TV4_UPDATE_OK) return result;
    if (ack == NULL) return TV4_UPDATE_REJECT_ARGUMENT;
    if (c->state == TV4_UPDATE_IDLE) return TV4_UPDATE_REJECT_STATE;
    if (!same_id(ack->id, c->descriptor.id)) return TV4_UPDATE_REJECT_SESSION;
    if (ack->next_sequence == c->next_sequence && ack->next_offset == c->next_offset)
        return TV4_UPDATE_STALE_ACK;
    if (c->state != TV4_UPDATE_WAIT_ACK || ack->next_sequence != c->next_sequence + 1u ||
        ack->next_offset != c->next_offset + tx->block.length)
        return TV4_UPDATE_REJECT_ORDER;
    c->next_sequence = ack->next_sequence;
    c->next_offset = ack->next_offset;
    c->progress_ms = now_ms;
    c->retries = 0;
    c->state = c->next_offset == c->descriptor.image_bytes ?
        TV4_UPDATE_COMPLETE_UNVERIFIED : TV4_UPDATE_NEED_DATA;
    return TV4_UPDATE_OK;
}

static tv4_update_result abort_session(tv4_update_context *c, tv4_update_id id,
                                      uint32_t now_ms, bool receiver)
{
    tv4_update_result result = poll(c, now_ms, receiver);
    if (result != TV4_UPDATE_OK) return result;
    if (c->state == TV4_UPDATE_IDLE) return TV4_UPDATE_REJECT_STATE;
    if (!same_id(id, c->descriptor.id)) return TV4_UPDATE_REJECT_SESSION;
    c->state = TV4_UPDATE_ABORTED;
    return TV4_UPDATE_OK;
}

tv4_update_result tv4_update_rx_abort(tv4_update_rx *rx, tv4_update_id id, uint32_t now_ms)
{
    return rx == NULL ? TV4_UPDATE_REJECT_ARGUMENT : abort_session(&rx->context, id, now_ms, true);
}

tv4_update_result tv4_update_tx_abort(tv4_update_tx *tx, tv4_update_id id, uint32_t now_ms)
{
    return tx == NULL ? TV4_UPDATE_REJECT_ARGUMENT : abort_session(&tx->context, id, now_ms, false);
}
