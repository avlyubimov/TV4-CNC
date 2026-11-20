#include "update_handoff.h"
#include <assert.h>
#include <inttypes.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <stdio.h>
#include <string.h>

enum { IMAGE_MAX = 2049 };
typedef enum {
    NONE, ACQUIRE_FAIL, ACQUIRE_BAD_TOKEN, ACQUIRE_BAD_OBJECT, INSPECT_FAIL,
    READ_FAIL, SOURCE_OBJECT, SOURCE_GENERATION, SOURCE_SIZE, SOURCE_SESSION,
    OWNER_BOOT, OWNER_POLICY, OWNER_FAIL, RX_ABORT, RX_REINIT, RX_HASH,
    READ_TIMEOUT, READ_BACKWARD, REENTER, REENTER_ABORT, ACQUIRE_ABORT, EOF_ABORT,
    READ_OVERSIZE, READ_STALL, READ_SHORT, READ_EXTRA, ACQUIRE_TIMEOUT,
    ADMISSION_TIMEOUT, CLEANUP_OWNER, EOF_SOURCE, INSPECT_BAD_TOKEN
} fault;
typedef struct {
    tv4_boot_handoff session;
    tv4_boot_handoff_config config;
    tv4_boot_workspace workspace;
    tv4_boot_handoff_receipt receipt;
    tv4_boot_key key;
    tv4_update_rx rx;
    tv4_update_tx tx;
    tv4_update_descriptor descriptor;
    tv4_boot_owner_stamp owner;
    tv4_boot_source_view view;
    EVP_PKEY *signer;
    uint8_t source[IMAGE_MAX + 1], staging[IMAGE_MAX + 1], metadata[1024], signature[72];
    uint32_t now;
    size_t source_bytes, staged_bytes, fragment;
    unsigned acquired, inspected, reads, releases, release_failures, lost_final_ack, lost_data;
    bool locked, cancel, release_reentry;
    fault inject;
} fixture;

