#include "update_session.h"
#include <assert.h>
#include <openssl/evp.h>
#include <stdio.h>
#include <string.h>

/* Host-only RAM/source/SHA oracle. Never linked into either MCU firmware. */
typedef struct {
    uint8_t before[16], bytes[1024], after[16];
    uint32_t cursor, deliveries;
} memory_sink;
typedef struct {
    unsigned transfers, dropped_data, dropped_ack, corrupt, reorder, pending_repeat, final_ack_loss;
} counts;

static uint32_t random_word(uint32_t *seed)
{
    *seed = *seed * UINT32_C(1664525) + UINT32_C(1013904223);
    return *seed;
}
static void sha256(const uint8_t *bytes, size_t length, uint8_t digest[TV4_UPDATE_HASH_BYTES])
{
    size_t digest_length = 0;
    assert(EVP_Q_digest(NULL, "SHA256", NULL, bytes, length, digest, &digest_length) == 1);
    assert(digest_length == TV4_UPDATE_HASH_BYTES);
}
static tv4_update_limits limits(uint16_t block)
{
    return (tv4_update_limits){.max_image_bytes = 1024, .block_bytes = block,
        .ack_timeout_ms = 10, .progress_timeout_ms = 100, .total_timeout_ms = 100000,
        .retry_limit = 5};
}
static void sink_init(memory_sink *sink)
{
    memset(sink, 0xa5, sizeof(*sink));
    sink->cursor = 0;
    sink->deliveries = 0;
}
static tv4_update_ack sink_consume(tv4_update_rx *rx, memory_sink *sink, uint32_t now)
{
    tv4_update_block block;
    tv4_update_ack ack;
    assert(tv4_update_rx_pending(rx, &block, now) == TV4_UPDATE_PENDING);
    assert(block.offset == sink->cursor && block.sequence == sink->deliveries);
    assert(block.length <= sizeof(sink->bytes) - sink->cursor);
    memcpy(sink->bytes + sink->cursor, block.bytes, block.length);
    sink->cursor += block.length;
    ++sink->deliveries;
    assert(tv4_update_rx_consume(rx, block.id, block.sequence, true, &ack, now) == TV4_UPDATE_ACK);
    assert(ack.next_offset == sink->cursor && ack.next_sequence == sink->deliveries);
    assert(tv4_update_rx_pending(rx, &block, now) == TV4_UPDATE_REJECT_STATE);
    return ack;
}
static void assert_guards(const memory_sink *sink, uint32_t size)
{
    for (size_t i = 0; i < sizeof(sink->before); ++i) assert(sink->before[i] == 0xa5);
    for (size_t i = 0; i < sizeof(sink->after); ++i) assert(sink->after[i] == 0xa5);
    for (size_t i = size; i < sizeof(sink->bytes); ++i) assert(sink->bytes[i] == 0xa5);
}
static void transfer(uint32_t size, uint16_t block_bytes, uint32_t seed, counts *totals)
{
    uint8_t source[1024];
    uint32_t state = seed;
    for (uint32_t i = 0; i < size; ++i) source[i] = (uint8_t)(random_word(&state) >> 24);
    tv4_update_descriptor d = {.id = {seed + 1, seed + 17}, .image_bytes = size};
    sha256(source, size, d.image_sha256);
    /* Some sessions cross uint32 wrap. This is simulated time only. */
    uint32_t now = seed % 2 == 0 ? UINT32_MAX - 20 : 0;
    tv4_update_rx rx; tv4_update_tx tx;
    assert(tv4_update_rx_init(&rx, limits(block_bytes), d.id.boot, now));
    assert(tv4_update_tx_init(&tx, limits(block_bytes), d.id.boot, now));
    assert(tv4_update_rx_begin(&rx, &d, now) == TV4_UPDATE_OK);
    assert(tv4_update_tx_begin(&tx, &d, now) == TV4_UPDATE_OK);
    memory_sink sink;
    sink_init(&sink);
    uint32_t offset = 0, sequence = 0;
    tv4_update_ack previous = {.id = d.id};
    while (offset < size) {
        uint32_t remaining = size - offset;
        uint16_t length = remaining < block_bytes ? (uint16_t)remaining : block_bytes;
        assert(tx.context.state == TV4_UPDATE_NEED_DATA);
        assert(tv4_update_tx_load(&tx, source + offset, length, now) == TV4_UPDATE_OK);
        assert(tv4_update_tx_ack(&tx, &previous, now) == TV4_UPDATE_STALE_ACK);
        tv4_update_block block, original;
        assert(tv4_update_tx_next(&tx, &original, now) == TV4_UPDATE_OK);
        assert(original.offset == offset && original.sequence == sequence && original.length == length);
        assert(memcmp(original.bytes, source + offset, length) == 0);
        block = original;
        tv4_update_ack ack = {.next_offset = UINT32_MAX};
        unsigned fault = random_word(&state) % 6;
        if (fault == 0) {
            ++totals->dropped_data;
            now += 10;
            assert(tv4_update_tx_next(&tx, &block, now) == TV4_UPDATE_OK);
            assert(block.crc32c == original.crc32c && memcmp(block.bytes, original.bytes, length) == 0);
        } else if (fault == 1) {
            ++totals->corrupt;
            block.bytes[length / 2] ^= 0x80;
            assert(tv4_update_rx_receive(&rx, &block, &ack, now) == TV4_UPDATE_REJECT_CHECKSUM);
            assert(ack.next_offset == UINT32_MAX && sink.cursor == offset);
            now += 10;
            assert(tv4_update_tx_next(&tx, &block, now) == TV4_UPDATE_OK);
        } else if (fault == 2) {
            ++totals->reorder;
            ++block.sequence;
            assert(tv4_update_block_checksum(&block, &block.crc32c));
            assert(tv4_update_rx_receive(&rx, &block, &ack, now) == TV4_UPDATE_REJECT_ORDER);
            assert(sink.cursor == offset);
            block = original;
        }
        assert(tv4_update_rx_receive(&rx, &block, &ack, now) == TV4_UPDATE_PENDING);
        assert(ack.next_offset == UINT32_MAX);
        if (fault == 3) {
            ++totals->pending_repeat;
            assert(tv4_update_rx_receive(&rx, &block, &ack, now) == TV4_UPDATE_PENDING);
            assert(ack.next_offset == UINT32_MAX && sink.cursor == offset);
        }
        ack = sink_consume(&rx, &sink, now);
        if (fault == 4) {
            ++totals->dropped_ack;
            if (offset + length == size) ++totals->final_ack_loss;
            now += 10;
            assert(tv4_update_tx_next(&tx, &block, now) == TV4_UPDATE_OK);
            assert(tv4_update_rx_receive(&rx, &block, &ack, now) == TV4_UPDATE_ACK);
            tv4_update_block pending;
            assert(tv4_update_rx_pending(&rx, &pending, now) == TV4_UPDATE_REJECT_STATE);
            assert(sink.deliveries == sequence + 1);
        }
        assert(tv4_update_tx_ack(&tx, &ack, now) == TV4_UPDATE_OK);
        previous = ack;
        offset += length;
        ++sequence;
        assert(tx.context.next_offset == offset && rx.context.next_offset == offset);
        assert(sink.cursor == offset && sink.deliveries == sequence);
        assert_guards(&sink, size);
        ++now;
        assert(tv4_update_rx_poll(&rx, now) == TV4_UPDATE_OK);
    }
    assert(sink.deliveries == (size - 1) / block_bytes + 1);
    assert(tx.context.state == TV4_UPDATE_COMPLETE_UNVERIFIED);
    assert(rx.context.state == TV4_UPDATE_COMPLETE_UNVERIFIED);
    assert(memcmp(sink.bytes, source, size) == 0);
    uint8_t digest[TV4_UPDATE_HASH_BYTES];
    sha256(sink.bytes, size, digest);
    assert(memcmp(digest, d.image_sha256, sizeof(digest)) == 0);
    ++totals->transfers;
}
static void changed_image_is_never_verified(void)
{
    uint8_t source[32] = {1, 2, 3};
    tv4_update_descriptor d = {.id = {7, 11}, .image_bytes = sizeof(source)};
    sha256(source, sizeof(source), d.image_sha256);
    tv4_update_rx rx; tv4_update_tx tx;
    assert(tv4_update_rx_init(&rx, limits(32), 7, 0));
    assert(tv4_update_tx_init(&tx, limits(32), 7, 0));
    assert(tv4_update_rx_begin(&rx, &d, 0) == TV4_UPDATE_OK);
    assert(tv4_update_tx_begin(&tx, &d, 0) == TV4_UPDATE_OK);
    assert(tv4_update_tx_load(&tx, source, sizeof(source), 0) == TV4_UPDATE_OK);
    tv4_update_block block;
    assert(tv4_update_tx_next(&tx, &block, 0) == TV4_UPDATE_OK);
    block.bytes[3] ^= 1; /* an attacker can recompute CRC; it is not trust */
    assert(tv4_update_block_checksum(&block, &block.crc32c));
    tv4_update_ack ack;
    assert(tv4_update_rx_receive(&rx, &block, &ack, 1) == TV4_UPDATE_PENDING);
    memory_sink sink;
    sink_init(&sink);
    ack = sink_consume(&rx, &sink, 1);
    assert(tv4_update_tx_ack(&tx, &ack, 1) == TV4_UPDATE_OK);
    uint8_t received_hash[TV4_UPDATE_HASH_BYTES];
    sha256(sink.bytes, sizeof(source), received_hash);
    assert(memcmp(received_hash, rx.context.descriptor.image_sha256, sizeof(received_hash)) != 0);
    assert(rx.context.state == TV4_UPDATE_COMPLETE_UNVERIFIED);
    assert(tx.context.state == TV4_UPDATE_COMPLETE_UNVERIFIED);
    puts("PASS altered payload + valid CRC completes transport only; SHA oracle rejects image");
}
static void resets_at_every_block_boundary(void)
{
    uint8_t source[256] = {0};
    for (unsigned cut = 0; cut <= 4; ++cut) {
        for (unsigned pending_cut = 0; pending_cut < 2; ++pending_cut) {
            tv4_update_descriptor d = {.id = {7, 11}, .image_bytes = 256};
            tv4_update_rx rx; tv4_update_tx tx;
            assert(tv4_update_rx_init(&rx, limits(64), 7, 0));
            assert(tv4_update_tx_init(&tx, limits(64), 7, 0));
            assert(tv4_update_rx_begin(&rx, &d, 0) == TV4_UPDATE_OK);
            assert(tv4_update_tx_begin(&tx, &d, 0) == TV4_UPDATE_OK);
            memory_sink sink;
            sink_init(&sink);
            tv4_update_ack ack = {.id = d.id};
            tv4_update_block block = {.id = d.id, .length = 64};
            for (unsigned i = 0; i < cut; ++i) {
                assert(tv4_update_tx_load(&tx, source + 64 * i, 64, i) == TV4_UPDATE_OK);
                assert(tv4_update_tx_next(&tx, &block, i) == TV4_UPDATE_OK);
                assert(tv4_update_rx_receive(&rx, &block, &ack, i) == TV4_UPDATE_PENDING);
                if (pending_cut && i + 1 == cut) break;
                ack = sink_consume(&rx, &sink, i);
                assert(tv4_update_tx_ack(&tx, &ack, i) == TV4_UPDATE_OK);
            }
            /* Simulate volatile reset only, with externally supplied new boot
             * identity. This is not a physical power-loss/recovery test. */
            assert(tv4_update_rx_init(&rx, limits(64), 8, 5));
            assert(tv4_update_tx_init(&tx, limits(64), 8, 5));
            assert(rx.context.state == TV4_UPDATE_IDLE && tx.context.state == TV4_UPDATE_IDLE);
            assert(tv4_update_rx_receive(&rx, &block, &ack, 5) == TV4_UPDATE_REJECT_STATE);
            assert(tv4_update_rx_begin(&rx, &d, 5) == TV4_UPDATE_REJECT_SESSION);
            d.id.boot = 8;
            assert(tv4_update_rx_begin(&rx, &d, 5) == TV4_UPDATE_OK);
            assert(tv4_update_tx_begin(&tx, &d, 5) == TV4_UPDATE_OK);
            assert(tv4_update_rx_receive(&rx, &block, &ack, 6) == TV4_UPDATE_REJECT_SESSION);
            ack.id.boot = 7;
            assert(tv4_update_tx_ack(&tx, &ack, 6) == TV4_UPDATE_REJECT_SESSION);
            assert(rx.context.next_offset == 0 && tx.context.next_offset == 0);
        }
    }
    puts("PASS 10 volatile reset cut cases; stale sessions/ACK rejected; no resume");
}
static void truncated_stream_and_partial_consumer(void)
{
    tv4_update_descriptor d = {.id = {7, 11}, .image_bytes = 32};
    uint8_t source[16] = {0};
    for (unsigned consumer_failure = 0; consumer_failure < 2; ++consumer_failure) {
        tv4_update_rx rx; tv4_update_tx tx;
        assert(tv4_update_rx_init(&rx, limits(16), 7, 0));
        assert(tv4_update_tx_init(&tx, limits(16), 7, 0));
        assert(tv4_update_rx_begin(&rx, &d, 0) == TV4_UPDATE_OK);
        assert(tv4_update_tx_begin(&tx, &d, 0) == TV4_UPDATE_OK);
        assert(tv4_update_tx_load(&tx, source, 16, 0) == TV4_UPDATE_OK);
        tv4_update_block block;
        tv4_update_ack ack = {.next_offset = 123};
        assert(tv4_update_tx_next(&tx, &block, 0) == TV4_UPDATE_OK);
        assert(tv4_update_rx_receive(&rx, &block, &ack, 0) == TV4_UPDATE_PENDING);
        memory_sink sink;
        sink_init(&sink);
        if (consumer_failure) {
            memcpy(sink.bytes, block.bytes, 3); /* partially changed mock storage */
            assert(tv4_update_rx_consume(&rx, d.id, 0, false, &ack, 1) == TV4_UPDATE_ERROR);
            assert(ack.next_offset == 123 && rx.context.next_offset == 0);
            assert(rx.context.error == TV4_UPDATE_ERROR_CONSUMER);
        } else {
            ack = sink_consume(&rx, &sink, 1);
            assert(tv4_update_tx_ack(&tx, &ack, 1) == TV4_UPDATE_OK);
            assert(tv4_update_rx_poll(&rx, 101) == TV4_UPDATE_ERROR);
            assert(tv4_update_tx_poll(&tx, 101) == TV4_UPDATE_ERROR);
            assert(rx.context.error == TV4_UPDATE_ERROR_PROGRESS_TIMEOUT);
        }
        assert(rx.context.state == TV4_UPDATE_FAILED);
        assert(tv4_update_rx_receive(&rx, &block, &ack, 102) == TV4_UPDATE_ERROR);
    }
    puts("PASS truncated stream / partial RAM consumer never reach completion");
}
int main(void)
{
    counts totals = {0};
    for (uint32_t seed = 1; seed <= 2048; ++seed) {
        uint32_t size = 1 + (seed * 37) % 1024;
        uint16_t block = (uint16_t)(1 + (seed * 53) % TV4_UPDATE_BLOCK_MAX);
        transfer(size, block, seed, &totals);
    }
    /* A one-byte stream with the largest allowed block capacity. */
    transfer(1, 256, 2049, &totals);
    assert(totals.dropped_data > 0 && totals.dropped_ack > 0 && totals.corrupt > 0 &&
           totals.reorder > 0 && totals.pending_repeat > 0 && totals.final_ack_loss > 0);
    printf("PASS %u memory transfers; lost DATA=%u ACK=%u (final=%u) corrupt=%u reorder=%u pending duplicates=%u\n",
        totals.transfers, totals.dropped_data, totals.dropped_ack, totals.final_ack_loss,
        totals.corrupt, totals.reorder, totals.pending_repeat);
    changed_image_is_never_verified();
    resets_at_every_block_boundary();
    truncated_stream_and_partial_consumer();
    return 0;
}
