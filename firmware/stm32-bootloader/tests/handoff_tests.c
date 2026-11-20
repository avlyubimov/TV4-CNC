#include "update_handoff.h"
#include "update_completion.h"
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
    ADMISSION_TIMEOUT, CLEANUP_OWNER, EOF_SOURCE, INSPECT_BAD_TOKEN, PREFLIGHT_PROFILE_MUTATE,
    COMPLETION_REENTER, COMPLETION_ABORT, COMPLETION_INPUT_MUTATE, RELEASE_POLICY, RELEASE_CANCEL, RELEASE_TIMEOUT
} fault;
typedef struct {
    tv4_boot_handoff session;
    tv4_boot_handoff_config config;
    tv4_boot_workspace workspace;
    tv4_boot_handoff_receipt receipt;
    tv4_boot_image_profile preflight_profile;
    tv4_boot_handoff_image_report image_report;
    tv4_boot_completion completion;
    tv4_update_binding expected;
    tv4_update_outcome outcome;
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
        "\"hardware_revision\":\"synthetic\",\"memory_profile\":\"%s\","
        "\"image_counter\":3,\"minimum_bootloader_counter\":5,\"protocol_min\":6,\"protocol_max\":8,"
        "\"image_size\":%" PRIu32 ",\"sha256\":\"%s\"}", TV4_BOOT_DRAFT_SCHEMA, f->config.policy.memory_profile, bytes, hex);
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
    if (f->inject == RELEASE_POLICY) ++f->owner.policy_generation;
    if (f->inject == RELEASE_CANCEL) f->cancel = true;
    if (f->inject == RELEASE_TIMEOUT) f->now += f->config.lifetime_ms;
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
        case PREFLIGHT_PROFILE_MUTATE: f->preflight_profile.image_base = 0; f->preflight_profile.id[0] = 'x'; break;
        case COMPLETION_INPUT_MUTATE: f->preflight_profile.image_base = 0; f->expected.metadata_sha256[0] ^= 1; break;
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
        memset(&f->image_report, 0xa5, sizeof(f->image_report));
        tv4_boot_handoff_image_report report_before = f->image_report;
        assert(tv4_boot_handoff_preflight(&f->session, &f->preflight_profile, &f->workspace, &f->image_report) == TV4_IMAGE_BUSY);
        assert(memcmp(&report_before, &f->image_report, sizeof(report_before)) == 0);
    }
    if (f->inject == COMPLETION_REENTER || f->inject == COMPLETION_ABORT) {
        memset(&f->outcome,0xa5,sizeof(f->outcome)); tv4_update_outcome before=f->outcome;
        assert(!tv4_boot_completion_start(&f->completion,&f->session,&f->expected,&f->preflight_profile,&f->workspace));
        assert(!tv4_boot_completion_clear(&f->completion) && !tv4_boot_completion_poll(&f->completion));
        assert(!tv4_boot_completion_snapshot(&f->completion,&f->outcome) && memcmp(&before,&f->outcome,sizeof(before)) == 0);
        if (f->inject == COMPLETION_ABORT) {
            tv4_boot_completion_abort(&f->completion); assert(f->locked && f->releases == 0);
        }
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
static void vector_word(uint8_t *out, uint32_t value)
{
    for (unsigned i = 0; i < 4; ++i) out[i] = (uint8_t)(value >> (i * 8u));
}
static void bind_changed_image(fixture *f)
{
    metadata(f, (uint32_t)f->source_bytes);
    digest(f->source, f->source_bytes, f->descriptor.image_sha256);
    ++f->owner.boot; ++f->descriptor.id.transfer; f->descriptor.id.boot = f->owner.boot;
    f->view.id = f->descriptor.id; ++f->view.generation;
    tv4_update_limits limits = f->rx.context.limits;
    assert(tv4_update_rx_init(&f->rx, limits, f->owner.boot, f->now));
    assert(tv4_update_tx_init(&f->tx, limits, f->owner.boot, f->now));
    assert(tv4_update_rx_begin(&f->rx, &f->descriptor, f->now) == TV4_UPDATE_OK);
    assert(tv4_update_tx_begin(&f->tx, &f->descriptor, f->now) == TV4_UPDATE_OK);
}
static void vector_fixture(fixture *f, uint16_t vectors)
{
    setup(f, vectors > 128 ? 1537 : 769, 31);
    memcpy(f->config.policy.memory_profile, "synthetic-vector-v1", 20);
    f->preflight_profile = (tv4_boot_image_profile){.id = "synthetic-vector-v1", .image_base = 0x01000000,
        .image_capacity = IMAGE_MAX, .minimum_stack_bytes = 128, .vector_alignment = 1024,
        .vector_count = vectors, .ram_count = 1, .ram = {{0x30000000,4096}}};
    for (unsigned i = 0; i < vectors; ++i) {
        bool reserved = (i >= 7 && i <= 10) || i == 13;
        vector_word(f->source + i * 4u, i == 0 ? 0x30001000u : reserved ? 0u : 0x01000001u + (uint32_t)vectors * 4u);
    }
    bind_changed_image(f);
}
static void signed_vector_success_and_snapshot(void)
{
    const uint16_t counts[] = {16,17,33,129,256};
    const size_t fragments[] = {1,7,256};
    for (size_t i = 0; i < 5; ++i) for (size_t j = 0; j < 3; ++j) {
        fixture f; vector_fixture(&f, counts[i]); transfer(&f); begin(&f);
        if (i == 0 && j == 0) {
            assert(tv4_boot_handoff_preflight(&f.session,&f.preflight_profile,&f.workspace,&f.image_report) == TV4_IMAGE_CONTEXT);
            assert(f.reads == 0 && !f.session.preflight_attempted && f.locked);
        }
        verified(&f); f.fragment = fragments[j];
        assert(tv4_boot_handoff_preflight(&f.session, &f.preflight_profile, &f.workspace, &f.image_report) == TV4_IMAGE_PROFILE_MODEL_MATCH);
        assert(f.image_report.vectors.initial_sp == 0x30001000u && f.image_report.vectors.vector_count == counts[i]);
        assert(memcmp(&f.image_report.receipt, &f.receipt, sizeof(f.receipt)) == 0);
        assert(f.session.preflight_attempted && f.session.preflight_result == TV4_IMAGE_PROFILE_MODEL_MATCH);
        assert(f.session.state == TV4_HANDOFF_VERIFIED && f.locked && f.releases == 0 && zero(&f.workspace, sizeof(f.workspace)));
        finish(&f);
    }
    fixture f; vector_fixture(&f,17); transfer(&f); begin(&f); verified(&f); f.reads = 0;
    f.inject = PREFLIGHT_PROFILE_MUTATE;
    assert(tv4_boot_handoff_preflight(&f.session, &f.preflight_profile, &f.workspace, &f.image_report) == TV4_IMAGE_PROFILE_MODEL_MATCH);
    assert(strcmp(f.image_report.vectors.profile_id,"synthetic-vector-v1") == 0 && f.preflight_profile.image_base == 0);
    finish(&f);
    puts("PASS 15 signed vector inspections with fragmented reads + captured local profile mutation");
}
static void valid_signature_invalid_vector_model(void)
{
    for (unsigned which = 0; which < 9; ++which) {
        fixture f; vector_fixture(&f,17);
        tv4_boot_image_result expected = TV4_IMAGE_HANDLER;
        switch (which) {
        case 0: vector_word(f.source,0x30000008u); expected = TV4_IMAGE_STACK; break;
        case 1: vector_word(f.source + 4,0x01000044u); break; /* even Reset */
        case 2: vector_word(f.source + 8,0); break; /* absent NMI */
        case 3: vector_word(f.source + 7u * 4u,0x01000101u); expected = TV4_IMAGE_RESERVED_VECTOR; break;
        case 4: vector_word(f.source + 4,0x01000401u); break; /* inside capacity, outside signed bytes */
        case 5: f.preflight_profile.id[0] = 'x'; expected = TV4_IMAGE_PROFILE; break;
        case 6: f.preflight_profile.vector_alignment = 0; expected = TV4_IMAGE_PROFILE; break;
        case 7: f.preflight_profile.image_capacity = 768; expected = TV4_IMAGE_LENGTH; break;
        default: f.preflight_profile.reserved_irq[0] = 1; expected = TV4_IMAGE_RESERVED_VECTOR; break;
        }
        bind_changed_image(&f); transfer(&f); begin(&f); verified(&f);
        assert(tv4_boot_handoff_preflight(&f.session, &f.preflight_profile, &f.workspace, &f.image_report) == expected);
        assert(zero(&f.image_report,sizeof(f.image_report)) && zero(&f.workspace,sizeof(f.workspace)));
        closed_failure(&f,TV4_HANDOFF_PREFLIGHT); finish(&f);
    }
}
static void preflight_context_cleanup_and_reentry(void)
{
    fixture invalid; vector_fixture(&invalid,17); invalid.signature[invalid.config.signature_bytes - 1] ^= 1;
    transfer(&invalid); begin(&invalid); assert(!tv4_boot_handoff_verify(&invalid.session,&invalid.workspace));
    memset(&invalid.image_report,0xa5,sizeof(invalid.image_report));
    assert(tv4_boot_handoff_preflight(&invalid.session,&invalid.preflight_profile,&invalid.workspace,&invalid.image_report) == TV4_IMAGE_CONTEXT);
    assert(!invalid.session.preflight_attempted && zero(&invalid.image_report,sizeof(invalid.image_report)) && invalid.reads == 0);
    finish(&invalid);
    const fault faults[] = {SOURCE_GENERATION, OWNER_POLICY, RX_ABORT, READ_TIMEOUT, REENTER_ABORT,
        READ_FAIL, READ_OVERSIZE, READ_STALL, READ_SHORT, CLEANUP_OWNER, REENTER};
    for (size_t i = 0; i < sizeof(faults)/sizeof(faults[0]); ++i) {
        fixture f; vector_fixture(&f,33); transfer(&f); begin(&f); verified(&f);
        f.reads = 0; f.fragment = 7; f.inject = faults[i];
        tv4_boot_image_result result = tv4_boot_handoff_preflight(&f.session, &f.preflight_profile, &f.workspace, &f.image_report);
        if (faults[i] == REENTER) { assert(result == TV4_IMAGE_PROFILE_MODEL_MATCH && f.locked); finish(&f); continue; }
        assert(result != TV4_IMAGE_PROFILE_MODEL_MATCH && zero(&f.image_report,sizeof(f.image_report)));
        assert(zero(&f.workspace,sizeof(f.workspace)) && !f.locked && f.releases == 1 && f.session.state == TV4_HANDOFF_CLOSED);
        finish(&f);
    }
    fixture f; vector_fixture(&f,17); vector_word(f.source,0); bind_changed_image(&f); transfer(&f); begin(&f); verified(&f);
    unsigned reads = f.reads;
    assert(tv4_boot_handoff_preflight(&f.session,&f.preflight_profile,(tv4_boot_workspace *)(void *)&f.session,&f.image_report) == TV4_IMAGE_ARGUMENT);
    assert(tv4_boot_handoff_preflight(&f.session,&f.preflight_profile,&f.workspace,(tv4_boot_handoff_image_report *)(void *)&f.session) == TV4_IMAGE_ARGUMENT);
    assert(f.reads == reads && f.locked && f.releases == 0);
    f.release_failures = 100;
    assert(tv4_boot_handoff_preflight(&f.session,&f.preflight_profile,&f.workspace,&f.image_report) == TV4_IMAGE_STACK);
    assert(f.session.state == TV4_HANDOFF_RELEASING && f.session.held && f.locked && f.releases == 1);
    for (unsigned i = 0; i < 5; ++i) assert(!tv4_boot_handoff_poll(&f.session));
    assert(f.session.state == TV4_HANDOFF_QUARANTINED && f.releases == 3 && f.session.source.token == 81);
    assert(!tv4_boot_handoff_clear(&f.session)); finish(&f);
}
static void expected_binding(fixture *f)
{
    f->expected.image=f->descriptor;
    digest(f->metadata,f->config.metadata_bytes,f->expected.metadata_sha256);
}
static void completed(fixture *f,tv4_outcome_code code,tv4_outcome_reason reason)
{
    assert(tv4_boot_completion_snapshot(&f->completion,&f->outcome));
    assert(f->outcome.code==code && f->outcome.reason==reason && tv4_update_outcome_valid(&f->outcome));
    assert(!f->locked && !f->session.held && f->session.state==TV4_HANDOFF_CLOSED && f->releases>=1);
    assert(f->completion.state==TV4_COMPLETION_READY && f->completion.handoff==NULL && f->completion.transfer==NULL);
    assert(zero(&f->workspace,sizeof(f->workspace)) && zero(&f->completion.profile,sizeof(f->completion.profile)));
    assert(zero(&f->completion.inspection,sizeof(f->completion.inspection)));
    assert(!tv4_boot_handoff_peek(&f->session,&f->receipt)); /* report is not a live candidate */
}
static void completion_end_to_end_history(void)
{
    const uint16_t counts[]={17,33,256};
    for (size_t i=0;i<3;++i) for (unsigned j=0;j<2;++j) {
        fixture f; vector_fixture(&f,counts[i]); transfer(&f); begin(&f); expected_binding(&f); f.fragment=j==0 ? 1 : 256;
        assert(tv4_boot_completion_start(&f.completion,&f.session,&f.expected,&f.preflight_profile,&f.workspace));
        completed(&f,TV4_OUTCOME_CHECKED_CLOSED,TV4_OUTCOME_REASON_NONE);
        assert(tv4_update_binding_equal(&f.expected,&f.outcome.binding));
        unsigned calls=f.reads; f.staging[100]^=1; /* legal after release, history is not current trust */
        assert(tv4_boot_completion_snapshot(&f.completion,&f.outcome) && f.reads==calls && f.releases==1);
        tv4_outcome_tx tx; tv4_outcome_rx rx; tv4_update_outcome packet,ack,delivery;
        tv4_outcome_limits limits={.ack_timeout_ms=10,.linger_ms=50,.total_timeout_ms=100,.retry_limit=3};
        assert(tv4_outcome_tx_init(&tx,limits,f.owner.boot,f.now)); assert(tv4_outcome_rx_init(&rx,limits,f.now));
        assert(tv4_outcome_tx_begin(&tx,&f.outcome,f.now)==TV4_OUTCOME_OK);
        assert(tv4_outcome_rx_begin(&rx,&f.tx,f.expected.metadata_sha256,f.now)==TV4_OUTCOME_OK);
        assert(tv4_outcome_tx_next(&tx,&packet,f.now)==TV4_OUTCOME_SENT); /* lost report */
        f.now+=10; assert(tv4_outcome_tx_next(&tx,&packet,f.now)==TV4_OUTCOME_SENT);
        assert(tv4_outcome_rx_receive(&rx,&packet,&ack,&delivery,f.now)==TV4_OUTCOME_DELIVERED); /* lost ACK */
        f.now+=10; assert(tv4_outcome_tx_next(&tx,&packet,f.now)==TV4_OUTCOME_SENT);
        assert(tv4_outcome_rx_receive(&rx,&packet,&ack,&delivery,f.now)==TV4_OUTCOME_ACK);
        assert(tv4_outcome_tx_ack(&tx,&ack,f.now)==TV4_OUTCOME_OK && tv4_update_outcome_equal(&delivery,&f.outcome));
        assert(f.reads==calls && f.releases==1 && !f.locked);
        assert(tv4_boot_completion_clear(&f.completion)); finish(&f);
    }
    puts("PASS 6 signed DATA -> crypto -> vectors -> close -> outcome/ACK sessions, lost report/final ACK; no lease retained");
}
static void completion_rejections(void)
{
    for (unsigned which=0;which<7;++which) {
        fixture f; vector_fixture(&f,17); tv4_outcome_reason reason=TV4_OUTCOME_SIGNATURE;
        if (which==0) f.signature[f.config.signature_bytes-1]^=1;
        if (which==1) { vector_word(f.source,0); bind_changed_image(&f); reason=TV4_OUTCOME_VECTORS; }
        if (which==2) { f.preflight_profile.id[0]='x'; reason=TV4_OUTCOME_PROFILE; }
        if (which==3) { f.preflight_profile.image_capacity=768; reason=TV4_OUTCOME_LENGTH; }
        transfer(&f);
        if (which==4) { f.staging[0]^=1; reason=TV4_OUTCOME_HASH; }
        begin(&f); expected_binding(&f);
        if (which==5) { f.expected.metadata_sha256[0]^=1; reason=TV4_OUTCOME_BINDING; }
        if (which==6) { f.cancel=true; reason=TV4_OUTCOME_CANCEL; }
        assert(tv4_boot_completion_start(&f.completion,&f.session,&f.expected,&f.preflight_profile,&f.workspace));
        completed(&f,reason==TV4_OUTCOME_CANCEL ? TV4_OUTCOME_CANCELLED_CLOSED : reason==TV4_OUTCOME_BINDING ?
            TV4_OUTCOME_FAILED_CLOSED : TV4_OUTCOME_REJECTED_CLOSED,reason);
        assert(f.releases==1); finish(&f);
    }
}
static void completion_release_and_late_context(void)
{
    fixture f; vector_fixture(&f,17); transfer(&f); begin(&f); expected_binding(&f); f.release_failures=1;
    assert(tv4_boot_completion_start(&f.completion,&f.session,&f.expected,&f.preflight_profile,&f.workspace));
    assert(f.completion.state==TV4_COMPLETION_CLOSING && f.releases==1 && f.locked);
    memset(&f.outcome,0xa5,sizeof(f.outcome));
    assert(!tv4_boot_completion_snapshot(&f.completion,&f.outcome) && zero(&f.outcome,sizeof(f.outcome)));
    assert(!tv4_boot_completion_clear(&f.completion));
    assert(tv4_boot_completion_poll(&f.completion)); completed(&f,TV4_OUTCOME_FAILED_CLOSED,TV4_OUTCOME_RELEASE);
    assert(f.releases==2); finish(&f);
    vector_fixture(&f,17); transfer(&f); begin(&f); expected_binding(&f); f.release_failures=100;
    assert(tv4_boot_completion_start(&f.completion,&f.session,&f.expected,&f.preflight_profile,&f.workspace));
    for (unsigned i=0;i<8;++i) assert(!tv4_boot_completion_poll(&f.completion));
    assert(f.completion.state==TV4_COMPLETION_QUARANTINED && f.session.held && f.locked && f.releases==3);
    assert(tv4_boot_completion_snapshot(&f.completion,&f.outcome) && f.outcome.code==TV4_OUTCOME_QUARANTINED);
    assert(f.completion.handoff==&f.session && !tv4_boot_completion_clear(&f.completion)); finish(&f);
    const fault late[]={RELEASE_POLICY,RELEASE_CANCEL,RELEASE_TIMEOUT};
    const tv4_outcome_reason reasons[]={TV4_OUTCOME_CONTEXT,TV4_OUTCOME_CANCEL,TV4_OUTCOME_TIMEOUT};
    for (size_t i=0;i<3;++i) {
        vector_fixture(&f,17); transfer(&f); begin(&f); expected_binding(&f); f.inject=late[i];
        assert(tv4_boot_completion_start(&f.completion,&f.session,&f.expected,&f.preflight_profile,&f.workspace));
        completed(&f,i==1 ? TV4_OUTCOME_CANCELLED_CLOSED : TV4_OUTCOME_FAILED_CLOSED,reasons[i]); finish(&f);
    }
    vector_fixture(&f,17); transfer(&f); f.config.lifetime_ms=20000; begin(&f); expected_binding(&f);
    f.now+=f.rx.context.limits.progress_timeout_ms;
    assert(tv4_boot_completion_start(&f.completion,&f.session,&f.expected,&f.preflight_profile,&f.workspace));
    completed(&f,TV4_OUTCOME_FAILED_CLOSED,TV4_OUTCOME_TIMEOUT); finish(&f);
}
static void completion_reentry_snapshot_and_arguments(void)
{
    const fault faults[]={COMPLETION_REENTER,COMPLETION_ABORT,COMPLETION_INPUT_MUTATE};
    for (size_t i=0;i<3;++i) {
        fixture f; vector_fixture(&f,17); transfer(&f); begin(&f); expected_binding(&f); f.inject=faults[i];
        assert(tv4_boot_completion_start(&f.completion,&f.session,&f.expected,&f.preflight_profile,&f.workspace));
        completed(&f,i==1 ? TV4_OUTCOME_CANCELLED_CLOSED : TV4_OUTCOME_CHECKED_CLOSED,
            i==1 ? TV4_OUTCOME_CANCEL : TV4_OUTCOME_REASON_NONE); finish(&f);
    }
    fixture f; vector_fixture(&f,17); transfer(&f); begin(&f); expected_binding(&f);
    assert(!tv4_boot_completion_start(&f.completion,&f.session,&f.expected,NULL,&f.workspace));
    assert(!tv4_boot_completion_start(&f.completion,&f.session,&f.expected,&f.preflight_profile,(tv4_boot_workspace *)(void *)&f.completion));
    ++f.expected.image.id.transfer;
    assert(!tv4_boot_completion_start(&f.completion,&f.session,&f.expected,&f.preflight_profile,&f.workspace));
    assert(f.reads==0 && f.locked && f.completion.state==TV4_COMPLETION_EMPTY); finish(&f);
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
    RUN(signed_vector_success_and_snapshot);
    RUN(valid_signature_invalid_vector_model);
    RUN(preflight_context_cleanup_and_reentry);
    RUN(completion_end_to_end_history); RUN(completion_rejections);
    RUN(completion_release_and_late_context); RUN(completion_reentry_snapshot_and_arguments);
    printf("PASS 17 handoff/completion groups; completion=%zu; state=%zu receipt=%zu workspace=%zu; RAM only, no install/device access\n",
        sizeof(tv4_boot_completion),
        sizeof(tv4_boot_handoff), sizeof(tv4_boot_handoff_receipt), sizeof(tv4_boot_workspace));
    return 0;
}