static bool zero(const void *p, size_t size)
{
    const uint8_t *bytes = p;
    for (size_t i = 0; i < size; ++i) if (bytes[i] != 0) return false;
    return true;
}
static void digest(const uint8_t *bytes, size_t length, uint8_t hash[32])
{
    size_t n = 0;
    assert(EVP_Q_digest(NULL, "SHA256", NULL, bytes, length, hash, &n) == 1 && n == 32);
}
static void sign_metadata(fixture *f)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    assert(ctx != NULL && EVP_DigestSignInit(ctx, NULL, EVP_sha256(), NULL, f->signer) == 1);
    size_t n = sizeof(f->signature);
    assert(EVP_DigestSign(ctx, f->signature, &n, f->metadata, f->config.metadata_bytes) == 1);
    f->config.signature_bytes = n;
    EVP_MD_CTX_free(ctx);
}
static void metadata(fixture *f, uint32_t bytes)
{
    uint8_t hash[32]; char hex[65];
    digest(f->source, bytes, hash);
    for (size_t i = 0; i < 32; ++i) (void)snprintf(hex + 2u * i, 3, "%02x", hash[i]);
    int n = snprintf((char *)f->metadata, sizeof(f->metadata),
        "{\"schema\":\"%s\",\"component\":\"motion\",\"hardware_id\":\"test-f407\","
        "\"hardware_revision\":\"synthetic\",\"memory_profile\":\"test-no-addresses\","
        "\"image_counter\":3,\"minimum_bootloader_counter\":5,\"protocol_min\":6,\"protocol_max\":8,"
        "\"image_size\":%" PRIu32 ",\"sha256\":\"%s\"}", TV4_BOOT_DRAFT_SCHEMA, bytes, hex);
    assert(n > 0 && (size_t)n < sizeof(f->metadata));
    f->config.metadata_bytes = (size_t)n;
    sign_metadata(f);
}
static uint32_t now_ms(void *context) { return ((fixture *)context)->now; }
static bool cancelled(void *context) { return ((fixture *)context)->cancel; }
static bool owner_stamp(void *context, tv4_boot_owner_stamp *stamp)
{
    fixture *f = context;
    if (f->inject == CLEANUP_OWNER && f->reads > 1 && zero(&f->workspace, sizeof(f->workspace)))
        ++f->owner.policy_generation;
    *stamp = f->owner;
    return f->inject != OWNER_FAIL;
}
static bool acquire(void *context, const tv4_update_descriptor *id, tv4_boot_source_view *view)
{
    fixture *f = context;
    assert(!f->locked && f->staged_bytes == id->image_bytes);
    assert(id->id.boot == f->view.id.boot && id->id.transfer == f->view.id.transfer);
    ++f->acquired;
    if (f->inject == ACQUIRE_FAIL) return false;
    f->locked = true;
    if (f->inject == ACQUIRE_BAD_TOKEN) f->view.token = 0;
    if (f->inject == ACQUIRE_BAD_OBJECT) f->view.object = 0;
    *view = f->view;
    if (f->inject == ACQUIRE_ABORT) tv4_boot_handoff_abort(&f->session);
    if (f->inject == ACQUIRE_TIMEOUT) f->now += f->config.lifetime_ms;
    return true;
}
static bool inspect(void *context, uint64_t token, tv4_boot_source_view *view)
{
    fixture *f = context;
    assert(f->locked && token == f->view.token);
    ++f->inspected;
    *view = f->view;
    if (f->inject == INSPECT_BAD_TOKEN) ++view->token;
    return f->inject != INSPECT_FAIL;
}
static bool release(void *context, uint64_t token)
{
    fixture *f = context;
    assert(f->locked && token == f->view.token && token != 0);
    ++f->releases;
    if (f->release_reentry) {
        assert(!tv4_boot_handoff_close(&f->session));
        assert(!tv4_boot_handoff_clear(&f->session));
        assert(!tv4_boot_handoff_poll(&f->session));
        assert(!tv4_boot_handoff_begin(&f->session, &f->config, &f->rx));
    }
    if (f->releases <= f->release_failures) return false;
    f->locked = false;
    return true;
}
static bool read_memory(void *context, uint64_t token, uint32_t offset,
                        uint8_t *bytes, size_t capacity, size_t *count, bool *eof)
{
    fixture *f = context;
    assert(f->locked && token == f->view.token && capacity > 0 && capacity <= 256);
    ++f->reads;
    if (f->reads == 1) {
        switch (f->inject) {
        case SOURCE_OBJECT: ++f->view.object; break;
        case SOURCE_GENERATION: ++f->view.generation; break;
        case SOURCE_SIZE: ++f->view.image_bytes; break;
        case SOURCE_SESSION: ++f->view.id.transfer; break;
        case OWNER_BOOT: ++f->owner.boot; break;
        case OWNER_POLICY: ++f->owner.policy_generation; break;
        case RX_ABORT: assert(tv4_update_rx_abort(&f->rx, f->descriptor.id, f->now) == TV4_UPDATE_OK); break;
        case RX_REINIT: assert(tv4_update_rx_init(&f->rx, f->rx.context.limits, f->owner.boot + 1u, f->now)); break;
        case RX_HASH: f->rx.context.descriptor.image_sha256[0] ^= 1; break;
        case READ_TIMEOUT: f->now += f->config.lifetime_ms; break;
        case ADMISSION_TIMEOUT: f->now += f->config.policy.timeout_ms; break;
        case READ_BACKWARD: --f->now; break;
        case REENTER_ABORT: tv4_boot_handoff_abort(&f->session); /* defer release until read returns */
            assert(f->locked && f->releases == 0); break;
        default: break;
        }
    }
    if (f->inject == REENTER || f->inject == REENTER_ABORT) {
        memset(&f->receipt, 0xa5, sizeof(f->receipt));
        tv4_boot_handoff_receipt before = f->receipt;
        assert(!tv4_boot_handoff_peek(&f->session, &f->receipt));
        assert(memcmp(&before, &f->receipt, sizeof(before)) == 0);
        assert(!tv4_boot_handoff_poll(&f->session) && !tv4_boot_handoff_clear(&f->session));
        assert(!tv4_boot_handoff_verify(&f->session, &f->workspace));
        assert(!tv4_boot_handoff_begin(&f->session, &f->config, &f->rx));
        assert(!tv4_boot_handoff_close(&f->session));
    }
    if (f->inject == READ_FAIL) return false;
    if (f->inject == READ_OVERSIZE) { *count = capacity + 1; return true; }
    if (f->inject == READ_STALL) { *count = 0; *eof = false; return true; }
    if (f->inject == READ_SHORT) { *count = 0; *eof = true; return true; }
    size_t available = f->staged_bytes;
    if (f->inject == READ_EXTRA) ++available;
    assert(offset <= available);
    size_t n = available - offset;
    if (n > capacity) n = capacity;
    if (n > f->fragment) n = f->fragment;
    memcpy(bytes, f->staging + offset, n);
    *count = n; *eof = offset + n == available;
    if (f->inject == EOF_SOURCE && *eof && n == 0) ++f->view.generation;
    if (f->inject == EOF_ABORT && *eof && n == 0) {
        tv4_boot_handoff_abort(&f->session);
        assert(f->locked && f->releases == 0);
    }
    return true;
}
static void setup(fixture *f, uint32_t bytes, uint16_t block)
{
    *f = (fixture){0};
    assert(bytes > 0 && bytes <= IMAGE_MAX);
    f->signer = EVP_PKEY_Q_keygen(NULL, NULL, "EC", "prime256v1");
    assert(f->signer != NULL);
    memcpy(f->key.id, "test-key", 9);
    unsigned char *der = f->key.spki;
    assert(i2d_PUBKEY(f->signer, &der) == TV4_BOOT_KEY_BYTES);
    f->now = 10;
    f->source_bytes = bytes; f->fragment = 256;
    for (size_t i = 0; i < bytes; ++i) f->source[i] = (uint8_t)(i * 37u + bytes);
    f->owner = (tv4_boot_owner_stamp){.boot = 77, .policy_generation = 1001};
    f->descriptor = (tv4_update_descriptor){.id = {77, 42}, .image_bytes = bytes};
    digest(f->source, bytes, f->descriptor.image_sha256);
    f->view = (tv4_boot_source_view){.token = 81, .object = 91, .generation = 101, .id = {77, 42}, .image_bytes = bytes};
    f->config = (tv4_boot_handoff_config){
        .policy = {.hardware_id = "test-f407", .hardware_revision = "synthetic", .memory_profile = "test-no-addresses",
            .installed_image_counter = 2, .minimum_image_counter = 3, .bootloader_counter = 5, .protocol = 7,
            .max_image_bytes = IMAGE_MAX, .timeout_ms = 100, .keys = &f->key, .key_count = 1},
        .ops = {.context = f, .now_ms = now_ms, .cancelled = cancelled, .owner_stamp = owner_stamp,
            .acquire = acquire, .inspect = inspect, .read = read_memory, .release = release},
        .metadata = f->metadata, .signature = f->signature, .key_id = (const uint8_t *)"test-key",
        .key_id_bytes = 8, .arena_bytes = TV4_BOOT_ARENA_BYTES, .lifetime_ms = 1000, .release_attempt_limit = 3};
    metadata(f, bytes);
    tv4_update_limits limits = {.max_image_bytes = IMAGE_MAX, .block_bytes = block, .retry_limit = 4,
        .ack_timeout_ms = 10, .progress_timeout_ms = 10000, .total_timeout_ms = 100000};
    assert(tv4_update_rx_init(&f->rx, limits, 77, f->now) && tv4_update_tx_init(&f->tx, limits, 77, f->now));
    assert(tv4_update_rx_begin(&f->rx, &f->descriptor, f->now) == TV4_UPDATE_OK);
    assert(tv4_update_tx_begin(&f->tx, &f->descriptor, f->now) == TV4_UPDATE_OK);
}
/* Actual portable sender/receiver. Only this test consumer writes RAM;
 * handoff itself has no write callback/backend. Deliberately lose one DATA
 * and final ACK, then consume duplicates without another write. */
