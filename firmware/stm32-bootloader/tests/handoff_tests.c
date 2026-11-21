#include "update_handoff.h"
#include "update_completion.h"
#include "update_negotiation.h"
#include "staging_source.h"
#include "update_wire.h"
#include "update_fragments.h"
#include "fragment_test_link.h"
#include "update_metadata.h"
#include <assert.h>
#include <inttypes.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <stdio.h>
#include <string.h>

enum { IMAGE_MAX = 2049 };
_Static_assert(TV4_META_DOCUMENT_MAX==TV4_BOOT_METADATA_MAX,"Metadata verifier bound differs");
_Static_assert(TV4_META_SIGNATURE_MAX==TV4_BOOT_SIGNATURE_MAX,"Signature verifier bound differs");
_Static_assert(TV4_META_KEY_MAX+1u==TV4_BOOT_ID_BYTES,"Key ID verifier bound differs");
/* Only --wire-staging opts into byte transport. Existing test modes keep their
 * typed path; no firmware/library runtime caller is introduced. */
static bool wire_mode;
static bool fragment_mode;
static unsigned wire_counts[9];
/* Host-only transport clock/route, deliberately separate from all unchanged
 * logical/crypto deadlines. No runtime timing or bus budget is claimed. */
static tv4_fragment_tx fragment_tx;
static tv4_fragment_rx fragment_rx;
static unsigned fragment_frames, fragment_duplicates;
static void fragment_hop(const tv4_wire_message *input,tv4_wire_message *output)
{
    assert(tv4_fragment_tx_begin(&fragment_tx,input,9,0)==TV4_FRAG_OK);
    while(fragment_tx.storage.state==TV4_FRAG_SENDING) {
        tv4_fragment_frame a,b;
        assert(tv4_fragment_tx_peek(&fragment_tx,&a,9,0)==TV4_FRAG_FRAME);
        /* A locally full queue does not consume/advance the frame. */
        assert(tv4_fragment_tx_peek(&fragment_tx,&b,9,0)==TV4_FRAG_FRAME && memcmp(&a,&b,sizeof(a))==0);
        tv4_fragment_result r=tv4_fragment_rx_receive(&fragment_rx,7,&a,9,0);
        assert(r==TV4_FRAG_PENDING || r==TV4_FRAG_READY);
        if(r==TV4_FRAG_PENDING && fragment_frames%17u==0) {
            assert(tv4_fragment_rx_receive(&fragment_rx,7,&a,9,0)==TV4_FRAG_DUPLICATE);++fragment_duplicates;
        }
        r=tv4_fragment_tx_commit(&fragment_tx,&a,9,0);
        assert(r==TV4_FRAG_COMMITTED || r==TV4_FRAG_FINISHED);++fragment_frames;
    }
    assert(tv4_fragment_rx_take(&fragment_rx,output,9,0)==TV4_FRAG_TAKEN);
}
static void wire_hop(tv4_wire_message *m)
{
    tv4_wire_message decoded;
    if(fragment_mode)fragment_hop(m,&decoded);
    else {
        uint8_t raw[TV4_WIRE_MESSAGE_MAX];size_t n=0;
        assert(tv4_update_wire_encode(m,raw,sizeof(raw),&n)==TV4_WIRE_OK);
        assert(tv4_update_wire_decode(raw,n,&decoded)==TV4_WIRE_OK);
    }
    assert(decoded.kind==m->kind);
    ++wire_counts[(unsigned)m->kind];*m=decoded;
}
static tv4_neg_result wire_receive_neg(tv4_negotiation *s,const tv4_neg_message *message,uint64_t generation,uint32_t now)
{
    if(!wire_mode)return tv4_neg_receive(s,message,generation,now);
    tv4_wire_message m={.body.negotiation=*message};
    switch(message->kind){case TV4_NEG_REQUEST:m.kind=TV4_WIRE_REQUEST;break;case TV4_NEG_OFFER:m.kind=TV4_WIRE_OFFER;break;
    case TV4_NEG_ACCEPT:m.kind=TV4_WIRE_ACCEPT;break;case TV4_NEG_CONFIRM:m.kind=TV4_WIRE_CONFIRM;break;default:assert(false);}
    wire_hop(&m);return tv4_neg_receive(s,&m.body.negotiation,generation,now);
}
static tv4_update_result wire_receive_data(tv4_update_rx *rx,const tv4_update_block *packet,tv4_update_ack *ack,uint32_t now)
{
    if(!wire_mode)return tv4_update_rx_receive(rx,packet,ack,now);
    tv4_wire_message m={.kind=TV4_WIRE_DATA,.body.data=*packet};wire_hop(&m);
    return tv4_update_rx_receive(rx,&m.body.data,ack,now);
}
static tv4_update_result wire_ack_data(tv4_update_tx *tx,const tv4_update_ack *ack,uint32_t now)
{
    if(!wire_mode)return tv4_update_tx_ack(tx,ack,now);
    tv4_wire_message m={.kind=TV4_WIRE_DATA_ACK,.body.ack=*ack};wire_hop(&m);
    return tv4_update_tx_ack(tx,&m.body.ack,now);
}
static tv4_outcome_result wire_receive_outcome(tv4_outcome_rx *rx,const tv4_update_outcome *packet,
    tv4_update_outcome *ack,tv4_update_outcome *delivery,uint32_t now)
{
    if(!wire_mode)return tv4_outcome_rx_receive(rx,packet,ack,delivery,now);
    tv4_wire_message m={.kind=TV4_WIRE_OUTCOME,.body.outcome=*packet};wire_hop(&m);
    return tv4_outcome_rx_receive(rx,&m.body.outcome,ack,delivery,now);
}
static tv4_outcome_result wire_ack_outcome(tv4_outcome_tx *tx,const tv4_update_outcome *ack,uint32_t now)
{
    if(!wire_mode)return tv4_outcome_tx_ack(tx,ack,now);
    tv4_wire_message m={.kind=TV4_WIRE_OUTCOME_ACK,.body.outcome=*ack};wire_hop(&m);
    return tv4_outcome_tx_ack(tx,&m.body.outcome,now);
}
typedef enum {
    NONE, ACQUIRE_FAIL, ACQUIRE_BAD_TOKEN, ACQUIRE_BAD_OBJECT, INSPECT_FAIL,
    READ_FAIL, SOURCE_OBJECT, SOURCE_GENERATION, SOURCE_SIZE, SOURCE_SESSION,
    OWNER_BOOT, OWNER_POLICY, OWNER_FAIL, RX_ABORT, RX_REINIT, RX_HASH,
    READ_TIMEOUT, READ_BACKWARD, REENTER, REENTER_ABORT, ACQUIRE_ABORT, EOF_ABORT,
    READ_OVERSIZE, READ_STALL, READ_SHORT, READ_EXTRA, ACQUIRE_TIMEOUT,
    ADMISSION_TIMEOUT, CLEANUP_OWNER, EOF_SOURCE, INSPECT_BAD_TOKEN, PREFLIGHT_PROFILE_MUTATE,
    COMPLETION_REENTER, COMPLETION_ABORT, COMPLETION_INPUT_MUTATE, RELEASE_POLICY, RELEASE_CANCEL, RELEASE_TIMEOUT, ACQUIRE_RELEASE_INPUTS, STAGE_LOCK
} fault;
typedef struct {
    tv4_boot_stage stage;
    bool use_stage;
    tv4_boot_handoff session;
    tv4_boot_handoff_config config;
    tv4_boot_release_handoff release_adapter;
    tv4_boot_release_handoff_config release_config;
    tv4_boot_release_handoff_receipt release_receipt;
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
    if (f->use_stage) {
        if (!tv4_boot_stage_acquire(&f->stage,id,view)) return false;
        f->locked=true;f->view=*view;return true;
    }
    f->locked = true;
    if (f->inject == ACQUIRE_BAD_TOKEN) f->view.token = 0;
    if (f->inject == ACQUIRE_BAD_OBJECT) f->view.object = 0;
    *view = f->view;
    if (f->inject == ACQUIRE_ABORT) tv4_boot_handoff_abort(&f->session);
    if (f->inject == ACQUIRE_TIMEOUT) f->now += f->config.lifetime_ms;
    if (f->inject == ACQUIRE_RELEASE_INPUTS) {
        memset(&f->release_config,0,sizeof(f->release_config));
        memset(f->metadata,0,sizeof(f->metadata));memset(f->signature,0,sizeof(f->signature));
        memset(&f->key,0,sizeof(f->key));
    }
    return true;
}
static bool inspect(void *context, uint64_t token, tv4_boot_source_view *view)
{
    fixture *f = context;
    assert(f->locked && token == f->view.token);
    ++f->inspected;
    if (f->use_stage) return tv4_boot_stage_inspect(&f->stage,token,view);
    *view = f->view;
    if (f->inject == INSPECT_BAD_TOKEN) ++view->token;
    return f->inject != INSPECT_FAIL;
}
static void release_busy(fixture *f)
{
    tv4_boot_release_handoff *a=&f->release_adapter;
    assert(a->handoff==&f->session && f->session.busy);
    memset(&f->release_receipt,0xa5,sizeof(f->release_receipt));
    tv4_boot_release_handoff_receipt before=f->release_receipt;
    assert(!tv4_boot_release_handoff_peek(a,&f->release_receipt));
    assert(memcmp(&before,&f->release_receipt,sizeof(before))==0);
    assert(!tv4_boot_release_handoff_begin(a,&f->session,&f->release_config,&f->rx));
    assert(!tv4_boot_release_handoff_verify(a,&f->workspace));
    assert(!tv4_boot_release_handoff_poll(a) && !tv4_boot_release_handoff_close(a));
    assert(!tv4_boot_release_handoff_clear(a));
    assert(tv4_boot_release_handoff_preflight(a,&f->preflight_profile,&f->workspace,&f->image_report)==TV4_IMAGE_BUSY);
    assert(!tv4_boot_completion_start_release(&f->completion,a,&f->expected,&f->preflight_profile,&f->workspace));
}
static bool release(void *context, uint64_t token)
{
    fixture *f = context;
    assert(f->locked && token == f->view.token && token != 0);
    ++f->releases;
    if (f->release_reentry) {
        if (f->release_adapter.handoff!=NULL) release_busy(f);
        assert(!tv4_boot_handoff_close(&f->session));
        assert(!tv4_boot_handoff_clear(&f->session));
        assert(!tv4_boot_handoff_poll(&f->session));
        assert(!tv4_boot_handoff_begin(&f->session, &f->config, &f->rx));
    }
    if (f->releases <= f->release_failures) return false;
    if (f->use_stage) assert(tv4_boot_stage_release(&f->stage,token));
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
        case REENTER_ABORT:
            if (f->release_adapter.handoff!=NULL) tv4_boot_release_handoff_abort(&f->release_adapter);
            else tv4_boot_handoff_abort(&f->session); /* defer release until read returns */
            assert(f->locked && f->releases == 0); break;
        case PREFLIGHT_PROFILE_MUTATE: f->preflight_profile.image_base = 0; f->preflight_profile.id[0] = 'x'; break;
        case COMPLETION_INPUT_MUTATE: f->preflight_profile.image_base = 0; f->expected.metadata_sha256[0] ^= 1; break;
        default: break;
        }
    }
    if (f->release_adapter.handoff!=NULL && (f->inject==REENTER || f->inject==REENTER_ABORT ||
        f->inject==COMPLETION_REENTER || f->inject==COMPLETION_ABORT)) release_busy(f);
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
    if (f->use_stage) {
        if (f->inject==STAGE_LOCK) {
            tv4_boot_stage snapshot=f->stage;tv4_update_ack ack;tv4_boot_source_view view;
            tv4_boot_stage_config cfg={.buffer=f->staging,.capacity=sizeof(f->staging),.boot=f->owner.boot,.object=91};
            assert(!tv4_boot_stage_discard(&f->stage) && !tv4_boot_stage_init(&f->stage,&cfg));
            assert(!tv4_boot_stage_begin(&f->stage,&f->rx));
            assert(tv4_boot_stage_consume(&f->stage,&ack,f->now)==TV4_UPDATE_REJECT_STATE);
            assert(!tv4_boot_stage_acquire(&f->stage,&f->descriptor,&view));
            assert(!tv4_boot_stage_release(&f->stage,token+1));
            assert(memcmp(&snapshot,&f->stage,sizeof(snapshot))==0);
        }
        if (capacity>f->fragment) capacity=f->fragment;
        return tv4_boot_stage_read(&f->stage,token,offset,bytes,capacity,count,eof);
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
/* Actual portable sender/receiver. Legacy fixtures use a test RAM consumer;
 * opt-in staging fixtures use the real portable RAM source. Lose one DATA
 * and final ACK, then consume duplicates without another write. */
static void transfer(fixture *f)
{
    if (f->use_stage) {
        tv4_boot_stage_config cfg={.buffer=f->staging,.capacity=sizeof(f->staging),.boot=f->owner.boot,.object=91};
        assert(tv4_boot_stage_init(&f->stage,&cfg) && tv4_boot_stage_begin(&f->stage,&f->rx));
    }
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
        tv4_update_result result = wire_receive_data(&f->rx, &packet, &ack, f->now);
        if (result == TV4_UPDATE_PENDING) {
            tv4_update_block pending;
            assert(tv4_update_rx_pending(&f->rx, &pending, f->now) == TV4_UPDATE_PENDING);
            assert(pending.offset == f->staged_bytes);
            assert(wire_receive_data(&f->rx, &packet, &ack, f->now) == TV4_UPDATE_PENDING);
            if (f->use_stage) {
                assert(tv4_boot_stage_consume(&f->stage,&ack,f->now)==TV4_UPDATE_ACK);
                f->staged_bytes=f->stage.written;
            } else {
                memcpy(f->staging + pending.offset, pending.bytes, pending.length);
                f->staged_bytes += pending.length;
                assert(tv4_update_rx_consume(&f->rx, pending.id, pending.sequence, true, &ack, f->now) == TV4_UPDATE_ACK);
            }
        } else assert(result == TV4_UPDATE_ACK);
        if (f->rx.context.state == TV4_UPDATE_COMPLETE_UNVERIFIED && f->lost_final_ack == 0) {
            ++f->lost_final_ack; f->now += 10; continue;
        }
        assert(wire_ack_data(&f->tx, &ack, f->now) == TV4_UPDATE_OK);
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
    assert(wire_receive_data(&f.rx, &duplicate, &ack, f.now) == TV4_UPDATE_ACK);
    assert(tv4_boot_handoff_peek(&f.session, &f.receipt) && f.releases == 0 && f.acquired == 1);
    /* Last ACK retries remain bounded even while a verified candidate exists. */
    for (unsigned i = 0; i < 4; ++i) (void)wire_receive_data(&f.rx, &duplicate, &ack, f.now);
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
        assert(wire_receive_outcome(&rx,&packet,&ack,&delivery,f.now)==TV4_OUTCOME_DELIVERED); /* lost ACK */
        f.now+=10; assert(tv4_outcome_tx_next(&tx,&packet,f.now)==TV4_OUTCOME_SENT);
        assert(wire_receive_outcome(&rx,&packet,&ack,&delivery,f.now)==TV4_OUTCOME_ACK);
        assert(wire_ack_outcome(&tx,&ack,f.now)==TV4_OUTCOME_OK && tv4_update_outcome_equal(&delivery,&f.outcome));
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

/* Test-only adapter: replace the fixture's unused legacy DATA setup with a
 * fresh negotiated ID. No wire, source lease or storage is opened by handshake.
 * Existing owner boot/metadata/signing policy are explicit model fixtures. */
static void negotiate_source(fixture *f,unsigned drop_mask)
{
    tv4_update_limits data=f->rx.context.limits;
    assert(f->staged_bytes==0 && f->acquired==0);
    assert(tv4_update_rx_abort(&f->rx,f->descriptor.id,f->now)==TV4_UPDATE_OK);
    assert(tv4_update_tx_abort(&f->tx,f->descriptor.id,f->now)==TV4_UPDATE_OK);
    tv4_neg_pool sender={0},receiver={0};tv4_negotiation a={0},b={0};
    uint8_t tag[32];digest((const uint8_t *)"test-negotiation-context",sizeof("test-negotiation-context")-1,tag);
    assert(tv4_neg_pool_init(&sender,11,f->owner.boot,0));
    assert(tv4_neg_pool_init(&receiver,f->owner.boot,11,f->descriptor.id.transfer));
    tv4_neg_limits limits={.max_image_bytes=IMAGE_MAX,.retry_ms=10,.total_ms=1000,.block_ceiling=128,.send_limit=8};
    assert(tv4_neg_init(&a,&sender,TV4_NEG_REQUESTER,limits,tag,2001,f->now));
    limits.block_ceiling=31;
    assert(tv4_neg_init(&b,&receiver,TV4_NEG_RESPONDER,limits,tag,f->owner.policy_generation,f->now));
    tv4_neg_image image={.image_bytes=f->descriptor.image_bytes};
    memcpy(image.image_sha256,f->descriptor.image_sha256,32);
    digest(f->metadata,f->config.metadata_bytes,image.metadata_sha256);
    assert(tv4_neg_begin(&a,&image,2001,f->now)==TV4_NEG_OK);
    unsigned dropped=0;
    for(unsigned step=0;step<40 && (a.state!=TV4_NEG_READY || b.state!=TV4_NEG_READY);step++) {
        for(unsigned side=0;side<2;side++) {
            tv4_negotiation *from=side?&b:&a,*to=side?&a:&b;
            uint64_t from_generation=side?f->owner.policy_generation:2001;
            uint64_t to_generation=side?2001:f->owner.policy_generation;
            tv4_neg_message packet;
            tv4_neg_result r=tv4_neg_next(from,&packet,from_generation,f->now);
            if(r==TV4_NEG_WAIT)continue;
            assert(r==TV4_NEG_SENT);
            unsigned bit=1u<<(unsigned)packet.kind;
            if((drop_mask&bit)!=0 && (dropped&bit)==0) { dropped|=bit;continue; }
            r=wire_receive_neg(to,&packet,to_generation,f->now);
            assert(r==TV4_NEG_OK || r==TV4_NEG_DUPLICATE);
        }
        f->now+=10;
    }
    assert(a.state==TV4_NEG_READY && b.state==TV4_NEG_READY && dropped==drop_mask);
    tv4_neg_message old_request=a.request;
    tv4_negotiated tx,rx;
    assert(tv4_neg_take(&a,&tx,2001,f->now)==TV4_NEG_OK);
    assert(tv4_neg_take(&b,&rx,f->owner.policy_generation,f->now)==TV4_NEG_OK);
    assert(tx.descriptor.id.boot==f->owner.boot && tx.descriptor.id.transfer==rx.descriptor.id.transfer);
    assert(tx.block_bytes==31 && rx.block_bytes==31);
    assert(!memcmp(tx.metadata_sha256,rx.metadata_sha256,32));
    f->descriptor=tx.descriptor;f->view.id=tx.descriptor.id;++f->view.generation;
    f->expected.image=tx.descriptor;memcpy(f->expected.metadata_sha256,tx.metadata_sha256,32);
    data.block_bytes=tx.block_bytes;
    assert(tv4_update_rx_init(&f->rx,data,rx.descriptor.id.boot,f->now));
    assert(tv4_update_tx_init(&f->tx,data,tx.descriptor.id.boot,f->now));
    assert(tv4_update_rx_begin(&f->rx,&rx.descriptor,f->now)==TV4_UPDATE_OK);
    assert(tv4_update_tx_begin(&f->tx,&tx.descriptor,f->now)==TV4_UPDATE_OK);
    assert(tv4_neg_take(&a,&tx,2001,f->now)==TV4_NEG_REJECT_STATE);
    assert(tv4_neg_close(&a,2001,f->now)==TV4_NEG_OK);
    assert(tv4_neg_close(&b,f->owner.policy_generation,f->now)==TV4_NEG_OK);
    assert(tv4_neg_clear(&a) && tv4_neg_clear(&b));
    assert(wire_receive_neg(&b,&old_request,f->owner.policy_generation,f->now)==TV4_NEG_REJECT_SESSION);
    assert(f->rx.context.next_offset==0 && f->tx.context.next_offset==0 && f->acquired==0);
}
static void negotiated_full_chain(void)
{
    for(unsigned i=0;i<6;i++) {
        fixture f;vector_fixture(&f,i%2u?256:33);
        unsigned mask=i<4 ? 1u<<(i+1u) : i==4 ? 0u : 30u;
        negotiate_source(&f,mask);transfer(&f);begin(&f);
        assert(tv4_boot_completion_start(&f.completion,&f.session,&f.expected,&f.preflight_profile,&f.workspace));
        completed(&f,TV4_OUTCOME_CHECKED_CLOSED,TV4_OUTCOME_REASON_NONE);
        tv4_outcome_tx tx;tv4_outcome_rx rx;tv4_update_outcome packet,ack,delivery;
        tv4_outcome_limits limits={.ack_timeout_ms=10,.linger_ms=50,.total_timeout_ms=100,.retry_limit=3};
        assert(tv4_outcome_tx_init(&tx,limits,f.owner.boot,f.now));
        assert(tv4_outcome_rx_init(&rx,limits,f.now));
        assert(tv4_outcome_tx_begin(&tx,&f.outcome,f.now)==TV4_OUTCOME_OK);
        assert(tv4_outcome_rx_begin(&rx,&f.tx,f.expected.metadata_sha256,f.now)==TV4_OUTCOME_OK);
        assert(tv4_outcome_tx_next(&tx,&packet,f.now)==TV4_OUTCOME_SENT); /* lost */
        f.now+=10;assert(tv4_outcome_tx_next(&tx,&packet,f.now)==TV4_OUTCOME_SENT);
        assert(wire_receive_outcome(&rx,&packet,&ack,&delivery,f.now)==TV4_OUTCOME_DELIVERED); /* ACK lost */
        f.now+=10;assert(tv4_outcome_tx_next(&tx,&packet,f.now)==TV4_OUTCOME_SENT);
        assert(wire_receive_outcome(&rx,&packet,&ack,&delivery,f.now)==TV4_OUTCOME_ACK);
        assert(wire_ack_outcome(&tx,&ack,f.now)==TV4_OUTCOME_OK);
        assert(tv4_update_outcome_equal(&delivery,&f.outcome) && f.releases==1 && !f.locked);
        finish(&f);
    }
    puts("PASS 6 signed negotiation -> DATA -> P-256/SHA -> vectors -> close -> outcome/ACK chains; all four handshake losses, DATA/final ACK/report losses");
}
static void negotiated_binding_still_requires_crypto(void)
{
    for(unsigned which=0;which<3;which++) {
        fixture f;vector_fixture(&f,33);negotiate_source(&f,0);
        if(which==0)f.source[f.source_bytes-1]^=1; /* CRC follows changed RAM; signed SHA must fail */
        else if(which==1)f.signature[f.config.signature_bytes-1]^=1;
        else {
            uint8_t *field=(uint8_t *)strstr((char *)f.metadata,"\"image_counter\":3");assert(field);
            field[strlen("\"image_counter\":")]='4';sign_metadata(&f); /* valid new metadata, wrong negotiated raw digest */
        }
        transfer(&f);begin(&f);
        assert(tv4_boot_completion_start(&f.completion,&f.session,&f.expected,&f.preflight_profile,&f.workspace));
        if(which==0)completed(&f,TV4_OUTCOME_REJECTED_CLOSED,TV4_OUTCOME_HASH);
        else if(which==1)completed(&f,TV4_OUTCOME_REJECTED_CLOSED,TV4_OUTCOME_SIGNATURE);
        else completed(&f,TV4_OUTCOME_FAILED_CLOSED,TV4_OUTCOME_BINDING);
        finish(&f);
    }
    puts("PASS 3 negotiated binding failures: changed image, invalid signature, valid re-signed metadata with mismatched raw SHA; no retained lease");
}

/* All identities/maps/versions below are synthetic host fixtures. No local
 * installed version, key provisioning or real F407 Flash geometry is inferred. */
#define RELEASE_SOURCE "0123456789abcdef0123456789abcdef01234567"
static void release_document(fixture *f)
{
    char hex[65];
    for (size_t i=0;i<32;++i) (void)snprintf(hex+2u*i,3,"%02x",f->descriptor.image_sha256[i]);
    int n=snprintf((char *)f->metadata,sizeof(f->metadata),
        "{\"schema\":\"%s\",\"component\":\"motion\",\"hardware_id\":\"test-f407\","
        "\"hardware_revision\":\"synthetic\",\"memory_profile\":\"synthetic-vector-v1\","
        "\"image_counter\":3,\"minimum_bootloader_counter\":5,\"protocol_min\":6,\"protocol_max\":8,"
        "\"image_size\":%zu,\"sha256\":\"%s\",\"release_version\":\"3.0.0\","
        "\"component_version\":\"2.1.0\",\"channel\":\"stable\",\"source_commit\":\"%s\"}",
        TV4_BOOT_RELEASE_SCHEMA,f->source_bytes,hex,RELEASE_SOURCE);
    assert(n>0 && (size_t)n<sizeof(f->metadata));f->config.metadata_bytes=(size_t)n;sign_metadata(f);
}
static void release_fixture(fixture *f,uint16_t vectors)
{
    vector_fixture(f,vectors);release_document(f);
    f->release_config=(tv4_boot_release_handoff_config){.policy={.image=f->config.policy,
        .installed_component_version="2.0.0",.minimum_component_version="2.0.1",
        .selected_release_version="3.0.0",.selected_source_commit=RELEASE_SOURCE,.channel="stable"},
        .ops=f->config.ops,.metadata=f->metadata,.signature=f->signature,.key_id=f->config.key_id,
        .metadata_bytes=f->config.metadata_bytes,.signature_bytes=f->config.signature_bytes,
        .key_id_bytes=f->config.key_id_bytes,.arena_bytes=f->config.arena_bytes,
        .lifetime_ms=f->config.lifetime_ms,.release_attempt_limit=f->config.release_attempt_limit};
}
static void release_begin(fixture *f)
{
    assert(tv4_boot_release_handoff_begin(&f->release_adapter,&f->session,&f->release_config,&f->rx));
    assert(f->session.release_document && f->session.state==TV4_HANDOFF_BOUND && f->locked);
}
static void release_clear(fixture *f)
{
    assert(tv4_boot_release_handoff_clear(&f->release_adapter));
    assert(zero(&f->release_adapter,sizeof(f->release_adapter)) && zero(&f->session,sizeof(f->session)));
}
static void release_full_chain(void)
{
    for (unsigned i=0;i<6;++i) {
        fixture f;release_fixture(&f,i%2u?256:33);
        negotiate_source(&f,i<4 ? 1u<<(i+1u) : i==4 ? 0u : 30u);transfer(&f);release_begin(&f);
        assert(tv4_boot_completion_start_release(&f.completion,&f.release_adapter,&f.expected,&f.preflight_profile,&f.workspace));
        completed(&f,TV4_OUTCOME_CHECKED_CLOSED,TV4_OUTCOME_REASON_NONE);
        assert(zero(&f.release_adapter.candidate,sizeof(f.release_adapter.candidate)));
        assert(zero(&f.release_adapter.policy,sizeof(f.release_adapter.policy)));
        unsigned reads=f.reads;
        tv4_outcome_tx tx;tv4_outcome_rx rx;tv4_update_outcome packet,ack,delivery;
        tv4_outcome_limits limits={.ack_timeout_ms=10,.linger_ms=50,.total_timeout_ms=100,.retry_limit=3};
        assert(tv4_outcome_tx_init(&tx,limits,f.owner.boot,f.now));
        assert(tv4_outcome_rx_init(&rx,limits,f.now));
        assert(tv4_outcome_tx_begin(&tx,&f.outcome,f.now)==TV4_OUTCOME_OK);
        assert(tv4_outcome_rx_begin(&rx,&f.tx,f.expected.metadata_sha256,f.now)==TV4_OUTCOME_OK);
        assert(tv4_outcome_tx_next(&tx,&packet,f.now)==TV4_OUTCOME_SENT); /* lost report */
        f.now+=10;assert(tv4_outcome_tx_next(&tx,&packet,f.now)==TV4_OUTCOME_SENT);
        assert(wire_receive_outcome(&rx,&packet,&ack,&delivery,f.now)==TV4_OUTCOME_DELIVERED); /* lost ACK */
        f.now+=10;assert(tv4_outcome_tx_next(&tx,&packet,f.now)==TV4_OUTCOME_SENT);
        assert(wire_receive_outcome(&rx,&packet,&ack,&delivery,f.now)==TV4_OUTCOME_ACK);
        assert(wire_ack_outcome(&tx,&ack,f.now)==TV4_OUTCOME_OK && tv4_update_outcome_equal(&delivery,&f.outcome));
        assert(f.releases==1 && f.reads==reads && !f.locked);
        assert(tv4_boot_completion_clear(&f.completion));release_clear(&f);finish(&f);
    }
    puts("PASS 6 release15 negotiation/DATA/signature/SHA/vector/close/outcome chains; handshake/DATA/ACK/report losses, exactly one release");
}
static void release_live_snapshot(void)
{
    fixture f;release_fixture(&f,256);transfer(&f);f.inject=ACQUIRE_RELEASE_INPUTS;release_begin(&f);
    uint32_t started=f.rx.context.started_ms,progress=f.rx.context.progress_ms;
    assert(tv4_boot_release_handoff_poll(&f.release_adapter));
    /* Caller data can change after capture; only owner generation changes the
     * live policy. Deliberate mutation below is a test, never a provider model. */
    memset(f.metadata,0,sizeof(f.metadata));memset(f.signature,0,sizeof(f.signature));
    memset(&f.key,0,sizeof(f.key));memset(&f.release_config,0,sizeof(f.release_config));
    assert(tv4_boot_release_handoff_verify(&f.release_adapter,&f.workspace));
    assert(tv4_boot_release_handoff_peek(&f.release_adapter,&f.release_receipt));
    assert(strcmp(f.release_receipt.release.release_version,"3.0.0")==0);
    assert(strcmp(f.release_receipt.release.component_version,"2.1.0")==0);
    assert(strcmp(f.release_receipt.release.source_commit,RELEASE_SOURCE)==0);
    assert(strcmp(f.release_receipt.release.channel,"stable")==0);
    assert(memcmp(&f.release_receipt.source.image,&f.release_receipt.release.image,sizeof(tv4_boot_candidate))==0);
    assert(f.release_receipt.source.source_generation==f.view.generation && f.release_receipt.source.policy_generation==f.owner.policy_generation);
    assert(tv4_boot_release_handoff_preflight(&f.release_adapter,&f.preflight_profile,&f.workspace,&f.image_report)==TV4_IMAGE_PROFILE_MODEL_MATCH);
    assert(f.image_report.vectors.vector_count==256 && f.rx.context.started_ms==started && f.rx.context.progress_ms==progress);
    assert(zero(&f.workspace,sizeof(f.workspace)) && !tv4_boot_release_handoff_clear(&f.release_adapter));
    assert(tv4_boot_release_handoff_close(&f.release_adapter));
    memset(&f.release_receipt,0xa5,sizeof(f.release_receipt));
    assert(!tv4_boot_release_handoff_peek(&f.release_adapter,&f.release_receipt) && zero(&f.release_receipt,sizeof(f.release_receipt)));
    release_clear(&f);finish(&f);
}
static void release_live_revocation(void)
{
    for (unsigned i=0;i<5;++i) {
        fixture f;release_fixture(&f,33);transfer(&f);release_begin(&f);
        assert(tv4_boot_release_handoff_verify(&f.release_adapter,&f.workspace));
        if (i==0) ++f.owner.policy_generation;
        if (i==1) ++f.view.generation;
        if (i==2) assert(tv4_update_rx_abort(&f.rx,f.descriptor.id,f.now)==TV4_UPDATE_OK);
        if (i==3) f.now+=f.release_config.lifetime_ms;
        if (i==4) f.cancel=true;
        memset(&f.release_receipt,0xa5,sizeof(f.release_receipt));
        assert(!tv4_boot_release_handoff_peek(&f.release_adapter,&f.release_receipt) && zero(&f.release_receipt,sizeof(f.release_receipt)));
        assert(!f.locked && f.releases==1 && zero(&f.release_adapter.candidate,sizeof(f.release_adapter.candidate)));
        release_clear(&f);finish(&f);
    }
    const fault faults[]={OWNER_BOOT,OWNER_POLICY,SOURCE_GENERATION,SOURCE_SESSION,RX_REINIT,RX_HASH,READ_TIMEOUT,READ_BACKWARD,EOF_SOURCE,READ_FAIL};
    for (size_t i=0;i<sizeof(faults)/sizeof(*faults);++i) {
        fixture f;release_fixture(&f,33);transfer(&f);release_begin(&f);f.inject=faults[i];
        assert(!tv4_boot_release_handoff_verify(&f.release_adapter,&f.workspace));
        assert(f.releases==1 && !f.locked && zero(&f.workspace,sizeof(f.workspace)));
        assert(zero(&f.release_adapter.candidate,sizeof(f.release_adapter.candidate)));
        release_clear(&f);finish(&f);
    }
}
static void release_completion_rejections(void)
{
    const tv4_outcome_reason reasons[]={TV4_OUTCOME_COMPATIBILITY,TV4_OUTCOME_COMPATIBILITY,TV4_OUTCOME_COMPATIBILITY,
        TV4_OUTCOME_ROLLBACK,TV4_OUTCOME_SIGNATURE,TV4_OUTCOME_HASH,TV4_OUTCOME_VECTORS,TV4_OUTCOME_BINDING};
    for (unsigned i=0;i<8;++i) {
        fixture f;release_fixture(&f,33);
        if (i==0) f.release_config.policy.selected_release_version[0]='4';
        if (i==1) f.release_config.policy.selected_source_commit[0]='1';
        if (i==2) { strcpy(f.release_config.policy.channel,"prerelease");
            strcpy(f.release_config.policy.selected_release_version,"3.0.0-rc.1"); }
        if (i==3) strcpy(f.release_config.policy.installed_component_version,"2.1.0");
        if (i==4) f.signature[f.config.signature_bytes-1]^=1;
        if (i==6) { vector_word(f.source,0);bind_changed_image(&f);release_document(&f);
            f.release_config.metadata_bytes=f.config.metadata_bytes;f.release_config.signature_bytes=f.config.signature_bytes; }
        negotiate_source(&f,0);
        if (i==5) f.source[f.source_bytes-1]^=1;
        if (i==7) {
            uint8_t *field=(uint8_t *)strstr((char *)f.metadata,"\"image_counter\":3");assert(field);
            field[strlen("\"image_counter\":")]='4';sign_metadata(&f);f.release_config.signature_bytes=f.config.signature_bytes;
        }
        transfer(&f);release_begin(&f);
        assert(tv4_boot_completion_start_release(&f.completion,&f.release_adapter,&f.expected,&f.preflight_profile,&f.workspace));
        completed(&f,i==7 ? TV4_OUTCOME_FAILED_CLOSED : TV4_OUTCOME_REJECTED_CLOSED,reasons[i]);
        assert(f.releases==1);release_clear(&f);finish(&f);
    }
}
static void release_explicit_mode(void)
{
    fixture f;release_fixture(&f,33);transfer(&f);release_begin(&f);expected_binding(&f);
    tv4_boot_handoff before=f.session;
    memset(&f.receipt,0xa5,sizeof(f.receipt));tv4_boot_handoff_receipt receipt=f.receipt;
    memset(&f.image_report,0xa5,sizeof(f.image_report));tv4_boot_handoff_image_report report=f.image_report;
    assert(!tv4_boot_handoff_verify(&f.session,&f.workspace));
    assert(!tv4_boot_handoff_peek(&f.session,&f.receipt) && memcmp(&receipt,&f.receipt,sizeof(receipt))==0);
    assert(tv4_boot_handoff_preflight(&f.session,&f.preflight_profile,&f.workspace,&f.image_report)==TV4_IMAGE_ARGUMENT);
    assert(memcmp(&report,&f.image_report,sizeof(report))==0);
    assert(!tv4_boot_completion_start(&f.completion,&f.session,&f.expected,&f.preflight_profile,&f.workspace));
    assert(!tv4_boot_handoff_clear(&f.session) && memcmp(&before,&f.session,sizeof(before))==0);
    assert(tv4_boot_handoff_poll(&f.session)); /* conservative guard/close remain available */
    assert(tv4_boot_handoff_close(&f.session));assert(!tv4_boot_handoff_clear(&f.session));release_clear(&f);finish(&f);
    /* Document contents never choose the mode, including valid signatures. */
    release_fixture(&f,33);transfer(&f);begin(&f);
    assert(!tv4_boot_handoff_verify(&f.session,&f.workspace) && f.session.admission_result==TV4_BOOT_METADATA);finish(&f);
    release_fixture(&f,33);metadata(&f,(uint32_t)f.source_bytes);
    f.release_config.metadata_bytes=f.config.metadata_bytes;f.release_config.signature_bytes=f.config.signature_bytes;
    transfer(&f);release_begin(&f);
    assert(!tv4_boot_release_handoff_verify(&f.release_adapter,&f.workspace) && f.session.admission_result==TV4_BOOT_METADATA);
    release_clear(&f);finish(&f);
}
static void release_cleanup_and_late_context(void)
{
    for (unsigned i=0;i<2;++i) {
        fixture f;release_fixture(&f,33);transfer(&f);release_begin(&f);expected_binding(&f);f.release_failures=i==0?1:100;
        f.release_reentry=true;
        assert(tv4_boot_completion_start_release(&f.completion,&f.release_adapter,&f.expected,&f.preflight_profile,&f.workspace));
        assert(f.completion.state==TV4_COMPLETION_CLOSING && f.locked && f.releases==1);
        memset(&f.outcome,0xa5,sizeof(f.outcome));
        assert(!tv4_boot_completion_snapshot(&f.completion,&f.outcome) && zero(&f.outcome,sizeof(f.outcome)));
        assert(!tv4_boot_release_handoff_clear(&f.release_adapter) && !tv4_boot_completion_clear(&f.completion));
        if (i==0) {
            assert(tv4_boot_completion_poll(&f.completion));completed(&f,TV4_OUTCOME_FAILED_CLOSED,TV4_OUTCOME_RELEASE);
            assert(f.releases==2);release_clear(&f);
        } else {
            for (unsigned j=0;j<8;++j) assert(!tv4_boot_completion_poll(&f.completion));
            assert(f.completion.state==TV4_COMPLETION_QUARANTINED && f.locked && f.session.held && f.releases==3);
            assert(f.release_adapter.handoff==&f.session && f.session.source.token==81);
            assert(!tv4_boot_release_handoff_clear(&f.release_adapter) && !tv4_boot_completion_clear(&f.completion));
        }
        finish(&f);
    }
    const fault faults[]={RELEASE_POLICY,RELEASE_CANCEL,RELEASE_TIMEOUT};
    const tv4_outcome_reason reasons[]={TV4_OUTCOME_CONTEXT,TV4_OUTCOME_CANCEL,TV4_OUTCOME_TIMEOUT};
    for (size_t i=0;i<3;++i) {
        fixture f;release_fixture(&f,33);transfer(&f);release_begin(&f);expected_binding(&f);f.inject=faults[i];
        assert(tv4_boot_completion_start_release(&f.completion,&f.release_adapter,&f.expected,&f.preflight_profile,&f.workspace));
        completed(&f,i==1?TV4_OUTCOME_CANCELLED_CLOSED:TV4_OUTCOME_FAILED_CLOSED,reasons[i]);release_clear(&f);finish(&f);
    }
    fixture f;release_fixture(&f,33);transfer(&f);f.release_config.lifetime_ms=20000;release_begin(&f);expected_binding(&f);
    f.now+=f.rx.context.limits.progress_timeout_ms;
    assert(tv4_boot_completion_start_release(&f.completion,&f.release_adapter,&f.expected,&f.preflight_profile,&f.workspace));
    completed(&f,TV4_OUTCOME_FAILED_CLOSED,TV4_OUTCOME_TIMEOUT);release_clear(&f);finish(&f);
}
static void release_reentry_and_completion_snapshot(void)
{
    const fault faults[]={REENTER,REENTER_ABORT,COMPLETION_REENTER,COMPLETION_ABORT,COMPLETION_INPUT_MUTATE};
    for (size_t i=0;i<5;++i) {
        fixture f;release_fixture(&f,33);transfer(&f);release_begin(&f);expected_binding(&f);f.inject=faults[i];
        if (i<2) {
            assert(tv4_boot_release_handoff_verify(&f.release_adapter,&f.workspace)==(i==0));
            if (i==0) assert(tv4_boot_release_handoff_close(&f.release_adapter));
            assert(f.releases==1 && !f.locked);
        } else {
            assert(tv4_boot_completion_start_release(&f.completion,&f.release_adapter,&f.expected,&f.preflight_profile,&f.workspace));
            completed(&f,i==3?TV4_OUTCOME_CANCELLED_CLOSED:TV4_OUTCOME_CHECKED_CLOSED,i==3?TV4_OUTCOME_CANCEL:TV4_OUTCOME_REASON_NONE);
        }
        release_clear(&f);finish(&f);
    }
}
static void release_arguments_and_aliases(void)
{
    fixture f;release_fixture(&f,33);transfer(&f);
    tv4_boot_release_handoff *a=&f.release_adapter;
    assert(!tv4_boot_release_handoff_begin(NULL,&f.session,&f.release_config,&f.rx));
    assert(!tv4_boot_release_handoff_begin(a,&f.session,NULL,&f.rx));
    f.release_config.policy.installed_component_version[0]=0;
    assert(!tv4_boot_release_handoff_begin(a,&f.session,&f.release_config,&f.rx));
    assert(f.acquired==0 && zero(a,sizeof(*a)) && zero(&f.session,sizeof(f.session)));
    strcpy(f.release_config.policy.installed_component_version,"2.0.0");
    assert(!tv4_boot_release_handoff_begin((tv4_boot_release_handoff *)(void *)&f.session,&f.session,&f.release_config,&f.rx));
    f.release_config.metadata=(const uint8_t *)a;
    assert(!tv4_boot_release_handoff_begin(a,&f.session,&f.release_config,&f.rx));f.release_config.metadata=f.metadata;
    release_begin(&f);expected_binding(&f);tv4_boot_handoff before=f.session;tv4_boot_release_handoff copy=*a;
    assert(!tv4_boot_release_handoff_verify(a,(tv4_boot_workspace *)(void *)a));
    assert(!tv4_boot_release_handoff_peek(a,(tv4_boot_release_handoff_receipt *)(void *)a));
    assert(!tv4_boot_release_handoff_peek(a,(tv4_boot_release_handoff_receipt *)(void *)&f.rx));
    assert(tv4_boot_release_handoff_preflight(a,&f.preflight_profile,&f.workspace,(tv4_boot_handoff_image_report *)(void *)a)==TV4_IMAGE_ARGUMENT);
    assert(!tv4_boot_completion_start_release(&f.completion,a,&f.expected,NULL,&f.workspace));
    assert(!tv4_boot_completion_start_release((tv4_boot_completion *)(void *)a,a,&f.expected,&f.preflight_profile,&f.workspace));
    assert(!tv4_boot_completion_start_release(&f.completion,a,&f.expected,&f.preflight_profile,(tv4_boot_workspace *)(void *)a));
    assert(memcmp(&before,&f.session,sizeof(before))==0 && memcmp(&copy,a,sizeof(copy))==0 && f.reads==0);
    tv4_boot_release_handoff other={0};
    assert(!tv4_boot_release_handoff_begin(&other,&f.session,&f.release_config,&f.rx) && zero(&other,sizeof(other)));
    assert(tv4_boot_release_handoff_close(a));release_clear(&f);finish(&f);
    release_fixture(&f,33);transfer(&f);f.inject=ACQUIRE_FAIL;
    assert(!tv4_boot_release_handoff_begin(&f.release_adapter,&f.session,&f.release_config,&f.rx));
    assert(f.release_adapter.handoff==&f.session && f.session.state==TV4_HANDOFF_CLOSED && !f.locked && f.releases==0);
    release_clear(&f);finish(&f);
}

static void staged_signed_chains(void)
{
    for(unsigned i=0;i<8;++i) {
        fixture f;if(i<6)release_fixture(&f,i%2u?256:33);else vector_fixture(&f,33);
        f.use_stage=true;negotiate_source(&f,i<4?1u<<(i+1u):30u);transfer(&f);
        assert(f.stage.state==TV4_STAGE_SEALED && f.stage.next_sequence==f.rx.context.next_sequence);
        if(i<6)release_begin(&f);else begin(&f);
        f.inject=STAGE_LOCK;f.fragment=i%3u==0?1:i%3u==1?7:256;
        bool ok=i<6?tv4_boot_completion_start_release(&f.completion,&f.release_adapter,&f.expected,&f.preflight_profile,&f.workspace):
            tv4_boot_completion_start(&f.completion,&f.session,&f.expected,&f.preflight_profile,&f.workspace);
        assert(ok);completed(&f,TV4_OUTCOME_CHECKED_CLOSED,TV4_OUTCOME_REASON_NONE);
        assert(f.stage.state==TV4_STAGE_SEALED && f.releases==1);
        assert(tv4_boot_stage_discard(&f.stage) && zero(f.staging,f.source_bytes));
        /* The report remains history after actual source bytes were wiped. */
        tv4_outcome_tx tx;tv4_outcome_rx rx;tv4_update_outcome packet,ack,delivery;
        tv4_outcome_limits limits={.ack_timeout_ms=10,.linger_ms=50,.total_timeout_ms=100,.retry_limit=3};
        assert(tv4_outcome_tx_init(&tx,limits,f.owner.boot,f.now) && tv4_outcome_rx_init(&rx,limits,f.now));
        assert(tv4_outcome_tx_begin(&tx,&f.outcome,f.now)==TV4_OUTCOME_OK);
        assert(tv4_outcome_rx_begin(&rx,&f.tx,f.expected.metadata_sha256,f.now)==TV4_OUTCOME_OK);
        assert(tv4_outcome_tx_next(&tx,&packet,f.now)==TV4_OUTCOME_SENT); /* lost report */
        f.now+=10;assert(tv4_outcome_tx_next(&tx,&packet,f.now)==TV4_OUTCOME_SENT);
        assert(wire_receive_outcome(&rx,&packet,&ack,&delivery,f.now)==TV4_OUTCOME_DELIVERED); /* lost ACK */
        f.now+=10;assert(tv4_outcome_tx_next(&tx,&packet,f.now)==TV4_OUTCOME_SENT);
        assert(wire_receive_outcome(&rx,&packet,&ack,&delivery,f.now)==TV4_OUTCOME_ACK);
        assert(wire_ack_outcome(&tx,&ack,f.now)==TV4_OUTCOME_OK && tv4_update_outcome_equal(&delivery,&f.outcome));
        assert(f.releases==1 && tv4_boot_completion_clear(&f.completion));
        if(i<6)release_clear(&f);else assert(tv4_boot_handoff_clear(&f.session));finish(&f);
    }
    puts("PASS 8 signed chains through real RAM staging backend, 6 release15 + 2 legacy11; losses, locked writes, post-wipe outcome/ACK");
}
static void staged_crypto_rejections(void)
{
    const tv4_outcome_reason reasons[]={TV4_OUTCOME_HASH,TV4_OUTCOME_SIGNATURE,TV4_OUTCOME_VECTORS,TV4_OUTCOME_COMPATIBILITY,TV4_OUTCOME_BINDING};
    for(unsigned i=0;i<5;++i) {
        fixture f;release_fixture(&f,33);f.use_stage=true;
        if(i==1)f.signature[f.config.signature_bytes-1]^=1;
        if(i==2) {
            vector_word(f.source,0);bind_changed_image(&f);release_document(&f);
            f.release_config.metadata_bytes=f.config.metadata_bytes;f.release_config.signature_bytes=f.config.signature_bytes;
        }
        if(i==3)f.release_config.policy.selected_source_commit[0]='1';
        negotiate_source(&f,0);
        if(i==0)f.source[f.source_bytes-1]^=1;
        if(i==4) {
            uint8_t *field=(uint8_t *)strstr((char *)f.metadata,"\"image_counter\":3");assert(field);
            field[strlen("\"image_counter\":")]='4';sign_metadata(&f);f.release_config.signature_bytes=f.config.signature_bytes;
        }
        transfer(&f);release_begin(&f);
        assert(tv4_boot_completion_start_release(&f.completion,&f.release_adapter,&f.expected,&f.preflight_profile,&f.workspace));
        completed(&f,i==4?TV4_OUTCOME_FAILED_CLOSED:TV4_OUTCOME_REJECTED_CLOSED,reasons[i]);
        assert(f.stage.state==TV4_STAGE_SEALED && f.releases==1);
        assert(tv4_boot_stage_discard(&f.stage));release_clear(&f);finish(&f);
    }
}
static void staged_cleanup_and_revocation(void)
{
    for(unsigned i=0;i<3;++i) {
        fixture f;release_fixture(&f,33);f.use_stage=true;negotiate_source(&f,0);transfer(&f);release_begin(&f);
        if(i==0)f.inject=OWNER_POLICY;else f.release_failures=i==1?1:100;
        assert(tv4_boot_completion_start_release(&f.completion,&f.release_adapter,&f.expected,&f.preflight_profile,&f.workspace));
        if(i==0)completed(&f,TV4_OUTCOME_FAILED_CLOSED,TV4_OUTCOME_CONTEXT);
        else {
            assert(f.completion.state==TV4_COMPLETION_CLOSING && f.stage.state==TV4_STAGE_LEASED);
            assert(!tv4_boot_stage_discard(&f.stage));
            memset(&f.outcome,0xa5,sizeof(f.outcome));assert(!tv4_boot_completion_snapshot(&f.completion,&f.outcome) && zero(&f.outcome,sizeof(f.outcome)));
            if(i==1) {assert(tv4_boot_completion_poll(&f.completion));completed(&f,TV4_OUTCOME_FAILED_CLOSED,TV4_OUTCOME_RELEASE);}
            else {
                for(unsigned j=0;j<8;++j)assert(!tv4_boot_completion_poll(&f.completion));
                assert(f.completion.state==TV4_COMPLETION_QUARANTINED && f.stage.state==TV4_STAGE_LEASED && f.releases==3);
                assert(!tv4_boot_stage_discard(&f.stage) && !tv4_boot_release_handoff_clear(&f.release_adapter));finish(&f);continue;
            }
        }
        assert(f.stage.state==TV4_STAGE_SEALED && tv4_boot_stage_discard(&f.stage));release_clear(&f);finish(&f);
    }
}
static void staged_receiver_deadline(void)
{
    fixture f;release_fixture(&f,33);f.use_stage=true;negotiate_source(&f,0);transfer(&f);
    f.now+=f.rx.context.limits.progress_timeout_ms;
    assert(!tv4_boot_release_handoff_begin(&f.release_adapter,&f.session,&f.release_config,&f.rx));
    assert(f.stage.state==TV4_STAGE_SEALED && f.acquired==0 && f.releases==0 && f.session.state==TV4_HANDOFF_CLOSED);
    assert(f.session.error==TV4_HANDOFF_RECEIVER && f.rx.context.error==TV4_UPDATE_ERROR_PROGRESS_TIMEOUT);
    assert(tv4_boot_stage_discard(&f.stage));release_clear(&f);finish(&f);
}
/* One-clock HOST fixture. All upper APIs, fragment polls, queued deliveries
 * and verifier now_ms observe f->now. Limits below are explicit test scenarios,
 * never rewritten after begin and never production timing defaults. */
static tv4_test_link_config timed_config(uint16_t drops,uint16_t index)
{
    return (tv4_test_link_config){.fragments={8,128,160,TV4_FRAGMENT_VERSION},
        .delivery_ms=1,.held_delivery_ms=3,.hold_every=11,.duplicate_every=17,
        .drop_kinds=drops,.drop_index=index,.recover_fragment_timeouts=true};
}
static bool timed_idle(const tv4_test_link *link)
{return tv4_test_link_idle(link);}
typedef struct {tv4_neg_pool pool[2];tv4_negotiation n[2];} timed_image_session;
static void timed_negotiate_impl(fixture *f,tv4_test_link *link,uint16_t block,timed_image_session *keep)
{
    assert(tv4_update_rx_abort(&f->rx,f->descriptor.id,f->now)==TV4_UPDATE_OK);
    assert(tv4_update_tx_abort(&f->tx,f->descriptor.id,f->now)==TV4_UPDATE_OK);
    timed_image_session local={0};timed_image_session *saved=keep!=NULL?keep:&local;
    tv4_neg_pool *pool=saved->pool;tv4_negotiation *n=saved->n;
    uint8_t tag[32];digest((const uint8_t *)"timed-fixture",13,tag);
    assert(tv4_neg_pool_init(&pool[0],11,f->owner.boot,0));
    assert(tv4_neg_pool_init(&pool[1],f->owner.boot,11,f->descriptor.id.transfer));
    tv4_neg_limits limits={.max_image_bytes=IMAGE_MAX,.retry_ms=200,.total_ms=4000,.block_ceiling=block,.send_limit=8};
    assert(tv4_neg_init(&n[0],&pool[0],TV4_NEG_REQUESTER,limits,tag,2001,f->now));
    assert(tv4_neg_init(&n[1],&pool[1],TV4_NEG_RESPONDER,limits,tag,f->owner.policy_generation,f->now));
    tv4_neg_image image={.image_bytes=f->descriptor.image_bytes};memcpy(image.image_sha256,f->descriptor.image_sha256,32);
    digest(f->metadata,f->config.metadata_bytes,image.metadata_sha256);
    assert(tv4_neg_begin(&n[0],&image,2001,f->now)==TV4_NEG_OK);
    uint32_t started=f->now,responder_started=0;bool responder_live=false;unsigned ticks;
    for(ticks=0;ticks<4000;++ticks) {
        for(unsigned side=0;side<2;++side) {
            uint64_t generation=side?f->owner.policy_generation:2001;
            assert(tv4_neg_poll(&n[side],generation,f->now)==TV4_NEG_OK);
            tv4_wire_message packet;
            if(tv4_test_link_take(link,side,&packet)==TV4_FRAG_TAKEN) {
                unsigned target=1u-side;uint64_t to_generation=target?f->owner.policy_generation:2001;
                tv4_neg_result r=tv4_neg_receive(&n[target],&packet.body.negotiation,to_generation,f->now);
                assert(r==TV4_NEG_OK || r==TV4_NEG_DUPLICATE);
            }
        }
        bool ready=n[0].state==TV4_NEG_READY && n[1].state==TV4_NEG_READY;
        if(ready && timed_idle(link))break;
        if(!ready)for(unsigned side=0;side<2;++side)if(tv4_test_link_can_send(link,side)) {
            tv4_wire_message packet={0};uint64_t generation=side?f->owner.policy_generation:2001;
            tv4_neg_result r=tv4_neg_next(&n[side],&packet.body.negotiation,generation,f->now);
            if(r==TV4_NEG_SENT) {
                packet.kind=(tv4_wire_kind)packet.body.negotiation.kind;
                assert(tv4_test_link_send(link,side,&packet)==TV4_FRAG_OK);
            }else assert(r==TV4_NEG_WAIT);
        }
        assert(n[0].started_ms==started);
        if(n[1].state!=TV4_NEG_IDLE) {
            if(!responder_live){responder_started=n[1].started_ms;responder_live=true;}
            assert(n[1].started_ms==responder_started);
        }
        tv4_test_link_step(link);
    }
    assert(ticks<4000);
    tv4_negotiated parameters[2];
    assert(tv4_neg_take(&n[0],&parameters[0],2001,f->now)==TV4_NEG_OK);
    assert(tv4_neg_take(&n[1],&parameters[1],f->owner.policy_generation,f->now)==TV4_NEG_OK);
    assert(parameters[0].block_bytes==block && parameters[1].block_bytes==block);
    f->descriptor=parameters[0].descriptor;f->view.id=f->descriptor.id;++f->view.generation;
    f->expected.image=f->descriptor;memcpy(f->expected.metadata_sha256,parameters[0].metadata_sha256,32);
    tv4_update_limits data={.max_image_bytes=IMAGE_MAX,.ack_timeout_ms=200,.progress_timeout_ms=3000,
        .total_timeout_ms=10000,.block_bytes=block,.retry_limit=4};
    assert(tv4_update_rx_init(&f->rx,data,f->owner.boot,f->now) && tv4_update_tx_init(&f->tx,data,f->owner.boot,f->now));
    assert(tv4_update_rx_begin(&f->rx,&f->descriptor,f->now)==TV4_UPDATE_OK && tv4_update_tx_begin(&f->tx,&f->descriptor,f->now)==TV4_UPDATE_OK);
    assert(tv4_neg_close(&n[0],2001,f->now)==TV4_NEG_OK && tv4_neg_close(&n[1],f->owner.policy_generation,f->now)==TV4_NEG_OK);
    if(keep==NULL)assert(tv4_neg_clear(&n[0]) && tv4_neg_clear(&n[1]));
}
static void timed_negotiate(fixture *f,tv4_test_link *link,uint16_t block)
{timed_negotiate_impl(f,link,block,NULL);}
static void timed_drain(fixture *f,tv4_test_link *link)
{
    for(unsigned tick=0;!timed_idle(link);++tick) {
        assert(tick<128);
        assert(tv4_update_rx_poll(&f->rx,f->now)==TV4_UPDATE_OK && tv4_update_tx_poll(&f->tx,f->now)==TV4_UPDATE_OK);
        for(unsigned side=0;side<2;++side){tv4_wire_message ignored;(void)tv4_test_link_take(link,side,&ignored);}
        tv4_test_link_step(link);
    }
}
static void timed_transfer(fixture *f,tv4_test_link *link)
{
    tv4_boot_stage_config stage={.buffer=f->staging,.capacity=sizeof(f->staging),.boot=f->owner.boot,.object=91};
    assert(tv4_boot_stage_init(&f->stage,&stage) && tv4_boot_stage_begin(&f->stage,&f->rx));
    uint32_t started=f->rx.context.started_ms;tv4_wire_message pending_ack={0};bool ack_pending=false;unsigned ticks;
    assert(f->tx.context.started_ms==started);
    for(ticks=0;ticks<10000;++ticks) {
        assert(tv4_update_tx_poll(&f->tx,f->now)==TV4_UPDATE_OK && tv4_update_rx_poll(&f->rx,f->now)==TV4_UPDATE_OK);
        tv4_wire_message packet;
        if(tv4_test_link_take(link,0,&packet)==TV4_FRAG_TAKEN) {
            assert(packet.kind==TV4_WIRE_DATA);tv4_update_ack ack;
            tv4_update_result r=tv4_update_rx_receive(&f->rx,&packet.body.data,&ack,f->now);
            if(r==TV4_UPDATE_PENDING)assert(tv4_boot_stage_consume(&f->stage,&ack,f->now)==TV4_UPDATE_ACK);
            else assert(r==TV4_UPDATE_ACK);
            pending_ack=(tv4_wire_message){.kind=TV4_WIRE_DATA_ACK,.body.ack=ack};ack_pending=true;
        }
        if(tv4_test_link_take(link,1,&packet)==TV4_FRAG_TAKEN) {
            assert(packet.kind==TV4_WIRE_DATA_ACK);
            tv4_update_result r=tv4_update_tx_ack(&f->tx,&packet.body.ack,f->now);
            assert(r==TV4_UPDATE_OK || r==TV4_UPDATE_STALE_ACK);
        }
        if(f->tx.context.state==TV4_UPDATE_COMPLETE_UNVERIFIED)break;
        if(ack_pending && tv4_test_link_can_send(link,1)) {
            assert(tv4_test_link_send(link,1,&pending_ack)==TV4_FRAG_OK);ack_pending=false;
        }
        if(tv4_test_link_can_send(link,0)) {
            if(f->tx.context.state==TV4_UPDATE_NEED_DATA) {
                size_t length=f->source_bytes-f->tx.context.next_offset;
                if(length>f->tx.context.limits.block_bytes)length=f->tx.context.limits.block_bytes;
                assert(tv4_update_tx_load(&f->tx,f->source+f->tx.context.next_offset,length,f->now)==TV4_UPDATE_OK);
            }
            tv4_wire_message outgoing={.kind=TV4_WIRE_DATA};tv4_update_result r=tv4_update_tx_next(&f->tx,&outgoing.body.data,f->now);
            if(r==TV4_UPDATE_OK)assert(tv4_test_link_send(link,0,&outgoing)==TV4_FRAG_OK);
            else assert(r==TV4_UPDATE_WAIT);
        }
        assert(f->tx.context.started_ms==started && f->rx.context.started_ms==started);tv4_test_link_step(link);
    }
    assert(ticks<10000 && f->stage.state==TV4_STAGE_SEALED && f->stage.written==f->source_bytes);
    assert(f->stage.next_sequence==f->rx.context.next_sequence && memcmp(f->staging,f->source,f->source_bytes)==0);
    f->staged_bytes=f->stage.written;timed_drain(f,link);
}
static void timed_outcome(fixture *f,tv4_test_link *link)
{
    if(link->config.multiplexed)tv4_test_link_advance(link,TV4_CHANNEL_OUTCOME,(tv4_update_id){0});
    tv4_outcome_tx tx;tv4_outcome_rx rx;tv4_outcome_limits limits={200,500,2000,4};
    assert(tv4_outcome_tx_init(&tx,limits,f->owner.boot,f->now) && tv4_outcome_rx_init(&rx,limits,f->now));
    assert(tv4_outcome_tx_begin(&tx,&f->outcome,f->now)==TV4_OUTCOME_OK);
    assert(tv4_outcome_rx_begin(&rx,&f->tx,f->expected.metadata_sha256,f->now)==TV4_OUTCOME_OK);
    tv4_wire_message pending_ack={0};bool ack_pending=false;unsigned deliveries=0,ticks;
    for(ticks=0;ticks<2000;++ticks) {
        assert(tv4_outcome_tx_poll(&tx,f->now)!=TV4_OUTCOME_ERROR && tv4_outcome_rx_poll(&rx,f->now)!=TV4_OUTCOME_ERROR);
        tv4_wire_message packet;
        if(tv4_test_link_take(link,1,&packet)==TV4_FRAG_TAKEN) {
            assert(packet.kind==TV4_WIRE_OUTCOME);tv4_update_outcome ack,delivery;
            tv4_outcome_result r=tv4_outcome_rx_receive(&rx,&packet.body.outcome,&ack,&delivery,f->now);
            assert(r==TV4_OUTCOME_ACK || r==TV4_OUTCOME_DELIVERED);
            if(r==TV4_OUTCOME_DELIVERED){++deliveries;assert(tv4_update_outcome_equal(&delivery,&f->outcome));}
            pending_ack=(tv4_wire_message){.kind=TV4_WIRE_OUTCOME_ACK,.body.outcome=ack};ack_pending=true;
        }
        if(tv4_test_link_take(link,0,&packet)==TV4_FRAG_TAKEN) {
            assert(packet.kind==TV4_WIRE_OUTCOME_ACK);
            assert(tv4_outcome_tx_ack(&tx,&packet.body.outcome,f->now)==TV4_OUTCOME_OK);
        }
        if(tx.context.state==TV4_OUTCOME_ACKED)break;
        if(ack_pending && tv4_test_link_can_send(link,0)) {
            assert(tv4_test_link_send(link,0,&pending_ack)==TV4_FRAG_OK);ack_pending=false;
        }
        if(tv4_test_link_can_send(link,1)) {
            tv4_wire_message outgoing={.kind=TV4_WIRE_OUTCOME};tv4_outcome_result r=tv4_outcome_tx_next(&tx,&outgoing.body.outcome,f->now);
            if(r==TV4_OUTCOME_SENT)assert(tv4_test_link_send(link,1,&outgoing)==TV4_FRAG_OK);
            else assert(r==TV4_OUTCOME_WAIT);
        }
        tv4_test_link_step(link);
    }
    assert(ticks<2000 && deliveries==1);timed_drain(f,link);
    if(link->config.multiplexed)tv4_test_link_advance(link,TV4_CHANNEL_NEGOTIATION,(tv4_update_id){0});
}
static void one_clock_signed_chains(void)
{
    unsigned frames=0,dropped=0,timeouts=0,peeks=0;uint32_t longest=0;
    for(unsigned i=0;i<8;++i) {
        fixture f;if(i<6)release_fixture(&f,i%2u?256:33);else vector_fixture(&f,33);
        f.use_stage=true;tv4_test_link link={0};uint16_t mask=i<2?0u:510u;
        tv4_test_link_config cfg=timed_config(mask,(uint16_t)(i%3u));tv4_test_link_init(&link,&f.now,&cfg);
        uint32_t started=f.now;timed_negotiate(&f,&link,i%2u?256u:31u);timed_transfer(&f,&link);
        if(i<6)release_begin(&f);else begin(&f);
        f.inject=STAGE_LOCK;f.fragment=i%3u==0?1:i%3u==1?7:256;
        bool ok=i<6?tv4_boot_completion_start_release(&f.completion,&f.release_adapter,&f.expected,&f.preflight_profile,&f.workspace):
            tv4_boot_completion_start(&f.completion,&f.session,&f.expected,&f.preflight_profile,&f.workspace);
        assert(ok);completed(&f,TV4_OUTCOME_CHECKED_CLOSED,TV4_OUTCOME_REASON_NONE);
        assert(tv4_boot_stage_discard(&f.stage) && zero(f.staging,f.source_bytes));
        timed_outcome(&f,&link);
        assert(link.dropped_kinds==mask && link.sticky_failures==0 && link.repeated_peeks>0);
        for(unsigned kind=1;kind<=8;++kind)assert(link.deliveries[kind]>0);
        assert(f.releases==1 && tv4_boot_completion_clear(&f.completion));
        if(i<6)release_clear(&f);else assert(tv4_boot_handoff_clear(&f.session));
        uint32_t elapsed=f.now-started;if(elapsed>longest)longest=elapsed;
        frames+=link.frames;dropped+=link.dropped_frames;timeouts+=link.timeouts;peeks+=link.repeated_peeks;finish(&f);
    }
    printf("PASS8 one-clock actual signed RAM chains: frames=%u lost=%u timeout-clears=%u repeated-peeks=%u longest=%u synthetic ms; all8 kinds, no deadline resets or duplicate outcome delivery\n",frames,dropped,timeouts,peeks,longest);
}
static void one_clock_admission_refusals(void)
{
    for(unsigned i=0;i<2;++i) {
        fixture f;release_fixture(&f,33);f.use_stage=true;
        if(i==0)f.signature[f.config.signature_bytes-1u]^=1;
        tv4_test_link link={0};tv4_test_link_config cfg=timed_config((1u<<5)|(1u<<6),1);
        tv4_test_link_init(&link,&f.now,&cfg);timed_negotiate(&f,&link,31);timed_transfer(&f,&link);
        if(i==0) {
            release_begin(&f);assert(tv4_boot_completion_start_release(&f.completion,&f.release_adapter,&f.expected,&f.preflight_profile,&f.workspace));
            completed(&f,TV4_OUTCOME_REJECTED_CLOSED,TV4_OUTCOME_SIGNATURE);
            assert(f.releases==1 && tv4_boot_completion_clear(&f.completion));
        }else {
            uint32_t progress=f.rx.context.progress_ms;
            while(f.now-progress<f.rx.context.limits.progress_timeout_ms)tv4_test_link_step(&link);
            assert(!tv4_boot_release_handoff_begin(&f.release_adapter,&f.session,&f.release_config,&f.rx));
            assert(f.session.error==TV4_HANDOFF_RECEIVER && f.rx.context.error==TV4_UPDATE_ERROR_PROGRESS_TIMEOUT);
            assert(f.acquired==0 && f.releases==0);
        }
        assert(tv4_boot_stage_discard(&f.stage));release_clear(&f);finish(&f);
    }
    puts("PASS one-clock transfer success does not bypass independent signature or expired DATA live-receiver admission");
}
/* Metadata uses a separate DATA identity. Legacy mode takes local descriptors;
 * setup modes exchange them through bounded MS messages. This is not production
 * authenticated discovery, hardware routing or install authority. */
static void timed_setup(fixture *f,timed_image_session *image,tv4_metadata_setup setup[2],
    tv4_update_descriptor *descriptor,uint16_t *block_bytes,tv4_test_link *shared)
{
    tv4_metadata_setup_limits limits={200,8000,TV4_META_BUNDLE_MAX,12};
    for(unsigned i=0;i<2;++i)assert(tv4_metadata_setup_init(&setup[i],&image->pool[i],
        i?TV4_NEG_RESPONDER:TV4_NEG_REQUESTER,limits,i?f->owner.policy_generation:2001,f->now));
    assert(tv4_metadata_setup_request(&setup[0],&image->n[0],&f->tx,descriptor->image_bytes,
        descriptor->image_sha256,2001,f->now)==TV4_SETUP_OK);
    assert(tv4_metadata_setup_respond(&setup[1],&image->n[1],&f->rx,f->owner.policy_generation,f->now)==TV4_SETUP_OK);
    tv4_test_link local={0};tv4_test_link *control;
    if(shared->config.multiplexed) {
        tv4_test_link_advance(shared,TV4_CHANNEL_SETUP,(tv4_update_id){0});control=shared;
    }else {
        tv4_test_link_config cfg=timed_config(30,1);cfg.raw_setup=true;
        tv4_test_link_init(&local,&f->now,&cfg);control=&local;
    }
    unsigned frames_before=control->frames,drops_before=control->dropped_frames;
    uint32_t started=f->now,image_started=f->rx.context.started_ms,image_progress=f->rx.context.progress_ms;
    unsigned ticks;
    for(ticks=0;ticks<8000;++ticks) {
        for(unsigned side=0;side<2;++side) {
            uint64_t generation=side?f->owner.policy_generation:2001;
            assert(tv4_metadata_setup_poll(&setup[side],generation,f->now)==TV4_SETUP_OK);
            tv4_metadata_setup_message m;
            if(tv4_test_link_take_setup(control,side,&m)==TV4_FRAG_RAW_TAKEN_UNCHECKED) {
                unsigned target=1u-side;tv4_metadata_setup_result r=tv4_metadata_setup_receive(&setup[target],&m,
                    target?f->owner.policy_generation:2001,f->now);
                assert(r==TV4_SETUP_OK || r==TV4_SETUP_DUPLICATE);
            }
        }
        bool ready=setup[0].state==TV4_SETUP_READY && setup[1].state==TV4_SETUP_READY;
        if(ready && timed_idle(control))break;
        if(!ready)for(unsigned side=0;side<2;++side)if(tv4_test_link_can_send(control,side)) {
            tv4_metadata_setup_message m;tv4_metadata_setup_result r=tv4_metadata_setup_next(&setup[side],&m,
                side?f->owner.policy_generation:2001,f->now);
            if(r==TV4_SETUP_SENT)assert(tv4_test_link_send_setup(control,side,&m)==TV4_FRAG_OK);
            else assert(r==TV4_SETUP_WAIT);
        }
        assert(setup[0].started==started && setup[1].started==started);
        assert(f->rx.context.started_ms==image_started && f->rx.context.progress_ms==image_progress);
        tv4_test_link_step(control);
    }
    assert(ticks<8000 && control->sticky_failures==0);
    assert(shared->config.multiplexed?(control->dropped_kinds&TV4_TEST_LINK_MS_DROP_MASK)==TV4_TEST_LINK_MS_DROP_MASK:control->dropped_kinds==30);
    tv4_metadata_parameters params[2];
    for(unsigned i=0;i<2;++i) {
        assert(tv4_metadata_setup_take(&setup[i],&params[i],i?f->owner.policy_generation:2001,f->now)==TV4_SETUP_OK);
        assert(tv4_update_binding_equal(&params[i].binding,&f->expected));
    }
    assert(params[0].metadata.id.transfer==params[1].metadata.id.transfer &&
        params[0].metadata.id.transfer==image->pool[1].last_issued && params[0].block_bytes==params[1].block_bytes);
    *descriptor=params[0].metadata;*block_bytes=params[0].block_bytes;
    printf("SETUP frames=%u drops=%u elapsed=%u synthetic ms; paired ID=%u pools held, raw MS codec before dispatch\n",
        control->frames-frames_before,control->dropped_frames-drops_before,f->now-started,descriptor->id.transfer);
    if(shared->config.multiplexed)tv4_test_link_advance(shared,TV4_CHANNEL_METADATA,descriptor->id);
}
static size_t timed_metadata_impl(fixture *f,tv4_test_link *link,tv4_metadata_document *downloaded,timed_image_session *image)
{
    tv4_metadata_document source={.binding=f->expected};
    assert(f->config.metadata_bytes<=TV4_META_DOCUMENT_MAX && f->config.signature_bytes<=TV4_META_SIGNATURE_MAX && f->config.key_id_bytes<=TV4_META_KEY_MAX);
    source.document_bytes=(uint16_t)f->config.metadata_bytes;source.signature_bytes=(uint16_t)f->config.signature_bytes;
    source.key_id_bytes=(uint16_t)f->config.key_id_bytes;
    memcpy(source.document,f->config.metadata,source.document_bytes);memcpy(source.signature,f->config.signature,source.signature_bytes);
    memcpy(source.key_id,f->config.key_id,source.key_id_bytes);
    uint8_t raw[TV4_META_BUNDLE_MAX];size_t n=0;assert(tv4_metadata_encode(&source,raw,sizeof(raw),&n)==TV4_META_OK);
    /* The verifier must later read the delivered snapshot, not producer arrays. */
    memset(f->metadata,0,sizeof(f->metadata));memset(f->signature,0,sizeof(f->signature));memset(&source,0,sizeof(source));
    assert(f->descriptor.id.transfer<UINT32_MAX);
    tv4_update_descriptor descriptor={.id={f->owner.boot,f->descriptor.id.transfer+1u},.image_bytes=(uint32_t)n};
    digest(raw,n,descriptor.image_sha256); /* opaque source expectation, not verification */
    tv4_metadata_setup setup[2]={{0},{0}};uint16_t metadata_block=256;
    if(image!=NULL)timed_setup(f,image,setup,&descriptor,&metadata_block,link);
    tv4_update_limits limits={.max_image_bytes=TV4_META_BUNDLE_MAX,.ack_timeout_ms=200,.progress_timeout_ms=3000,
        .total_timeout_ms=10000,.block_bytes=metadata_block,.retry_limit=4};
    tv4_metadata_rx metadata={0};tv4_update_tx tx;
    assert(tv4_metadata_rx_init(&metadata,limits,f->owner.boot,f->owner.policy_generation,0,f->now)==TV4_META_OK);
    assert(tv4_metadata_rx_begin(&metadata,&descriptor,&f->expected,f->owner.policy_generation,f->now)==TV4_META_OK);
    assert(tv4_update_tx_init(&tx,limits,f->owner.boot,f->now) && tv4_update_tx_begin(&tx,&descriptor,f->now)==TV4_UPDATE_OK);
    tv4_update_block wrong={.id=f->descriptor.id,.length=1,.bytes={0x12}};tv4_update_ack ack;
    assert(tv4_update_block_checksum(&wrong,&wrong.crc32c));
    assert(tv4_metadata_rx_receive(&metadata,&wrong,&ack,f->owner.policy_generation,f->now)==TV4_UPDATE_REJECT_SESSION);
    assert(metadata.stream.context.next_offset==0);
    uint32_t image_started=f->rx.context.started_ms,image_progress=f->rx.context.progress_ms;
    tv4_wire_message pending_ack={0};bool ack_pending=false,role_checked=false;unsigned ticks;
    for(ticks=0;ticks<10000;++ticks) {
        if(image!=NULL)for(unsigned side=0;side<2;++side)
            assert(tv4_metadata_setup_poll(&setup[side],side?f->owner.policy_generation:2001,f->now)==TV4_SETUP_OK);
        assert(tv4_update_rx_poll(&f->rx,f->now)==TV4_UPDATE_OK && tv4_update_tx_poll(&f->tx,f->now)==TV4_UPDATE_OK);
        assert(tv4_update_tx_poll(&tx,f->now)==TV4_UPDATE_OK && tv4_metadata_rx_poll(&metadata,f->owner.policy_generation,f->now)==TV4_META_OK);
        tv4_wire_message packet;
        if(tv4_test_link_take(link,0,&packet)==TV4_FRAG_TAKEN) {
            assert(packet.kind==TV4_WIRE_DATA);
            if(!role_checked) {
                assert(tv4_update_rx_receive(&f->rx,&packet.body.data,&ack,f->now)==TV4_UPDATE_REJECT_SESSION);
                assert(f->rx.context.next_offset==0);role_checked=true;
            }
            assert(tv4_metadata_rx_receive(&metadata,&packet.body.data,&ack,f->owner.policy_generation,f->now)==TV4_UPDATE_ACK);
            pending_ack=(tv4_wire_message){.kind=TV4_WIRE_DATA_ACK,.body.ack=ack};ack_pending=true;
        }
        if(tv4_test_link_take(link,1,&packet)==TV4_FRAG_TAKEN) {
            assert(packet.kind==TV4_WIRE_DATA_ACK);
            tv4_update_result r=tv4_update_tx_ack(&tx,&packet.body.ack,f->now);assert(r==TV4_UPDATE_OK || r==TV4_UPDATE_STALE_ACK);
        }
        if(tx.context.state==TV4_UPDATE_COMPLETE_UNVERIFIED)break;
        if(ack_pending && tv4_test_link_can_send(link,1)) {
            assert(tv4_test_link_send(link,1,&pending_ack)==TV4_FRAG_OK);ack_pending=false;
        }
        if(tv4_test_link_can_send(link,0)) {
            if(tx.context.state==TV4_UPDATE_NEED_DATA) {
                size_t count=n-tx.context.next_offset;if(count>metadata_block)count=metadata_block;
                assert(tv4_update_tx_load(&tx,raw+tx.context.next_offset,count,f->now)==TV4_UPDATE_OK);
            }
            tv4_wire_message outgoing={.kind=TV4_WIRE_DATA};tv4_update_result r=tv4_update_tx_next(&tx,&outgoing.body.data,f->now);
            if(r==TV4_UPDATE_OK)assert(tv4_test_link_send(link,0,&outgoing)==TV4_FRAG_OK);else assert(r==TV4_UPDATE_WAIT);
        }
        assert(f->rx.context.started_ms==image_started && f->rx.context.progress_ms==image_progress);
        tv4_test_link_step(link);
    }
    assert(ticks<10000 && role_checked && f->acquired==0 && f->rx.context.next_offset==0);
    assert(tv4_metadata_rx_take(&metadata,downloaded,f->owner.policy_generation,f->now)==TV4_META_TAKEN_UNVERIFIED);
    assert(zero(metadata.bytes,sizeof(metadata.bytes)) && tv4_update_binding_equal(&downloaded->binding,&f->expected));
    if(image!=NULL) {
        assert(tv4_metadata_setup_close_tx(&setup[0],&tx,2001,f->now)==TV4_SETUP_OK);
        assert(tv4_metadata_setup_close_rx(&setup[1],&metadata,f->owner.policy_generation,f->now)==TV4_SETUP_OK);
        assert(image->pool[0].owner==NULL && image->pool[1].owner==NULL);
        for(unsigned side=0;side<2;++side)assert(tv4_neg_clear(&image->n[side]));
    }
    assert(tv4_metadata_rx_discard(&metadata,f->owner.policy_generation,f->now)==TV4_META_OK);
    assert(metadata.last_transfer==descriptor.id.transfer);
    f->config.metadata=downloaded->document;f->config.metadata_bytes=downloaded->document_bytes;
    f->config.signature=downloaded->signature;f->config.signature_bytes=downloaded->signature_bytes;
    f->config.key_id=downloaded->key_id;f->config.key_id_bytes=downloaded->key_id_bytes;
    f->release_config.metadata=f->config.metadata;f->release_config.metadata_bytes=f->config.metadata_bytes;
    f->release_config.signature=f->config.signature;f->release_config.signature_bytes=f->config.signature_bytes;
    f->release_config.key_id=f->config.key_id;f->release_config.key_id_bytes=f->config.key_id_bytes;
    assert(f->config.metadata!=f->metadata && f->config.signature!=f->signature);
    timed_drain(f,link);
    if(link->config.multiplexed)tv4_test_link_advance(link,TV4_CHANNEL_IMAGE,f->descriptor.id);
    memset(raw,0,sizeof(raw));return n;
}
static size_t timed_metadata(fixture *f,tv4_test_link *link,tv4_metadata_document *downloaded)
{return timed_metadata_impl(f,link,downloaded,NULL);}
typedef enum {META_DIRECT,META_SETUP,META_CHANNEL,META_EGRESS,META_EVENTS,META_MAILBOX,META_DISPATCH,META_INGRESS,META_OWNER} metadata_link_mode;
static void metadata_signed_chains(metadata_link_mode mode)
{
    bool setup_mode=mode>=META_SETUP,channel_mode=mode>=META_CHANNEL,egress_mode=mode>=META_EGRESS,event_mode=mode>=META_EVENTS;
    unsigned frames=0,drops=0,container_bytes=0,sig_min=72,sig_max=0;uint32_t longest=0;
    for(unsigned i=0;i<8;++i) {
        fixture f;if(i<6)release_fixture(&f,i%2u?256:33);else vector_fixture(&f,33);
        f.use_stage=true;tv4_test_link link={0};uint16_t mask=i<2?0u:510u;
        if(channel_mode)mask=(uint16_t)(mask|TV4_TEST_LINK_MS_DROP_MASK);
        tv4_test_link_config cfg=timed_config(mask,(uint16_t)(i%3u));cfg.multiplexed=channel_mode;cfg.egress=egress_mode;
        if(egress_mode)cfg.custody=(tv4_frame_queue_limits){16,2};
        if(event_mode){cfg.driver_events=true;cfg.event_capacity=2;cfg.event_service_ms=1;cfg.fragments.total_timeout_ms=256;cfg.completion_mailbox=mode>=META_MAILBOX;cfg.driver_dispatch=mode>=META_DISPATCH;if(mode>=META_INGRESS){cfg.frame_ingress=true;cfg.ingress_capacity=2;cfg.ingress_residency_ms=8;cfg.ingress_service_ms=1;cfg.ingress_owner=mode==META_OWNER;}}
        tv4_test_link_init(&link,&f.now,&cfg);
        uint32_t started=f.now;tv4_metadata_document downloaded;
        if(setup_mode) {
            timed_image_session image={0};timed_negotiate_impl(&f,&link,256,&image);
            container_bytes+=(unsigned)timed_metadata_impl(&f,&link,&downloaded,&image);
        }else {
            timed_negotiate(&f,&link,i%2u?256u:31u);container_bytes+=(unsigned)timed_metadata(&f,&link,&downloaded);
        }
        unsigned signature=downloaded.signature_bytes;if(signature<sig_min)sig_min=signature;if(signature>sig_max)sig_max=signature;
        timed_transfer(&f,&link);
        if(i<6)release_begin(&f);else begin(&f);
        f.inject=STAGE_LOCK;f.fragment=i%3u==0?1:i%3u==1?7:256;
        bool ok=i<6?tv4_boot_completion_start_release(&f.completion,&f.release_adapter,&f.expected,&f.preflight_profile,&f.workspace):
            tv4_boot_completion_start(&f.completion,&f.session,&f.expected,&f.preflight_profile,&f.workspace);
        assert(ok);completed(&f,TV4_OUTCOME_CHECKED_CLOSED,TV4_OUTCOME_REASON_NONE);
        assert(tv4_boot_stage_discard(&f.stage) && zero(f.staging,f.source_bytes));timed_outcome(&f,&link);
        assert(link.dropped_kinds==mask && link.sticky_failures==0);
        for(unsigned kind=1;kind<=(channel_mode?12u:8u);++kind)assert(link.deliveries[kind]>0);
        if(channel_mode) {
            unsigned attempts=0,counters=0;for(unsigned k=1;k<=12;++k)attempts+=link.attempts[k];
            for(unsigned d=0;d<2;++d) {
                const tv4_channel_tx *tx=mode>=META_DISPATCH?&link.direction[d].dispatch.completion.events.egress.channel:mode==META_MAILBOX?&link.direction[d].completion.events.egress.channel:event_mode?&link.direction[d].events.egress.channel:egress_mode?&link.direction[d].egress.channel:&link.direction[d].channel_tx;
                assert(tx->policy.phase==TV4_CHANNEL_NEGOTIATION);
                assert(tx->fragments.storage.counter==(mode==META_OWNER?link.direction[d].owner.channel.fragments.storage.counter:link.direction[d].channel_rx.fragments.storage.counter));
                counters+=tx->fragments.storage.counter;
            }
            assert(attempts==counters);
            if(egress_mode) {
                unsigned tickets=0;
                for(unsigned d=0;d<2;++d) {
                    const tv4_update_egress *owner=mode>=META_DISPATCH?&link.direction[d].dispatch.completion.events.egress:mode==META_MAILBOX?&link.direction[d].completion.events.egress:event_mode?&link.direction[d].events.egress:&link.direction[d].egress;
                    assert(owner->queue.count==0 && owner->queue.flight==0);
                    assert(!event_mode || (mode>=META_DISPATCH?link.direction[d].dispatch.completion.events.count:mode==META_MAILBOX?link.direction[d].completion.events.count:link.direction[d].events.count)==0);
                    if(mode>=META_DISPATCH)assert(atomic_load(&link.direction[d].dispatch.state)==TV4_DISPATCH_EMPTY && !link.direction[d].dispatch.completion.has_pending && tv4_completion_mailbox_empty(&link.direction[d].dispatch.completion.mailbox)==TV4_QUEUE_OK);
                    if(mode==META_MAILBOX)assert(!link.direction[d].completion.has_pending && tv4_completion_mailbox_empty(&link.direction[d].completion.mailbox)==TV4_QUEUE_OK);
                    tickets+=owner->queue.last_issued;
                }
                assert(link.driver_admissions==link.frames && link.driver_claims==link.frames && link.driver_returns==link.frames && tickets==link.frames);
                assert(link.driver_max_pending==2 && link.driver_held_ticks>0);
                printf("EGRESS claims=%u returns=%u held-ticks=%u max-pending=%u; local completion tickets, no driver-owned frame forgotten\n",
                    link.driver_claims,link.driver_returns,link.driver_held_ticks,link.driver_max_pending);
                if(event_mode) {
                    assert(link.event_posts==link.event_services && link.event_held_ticks>0);
                    if(mode>=META_DISPATCH) {
                        assert(link.event_stales==0 && link.event_max_pending==1 && link.dispatch_publications==link.frames && link.dispatch_rejected_callbacks>0);
                        printf("DISPATCH published=%u driver-rejected-callbacks=%u; original admission/fresh check/exact terminal/slot reuse, no driver-copy gap\n",link.dispatch_publications,link.dispatch_rejected_callbacks);
                        if(mode>=META_INGRESS){assert(link.ingress_posts==link.ingress_checks && link.ingress_checks==link.delivered_frames+link.owner_retirements);
                            for(unsigned d=0;d<2;++d){const tv4_frame_ingress *in=mode==META_OWNER?&link.direction[d].owner.ingress:&link.direction[d].ingress;assert(tv4_frame_ingress_empty(in)==TV4_INGRESS_WAIT && tv4_frame_ingress_faults(in)==0);}
                            printf("INGRESS owned copies=%u checked=%u; capture age/current service time, one-frame service budget\n",link.ingress_posts,link.ingress_checks);}
                        if(mode==META_OWNER){assert(link.owner_advances==10 && link.owner_pauses==link.owner_advances+link.owner_recovers);
                            for(unsigned d=0;d<2;++d)assert(atomic_load(&link.direction[d].owner.gate)==TV4_GATE_OPEN && !link.direction[d].owner.failed);
                            printf("RX OWNER pauses=%u advances=%u timeout-recoveries=%u retired-without-dispatch=%u; actual ACK+drain barrier, epochs/counters/time retained\n",link.owner_pauses,link.owner_advances,link.owner_recovers,link.owner_retirements);}
                    }else assert(link.event_stales>0 && link.event_max_pending==2);
                    assert(link.event_services==link.driver_returns+link.event_stales);
                    printf("EVENTS posted=%u serviced=%u stale=%u wait-ticks=%u max-pending=%u; owned copies, one-event budget/current service time\n",
                        link.event_posts,link.event_services,link.event_stales,link.event_held_ticks,link.event_max_pending);
                }
            }
        }
        assert(tv4_boot_completion_clear(&f.completion));
        if(i<6)release_clear(&f);else assert(tv4_boot_handoff_clear(&f.session));
        if(f.now-started>longest)longest=f.now-started;
        frames+=link.frames;drops+=link.dropped_frames;
        memset(&downloaded,0,sizeof(downloaded));finish(&f);
    }
    printf("PASS8 transported metadata signed chains: frames=%u drops=%u container-bytes=%u DER=%u..%u longest=%u synthetic ms; producer wiped, actual delivered signature/raw digest/lease/model/close, no image deadline reset\n",frames,drops,container_bytes,sig_min,sig_max,longest);
}
static void metadata_admission_refusals(metadata_link_mode mode)
{
    bool setup_mode=mode>=META_SETUP,channel_mode=mode>=META_CHANNEL,egress_mode=mode>=META_EGRESS,event_mode=mode>=META_EVENTS;
    for(unsigned i=0;i<3;++i) {
        fixture f;release_fixture(&f,33);f.use_stage=true;tv4_test_link link={0};tv4_metadata_document downloaded;
        tv4_test_link_config cfg=timed_config((uint16_t)((1u<<5)|(1u<<6)|(channel_mode?TV4_TEST_LINK_MS_DROP_MASK:0u)),1);
        cfg.multiplexed=channel_mode;cfg.egress=egress_mode;
        if(egress_mode)cfg.custody=(tv4_frame_queue_limits){16,2};
        if(event_mode){cfg.driver_events=true;cfg.event_capacity=2;cfg.event_service_ms=1;cfg.fragments.total_timeout_ms=256;cfg.completion_mailbox=mode>=META_MAILBOX;cfg.driver_dispatch=mode>=META_DISPATCH;if(mode>=META_INGRESS){cfg.frame_ingress=true;cfg.ingress_capacity=2;cfg.ingress_residency_ms=8;cfg.ingress_service_ms=1;cfg.ingress_owner=mode==META_OWNER;}}
        tv4_test_link_init(&link,&f.now,&cfg);
        timed_image_session image={0};
        if(setup_mode)timed_negotiate_impl(&f,&link,256,&image);else timed_negotiate(&f,&link,31);
        if(i==0)f.signature[f.config.signature_bytes-1u]^=1;
        if(i==1) {
            uint8_t *field=(uint8_t *)strstr((char *)f.metadata,"\"image_counter\":3");assert(field);
            field[strlen("\"image_counter\":")]='4';sign_metadata(&f); /* valid signature, wrong negotiated raw digest */
        }
        (void)timed_metadata_impl(&f,&link,&downloaded,setup_mode?&image:NULL);
        if(i<2) {
            timed_transfer(&f,&link);release_begin(&f);
            assert(tv4_boot_completion_start_release(&f.completion,&f.release_adapter,&f.expected,&f.preflight_profile,&f.workspace));
            completed(&f,i==0?TV4_OUTCOME_REJECTED_CLOSED:TV4_OUTCOME_FAILED_CLOSED,i==0?TV4_OUTCOME_SIGNATURE:TV4_OUTCOME_BINDING);
            assert(tv4_boot_completion_clear(&f.completion) && tv4_boot_stage_discard(&f.stage));
        }else {
            uint32_t progress=f.rx.context.progress_ms;
            while(f.now-progress<f.rx.context.limits.progress_timeout_ms)tv4_test_link_step(&link);
            assert(tv4_update_rx_poll(&f.rx,f.now)==TV4_UPDATE_ERROR && f.rx.context.error==TV4_UPDATE_ERROR_PROGRESS_TIMEOUT);
            assert(!tv4_boot_release_handoff_begin(&f.release_adapter,&f.session,&f.release_config,&f.rx));
            assert(f.acquired==0 && f.rx.context.next_offset==0);
        }
        release_clear(&f);memset(&downloaded,0,sizeof(downloaded));finish(&f);
    }
    puts("PASS transported metadata does not bypass invalid signature, correctly re-signed wrong raw digest, or expired independent image RX");
}
static void egress_driver_lifetime_refusals(void)
{
    for(unsigned which=0;which<2;++which) {
        fixture f;release_fixture(&f,33);f.use_stage=true;
        tv4_test_link link={0};tv4_test_link_config cfg=timed_config(0,1);
        cfg.multiplexed=true;cfg.egress=true;cfg.custody=(tv4_frame_queue_limits){16,2};
        /* Setup must still exercise its four loss/retry phases. */
        cfg.drop_kinds=TV4_TEST_LINK_MS_DROP_MASK;tv4_test_link_init(&link,&f.now,&cfg);
        timed_image_session image={0};timed_negotiate_impl(&f,&link,256,&image);
        tv4_metadata_document downloaded;(void)timed_metadata_impl(&f,&link,&downloaded,&image);
        uint32_t image_started=f.rx.context.started_ms,image_progress=f.rx.context.progress_ms;
        assert(tv4_update_tx_load(&f.tx,f.source,256,f.now)==TV4_UPDATE_OK);
        tv4_wire_message packet={.kind=TV4_WIRE_DATA};
        assert(tv4_update_tx_next(&f.tx,&packet.body.data,f.now)==TV4_UPDATE_OK);
        assert(tv4_test_link_send(&link,0,&packet)==TV4_FRAG_OK);
        tv4_update_egress *tx=&link.direction[0].egress;
        assert(tv4_update_egress_pump(tx,link.generation[0],f.now)==TV4_QUEUE_ADMITTED);
        tv4_frame_delivery held;assert(tv4_update_egress_claim(tx,&held,link.generation[0],f.now)==TV4_QUEUE_FRAME);
        if(which==0)assert(tv4_update_egress_complete(tx,held.ticket,TV4_FRAME_OWNERSHIP_UNCERTAIN,f.now)==TV4_QUEUE_HELD);
        while(f.now-image_progress<f.rx.context.limits.progress_timeout_ms) {
            ++f.now;(void)tv4_update_egress_poll(tx,link.generation[0],f.now);
            (void)tv4_update_rx_poll(&f.rx,f.now);(void)tv4_update_tx_poll(&f.tx,f.now);
            assert(tx->queue.flight==held.ticket.serial && tx->queue.count==1);
        }
        assert(f.rx.context.state==TV4_UPDATE_FAILED && f.rx.context.error==TV4_UPDATE_ERROR_PROGRESS_TIMEOUT);
        assert(f.rx.context.started_ms==image_started && f.rx.context.progress_ms==image_progress && f.rx.context.next_offset==0);
        assert(tv4_update_egress_advance(tx,TV4_CHANNEL_OUTCOME,(tv4_update_id){0},link.generation[0],f.now)==TV4_CHANNEL_ERROR);
        assert(tv4_update_egress_clear(tx,link.generation[0],f.now)==TV4_QUEUE_HELD);
        assert(tv4_update_egress_complete(tx,held.ticket,which?TV4_FRAME_FAILED_LOCAL:TV4_FRAME_SENT_LOCAL,f.now)==TV4_QUEUE_RECLAIMED_FAILED);
        uint32_t serial=tx->queue.last_issued;uint16_t fragments_issued=tx->channel.fragments.storage.counter;
        assert(tv4_update_egress_clear(tx,link.generation[0],f.now)==TV4_QUEUE_OK);
        assert(tx->queue.last_issued==serial && tx->channel.fragments.storage.counter==fragments_issued && tx->channel.policy.phase==TV4_CHANNEL_IMAGE);
        assert(!tv4_boot_release_handoff_begin(&f.release_adapter,&f.session,&f.release_config,&f.rx));
        assert(f.acquired==0 && f.rx.context.state==TV4_UPDATE_FAILED && f.rx.context.started_ms==image_started);
        release_clear(&f);memset(&downloaded,0,sizeof(downloaded));finish(&f);
    }
    puts("PASS2 driver uncertain/missing-completion refusals: actual claim retained through original image timeout; late return reclaims only, clear does not revive RX or admission");
}
static void driver_event_lifetime_refusals(bool mailbox)
{
    for(unsigned which=0;which<2;++which) {
        fixture f;release_fixture(&f,33);f.use_stage=true;
        tv4_test_link link={0};tv4_test_link_config cfg=timed_config(TV4_TEST_LINK_MS_DROP_MASK,1);
        cfg.multiplexed=true;cfg.egress=true;cfg.custody=(tv4_frame_queue_limits){16,2};
        cfg.driver_events=true;cfg.event_capacity=2;cfg.event_service_ms=1;cfg.fragments.total_timeout_ms=256;cfg.completion_mailbox=mailbox;
        tv4_test_link_init(&link,&f.now,&cfg);timed_image_session image={0};
        timed_negotiate_impl(&f,&link,256,&image);tv4_metadata_document downloaded;
        (void)timed_metadata_impl(&f,&link,&downloaded,&image);
        uint32_t started=f.rx.context.started_ms,progress=f.rx.context.progress_ms;
        assert(tv4_update_tx_load(&f.tx,f.source,256,f.now)==TV4_UPDATE_OK);
        tv4_wire_message packet={.kind=TV4_WIRE_DATA};
        assert(tv4_update_tx_next(&f.tx,&packet.body.data,f.now)==TV4_UPDATE_OK);
        assert(tv4_test_link_send(&link,0,&packet)==TV4_FRAG_OK);
        tv4_completion_owner *completion=&link.direction[0].completion;
        tv4_driver_events *events=mailbox?&completion->events:&link.direction[0].events;
        assert((mailbox?tv4_completion_owner_pump(completion,link.generation[0],f.now):tv4_driver_events_pump(events,link.generation[0],f.now))==TV4_QUEUE_ADMITTED);
        tv4_frame_delivery held;assert((mailbox?tv4_completion_owner_claim(completion,&held,link.generation[0],f.now):tv4_driver_events_claim(events,&held,link.generation[0],f.now))==TV4_QUEUE_FRAME);
        tv4_driver_event report={held.ticket,TV4_FRAME_OWNERSHIP_UNCERTAIN};
        if(which==0)assert((mailbox?tv4_completion_owner_post(completion,&report):tv4_driver_events_post(events,&report))==TV4_QUEUE_ADMITTED);
        report.result=TV4_FRAME_SENT_LOCAL;
        assert((mailbox?tv4_completion_owner_post(completion,&report):tv4_driver_events_post(events,&report))==TV4_QUEUE_ADMITTED);memset(&report,0,sizeof(report));
        while(f.now-progress<f.rx.context.limits.progress_timeout_ms) {
            ++f.now;(void)(mailbox?tv4_completion_owner_poll(completion,link.generation[0],f.now):tv4_driver_events_poll(events,link.generation[0],f.now));
            (void)tv4_update_rx_poll(&f.rx,f.now);(void)tv4_update_tx_poll(&f.tx,f.now);
            assert(events->egress.queue.flight==held.ticket.serial);
            if(mailbox)assert(tv4_completion_mailbox_empty(&completion->mailbox)==TV4_QUEUE_BUSY);
            else assert(events->count==(which==0?2u:1u));
        }
        assert(f.rx.context.state==TV4_UPDATE_FAILED && f.rx.context.error==TV4_UPDATE_ERROR_PROGRESS_TIMEOUT);
        assert(f.rx.context.started_ms==started && f.rx.context.progress_ms==progress && f.rx.context.next_offset==0);
        assert((mailbox?tv4_completion_owner_clear(completion,link.generation[0],f.now):tv4_driver_events_clear(events,link.generation[0],f.now))==TV4_QUEUE_HELD);
        if(which==0)assert((mailbox?tv4_completion_owner_service(completion,link.generation[0],f.now):tv4_driver_events_service(events,link.generation[0],f.now))==TV4_QUEUE_HELD);
        assert((mailbox?tv4_completion_owner_service(completion,link.generation[0],f.now):tv4_driver_events_service(events,link.generation[0],f.now))==TV4_QUEUE_RECLAIMED_FAILED);
        uint32_t serial=events->egress.queue.last_issued;uint16_t counter=events->egress.channel.fragments.storage.counter;
        assert((mailbox?tv4_completion_owner_clear(completion,link.generation[0],f.now):tv4_driver_events_clear(events,link.generation[0],f.now))==TV4_QUEUE_OK);
        assert(events->count==0 && events->egress.queue.last_issued==serial && events->egress.channel.fragments.storage.counter==counter);
        assert(events->egress.channel.policy.phase==TV4_CHANNEL_IMAGE);
        assert(!tv4_boot_release_handoff_begin(&f.release_adapter,&f.session,&f.release_config,&f.rx));
        assert(f.acquired==0 && f.rx.context.state==TV4_UPDATE_FAILED && f.rx.context.started_ms==started);
        release_clear(&f);memset(&downloaded,0,sizeof(downloaded));finish(&f);
    }
    puts("PASS2 delayed queued terminal/uncertain event refusals: original image timeout retained; current-time service reclaims only, no RX/admission revival");
}
static void dispatch_lifetime_refusals(bool ingress)
{
    for(unsigned which=0;which<2;++which) {
        fixture f;release_fixture(&f,33);f.use_stage=true;tv4_test_link link={0};
        tv4_test_link_config cfg=timed_config(TV4_TEST_LINK_MS_DROP_MASK,1);
        cfg.multiplexed=true;cfg.egress=true;cfg.driver_events=true;cfg.completion_mailbox=true;cfg.driver_dispatch=true;
        cfg.custody=(tv4_frame_queue_limits){16,2};cfg.event_capacity=2;cfg.event_service_ms=1;cfg.fragments.total_timeout_ms=256;
        if(ingress){cfg.frame_ingress=true;cfg.ingress_capacity=2;cfg.ingress_residency_ms=8;cfg.ingress_service_ms=1;}
        tv4_test_link_init(&link,&f.now,&cfg);timed_image_session image={0};timed_negotiate_impl(&f,&link,256,&image);
        tv4_metadata_document downloaded;(void)timed_metadata_impl(&f,&link,&downloaded,&image);
        uint32_t started=f.rx.context.started_ms,progress=f.rx.context.progress_ms;
        assert(tv4_update_tx_load(&f.tx,f.source,256,f.now)==TV4_UPDATE_OK);
        tv4_wire_message packet={.kind=TV4_WIRE_DATA};assert(tv4_update_tx_next(&f.tx,&packet.body.data,f.now)==TV4_UPDATE_OK);
        assert(tv4_test_link_send(&link,0,&packet)==TV4_FRAG_OK);tv4_driver_dispatch *dispatch=&link.direction[0].dispatch;
        assert(tv4_driver_dispatch_pump(dispatch,link.generation[0],f.now)==TV4_QUEUE_ADMITTED);
        assert(tv4_driver_dispatch_publish(dispatch,link.generation[0],f.now)==TV4_QUEUE_ADMITTED);
        tv4_dispatch_delivery token,checked;assert(tv4_driver_dispatch_take(dispatch,&token)==TV4_DISPATCH_UNCHECKED);
        assert(tv4_driver_dispatch_check(dispatch,token.delivery.ticket,&checked,f.now)==TV4_DISPATCH_SENDABLE);
        tv4_ingress_token incoming={0};
        if(ingress){assert(tv4_frame_ingress_offer(&link.direction[0].ingress,1,link.generation[0],&checked.delivery.frame,f.now,f.now)==TV4_INGRESS_ADMITTED);
            assert(tv4_frame_ingress_stage(&link.direction[0].ingress,&incoming)==TV4_INGRESS_UNCHECKED);
            if(which==1)assert(tv4_frame_ingress_lost(&link.direction[0].ingress)==TV4_INGRESS_FAULT);}
        tv4_driver_event report={token.delivery.ticket,TV4_FRAME_OWNERSHIP_UNCERTAIN};
        if(which==0)assert(tv4_driver_dispatch_report(dispatch,&report)==TV4_QUEUE_ADMITTED);
        report.result=TV4_FRAME_SENT_LOCAL;assert(tv4_driver_dispatch_report(dispatch,&report)==TV4_QUEUE_ADMITTED);memset(&report,0,sizeof(report));
        while(f.now-progress<f.rx.context.limits.progress_timeout_ms) {
            ++f.now;(void)tv4_driver_dispatch_poll(dispatch,link.generation[0],f.now);
            (void)tv4_update_rx_poll(&f.rx,f.now);(void)tv4_update_tx_poll(&f.tx,f.now);
            assert(dispatch->completion.events.egress.queue.flight==token.delivery.ticket.serial && atomic_load(&dispatch->state)==TV4_DISPATCH_RETURNED);
        }
        assert(f.rx.context.state==TV4_UPDATE_FAILED && f.rx.context.error==TV4_UPDATE_ERROR_PROGRESS_TIMEOUT);
        assert(f.rx.context.started_ms==started && f.rx.context.progress_ms==progress && f.rx.context.next_offset==0);
        if(ingress){tv4_fragment_frame refused;assert(tv4_frame_ingress_check(&link.direction[0].ingress,incoming,&refused,f.now)==TV4_INGRESS_FAULT && zero(&refused,sizeof(refused)));
            assert((tv4_frame_ingress_faults(&link.direction[0].ingress)&(which==0?TV4_INGRESS_EXPIRED:TV4_INGRESS_OVERRUN))!=0);
            (void)tv4_channel_rx_cancel(&link.direction[0].channel_rx,link.generation[0],f.now);}
        assert(tv4_driver_dispatch_clear(dispatch,link.generation[0],f.now)==TV4_QUEUE_HELD);
        if(which==0)assert(tv4_driver_dispatch_service(dispatch,link.generation[0],f.now)==TV4_QUEUE_HELD);
        assert(tv4_driver_dispatch_service(dispatch,link.generation[0],f.now)==TV4_QUEUE_RECLAIMED_FAILED);
        assert(atomic_load(&dispatch->state)==TV4_DISPATCH_EMPTY);
        uint32_t serial=dispatch->completion.events.egress.queue.last_issued;uint16_t counter=dispatch->completion.events.egress.channel.fragments.storage.counter;
        assert(tv4_driver_dispatch_clear(dispatch,link.generation[0],f.now)==TV4_QUEUE_OK);
        assert(dispatch->completion.events.egress.queue.last_issued==serial && dispatch->completion.events.egress.channel.fragments.storage.counter==counter && dispatch->completion.events.egress.channel.policy.phase==TV4_CHANNEL_IMAGE);
        assert(!tv4_boot_release_handoff_begin(&f.release_adapter,&f.session,&f.release_config,&f.rx));assert(f.acquired==0 && f.rx.context.state==TV4_UPDATE_FAILED);
        release_clear(&f);memset(&downloaded,0,sizeof(downloaded));finish(&f);
    }
    if(ingress)puts("PASS2 ingress expiry/overrun refuses staged payload, original image deadline retained, no RX/admission revival");
    else puts("PASS2 dispatch/completion retained through original image expiry; late correlated return reclaims slot only, no RX/admission revival");
}
static void ingress_owner_lifetime_refusals(void)
{
    for(unsigned which=0;which<2;++which) {
        fixture f;release_fixture(&f,33);f.use_stage=true;tv4_test_link link={0};
        tv4_test_link_config cfg=timed_config(TV4_TEST_LINK_MS_DROP_MASK,1);
        cfg.multiplexed=true;cfg.egress=true;cfg.driver_events=true;cfg.completion_mailbox=true;cfg.driver_dispatch=true;
        cfg.frame_ingress=true;cfg.ingress_owner=true;cfg.ingress_capacity=2;cfg.ingress_residency_ms=8;cfg.ingress_service_ms=1;
        cfg.custody=(tv4_frame_queue_limits){16,2};cfg.event_capacity=2;cfg.event_service_ms=1;cfg.fragments.total_timeout_ms=256;
        tv4_test_link_init(&link,&f.now,&cfg);timed_image_session image={0};timed_negotiate_impl(&f,&link,256,&image);
        tv4_metadata_document downloaded;(void)timed_metadata_impl(&f,&link,&downloaded,&image);
        uint32_t started=f.rx.context.started_ms,progress=f.rx.context.progress_ms;
        assert(tv4_update_tx_load(&f.tx,f.source,256,f.now)==TV4_UPDATE_OK);
        tv4_wire_message packet={.kind=TV4_WIRE_DATA};assert(tv4_update_tx_next(&f.tx,&packet.body.data,f.now)==TV4_UPDATE_OK);
        assert(tv4_test_link_send(&link,0,&packet)==TV4_FRAG_OK);tv4_driver_dispatch *dispatch=&link.direction[0].dispatch;
        assert(tv4_driver_dispatch_pump(dispatch,link.generation[0],f.now)==TV4_QUEUE_ADMITTED && tv4_driver_dispatch_publish(dispatch,link.generation[0],f.now)==TV4_QUEUE_ADMITTED);
        tv4_dispatch_delivery token,checked;assert(tv4_driver_dispatch_take(dispatch,&token)==TV4_DISPATCH_UNCHECKED && tv4_driver_dispatch_check(dispatch,token.delivery.ticket,&checked,f.now)==TV4_DISPATCH_SENDABLE);
        tv4_ingress_owner *owner=&link.direction[0].owner;tv4_ingress_permit p;assert(tv4_ingress_owner_permit(owner,&p)==TV4_GATE_OPEN);
        assert(tv4_ingress_owner_offer(owner,p,1,link.generation[0],&checked.delivery.frame,f.now,f.now)==TV4_INGRESS_ADMITTED);
        tv4_ingress_token incoming;assert(tv4_ingress_owner_stage(owner,&incoming)==TV4_INGRESS_UNCHECKED);
        if(which==1)assert(tv4_ingress_owner_receive(owner,incoming,link.generation[0],f.now)==TV4_FRAG_PENDING);
        tv4_driver_event report={token.delivery.ticket,TV4_FRAME_SENT_LOCAL};assert(tv4_driver_dispatch_report(dispatch,&report)==TV4_QUEUE_ADMITTED);
        while(f.now-progress<f.rx.context.limits.progress_timeout_ms) {
            ++f.now;(void)tv4_ingress_owner_poll(owner,link.generation[0],f.now);(void)tv4_driver_dispatch_poll(dispatch,link.generation[0],f.now);
            (void)tv4_update_rx_poll(&f.rx,f.now);(void)tv4_update_tx_poll(&f.tx,f.now);
        }
        assert(f.rx.context.state==TV4_UPDATE_FAILED && f.rx.context.error==TV4_UPDATE_ERROR_PROGRESS_TIMEOUT && f.rx.context.started_ms==started && f.rx.context.progress_ms==progress && f.rx.context.next_offset==0);
        if(which==0){assert(tv4_ingress_owner_receive(owner,incoming,link.generation[0],f.now)==TV4_FRAG_ERROR && tv4_ingress_owner_recover(owner,link.generation[0],f.now)==TV4_FRAG_ERROR);}
        else {
            tv4_ingress_pause paused;assert(tv4_ingress_owner_pause_request(owner,&paused)==TV4_GATE_REQUESTED && tv4_ingress_owner_ack(owner,paused,f.now)==TV4_GATE_PAUSED);
            uint32_t serial=owner->ingress.issued;uint16_t counter=owner->channel.fragments.storage.counter;
            assert(tv4_ingress_owner_recover(owner,link.generation[0],f.now)==TV4_FRAG_OK && tv4_ingress_owner_resume(owner,link.generation[0],f.now)==TV4_GATE_OPEN);
            assert(owner->ingress.issued==serial && owner->channel.fragments.storage.counter==counter && owner->channel.policy.phase==TV4_CHANNEL_IMAGE);
            assert(tv4_ingress_owner_offer(owner,p,1,link.generation[0],&checked.delivery.frame,incoming.captured,f.now)==TV4_INGRESS_STALE && tv4_ingress_owner_ack(owner,paused,f.now)==TV4_GATE_STALE);
        }
        assert(tv4_driver_dispatch_service(dispatch,link.generation[0],f.now)==TV4_QUEUE_RECLAIMED_FAILED);
        assert(!tv4_boot_release_handoff_begin(&f.release_adapter,&f.session,&f.release_config,&f.rx) && f.acquired==0 && f.rx.context.state==TV4_UPDATE_FAILED);
        release_clear(&f);memset(&downloaded,0,sizeof(downloaded));finish(&f);
    }
    puts("PASS2 ingress owner refusals: original capture age stays fatal; ACK+drain lower-timeout recovery/fresh epoch/late TX cleanup cannot revive expired independent image RX or admission");
}
#define RUN(name) do { name(); puts("PASS " #name); } while (0)
int main(int argc,char **argv)
{
    if(argc==2 && strcmp(argv[1],"--ingress-owner-staging")==0) {
        metadata_signed_chains(META_OWNER);metadata_admission_refusals(META_OWNER);ingress_owner_lifetime_refusals();return 0;
    }
    if(argc==2 && strcmp(argv[1],"--frame-ingress-staging")==0) {
        metadata_signed_chains(META_INGRESS);metadata_admission_refusals(META_INGRESS);dispatch_lifetime_refusals(true);return 0;
    }
    if(argc==2 && strcmp(argv[1],"--driver-dispatch-staging")==0) {
        metadata_signed_chains(META_DISPATCH);metadata_admission_refusals(META_DISPATCH);dispatch_lifetime_refusals(false);return 0;
    }
    if(argc==2 && strcmp(argv[1],"--completion-mailbox-staging")==0) {
        metadata_signed_chains(META_MAILBOX);metadata_admission_refusals(META_MAILBOX);driver_event_lifetime_refusals(true);return 0;
    }
    if(argc==2 && strcmp(argv[1],"--driver-events-staging")==0) {
        metadata_signed_chains(META_EVENTS);metadata_admission_refusals(META_EVENTS);driver_event_lifetime_refusals(false);return 0;
    }
    if(argc==2 && strcmp(argv[1],"--egress-staging")==0) {
        metadata_signed_chains(META_EGRESS);metadata_admission_refusals(META_EGRESS);egress_driver_lifetime_refusals();return 0;
    }
    if(argc==2 && strcmp(argv[1],"--channel-staging")==0) {
        metadata_signed_chains(META_CHANNEL);metadata_admission_refusals(META_CHANNEL);return 0;
    }
    if(argc==2 && strcmp(argv[1],"--metadata-setup-staging")==0) {
        metadata_signed_chains(META_SETUP);metadata_admission_refusals(META_SETUP);return 0;
    }
    if(argc==2 && strcmp(argv[1],"--metadata-staging")==0) {
        metadata_signed_chains(META_DIRECT);metadata_admission_refusals(META_DIRECT);return 0;
    }
    if(argc==2 && strcmp(argv[1],"--fragment-timing")==0) {
        RUN(one_clock_signed_chains);RUN(one_clock_admission_refusals);return 0;
    }
    if(argc==2 && strcmp(argv[1],"--fragment-staging")==0) {
        const tv4_fragment_config limits={100,500,160,TV4_FRAGMENT_VERSION};
        assert(tv4_fragment_tx_init(&fragment_tx,&limits,7,9,0,0)==TV4_FRAG_OK);
        assert(tv4_fragment_rx_init(&fragment_rx,&limits,7,9,0,0)==TV4_FRAG_OK);
        wire_mode=true;fragment_mode=true;
        RUN(staged_signed_chains);RUN(staged_crypto_rejections);RUN(staged_cleanup_and_revocation);RUN(staged_receiver_deadline);
        printf("PASS fragments across 8 actual signed RAM chains; kind counts:");
        for(unsigned i=1;i<=8;++i){assert(wire_counts[i]>0);printf(" %u=%u",i,wire_counts[i]);}
        assert(fragment_duplicates>0);
        printf("; frames=%u duplicates=%u; original logical/crypto/lease/model/deadline/cleanup gates unchanged\n",fragment_frames,fragment_duplicates);
        return 0;
    }
    if(argc==2 && strcmp(argv[1],"--wire-staging")==0) {
        wire_mode=true;
        RUN(staged_signed_chains);RUN(staged_crypto_rejections);RUN(staged_cleanup_and_revocation);RUN(staged_receiver_deadline);
        printf("PASS byte codec across existing signed RAM chains; kind counts:");
        for(unsigned i=1;i<=8;++i){assert(wire_counts[i]>0);printf(" %u=%u",i,wire_counts[i]);}
        puts("; no wire/CRC success replaces signature/lease/model/deadline/cleanup or authorizes install");return 0;
    }
    if (argc==2 && strcmp(argv[1],"--staging")==0) {
        RUN(staged_signed_chains);RUN(staged_crypto_rejections);RUN(staged_cleanup_and_revocation);RUN(staged_receiver_deadline);
        printf("PASS 4 staging integration groups; source=%zu core=%zu completion=%zu workspace=%zu; no runtime/Flash/device access\n",
            sizeof(tv4_boot_stage),sizeof(tv4_boot_handoff),sizeof(tv4_boot_completion),sizeof(tv4_boot_workspace));return 0;
    }
    if (argc==2 && strcmp(argv[1],"--release")==0) {
        RUN(release_full_chain);RUN(release_live_snapshot);RUN(release_live_revocation);
        RUN(release_completion_rejections);RUN(release_explicit_mode);RUN(release_cleanup_and_late_context);
        RUN(release_reentry_and_completion_snapshot);RUN(release_arguments_and_aliases);
        printf("PASS 8 release handoff groups; adapter=%zu full_receipt=%zu completion=%zu core=%zu workspace=%zu; RAM only, no install/device access\n",
            sizeof(tv4_boot_release_handoff),sizeof(tv4_boot_release_handoff_receipt),sizeof(tv4_boot_completion),sizeof(tv4_boot_handoff),sizeof(tv4_boot_workspace));
        return 0;
    }
    assert(argc==1);
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
    RUN(negotiated_full_chain);RUN(negotiated_binding_still_requires_crypto);
    printf("PASS 19 handoff/completion groups; completion=%zu; state=%zu receipt=%zu workspace=%zu; RAM only, no install/device access\n",
        sizeof(tv4_boot_completion),
        sizeof(tv4_boot_handoff), sizeof(tv4_boot_handoff_receipt), sizeof(tv4_boot_workspace));
    return 0;
}
