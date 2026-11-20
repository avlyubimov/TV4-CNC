#include "admission.h"
#include <assert.h>
#include <inttypes.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <mbedtls/platform.h>
#include <mbedtls/sha256.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    tv4_boot_policy policy;
    tv4_boot_request request;
    tv4_boot_workspace workspace;
    tv4_boot_candidate candidate;
    tv4_boot_key keys[2];
    EVP_PKEY *signer;
    uint8_t metadata[1024], signature[72], image[1536];
    size_t source_bytes, fragment;
    uint32_t now, cost;
    unsigned reads, now_calls;
    int fault;
    bool cancel, cancel_eof, cancel_cleanup, reenter, mutate;
} fixture;
static bool zero(const void *pointer, size_t size)
{
    const uint8_t *p = pointer;
    for (size_t i = 0; i < size; ++i) if (p[i] != 0) return false;
    return true;
}
static void sha(const uint8_t *bytes, size_t length, uint8_t out[32])
{
    size_t n = 0;
    assert(EVP_Q_digest(NULL, "SHA256", NULL, bytes, length, out, &n) == 1 && n == 32);
}
static void sign_metadata(fixture *f)
{
    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    assert(ctx != NULL);
    assert(EVP_DigestSignInit(ctx, NULL, EVP_sha256(), NULL, f->signer) == 1);
    size_t n = sizeof(f->signature);
    assert(EVP_DigestSign(ctx, f->signature, &n, f->metadata, f->request.metadata_bytes) == 1);
    f->request.signature_bytes = n;
    EVP_MD_CTX_free(ctx);
}
static void metadata(fixture *f, uint32_t size, uint32_t counter, const char *hardware,
                     uint32_t boot_min, uint32_t protocol_min, uint32_t protocol_max)
{
    uint8_t hash[32]; char hex[65];
    assert(size <= sizeof(f->image));
    sha(f->image, size, hash);
    for (size_t i = 0; i < 32; ++i) (void)snprintf(hex + i * 2, 3, "%02x", hash[i]);
    int n = snprintf((char *)f->metadata, sizeof(f->metadata),
        "{\"schema\":\"%s\",\"component\":\"motion\",\"hardware_id\":\"%s\","
        "\"hardware_revision\":\"synthetic\",\"memory_profile\":\"test-no-addresses\","
        "\"image_counter\":%" PRIu32 ",\"minimum_bootloader_counter\":%" PRIu32 ","
        "\"protocol_min\":%" PRIu32 ",\"protocol_max\":%" PRIu32 ","
        "\"image_size\":%" PRIu32 ",\"sha256\":\"%s\"}",
        TV4_BOOT_DRAFT_SCHEMA, hardware, counter, boot_min, protocol_min, protocol_max, size, hex);
    assert(n > 0 && (size_t)n < sizeof(f->metadata));
    f->request.metadata_bytes = (size_t)n;
    sign_metadata(f);
}
static uint32_t now_ms(void *context)
{
    fixture *f = context;
    ++f->now_calls;
    if (f->mutate && f->now_calls == 1) {
        memset(f->metadata, 0xee, sizeof(f->metadata));
        memset(f->signature, 0xee, sizeof(f->signature));
        memset(f->keys, 0, sizeof(f->keys));
        f->policy.protocol = 0;
        f->request.read = NULL; /* active operation must use captured pointers */
    }
    return f->now;
}
static bool cancelled(void *context)
{
    fixture *f = context;
    return f->cancel || (f->cancel_cleanup && f->now_calls > 2 && zero(&f->workspace, sizeof(f->workspace)));
}
static bool read_memory(void *context, uint32_t offset, uint8_t *bytes,
                         size_t capacity, size_t *count, bool *eof)
{
    fixture *f = context;
    ++f->reads;
    f->now += f->cost;
    if (f->reenter) {
        tv4_boot_candidate other;
        memset(&other, 0xa5, sizeof(other));
        tv4_boot_candidate before = other;
        assert(tv4_boot_admit(&f->policy, &f->request, &f->workspace, &other) == TV4_BOOT_BUSY);
        assert(memcmp(&before, &other, sizeof(other)) == 0);
    }
    if (f->fault == 1) return false;
    if (f->fault == 2) { *count = capacity + 1; return true; } /* no overflowing write */
    if (f->fault == 3) { *count = 0; *eof = false; return true; }
    if (f->fault == 4) { *count = 0; *eof = true; return true; }
    if (f->fault == 5) f->now = 9;
    assert(offset <= f->source_bytes);
    size_t n = f->source_bytes - offset;
    if (n > capacity) n = capacity;
    if (n > f->fragment) n = f->fragment;
    memcpy(bytes, f->image + offset, n);
    *count = n;
    *eof = offset + n == f->source_bytes;
    if (f->cancel_eof && *eof && n == 0) f->cancel = true;
    return true;
}
static void setup(fixture *f)
{
    *f = (fixture){0};
    f->signer = EVP_PKEY_Q_keygen(NULL, NULL, "EC", "prime256v1");
    assert(f->signer != NULL);
    memcpy(f->keys[0].id, "test-key", 9);
    unsigned char *der = f->keys[0].spki;
    assert(i2d_PUBKEY(f->signer, &der) == TV4_BOOT_KEY_BYTES);
    f->policy = (tv4_boot_policy){.hardware_id = "test-f407", .hardware_revision = "synthetic",
        .memory_profile = "test-no-addresses", .installed_image_counter = 2,
        .minimum_image_counter = 3, .bootloader_counter = 5, .protocol = 7,
        .max_image_bytes = 4096, .timeout_ms = 100, .keys = f->keys, .key_count = 1};
    f->request = (tv4_boot_request){.context = f, .now_ms = now_ms, .cancelled = cancelled,
        .read = read_memory, .metadata = f->metadata, .signature = f->signature,
        .key_id = (const uint8_t *)"test-key", .key_id_bytes = 8, .arena_bytes = TV4_BOOT_ARENA_BYTES};
    f->source_bytes = 769;
    f->fragment = 256;
    for (size_t i = 0; i < sizeof(f->image); ++i) f->image[i] = (uint8_t)(i * 37u);
    metadata(f, 769, 3, "test-f407", 5, 6, 8);
    memset(&f->workspace, 0xa5, sizeof(f->workspace));
    memset(&f->candidate, 0xa5, sizeof(f->candidate));
}
static void finish(fixture *f) { EVP_PKEY_free(f->signer); }
static tv4_boot_result run(fixture *f)
{
    tv4_boot_result r = tv4_boot_admit(&f->policy, &f->request, &f->workspace, &f->candidate);
    if (r != TV4_BOOT_VERIFIED_CANDIDATE) assert(zero(&f->candidate, sizeof(f->candidate)));
    if (f->now_calls != 0) assert(zero(&f->workspace, sizeof(f->workspace)));
    assert(mbedtls_calloc(1, 8) == NULL); /* detached buffer / no libc fallback */
    return r;
}
static void sha_vectors(void)
{
    static const uint8_t abc[32] = {0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
        0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    uint8_t out[32], oracle[32];
    assert(mbedtls_sha256((const uint8_t *)"abc", 3, out, 0) == 0);
    assert(memcmp(out, abc, 32) == 0);
    sha((const uint8_t *)"", 0, oracle);
    assert(mbedtls_sha256((const uint8_t *)"", 0, out, 0) == 0);
    assert(memcmp(out, oracle, 32) == 0);
}
static void success_and_fragmentation(void)
{
    const size_t fragments[] = {1, 7, 255, 256};
    for (size_t i = 0; i < sizeof(fragments) / sizeof(fragments[0]); ++i) {
        fixture f; setup(&f);
        f.fragment = fragments[i];
        uint8_t meta_hash[32], image_hash[32];
        sha(f.metadata, f.request.metadata_bytes, meta_hash);
        sha(f.image, 769, image_hash);
        assert(run(&f) == TV4_BOOT_VERIFIED_CANDIDATE);
        assert(f.candidate.image_bytes == 769 && f.candidate.image_counter == 3);
        assert(memcmp(f.candidate.metadata_sha256, meta_hash, 32) == 0);
        assert(memcmp(f.candidate.image_sha256, image_hash, 32) == 0);
        assert(strcmp(f.candidate.key_id, "test-key") == 0);
        assert(f.reads == (769u + fragments[i] - 1u) / fragments[i] + 1u);
        finish(&f);
    }
}
static void invalid_signature_and_key(void)
{
    for (unsigned fault = 0; fault < 7; ++fault) {
        fixture f; setup(&f);
        tv4_boot_result expected = TV4_BOOT_SIGNATURE;
        switch (fault) {
        case 0: f.signature[f.request.signature_bytes - 1] ^= 1; break;
        case 1: f.metadata[15] ^= 1; break;
        case 2: f.signature[1] = 0x80; break;
        case 3: --f.request.signature_bytes; break;
        case 4: f.request.key_id = (const uint8_t *)"bad-key!"; expected = TV4_BOOT_KEY; break;
        case 5: f.keys[0].spki[20] ^= 1; expected = TV4_BOOT_KEY; break;
        default: memset(f.keys[0].spki + 27, 0, 64); expected = TV4_BOOT_KEY; break;
        }
        assert(run(&f) == expected && f.reads == 0);
        finish(&f);
    }
}
static void wrong_signer(void)
{
    fixture f, other; setup(&f); setup(&other);
    memcpy(f.keys[0].spki, other.keys[0].spki, sizeof(f.keys[0].spki));
    assert(run(&f) == TV4_BOOT_SIGNATURE && f.reads == 0);
    finish(&f); finish(&other);
}
static void compatibility_and_rollback(void)
{
    for (unsigned fault = 0; fault < 10; ++fault) {
        fixture f; setup(&f);
        tv4_boot_result expected = TV4_BOOT_COMPATIBILITY;
        switch (fault) {
        case 0: metadata(&f, 769, 3, "other-board", 5, 6, 8); break;
        case 1: f.policy.hardware_revision[0] = 'x'; break;
        case 2: f.policy.memory_profile[0] = 'x'; break;
        case 3: metadata(&f, 769, 3, "test-f407", 6, 6, 8); break;
        case 4: metadata(&f, 769, 3, "test-f407", 5, 8, 9); break;
        case 5: metadata(&f, 769, 3, "test-f407", 5, 8, 6); break;
        case 6: metadata(&f, 769, 3, "test-f407", 0, 6, 8); break;
        case 7: f.policy.max_image_bytes = 768; break;
        case 8: metadata(&f, 769, 2, "test-f407", 5, 6, 8); expected = TV4_BOOT_ROLLBACK; break;
        default: f.policy.minimum_image_counter = 4; expected = TV4_BOOT_ROLLBACK; break;
        }
        assert(run(&f) == expected && f.reads == 0);
        finish(&f);
    }
}
static void policy_errors(void)
{
    for (unsigned fault = 0; fault < 10; ++fault) {
        fixture f; setup(&f);
        switch (fault) {
        case 0: f.policy.protocol = 0; break;
        case 1: f.policy.installed_image_counter = 0; break;
        case 2: f.policy.minimum_image_counter = 0; break;
        case 3: f.policy.bootloader_counter = 0; break;
        case 4: f.policy.timeout_ms = UINT32_MAX; break;
        case 5: f.policy.key_count = TV4_BOOT_KEYS_MAX + 1; break;
        case 6: f.keys[1] = f.keys[0]; f.policy.key_count = 2; break;
        case 7: memset(f.policy.hardware_id, 'x', sizeof(f.policy.hardware_id)); break;
        case 8: f.policy.keys = NULL; break;
        default: f.policy.hardware_id[0] = '\0'; break;
        }
        assert(run(&f) == TV4_BOOT_POLICY && f.reads == 0 && f.now_calls == 0);
        finish(&f);
    }
}
static void replace(fixture *f, const char *from, const char *to)
{
    char *where = strstr((char *)f->metadata, from);
    assert(where != NULL);
    size_t offset = (size_t)(where - (char *)f->metadata);
    size_t old = strlen(from), add = strlen(to);
    assert(f->request.metadata_bytes - old + add < sizeof(f->metadata));
    memmove(where + add, where + old, f->request.metadata_bytes - offset - old + 1);
    memcpy(where, to, add);
    f->request.metadata_bytes = f->request.metadata_bytes - old + add;
    sign_metadata(f);
}
static void signed_malformed_metadata(void)
{
    const char *from[] = {"\"component\":\"motion\"", "\"image_counter\":3", "\"image_counter\":3", "\"image_counter\":3",
        "\"image_counter\":3", "\"component\":\"motion\"", "\"protocol_min\":6", "\"schema\":\"" TV4_BOOT_DRAFT_SCHEMA "\"",
        "\"image_counter\":3", "\"image_counter\":3", "\"image_counter\":3"};
    const char *to[] = {"\"component\":\"hmi\"", "\"image_counter\":3,\"image_counter\":4", "\"image_counter\":03", "\"image_counter\":3.0",
        "\"image_counter\":4294967296", "\"component\":\"mot\\u0069on\"", "\"unknown\":6", "\"schema\":\"production\"",
        "\"image_counter\":-1", "\"image_counter\":3e0", "\"image_counter\":[]"};
    for (size_t i = 0; i < sizeof(from) / sizeof(from[0]); ++i) {
        fixture f; setup(&f); replace(&f, from[i], to[i]);
        assert(run(&f) == TV4_BOOT_METADATA && f.reads == 0);
        finish(&f);
    }
    fixture f; setup(&f);
    f.metadata[f.request.metadata_bytes++] = 'x'; sign_metadata(&f);
    assert(run(&f) == TV4_BOOT_METADATA && f.reads == 0);
    finish(&f);
}
static void stream_faults(void)
{
    for (int fault = 1; fault <= 7; ++fault) {
        fixture f; setup(&f);
        tv4_boot_result expected;
        if (fault <= 4) { f.fault = fault; expected = fault == 4 ? TV4_BOOT_LENGTH : TV4_BOOT_SOURCE; }
        else if (fault == 5) { --f.source_bytes; expected = TV4_BOOT_LENGTH; }
        else if (fault == 6) { ++f.source_bytes; expected = TV4_BOOT_LENGTH; }
        else { f.image[0] ^= 1; expected = TV4_BOOT_HASH; }
        assert(run(&f) == expected);
        finish(&f);
    }
}
static void deadlines_cancellation_and_wrap(void)
{
    fixture f; setup(&f); f.cost = 100;
    assert(run(&f) == TV4_BOOT_TIMEOUT && f.reads == 1); finish(&f);
    setup(&f); f.cancel = true;
    assert(run(&f) == TV4_BOOT_CANCELLED && f.reads == 0); finish(&f);
    setup(&f); f.cancel_eof = true;
    assert(run(&f) == TV4_BOOT_CANCELLED && f.reads == 5); finish(&f);
    setup(&f); f.cancel_cleanup = true;
    assert(run(&f) == TV4_BOOT_CANCELLED); finish(&f);
    setup(&f); f.now = 10; f.fault = 5;
    assert(run(&f) == TV4_BOOT_CLOCK); finish(&f);
    setup(&f); f.now = UINT32_MAX - 2; f.cost = 1;
    assert(run(&f) == TV4_BOOT_VERIFIED_CANDIDATE && f.now == 2); finish(&f);
}
static void arena_exhaustion_and_reuse(void)
{
    fixture f; setup(&f);
    const size_t small[] = {1, 16, 64, 256};
    for (size_t i = 0; i < sizeof(small) / sizeof(small[0]); ++i) {
        f.request.arena_bytes = small[i];
        assert(run(&f) == TV4_BOOT_RESOURCE && f.reads == 0);
    }
    f.request.arena_bytes = TV4_BOOT_ARENA_BYTES;
    f.now_calls = 0;
    for (unsigned i = 0; i < 32; ++i) {
        f.reads = 0;
        assert(run(&f) == TV4_BOOT_VERIFIED_CANDIDATE);
    }
    finish(&f);
}
static void snapshot_and_reentry(void)
{
    fixture f; setup(&f); f.mutate = true;
    assert(run(&f) == TV4_BOOT_VERIFIED_CANDIDATE);
    assert(f.policy.protocol == 0 && f.request.read == NULL);
    finish(&f);
    setup(&f); f.reenter = true;
    assert(run(&f) == TV4_BOOT_VERIFIED_CANDIDATE && f.reads == 5);
    finish(&f);
}
static void arguments_and_limits(void)
{
    fixture f; setup(&f);
    assert(tv4_boot_admit(NULL, &f.request, &f.workspace, &f.candidate) == TV4_BOOT_ARGUMENT);
    assert(zero(&f.candidate, sizeof(f.candidate)));
    assert(tv4_boot_admit(&f.policy, NULL, &f.workspace, &f.candidate) == TV4_BOOT_ARGUMENT);
    assert(tv4_boot_admit(&f.policy, &f.request, NULL, &f.candidate) == TV4_BOOT_ARGUMENT);
    assert(tv4_boot_admit(&f.policy, &f.request, &f.workspace, NULL) == TV4_BOOT_ARGUMENT);
    f.request.metadata_bytes = SIZE_MAX;
    assert(run(&f) == TV4_BOOT_ARGUMENT);
    f.request.metadata_bytes = 0;
    assert(run(&f) == TV4_BOOT_ARGUMENT);
    f.request.metadata_bytes = 1; f.request.signature_bytes = SIZE_MAX;
    assert(run(&f) == TV4_BOOT_ARGUMENT);
    f.request.signature_bytes = 8; f.request.arena_bytes = TV4_BOOT_ARENA_BYTES + 1;
    assert(run(&f) == TV4_BOOT_ARGUMENT);
    finish(&f);
}
static void boundary_metadata_and_key_selection(void)
{
    fixture f; setup(&f);
    memset(f.metadata + f.request.metadata_bytes, ' ', sizeof(f.metadata) - f.request.metadata_bytes);
    f.request.metadata_bytes = sizeof(f.metadata);
    sign_metadata(&f);
    assert(run(&f) == TV4_BOOT_VERIFIED_CANDIDATE);
    f.request.metadata_bytes = sizeof(f.metadata) + 1;
    assert(run(&f) == TV4_BOOT_ARGUMENT);
    finish(&f);
    setup(&f);
    metadata(&f, 769, UINT32_MAX, "test-f407", 5, 6, 8);
    f.policy.installed_image_counter = UINT32_MAX - 1;
    f.policy.minimum_image_counter = UINT32_MAX;
    assert(run(&f) == TV4_BOOT_VERIFIED_CANDIDATE && f.candidate.image_counter == UINT32_MAX);
    f.policy.installed_image_counter = UINT32_MAX;
    assert(run(&f) == TV4_BOOT_ROLLBACK);
    finish(&f);
    setup(&f);
    f.keys[1] = f.keys[0];
    memcpy(f.keys[1].id, "next-key", 9);
    f.policy.key_count = 2;
    f.request.key_id = (const uint8_t *)"next-key";
    assert(run(&f) == TV4_BOOT_VERIFIED_CANDIDATE && strcmp(f.candidate.key_id, "next-key") == 0);
    finish(&f);
}
static void storage_aliases_are_write_free(void)
{
    fixture f; setup(&f);
    tv4_boot_candidate before = f.candidate;
    f.request.metadata = f.workspace.metadata;
    assert(tv4_boot_admit(&f.policy, &f.request, &f.workspace, &f.candidate) == TV4_BOOT_ARGUMENT);
    assert(memcmp(&before, &f.candidate, sizeof(before)) == 0);
    assert(f.now_calls == 0 && f.reads == 0);
    f.request.metadata = f.metadata;
    assert(tv4_boot_admit(&f.policy, &f.request, &f.workspace,
        (tv4_boot_candidate *)(void *)f.workspace.arena) == TV4_BOOT_ARGUMENT);
    for (size_t i = 0; i < sizeof(f.workspace.arena); ++i) assert(f.workspace.arena[i] == 0xa5);
    finish(&f);
}
#define RUN(name) do { name(); puts("PASS " #name); } while (0)
int main(void)
{
    RUN(sha_vectors);
    RUN(success_and_fragmentation);
    RUN(invalid_signature_and_key);
    RUN(wrong_signer);
    RUN(compatibility_and_rollback);
    RUN(policy_errors);
    RUN(signed_malformed_metadata);
    RUN(stream_faults);
    RUN(deadlines_cancellation_and_wrap);
    RUN(arena_exhaustion_and_reuse);
    RUN(snapshot_and_reentry);
    RUN(arguments_and_limits);
    RUN(boundary_metadata_and_key_selection);
    RUN(storage_aliases_are_write_free);
    printf("PASS 14 admission groups; workspace=%zu bytes; ephemeral host signing keys; no device access\n", sizeof(tv4_boot_workspace));
    return 0;
}