static void transfer(fixture *f)
{
    unsigned bound = 0;
    while (f->tx.context.state != TV4_UPDATE_COMPLETE_UNVERIFIED) {
        assert(++bound <= IMAGE_MAX + 10u && !f->locked);
        if (f->tx.context.state == TV4_UPDATE_NEED_DATA) {
            size_t n = f->source_bytes - f->tx.context.next_offset;
            if (n > f->tx.context.limits.block_bytes) n = f->tx.context.limits.block_bytes;
            assert(tv4_update_tx_load(&f->tx, f->source + f->tx.context.next_offset, n, f->now) == TV4_UPDATE_OK);
        }
        tv4_update_block packet;
        assert(tv4_update_tx_next(&f->tx, &packet, f->now) == TV4_UPDATE_OK);
        if (f->lost_data == 0) { ++f->lost_data; f->now += 10; continue; }
        tv4_update_ack ack;
        tv4_update_result result = tv4_update_rx_receive(&f->rx, &packet, &ack, f->now);
        if (result == TV4_UPDATE_PENDING) {
            tv4_update_block pending;
            assert(tv4_update_rx_pending(&f->rx, &pending, f->now) == TV4_UPDATE_PENDING);
            assert(pending.offset == f->staged_bytes);
            memcpy(f->staging + pending.offset, pending.bytes, pending.length);
            f->staged_bytes += pending.length;
            assert(tv4_update_rx_receive(&f->rx, &packet, &ack, f->now) == TV4_UPDATE_PENDING);
            assert(tv4_update_rx_consume(&f->rx, pending.id, pending.sequence, true, &ack, f->now) == TV4_UPDATE_ACK);
        } else assert(result == TV4_UPDATE_ACK);
        if (f->rx.context.state == TV4_UPDATE_COMPLETE_UNVERIFIED && f->lost_final_ack == 0) {
            ++f->lost_final_ack; f->now += 10; continue;
        }
        assert(tv4_update_tx_ack(&f->tx, &ack, f->now) == TV4_UPDATE_OK);
    }
    assert(f->staged_bytes == f->source_bytes && memcmp(f->staging, f->source, f->source_bytes) == 0);
    assert(f->rx.context.state == TV4_UPDATE_COMPLETE_UNVERIFIED && f->lost_final_ack == 1);
}
static void begin(fixture *f) { assert(tv4_boot_handoff_begin(&f->session, &f->config, &f->rx) && f->locked); }
static void verified(fixture *f)
{
    assert(tv4_boot_handoff_verify(&f->session, &f->workspace));
    assert(f->session.state == TV4_HANDOFF_VERIFIED && f->locked && f->releases == 0);
    assert(f->session.admission_attempted && f->session.admission_result == TV4_BOOT_VERIFIED_CANDIDATE);
    assert(zero(&f->workspace, sizeof(f->workspace)));
    assert(tv4_boot_handoff_peek(&f->session, &f->receipt));
    assert(f->receipt.image.image_bytes == f->source_bytes && f->receipt.id.boot == f->descriptor.id.boot && f->receipt.id.transfer == f->descriptor.id.transfer);
    assert(f->receipt.policy_generation == f->owner.policy_generation && f->receipt.source_object == f->view.object && f->receipt.source_generation == f->view.generation);
    assert(memcmp(f->receipt.image.image_sha256, f->descriptor.image_sha256, 32) == 0);
}
static void closed_failure(fixture *f, tv4_boot_handoff_error error)
{
    assert(f->session.error == error && f->session.state == TV4_HANDOFF_CLOSED && !f->locked);
    assert(f->session.held == false && f->releases == 1 && zero(&f->session.candidate, sizeof(f->session.candidate)));
    memset(&f->receipt, 0xa5, sizeof(f->receipt));
    assert(!tv4_boot_handoff_peek(&f->session, &f->receipt) && zero(&f->receipt, sizeof(f->receipt)));
    assert(!tv4_boot_handoff_begin(&f->session, &f->config, &f->rx));
}
static void finish(fixture *f)
{
    if (f->session.held && f->session.state != TV4_HANDOFF_QUARANTINED) assert(tv4_boot_handoff_close(&f->session));
    EVP_PKEY_free(f->signer);
}
static void end_to_end_sizes_and_fragments(void)
{
    const uint32_t sizes[] = {1, 255, 256, 257, 769, IMAGE_MAX};
    const uint16_t blocks[] = {1, 31, 256};
    const size_t fragments[] = {1, 7, 256};
    unsigned count = 0;
    for (size_t i = 0; i < 6; ++i) for (size_t j = 0; j < 3; ++j) for (size_t k = 0; k < 3; ++k) {
        fixture f; setup(&f, sizes[i], blocks[j]); transfer(&f); f.fragment = fragments[k]; begin(&f); verified(&f);
        assert(!tv4_boot_handoff_verify(&f.session, &f.workspace));
        assert(!tv4_boot_handoff_clear(&f.session));
        assert(tv4_boot_handoff_close(&f.session) && f.releases == 1 && !f.locked);
        assert(tv4_boot_handoff_close(&f.session) && f.releases == 1);
        assert(tv4_boot_handoff_clear(&f.session) && zero(&f.session, sizeof(f.session)));
        finish(&f); ++count;
    }
    printf("PASS %u signed end-to-end RAM transfers with lost DATA/final ACK and pending duplicate\n", count);
}
static void snapshots_and_reentry(void)
{
    fixture f; setup(&f, 769, 256); transfer(&f); begin(&f);
    memset(f.metadata, 0xee, sizeof(f.metadata)); memset(f.signature, 0xee, sizeof(f.signature));
    memset(&f.key, 0, sizeof(f.key)); f.config.policy.protocol = 0; f.config.ops.read = NULL;
    f.inject = REENTER; f.release_reentry = true; verified(&f); finish(&f);
    setup(&f, 769, 256); transfer(&f); f.inject = REENTER_ABORT; begin(&f);
    assert(!tv4_boot_handoff_verify(&f.session, &f.workspace)); closed_failure(&f, TV4_HANDOFF_CANCELLED); finish(&f);
    setup(&f, 769, 256); transfer(&f); f.inject = ACQUIRE_ABORT;
    assert(!tv4_boot_handoff_begin(&f.session, &f.config, &f.rx)); closed_failure(&f, TV4_HANDOFF_CANCELLED); finish(&f);
    setup(&f, 769, 256); transfer(&f); f.inject = EOF_ABORT; begin(&f);
    assert(!tv4_boot_handoff_verify(&f.session, &f.workspace)); closed_failure(&f, TV4_HANDOFF_CANCELLED); finish(&f);
}
static void binding_is_not_transport_trust(void)
{
    for (unsigned which = 0; which < 5; ++which) {
        fixture f; setup(&f, 769, 256);
        if (which == 0) { f.source[0] ^= 1; } /* transport CRC valid, signed hash is not */
        if (which == 1) { f.descriptor.image_sha256[0] ^= 1; f.rx.context.descriptor = f.descriptor; f.tx.context.descriptor = f.descriptor; }
        if (which == 2) metadata(&f, 768); /* valid signature for a different length */
        if (which == 3) f.signature[f.config.signature_bytes - 1] ^= 1;
        if (which == 4) f.config.policy.minimum_image_counter = 4;
        transfer(&f); begin(&f);
        assert(!tv4_boot_handoff_verify(&f.session, &f.workspace));
        closed_failure(&f, which == 1 ? TV4_HANDOFF_BINDING : TV4_HANDOFF_ADMISSION);
        if (which == 0) assert(f.session.admission_result == TV4_BOOT_HASH);
        if (which == 2) assert(f.session.admission_result == TV4_BOOT_LENGTH);
        if (which == 3) assert(f.session.admission_result == TV4_BOOT_SIGNATURE && f.reads == 0);
        if (which == 4) assert(f.session.admission_result == TV4_BOOT_ROLLBACK && f.reads == 0);
        finish(&f);
    }
}
static void changes_during_verification(void)
{
    const fault faults[] = {SOURCE_OBJECT, SOURCE_GENERATION, SOURCE_SIZE, SOURCE_SESSION, OWNER_BOOT, OWNER_POLICY,
        RX_ABORT, RX_REINIT, RX_HASH, READ_TIMEOUT, READ_BACKWARD, READ_FAIL, READ_OVERSIZE, READ_STALL, READ_SHORT, READ_EXTRA};
    for (size_t i = 0; i < sizeof(faults) / sizeof(faults[0]); ++i) {
        fixture f; setup(&f, 769, 256); transfer(&f); begin(&f); f.inject = faults[i];
        assert(!tv4_boot_handoff_verify(&f.session, &f.workspace));
        tv4_boot_handoff_error expected = TV4_HANDOFF_ADMISSION;
        if (i < 4) expected = TV4_HANDOFF_SOURCE;
        else if (i < 6) expected = TV4_HANDOFF_OWNER;
        else if (i < 9) expected = TV4_HANDOFF_RECEIVER;
        else if (i == 9) expected = TV4_HANDOFF_TIMEOUT;
        else if (i == 10) expected = TV4_HANDOFF_CLOCK;
        closed_failure(&f, expected); finish(&f);
    }
}
static void revoke_after_success(void)
{
    for (unsigned which = 0; which < 10; ++which) {
        fixture f; setup(&f, 769, 256); transfer(&f); begin(&f); verified(&f);
        tv4_boot_handoff_error expected = TV4_HANDOFF_SOURCE;
        switch (which) {
        case 0: ++f.view.generation; break;
        case 1: ++f.view.object; break;
        case 2: ++f.owner.boot; expected = TV4_HANDOFF_OWNER; break;
        case 3: ++f.owner.policy_generation; expected = TV4_HANDOFF_OWNER; break;
        case 4: f.inject = INSPECT_FAIL; break;
        case 5: assert(tv4_update_rx_abort(&f.rx, f.descriptor.id, f.now) == TV4_UPDATE_OK); expected = TV4_HANDOFF_RECEIVER; break;
        case 6: ++f.rx.context.next_offset; expected = TV4_HANDOFF_RECEIVER; break;
        case 7: f.now += f.config.lifetime_ms; expected = TV4_HANDOFF_TIMEOUT; break;
        case 8: f.cancel = true; expected = TV4_HANDOFF_CANCELLED; break;
        default: f.inject = OWNER_FAIL; expected = TV4_HANDOFF_OWNER; break;
        }
        assert(!tv4_boot_handoff_peek(&f.session, &f.receipt) && zero(&f.receipt, sizeof(f.receipt)));
        closed_failure(&f, expected); finish(&f);
    }
}
static void acquire_and_release_failures(void)
{
    fixture f; setup(&f, 769, 256); transfer(&f); f.inject = ACQUIRE_FAIL;
    assert(!tv4_boot_handoff_begin(&f.session, &f.config, &f.rx));
    assert(!f.locked && !f.session.held && f.releases == 0 && f.session.state == TV4_HANDOFF_CLOSED);
    assert(tv4_boot_handoff_clear(&f.session)); finish(&f);
    setup(&f, 769, 256); transfer(&f); f.inject = ACQUIRE_BAD_OBJECT;
    assert(!tv4_boot_handoff_begin(&f.session, &f.config, &f.rx)); closed_failure(&f, TV4_HANDOFF_SOURCE); finish(&f);
    setup(&f, 769, 256); transfer(&f); f.inject = ACQUIRE_BAD_TOKEN;
    assert(!tv4_boot_handoff_begin(&f.session, &f.config, &f.rx));
    assert(f.session.state == TV4_HANDOFF_QUARANTINED && f.session.held && f.locked && f.releases == 0);
    assert(!tv4_boot_handoff_clear(&f.session) && !tv4_boot_handoff_close(&f.session)); finish(&f);
    setup(&f, 769, 256); transfer(&f); begin(&f); verified(&f);
    f.release_failures = 2; f.release_reentry = true;
    assert(!tv4_boot_handoff_close(&f.session) && f.releases == 1 && f.locked && f.session.held);
    assert(f.session.state == TV4_HANDOFF_RELEASING && !tv4_boot_handoff_clear(&f.session));
    assert(!tv4_boot_handoff_peek(&f.session, &f.receipt) && f.releases == 2 && zero(&f.receipt, sizeof(f.receipt)));
    assert(tv4_boot_handoff_close(&f.session) && f.releases == 3 && !f.locked && !f.session.held);
    assert(f.session.error == TV4_HANDOFF_RELEASE && tv4_boot_handoff_clear(&f.session)); finish(&f);
    setup(&f, 769, 256); transfer(&f); begin(&f); f.cancel = true; f.release_failures = 100;
    for (unsigned i = 0; i < 10; ++i) assert(!tv4_boot_handoff_poll(&f.session));
    assert(f.releases == 3 && f.session.release_attempts == 3 && f.locked && f.session.held && f.session.source.token == 81);
    assert(f.session.state == TV4_HANDOFF_QUARANTINED && f.session.error == TV4_HANDOFF_CANCELLED);
    assert(!tv4_boot_handoff_clear(&f.session) && !tv4_boot_handoff_begin(&f.session, &f.config, &f.rx)); finish(&f);
}
static void deadlines_before_verification_and_wrap(void)
{
    fixture f; setup(&f, 1, 256); transfer(&f); begin(&f);
    f.now += f.config.lifetime_ms;
    assert(!tv4_boot_handoff_verify(&f.session, &f.workspace) && f.reads == 0);
    closed_failure(&f, TV4_HANDOFF_TIMEOUT); finish(&f);
    setup(&f, 1, 256); transfer(&f); f.now += f.rx.context.limits.progress_timeout_ms;
    assert(!tv4_boot_handoff_begin(&f.session, &f.config, &f.rx) && f.acquired == 0);
    assert(f.session.error == TV4_HANDOFF_RECEIVER && f.session.state == TV4_HANDOFF_CLOSED); finish(&f);
    setup(&f, 1, 256); transfer(&f); f.config.lifetime_ms = 20000; begin(&f); verified(&f);
    f.now += f.rx.context.limits.progress_timeout_ms;
    assert(!tv4_boot_handoff_poll(&f.session)); closed_failure(&f, TV4_HANDOFF_RECEIVER); finish(&f);
    setup(&f, 1, 256);
    f.now = UINT32_MAX - 5u; f.lost_data = 1; f.lost_final_ack = 1;
    f.owner.boot = 78; f.descriptor.id = (tv4_update_id){78, 43}; f.view.id = f.descriptor.id;
    /* New volatile identities and timestamps, not a forged backwards jump. */
    tv4_update_limits limits = f.rx.context.limits;
    assert(tv4_update_rx_init(&f.rx, limits, 78, f.now) && tv4_update_tx_init(&f.tx, limits, 78, f.now));
    assert(tv4_update_rx_begin(&f.rx, &f.descriptor, f.now) == TV4_UPDATE_OK);
    assert(tv4_update_tx_begin(&f.tx, &f.descriptor, f.now) == TV4_UPDATE_OK);
    transfer(&f); begin(&f); verified(&f); f.now += 6;
    assert(f.now == 0);
    assert(tv4_boot_handoff_poll(&f.session)); finish(&f);
}
static void boundaries_and_final_cleanup(void)
{
    const fault faults[] = {ADMISSION_TIMEOUT, CLEANUP_OWNER, EOF_SOURCE, INSPECT_BAD_TOKEN};
    for (size_t i = 0; i < 4; ++i) {
        fixture f; setup(&f, 769, 256); transfer(&f); begin(&f); f.inject = faults[i];
        assert(!tv4_boot_handoff_verify(&f.session, &f.workspace));
        closed_failure(&f, i == 0 ? TV4_HANDOFF_ADMISSION : i == 1 ? TV4_HANDOFF_OWNER : TV4_HANDOFF_SOURCE);
        if (i == 0) assert(f.session.admission_result == TV4_BOOT_TIMEOUT);
        finish(&f);
    }
    for (unsigned which = 0; which < 3; ++which) {
        fixture f; setup(&f, 769, 256); transfer(&f); begin(&f);
        if (which == 0) ++f.view.generation;
        if (which == 1) ++f.owner.policy_generation;
        if (which == 2) ++f.rx.context.descriptor.id.transfer;
        assert(!tv4_boot_handoff_verify(&f.session, &f.workspace) && f.reads == 0);
        assert(!f.session.admission_attempted);
        closed_failure(&f, which == 0 ? TV4_HANDOFF_SOURCE : which == 1 ? TV4_HANDOFF_OWNER : TV4_HANDOFF_RECEIVER);
        finish(&f);
    }
    fixture f; setup(&f, 769, 256); transfer(&f); f.inject = ACQUIRE_TIMEOUT;
    assert(!tv4_boot_handoff_begin(&f.session, &f.config, &f.rx)); closed_failure(&f, TV4_HANDOFF_TIMEOUT); finish(&f);
    setup(&f, 769, 256); transfer(&f); f.cancel = true;
    assert(!tv4_boot_handoff_begin(&f.session, &f.config, &f.rx) && f.acquired == 0 && !f.locked);
    assert(f.session.error == TV4_HANDOFF_CANCELLED && f.session.state == TV4_HANDOFF_CLOSED); finish(&f);
    setup(&f, 769, 256); transfer(&f); f.owner.policy_generation = 0;
    assert(!tv4_boot_handoff_begin(&f.session, &f.config, &f.rx) && f.acquired == 0 && !f.locked);
    assert(f.session.error == TV4_HANDOFF_OWNER && f.session.state == TV4_HANDOFF_CLOSED); finish(&f);
}
static void retained_ack_does_not_extend_handoff(void)
{
    fixture f; setup(&f, 769, 256); transfer(&f); begin(&f); verified(&f);
    tv4_update_block duplicate = f.rx.block;
    tv4_update_ack ack;
    ++f.now;
    assert(tv4_update_rx_receive(&f.rx, &duplicate, &ack, f.now) == TV4_UPDATE_ACK);
    assert(tv4_boot_handoff_peek(&f.session, &f.receipt) && f.releases == 0 && f.acquired == 1);
    /* Last ACK retries remain bounded even while a verified candidate exists. */
    for (unsigned i = 0; i < 4; ++i) (void)tv4_update_rx_receive(&f.rx, &duplicate, &ack, f.now);
    assert(f.rx.context.state == TV4_UPDATE_FAILED && f.rx.context.error == TV4_UPDATE_ERROR_RETRIES);
    assert(!tv4_boot_handoff_peek(&f.session, &f.receipt));
    closed_failure(&f, TV4_HANDOFF_RECEIVER); finish(&f);
}
static void rejected_inputs_and_aliases(void)
{
    fixture f; setup(&f, 769, 256);
    assert(!tv4_boot_handoff_begin(&f.session, &f.config, &f.rx) && f.acquired == 0); /* partial RX */
    transfer(&f);
    f.config.release_attempt_limit = TV4_BOOT_RELEASE_ATTEMPTS_MAX + 1u;
    assert(!tv4_boot_handoff_begin(&f.session, &f.config, &f.rx) && f.session.state == TV4_HANDOFF_EMPTY);
    f.config.release_attempt_limit = 3; f.config.metadata_bytes = SIZE_MAX;
    assert(!tv4_boot_handoff_begin(&f.session, &f.config, &f.rx));
    f.config.metadata_bytes = 1; f.config.metadata = f.session.metadata;
    assert(!tv4_boot_handoff_begin(&f.session, &f.config, &f.rx));
    f.config.metadata = f.metadata; metadata(&f, 769); begin(&f);
    assert(!tv4_boot_handoff_verify(&f.session, (tv4_boot_workspace *)(void *)&f.session));
    assert(!tv4_boot_handoff_peek(&f.session, (tv4_boot_handoff_receipt *)(void *)&f.session));
    assert(f.session.state == TV4_HANDOFF_BOUND && f.locked && f.reads == 0);
    f.config.arena_bytes = 16; /* live config changes cannot change captured input */
    verified(&f); finish(&f);
    setup(&f, 769, 256); transfer(&f); f.config.arena_bytes = 16; begin(&f);
    assert(!tv4_boot_handoff_verify(&f.session, &f.workspace));
    assert(f.session.admission_result == TV4_BOOT_RESOURCE); closed_failure(&f, TV4_HANDOFF_ADMISSION); finish(&f);
}
#define RUN(name) do { name(); puts("PASS " #name); } while (0)
int main(void)
{
    RUN(end_to_end_sizes_and_fragments);
    RUN(snapshots_and_reentry);
    RUN(binding_is_not_transport_trust);
    RUN(changes_during_verification);
    RUN(revoke_after_success);
    RUN(acquire_and_release_failures);
    RUN(deadlines_before_verification_and_wrap);
    RUN(boundaries_and_final_cleanup);
    RUN(retained_ack_does_not_extend_handoff);
    RUN(rejected_inputs_and_aliases);
    printf("PASS 10 handoff groups; state=%zu receipt=%zu workspace=%zu; RAM only, no install/device access\n",
        sizeof(tv4_boot_handoff), sizeof(tv4_boot_handoff_receipt), sizeof(tv4_boot_workspace));
    return 0;
}
