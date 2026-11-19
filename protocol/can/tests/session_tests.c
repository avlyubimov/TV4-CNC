#include "update_session.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static tv4_update_limits limits(void)
{
    return (tv4_update_limits){.max_image_bytes = 1024, .block_bytes = 16,
        .ack_timeout_ms = 10, .progress_timeout_ms = 100, .total_timeout_ms = 500,
        .retry_limit = 3};
}
static tv4_update_descriptor descriptor(uint32_t size)
{
    return (tv4_update_descriptor){.id = {7, 11}, .image_bytes = size,
        .image_sha256 = {0x12, 0x34}};
}
static void setup(tv4_update_rx *rx, tv4_update_tx *tx, uint32_t size, uint32_t now)
{
    tv4_update_descriptor d = descriptor(size);
    assert(tv4_update_rx_init(rx, limits(), d.id.boot, now));
    assert(tv4_update_tx_init(tx, limits(), d.id.boot, now));
    assert(tv4_update_rx_begin(rx, &d, now) == TV4_UPDATE_OK);
    assert(tv4_update_tx_begin(tx, &d, now) == TV4_UPDATE_OK);
}
static tv4_update_block load(tv4_update_tx *tx, uint32_t now)
{
    uint8_t bytes[TV4_UPDATE_BLOCK_MAX];
    for (size_t i = 0; i < sizeof(bytes); ++i) bytes[i] = (uint8_t)i;
    uint32_t remaining = tx->context.descriptor.image_bytes - tx->context.next_offset;
    size_t length = remaining < tx->context.limits.block_bytes ? remaining : tx->context.limits.block_bytes;
    assert(tv4_update_tx_load(tx, bytes, length, now) == TV4_UPDATE_OK);
    memset(bytes, 0xee, sizeof(bytes)); /* source lifetime ends after the copy */
    tv4_update_block block;
    assert(tv4_update_tx_next(tx, &block, now) == TV4_UPDATE_OK);
    assert(block.bytes[0] == 0 && block.bytes[length - 1] == (uint8_t)(length - 1));
    for (size_t i = length; i < sizeof(block.bytes); ++i) assert(block.bytes[i] == 0);
    return block;
}
static tv4_update_ack consume(tv4_update_rx *rx, const tv4_update_block *block, uint32_t now)
{
    tv4_update_ack ack = {0};
    tv4_update_block pending;
    assert(tv4_update_rx_receive(rx, block, &ack, now) == TV4_UPDATE_PENDING);
    assert(tv4_update_rx_pending(rx, &pending, now) == TV4_UPDATE_PENDING);
    assert(pending.length == block->length && pending.crc32c == block->crc32c);
    assert(memcmp(pending.bytes, block->bytes, block->length) == 0);
    assert(tv4_update_rx_consume(rx, block->id, block->sequence, true, &ack, now) == TV4_UPDATE_ACK);
    return ack;
}
static void checksum_vectors(void)
{
    uint32_t crc = 123;
    const uint8_t digits[] = "123456789";
    uint8_t zeros[32] = {0};
    assert(tv4_update_crc32c(digits, 9, &crc) && crc == UINT32_C(0xe3069283));
    assert(tv4_update_crc32c(zeros, sizeof(zeros), &crc) && crc == UINT32_C(0x8a9136aa));
    assert(tv4_update_crc32c(NULL, 0, &crc) && crc == 0);
    crc = 123;
    assert(!tv4_update_crc32c(NULL, 1, &crc) && crc == 123);
    assert(!tv4_update_crc32c(digits, SIZE_MAX, &crc) && crc == 123);
    assert(!tv4_update_crc32c(digits, 9, NULL));
    assert(!tv4_update_block_checksum(NULL, &crc));
    tv4_update_block block = {.id = {7, 11}, .length = 3, .bytes = {1, 2, 3}};
    assert(tv4_update_block_checksum(&block, &crc));
    tv4_update_block changed = block;
    changed.bytes[255] = 99;
    uint32_t other;
    assert(tv4_update_block_checksum(&changed, &other) && other == crc);
    changed.id.boot++;
    assert(tv4_update_block_checksum(&changed, &other) && other != crc);
    changed = block; changed.id.transfer++;
    assert(tv4_update_block_checksum(&changed, &other) && other != crc);
    changed = block; changed.sequence++;
    assert(tv4_update_block_checksum(&changed, &other) && other != crc);
    changed = block; changed.offset++;
    assert(tv4_update_block_checksum(&changed, &other) && other != crc);
    changed = block; changed.length--;
    assert(tv4_update_block_checksum(&changed, &other) && other != crc);
    changed = block; changed.bytes[0]++;
    assert(tv4_update_block_checksum(&changed, &other) && other != crc);
    changed.length = 0;
    assert(!tv4_update_block_checksum(&changed, &other));
    changed.length = UINT16_MAX;
    assert(!tv4_update_block_checksum(&changed, &other));
}
static void invalid_limits(void)
{
    tv4_update_limits configs[10];
    for (size_t i = 0; i < 10; ++i) configs[i] = limits();
    configs[0].max_image_bytes = 0;
    configs[1].block_bytes = 0;
    configs[2].block_bytes = 257;
    configs[3].retry_limit = 0;
    configs[4].ack_timeout_ms = 0;
    configs[5].ack_timeout_ms = 100;
    configs[6].progress_timeout_ms = 501;
    configs[7].total_timeout_ms = UINT32_MAX;
    configs[8].progress_timeout_ms = 0;
    configs[9].total_timeout_ms = 0;
    for (size_t i = 0; i < 10; ++i) {
        tv4_update_rx rx; tv4_update_tx tx;
        assert(!tv4_update_rx_init(&rx, configs[i], 7, 0));
        assert(!tv4_update_tx_init(&tx, configs[i], 7, 0));
        assert(rx.context.error == TV4_UPDATE_ERROR_CONFIG && tx.context.error == TV4_UPDATE_ERROR_CONFIG);
        assert(tv4_update_rx_poll(&rx, 1) == TV4_UPDATE_ERROR);
        assert(tv4_update_tx_poll(&tx, 1) == TV4_UPDATE_ERROR);
    }
    tv4_update_rx rx;
    assert(!tv4_update_rx_init(&rx, limits(), 0, 0));
    assert(!tv4_update_rx_init(NULL, limits(), 7, 0));
    assert(!tv4_update_tx_init(NULL, limits(), 7, 0));
    tv4_update_limits max = limits();
    max.max_image_bytes = UINT32_MAX; max.block_bytes = 256;
    max.retry_limit = UINT8_MAX; max.total_timeout_ms = INT32_MAX;
    assert(tv4_update_rx_init(&rx, max, UINT32_MAX, 0));
}
static void begin_and_exclusivity(void)
{
    tv4_update_rx rx; tv4_update_tx tx;
    assert(tv4_update_rx_init(&rx, limits(), 7, 0));
    assert(tv4_update_tx_init(&tx, limits(), 7, 0));
    tv4_update_descriptor d = descriptor(32);
    assert(tv4_update_rx_begin(&rx, NULL, 0) == TV4_UPDATE_REJECT_ARGUMENT);
    assert(tv4_update_tx_begin(&tx, NULL, 0) == TV4_UPDATE_REJECT_ARGUMENT);
    d.id.boot = 6;
    assert(tv4_update_rx_begin(&rx, &d, 0) == TV4_UPDATE_REJECT_SESSION);
    assert(tv4_update_tx_begin(&tx, &d, 0) == TV4_UPDATE_REJECT_SESSION);
    d = descriptor(0);
    assert(tv4_update_rx_begin(&rx, &d, 0) == TV4_UPDATE_REJECT_SIZE);
    d = descriptor(1025);
    assert(tv4_update_tx_begin(&tx, &d, 0) == TV4_UPDATE_REJECT_SIZE);
    d = descriptor(32); d.id.transfer = 0;
    assert(tv4_update_rx_begin(&rx, &d, 0) == TV4_UPDATE_REJECT_SESSION);
    d.id.transfer = 11;
    assert(tv4_update_rx_begin(&rx, &d, 0) == TV4_UPDATE_OK);
    assert(tv4_update_tx_begin(&tx, &d, 0) == TV4_UPDATE_OK);
    d.id.transfer++;
    assert(tv4_update_rx_begin(&rx, &d, 1) == TV4_UPDATE_REJECT_STATE);
    assert(tv4_update_tx_begin(&tx, &d, 1) == TV4_UPDATE_REJECT_STATE);
    assert(rx.context.descriptor.id.transfer == 11 && rx.context.started_ms == 0);
}
static void two_phase_and_exact_duplicates(void)
{
    tv4_update_rx rx; tv4_update_tx tx;
    setup(&rx, &tx, 31, 0);
    tv4_update_block block = load(&tx, 0), pending;
    tv4_update_ack ack = {.next_offset = 0xabcdef};
    assert(tv4_update_rx_receive(&rx, &block, &ack, 1) == TV4_UPDATE_PENDING);
    assert(ack.next_offset == 0xabcdef && rx.context.next_offset == 0);
    assert(tv4_update_rx_receive(&rx, &block, &ack, 2) == TV4_UPDATE_PENDING);
    assert(ack.next_offset == 0xabcdef && rx.context.progress_ms == 0);
    assert(tv4_update_rx_pending(&rx, &pending, 2) == TV4_UPDATE_PENDING);
    assert(tv4_update_rx_consume(&rx, (tv4_update_id){7, 12}, 0, true, &ack, 2) == TV4_UPDATE_REJECT_SESSION);
    assert(tv4_update_rx_consume(&rx, block.id, 1, true, &ack, 2) == TV4_UPDATE_REJECT_ORDER);
    assert(tv4_update_rx_consume(&rx, block.id, 0, true, &ack, 3) == TV4_UPDATE_ACK);
    assert(ack.next_offset == 16 && ack.next_sequence == 1);
    assert(rx.context.retries == 0 && rx.context.progress_ms == 3);
    assert(tv4_update_rx_consume(&rx, block.id, 0, true, &ack, 3) == TV4_UPDATE_REJECT_STATE);
    assert(tv4_update_rx_receive(&rx, &block, &ack, 4) == TV4_UPDATE_ACK);
    assert(tv4_update_rx_pending(&rx, &pending, 4) == TV4_UPDATE_REJECT_STATE);
    assert(rx.context.next_offset == 16 && rx.context.progress_ms == 3);
    assert(tv4_update_tx_ack(&tx, &ack, 4) == TV4_UPDATE_OK);
    block = load(&tx, 5);
    assert(block.length == 15 && block.offset == 16 && block.sequence == 1);
    ack = consume(&rx, &block, 5);
    assert(tv4_update_tx_ack(&tx, &ack, 5) == TV4_UPDATE_OK);
    assert(rx.context.state == TV4_UPDATE_COMPLETE_UNVERIFIED);
    assert(tx.context.state == TV4_UPDATE_COMPLETE_UNVERIFIED);
    assert(tv4_update_rx_receive(&rx, &block, &ack, 6) == TV4_UPDATE_ACK);
    assert(tv4_update_tx_ack(&tx, &ack, 6) == TV4_UPDATE_STALE_ACK);
    assert(tv4_update_tx_load(&tx, block.bytes, 15, 6) == TV4_UPDATE_REJECT_STATE);
}
static void malformed_blocks(void)
{
    for (unsigned fault = 0; fault < 7; ++fault) {
        tv4_update_rx rx; tv4_update_tx tx;
        setup(&rx, &tx, 32, 0);
        tv4_update_block valid = load(&tx, 0), bad = valid;
        tv4_update_ack ack = {.next_offset = 0xabcdef};
        tv4_update_result expected;
        switch (fault) {
        case 0: bad.length = 0; expected = TV4_UPDATE_REJECT_SIZE; break;
        case 1: bad.length = UINT16_MAX; expected = TV4_UPDATE_REJECT_SIZE; break;
        case 2: bad.bytes[0] ^= 1; expected = TV4_UPDATE_REJECT_CHECKSUM; break;
        case 3: bad.sequence = UINT32_MAX; expected = TV4_UPDATE_REJECT_ORDER; break;
        case 4: bad.offset = UINT32_MAX; expected = TV4_UPDATE_REJECT_ORDER; break;
        case 5: bad.length = 15; expected = TV4_UPDATE_REJECT_SIZE; break;
        default: bad.id.transfer++; expected = TV4_UPDATE_REJECT_SESSION; break;
        }
        if (fault >= 3 && fault <= 5) assert(tv4_update_block_checksum(&bad, &bad.crc32c));
        assert(tv4_update_rx_receive(&rx, &bad, &ack, 1) == expected);
        assert(ack.next_offset == 0xabcdef && rx.context.next_offset == 0);
        assert(rx.context.state == TV4_UPDATE_RECEIVING && !rx.retained);
        ack = consume(&rx, &valid, 2);
        assert(ack.next_offset == 16);
    }
}
static void conflicting_duplicate(void)
{
    tv4_update_rx rx; tv4_update_tx tx;
    setup(&rx, &tx, 32, 0);
    tv4_update_block block = load(&tx, 0);
    tv4_update_ack ack = consume(&rx, &block, 0);
    block.bytes[0] ^= 1;
    assert(tv4_update_block_checksum(&block, &block.crc32c));
    assert(tv4_update_rx_receive(&rx, &block, &ack, 1) == TV4_UPDATE_REJECT_CONTENT);
    assert(rx.context.next_offset == 16 && rx.context.state == TV4_UPDATE_RECEIVING);
}
static void crc_collision_does_not_match_duplicate(void)
{
    tv4_update_rx rx; tv4_update_tx tx;
    setup(&rx, &tx, 32, 0);
    tv4_update_block block = load(&tx, 0);
    tv4_update_ack ack = consume(&rx, &block, 0);
    uint32_t original_crc = block.crc32c;
    /* Same-length Castagnoli collision, generated offline by GF(2) elimination.
     * Header and CRC remain identical: only exact payload comparison catches it. */
    block.bytes[0] = 1;
    block.bytes[12] = 0x3c; block.bytes[13] = 0xd9;
    block.bytes[14] = 0x7f; block.bytes[15] = 0x3e;
    assert(tv4_update_block_checksum(&block, &block.crc32c));
    assert(block.crc32c == original_crc);
    assert(tv4_update_rx_receive(&rx, &block, &ack, 1) == TV4_UPDATE_REJECT_CONTENT);
    assert(rx.context.next_offset == 16 && rx.context.progress_ms == 0);
}
static void consumer_failure_is_sticky(void)
{
    tv4_update_rx rx; tv4_update_tx tx;
    setup(&rx, &tx, 16, 0);
    tv4_update_block block = load(&tx, 0), out;
    tv4_update_ack ack = {.next_offset = 123};
    assert(tv4_update_rx_receive(&rx, &block, &ack, 0) == TV4_UPDATE_PENDING);
    assert(tv4_update_rx_consume(&rx, block.id, 0, false, &ack, 1) == TV4_UPDATE_ERROR);
    assert(rx.context.error == TV4_UPDATE_ERROR_CONSUMER && ack.next_offset == 123);
    assert(tv4_update_rx_receive(&rx, &block, &ack, 2) == TV4_UPDATE_ERROR);
    assert(tv4_update_rx_consume(&rx, block.id, 0, true, &ack, 2) == TV4_UPDATE_ERROR);
    assert(tv4_update_rx_pending(&rx, &out, 2) == TV4_UPDATE_ERROR);
    assert(tv4_update_rx_abort(&rx, block.id, 2) == TV4_UPDATE_ERROR);
    tv4_update_descriptor d = descriptor(16);
    assert(tv4_update_rx_begin(&rx, &d, 2) == TV4_UPDATE_ERROR);
}
static void sender_retry_budget(void)
{
    tv4_update_rx rx; tv4_update_tx tx;
    setup(&rx, &tx, 16, 0);
    tv4_update_block first = load(&tx, 0), block = {.offset = 123};
    assert(tv4_update_tx_next(&tx, &block, 9) == TV4_UPDATE_WAIT && block.offset == 123);
    for (uint32_t retry = 1; retry <= 3; ++retry) {
        assert(tv4_update_tx_next(&tx, &block, retry * 10) == TV4_UPDATE_OK);
        assert(memcmp(block.bytes, first.bytes, first.length) == 0);
        assert(block.crc32c == first.crc32c && block.offset == first.offset && block.sequence == 0);
        assert(tx.context.progress_ms == 0);
    }
    block.offset = 123;
    assert(tv4_update_tx_next(&tx, &block, 40) == TV4_UPDATE_ERROR);
    assert(tx.context.error == TV4_UPDATE_ERROR_RETRIES && block.offset == 123);
    tv4_update_ack ack = {.id = first.id, .next_offset = 16, .next_sequence = 1};
    assert(tv4_update_tx_ack(&tx, &ack, 41) == TV4_UPDATE_ERROR);
}
static void receiver_retry_budget(void)
{
    tv4_update_rx rx; tv4_update_tx tx;
    setup(&rx, &tx, 16, 0);
    tv4_update_block block = load(&tx, 0);
    tv4_update_ack ack = consume(&rx, &block, 0);
    for (uint32_t retry = 1; retry <= 3; ++retry)
        assert(tv4_update_rx_receive(&rx, &block, &ack, retry) == TV4_UPDATE_ACK);
    assert(tv4_update_rx_receive(&rx, &block, &ack, 4) == TV4_UPDATE_ERROR);
    assert(rx.context.error == TV4_UPDATE_ERROR_RETRIES);
    assert(tv4_update_rx_receive(&rx, &block, &ack, 5) == TV4_UPDATE_ERROR);
    setup(&rx, &tx, 16, 0);
    block = load(&tx, 0); block.crc32c ^= 1;
    for (uint32_t retry = 0; retry < 3; ++retry)
        assert(tv4_update_rx_receive(&rx, &block, &ack, retry) == TV4_UPDATE_REJECT_CHECKSUM);
    assert(tv4_update_rx_receive(&rx, &block, &ack, 3) == TV4_UPDATE_ERROR);
}
static void ack_validation(void)
{
    tv4_update_rx rx; tv4_update_tx tx;
    setup(&rx, &tx, 32, 0);
    tv4_update_block block = load(&tx, 0);
    tv4_update_ack ack = consume(&rx, &block, 0), bad = ack;
    bad.id.boot++;
    assert(tv4_update_tx_ack(&tx, &bad, 1) == TV4_UPDATE_REJECT_SESSION);
    bad = ack; bad.next_offset = UINT32_MAX;
    assert(tv4_update_tx_ack(&tx, &bad, 1) == TV4_UPDATE_REJECT_ORDER);
    bad = ack; bad.next_sequence = UINT32_MAX;
    assert(tv4_update_tx_ack(&tx, &bad, 1) == TV4_UPDATE_REJECT_ORDER);
    bad = ack; bad.next_sequence = 0; bad.next_offset = 0;
    assert(tv4_update_tx_ack(&tx, &bad, 1) == TV4_UPDATE_STALE_ACK);
    assert(tx.context.next_offset == 0 && tx.context.progress_ms == 0);
    assert(tv4_update_tx_ack(&tx, &ack, 2) == TV4_UPDATE_OK);
    assert(tv4_update_tx_ack(&tx, &ack, 3) == TV4_UPDATE_STALE_ACK);
    assert(tv4_update_tx_load(&tx, block.bytes, 16, 4) == TV4_UPDATE_OK);
    assert(tv4_update_tx_ack(&tx, &ack, 5) == TV4_UPDATE_STALE_ACK);
    assert(tx.context.state == TV4_UPDATE_TX_READY && tx.context.progress_ms == 2);
    bad = ack; bad.next_offset = 32; bad.next_sequence = 2;
    assert(tv4_update_tx_ack(&tx, &bad, 5) == TV4_UPDATE_REJECT_ORDER); /* not sent */
}
static void deadline_boundaries(void)
{
    tv4_update_rx rx; tv4_update_tx tx;
    setup(&rx, &tx, 32, 0);
    tv4_update_block block = load(&tx, 0);
    tv4_update_ack ack;
    assert(tv4_update_rx_poll(&rx, 99) == TV4_UPDATE_OK);
    assert(tv4_update_rx_receive(&rx, &block, &ack, 100) == TV4_UPDATE_ERROR);
    assert(rx.context.error == TV4_UPDATE_ERROR_PROGRESS_TIMEOUT);
    assert(tv4_update_tx_ack(&tx, &(tv4_update_ack){block.id, 1, 16}, 100) == TV4_UPDATE_ERROR);
    assert(tx.context.error == TV4_UPDATE_ERROR_PROGRESS_TIMEOUT);
    setup(&rx, &tx, 32, 0);
    block = load(&tx, 0);
    assert(tv4_update_rx_receive(&rx, &block, &ack, 99) == TV4_UPDATE_PENDING);
    assert(tv4_update_rx_consume(&rx, block.id, 0, true, &ack, 100) == TV4_UPDATE_ERROR);
    setup(&rx, &tx, 32, 0);
    block = load(&tx, 0); ack = consume(&rx, &block, 1);
    assert(tv4_update_tx_ack(&tx, &ack, 1) == TV4_UPDATE_OK);
    assert(tv4_update_rx_receive(&rx, &block, &ack, 99) == TV4_UPDATE_ACK);
    assert(tv4_update_rx_poll(&rx, 101) == TV4_UPDATE_ERROR); /* duplicate is not progress */
    assert(tv4_update_tx_ack(&tx, &ack, 100) == TV4_UPDATE_STALE_ACK);
    assert(tv4_update_tx_poll(&tx, 101) == TV4_UPDATE_ERROR);
    setup(&rx, &tx, 16, 0);
    block = load(&tx, 0); ack = consume(&rx, &block, 1);
    assert(tv4_update_tx_ack(&tx, &ack, 1) == TV4_UPDATE_OK);
    assert(tv4_update_tx_poll(&tx, UINT32_MAX) == TV4_UPDATE_OK); /* terminal */
    assert(tv4_update_rx_receive(&rx, &block, &ack, 101) == TV4_UPDATE_ERROR); /* final ACK expires */
}
static void total_deadline(void)
{
    tv4_update_rx rx; tv4_update_tx tx;
    setup(&rx, &tx, 1024, 0);
    for (uint32_t now = 80; now <= 480; now += 80) {
        tv4_update_block block = load(&tx, now);
        tv4_update_ack ack = consume(&rx, &block, now);
        assert(tv4_update_tx_ack(&tx, &ack, now) == TV4_UPDATE_OK);
    }
    assert(tv4_update_rx_poll(&rx, 499) == TV4_UPDATE_OK);
    assert(tv4_update_tx_poll(&tx, 499) == TV4_UPDATE_OK);
    assert(tv4_update_rx_poll(&rx, 500) == TV4_UPDATE_ERROR);
    assert(tv4_update_tx_poll(&tx, 500) == TV4_UPDATE_ERROR);
    assert(rx.context.error == TV4_UPDATE_ERROR_TOTAL_TIMEOUT && tx.context.error == TV4_UPDATE_ERROR_TOTAL_TIMEOUT);
}
static void clocks_and_wraparound(void)
{
    tv4_update_rx rx; tv4_update_tx tx;
    setup(&rx, &tx, 16, UINT32_MAX - 4);
    tv4_update_block block = load(&tx, UINT32_MAX - 4);
    tv4_update_ack ack = consume(&rx, &block, 3);
    assert(tv4_update_tx_ack(&tx, &ack, 3) == TV4_UPDATE_OK);
    assert(rx.context.progress_ms == 3 && tx.context.progress_ms == 3);
    setup(&rx, &tx, 16, 100);
    assert(tv4_update_rx_poll(&rx, 99) == TV4_UPDATE_ERROR);
    assert(tv4_update_tx_poll(&tx, 99) == TV4_UPDATE_ERROR);
    assert(rx.context.error == TV4_UPDATE_ERROR_CLOCK && tx.context.error == TV4_UPDATE_ERROR_CLOCK);
    setup(&rx, &tx, 16, 0);
    assert(tv4_update_rx_poll(&rx, UINT32_C(0x80000000)) == TV4_UPDATE_ERROR);
    assert(tv4_update_tx_poll(&tx, UINT32_C(0x80000000)) == TV4_UPDATE_ERROR);
}
static void abort_and_reboot(void)
{
    tv4_update_rx rx; tv4_update_tx tx;
    setup(&rx, &tx, 32, 0);
    tv4_update_block block = load(&tx, 0), pending;
    tv4_update_ack ack;
    assert(tv4_update_rx_receive(&rx, &block, &ack, 0) == TV4_UPDATE_PENDING);
    assert(tv4_update_rx_abort(&rx, (tv4_update_id){7, 12}, 1) == TV4_UPDATE_REJECT_SESSION);
    assert(tv4_update_tx_abort(&tx, (tv4_update_id){7, 12}, 1) == TV4_UPDATE_REJECT_SESSION);
    assert(tv4_update_rx_abort(&rx, block.id, 2) == TV4_UPDATE_OK);
    assert(tv4_update_tx_abort(&tx, block.id, 2) == TV4_UPDATE_OK);
    assert(tv4_update_rx_pending(&rx, &pending, 3) == TV4_UPDATE_REJECT_STATE);
    assert(tv4_update_rx_consume(&rx, block.id, 0, true, &ack, 3) == TV4_UPDATE_REJECT_STATE);
    assert(tv4_update_tx_next(&tx, &pending, 3) == TV4_UPDATE_REJECT_STATE);
    tv4_update_descriptor d = descriptor(32);
    assert(tv4_update_rx_begin(&rx, &d, 3) == TV4_UPDATE_REJECT_STATE);
    assert(tv4_update_tx_begin(&tx, &d, 3) == TV4_UPDATE_REJECT_STATE);
    assert(tv4_update_rx_init(&rx, limits(), 8, 4));
    assert(tv4_update_tx_init(&tx, limits(), 8, 4));
    assert(tv4_update_rx_begin(&rx, &d, 4) == TV4_UPDATE_REJECT_SESSION);
    assert(tv4_update_tx_begin(&tx, &d, 4) == TV4_UPDATE_REJECT_SESSION);
    d.id.boot = 8;
    assert(tv4_update_rx_begin(&rx, &d, 4) == TV4_UPDATE_OK);
    assert(tv4_update_tx_begin(&tx, &d, 4) == TV4_UPDATE_OK);
    assert(tv4_update_rx_receive(&rx, &block, &ack, 5) == TV4_UPDATE_REJECT_SESSION);
    assert(tv4_update_rx_consume(&rx, block.id, 0, true, &ack, 5) == TV4_UPDATE_REJECT_SESSION);
    assert(tv4_update_tx_ack(&tx, &(tv4_update_ack){block.id, 1, 16}, 5) == TV4_UPDATE_REJECT_SESSION);
    assert(rx.context.next_offset == 0 && tx.context.next_offset == 0);
}
static void null_and_output_contracts(void)
{
    tv4_update_rx rx; tv4_update_tx tx;
    setup(&rx, &tx, 16, 0);
    tv4_update_block block = {.offset = 123};
    tv4_update_ack ack = {.next_offset = 123};
    assert(tv4_update_rx_receive(NULL, &block, &ack, 0) == TV4_UPDATE_REJECT_ARGUMENT);
    assert(tv4_update_rx_receive(&rx, NULL, &ack, 0) == TV4_UPDATE_REJECT_ARGUMENT);
    assert(tv4_update_rx_pending(&rx, NULL, 0) == TV4_UPDATE_REJECT_ARGUMENT);
    assert(tv4_update_rx_consume(&rx, descriptor(16).id, 0, true, NULL, 0) == TV4_UPDATE_REJECT_ARGUMENT);
    assert(tv4_update_tx_load(&tx, NULL, 16, 0) == TV4_UPDATE_REJECT_ARGUMENT);
    assert(tv4_update_tx_load(&tx, block.bytes, SIZE_MAX, 0) == TV4_UPDATE_REJECT_SIZE);
    assert(tv4_update_tx_load(&tx, block.bytes, 0, 0) == TV4_UPDATE_REJECT_SIZE);
    assert(tv4_update_tx_next(&tx, &block, 0) == TV4_UPDATE_WAIT && block.offset == 123);
    assert(tv4_update_tx_next(&tx, NULL, 0) == TV4_UPDATE_REJECT_ARGUMENT);
    assert(tv4_update_tx_ack(&tx, NULL, 0) == TV4_UPDATE_REJECT_ARGUMENT);
    assert(ack.next_offset == 123);
    assert(tv4_update_rx_begin(NULL, NULL, 0) == TV4_UPDATE_REJECT_ARGUMENT);
    assert(tv4_update_tx_begin(NULL, NULL, 0) == TV4_UPDATE_REJECT_ARGUMENT);
    assert(tv4_update_rx_poll(NULL, 0) == TV4_UPDATE_REJECT_ARGUMENT);
    assert(tv4_update_tx_poll(NULL, 0) == TV4_UPDATE_REJECT_ARGUMENT);
    assert(tv4_update_rx_pending(NULL, &block, 0) == TV4_UPDATE_REJECT_ARGUMENT);
    assert(tv4_update_rx_consume(NULL, (tv4_update_id){0}, 0, true, &ack, 0) == TV4_UPDATE_REJECT_ARGUMENT);
    assert(tv4_update_tx_load(NULL, block.bytes, 0, 0) == TV4_UPDATE_REJECT_ARGUMENT);
    assert(tv4_update_tx_next(NULL, &block, 0) == TV4_UPDATE_REJECT_ARGUMENT);
    assert(tv4_update_tx_ack(NULL, &ack, 0) == TV4_UPDATE_REJECT_ARGUMENT);
    assert(tv4_update_rx_abort(NULL, (tv4_update_id){0}, 0) == TV4_UPDATE_REJECT_ARGUMENT);
    assert(tv4_update_tx_abort(NULL, (tv4_update_id){0}, 0) == TV4_UPDATE_REJECT_ARGUMENT);
}
static void retry_counter_boundary(void)
{
    tv4_update_rx rx; tv4_update_tx tx;
    tv4_update_limits config = limits();
    config.retry_limit = UINT8_MAX; config.progress_timeout_ms = 10000; config.total_timeout_ms = 20000;
    tv4_update_descriptor d = descriptor(16);
    assert(tv4_update_rx_init(&rx, config, 7, 0) && tv4_update_tx_init(&tx, config, 7, 0));
    assert(tv4_update_rx_begin(&rx, &d, 0) == TV4_UPDATE_OK);
    assert(tv4_update_tx_begin(&tx, &d, 0) == TV4_UPDATE_OK);
    tv4_update_block block = load(&tx, 0), out;
    tv4_update_ack ack = consume(&rx, &block, 0);
    for (uint32_t i = 1; i <= UINT8_MAX; ++i) {
        assert(tv4_update_rx_receive(&rx, &block, &ack, i) == TV4_UPDATE_ACK);
        assert(tv4_update_tx_next(&tx, &out, i * 10) == TV4_UPDATE_OK);
    }
    assert(rx.context.retries == UINT8_MAX && tx.context.retries == UINT8_MAX);
    assert(tv4_update_rx_receive(&rx, &block, &ack, 256) == TV4_UPDATE_ERROR);
    assert(tv4_update_tx_next(&tx, &out, 2560) == TV4_UPDATE_ERROR);
    assert(rx.context.error == TV4_UPDATE_ERROR_RETRIES && tx.context.error == TV4_UPDATE_ERROR_RETRIES);
}

#define RUN(test) do { test(); puts("PASS " #test); } while (0)
int main(void)
{
    RUN(checksum_vectors);
    RUN(invalid_limits);
    RUN(begin_and_exclusivity);
    RUN(two_phase_and_exact_duplicates);
    RUN(malformed_blocks);
    RUN(conflicting_duplicate);
    RUN(crc_collision_does_not_match_duplicate);
    RUN(consumer_failure_is_sticky);
    RUN(sender_retry_budget);
    RUN(receiver_retry_budget);
    RUN(ack_validation);
    RUN(deadline_boundaries);
    RUN(total_deadline);
    RUN(clocks_and_wraparound);
    RUN(abort_and_reboot);
    RUN(null_and_output_contracts);
    RUN(retry_counter_boundary);
    printf("PASS 17 unit groups; rx=%zu tx=%zu bytes; no device access\n",
           sizeof(tv4_update_rx), sizeof(tv4_update_tx));
    return 0;
}
