#include "admission.h"
#include "metadata.h"
#include "release_metadata.h"
#include <limits.h>
#include <string.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/memory_buffer_alloc.h>
#include <mbedtls/platform.h>
#include <mbedtls/platform_util.h>
#include <mbedtls/sha256.h>
#include <mbedtls/version.h>

#if !defined(MBEDTLS_MEMORY_BUFFER_ALLOC_C) || !defined(MBEDTLS_PLATFORM_NO_STD_FUNCTIONS) || defined(MBEDTLS_SSL_TLS_C) || defined(MBEDTLS_ENTROPY_C)
#error "Expected isolated bounded verification config, never default TLS/heap config"
#endif
#if MBEDTLS_VERSION_NUMBER != 0x03060700
#error "Expected pinned Mbed TLS 3.6.7"
#endif
_Static_assert(sizeof(tv4_boot_workspace) <= 18000, "admission workspace regression");

/* Upstream buffer allocator is process-global. A second owner/ISR is forbidden.
 * A synchronous reentrant call returns BUSY before touching any live storage. */
static bool active;
_Noreturn void tv4_boot_crypto_panic(int status)
{
    (void)status;
    /* Only allocator metadata corruption reaches upstream fatal exit paths.
     * No stdout, exit(), reset or pretend physical safe state. Not board code. */
    __builtin_trap();
}
static void *unavailable_calloc(size_t n, size_t size)
{
    (void)n; (void)size;
    return NULL;
}
static void unavailable_free(void *pointer) { (void)pointer; }

static bool separate(const void *a, size_t a_size, const void *b, size_t b_size)
{
    uintptr_t x = (uintptr_t)a, y = (uintptr_t)b;
    if (a == NULL || b == NULL || a_size > UINTPTR_MAX - x || b_size > UINTPTR_MAX - y) return false;
    return x + a_size <= y || y + b_size <= x;
}
static bool aliases(const void *owner, size_t size, const void *input, size_t length, size_t maximum)
{
    return input != NULL && length != 0 && length <= maximum && !separate(owner, size, input, length);
}
static bool policy_valid(const tv4_boot_policy *p)
{
    if (!tv4_boot_identifier(p->hardware_id) || !tv4_boot_identifier(p->hardware_revision) ||
        !tv4_boot_identifier(p->memory_profile) || p->installed_image_counter == 0 ||
        p->minimum_image_counter == 0 || p->bootloader_counter == 0 || p->protocol == 0 ||
        p->max_image_bytes == 0 || p->timeout_ms == 0 || p->timeout_ms > INT32_MAX ||
        p->keys == NULL || p->key_count == 0 || p->key_count > TV4_BOOT_KEYS_MAX) return false;
    for (size_t i = 0; i < p->key_count; ++i) {
        if (!tv4_boot_identifier(p->keys[i].id)) return false;
        for (size_t j = 0; j < i; ++j)
            if (strcmp(p->keys[i].id, p->keys[j].id) == 0) return false;
    }
    return true;
}
static bool key_format(const tv4_boot_key *key)
{
    static const uint8_t prefix[] = {0x30,0x59,0x30,0x13,0x06,0x07,0x2a,0x86,
        0x48,0xce,0x3d,0x02,0x01,0x06,0x08,0x2a,0x86,0x48,0xce,0x3d,0x03,0x01,
        0x07,0x03,0x42,0x00,0x04};
    return memcmp(key->spki, prefix, sizeof(prefix)) == 0;
}
static bool signature_format(const uint8_t *sig, size_t length)
{
    if (length < 8 || length > TV4_BOOT_SIGNATURE_MAX || sig[0] != 0x30 || sig[1] != length - 2) return false;
    size_t offset = 2;
    for (unsigned i = 0; i < 2; ++i) {
        if (offset + 2 > length || sig[offset++] != 0x02) return false;
        size_t size = sig[offset++];
        if (size == 0 || size > 33 || size > length - offset || (sig[offset] & 0x80u) != 0 ||
            (size > 1 && sig[offset] == 0 && (sig[offset + 1] & 0x80u) == 0)) return false;
        offset += size;
    }
    return offset == length;
}
typedef struct { uint32_t start, last, timeout; } deadline;
static tv4_boot_result guard(const tv4_boot_request *r, deadline *d)
{
    if (r->cancelled(r->context)) return TV4_BOOT_CANCELLED;
    uint32_t now = r->now_ms(r->context);
    if ((uint32_t)(now - d->last) > INT32_MAX) return TV4_BOOT_CLOCK;
    d->last = now;
    return (uint32_t)(now - d->start) >= d->timeout ? TV4_BOOT_TIMEOUT : TV4_BOOT_VERIFIED_CANDIDATE;
}
static tv4_boot_result compatible(const tv4_boot_policy *p, const tv4_boot_metadata *m)
{
    if (strcmp(p->hardware_id, m->hardware_id) != 0 ||
        strcmp(p->hardware_revision, m->hardware_revision) != 0 ||
        strcmp(p->memory_profile, m->memory_profile) != 0 || m->image_size == 0 ||
        m->image_size > p->max_image_bytes || m->minimum_bootloader_counter == 0 ||
        m->minimum_bootloader_counter > p->bootloader_counter || m->protocol_min == 0 ||
        m->protocol_min > m->protocol_max || p->protocol < m->protocol_min || p->protocol > m->protocol_max)
        return TV4_BOOT_COMPATIBILITY;
    if (m->image_counter <= p->installed_image_counter || m->image_counter < p->minimum_image_counter)
        return TV4_BOOT_ROLLBACK;
    return TV4_BOOT_VERIFIED_CANDIDATE;
}
static bool resource_failure(int code)
{
    return code == MBEDTLS_ERR_MPI_ALLOC_FAILED || code == MBEDTLS_ERR_ECP_ALLOC_FAILED;
}

static tv4_boot_result admit(const tv4_boot_policy *policy,
    const tv4_boot_request *request, tv4_boot_workspace *w, tv4_boot_candidate *out,
    const tv4_boot_release_policy *release_policy, tv4_boot_release_candidate *release_out)
{
    if (active) return TV4_BOOT_BUSY; /* including output: no live-storage writes */
    if (out == NULL) return TV4_BOOT_ARGUMENT;
    if (policy == NULL || request == NULL || w == NULL) {
        *out = (tv4_boot_candidate){0};
        return TV4_BOOT_ARGUMENT;
    }
    /* Invalid aliases do not receive even a cleared output: avoid changing
     * caller inputs or a workspace through a forged receipt pointer. */
    if (!separate(out, sizeof(*out), w, sizeof(*w)) ||
        !separate(out, sizeof(*out), policy, sizeof(*policy)) ||
        !separate(out, sizeof(*out), request, sizeof(*request)) ||
        !separate(w, sizeof(*w), policy, sizeof(*policy)) ||
        !separate(w, sizeof(*w), request, sizeof(*request))) return TV4_BOOT_ARGUMENT;
    tv4_boot_policy p = *policy;
    tv4_boot_request r = *request;
    if (aliases(w, sizeof(*w), r.metadata, r.metadata_bytes, TV4_BOOT_METADATA_MAX) ||
        aliases(w, sizeof(*w), r.signature, r.signature_bytes, TV4_BOOT_SIGNATURE_MAX) ||
        aliases(w, sizeof(*w), r.key_id, r.key_id_bytes, TV4_BOOT_ID_BYTES) ||
        aliases(out, sizeof(*out), r.metadata, r.metadata_bytes, TV4_BOOT_METADATA_MAX) ||
        aliases(out, sizeof(*out), r.signature, r.signature_bytes, TV4_BOOT_SIGNATURE_MAX) ||
        aliases(out, sizeof(*out), r.key_id, r.key_id_bytes, TV4_BOOT_ID_BYTES) ||
        (p.key_count <= TV4_BOOT_KEYS_MAX &&
          (aliases(w, sizeof(*w), p.keys, p.key_count * sizeof(*p.keys), TV4_BOOT_KEYS_MAX * sizeof(*p.keys)) ||
           aliases(out, sizeof(*out), p.keys, p.key_count * sizeof(*p.keys), TV4_BOOT_KEYS_MAX * sizeof(*p.keys))))) return TV4_BOOT_ARGUMENT;
    *out = (tv4_boot_candidate){0};
    if (!policy_valid(&p)) return TV4_BOOT_POLICY;
    if (r.metadata == NULL || r.signature == NULL || r.key_id == NULL ||
        r.metadata_bytes == 0 || r.metadata_bytes > TV4_BOOT_METADATA_MAX ||
        r.signature_bytes < 8 || r.signature_bytes > TV4_BOOT_SIGNATURE_MAX ||
        r.key_id_bytes == 0 || r.key_id_bytes >= TV4_BOOT_ID_BYTES ||
        r.now_ms == NULL || r.cancelled == NULL || r.read == NULL ||
        r.arena_bytes == 0 || r.arena_bytes > TV4_BOOT_ARENA_BYTES) return TV4_BOOT_ARGUMENT;
    tv4_boot_key key = {0};
    bool found = false;
    for (size_t i = 0; i < p.key_count; ++i) {
        if (strlen(p.keys[i].id) == r.key_id_bytes && memcmp(p.keys[i].id, r.key_id, r.key_id_bytes) == 0) {
            key = p.keys[i]; found = true;
        }
    }
    if (!found || !key_format(&key)) return TV4_BOOT_KEY;
    uint8_t signature[TV4_BOOT_SIGNATURE_MAX] = {0};
    memcpy(signature, r.signature, r.signature_bytes);
    if (!signature_format(signature, r.signature_bytes)) return TV4_BOOT_SIGNATURE;
    memcpy(w->metadata, r.metadata, r.metadata_bytes);
    active = true;
    deadline d = {.start = r.now_ms(r.context), .timeout = p.timeout_ms};
    d.last = d.start;
    tv4_boot_result result = guard(&r, &d);
    bool pool = false, pool_valid = false;
    mbedtls_ecdsa_context ec;
    mbedtls_sha256_context hash;
    mbedtls_ecdsa_init(&ec);
    mbedtls_sha256_init(&hash);
    tv4_boot_metadata metadata = {0};
    tv4_boot_candidate candidate = {0};
    if (result != TV4_BOOT_VERIFIED_CANDIDATE) goto cleanup;
    mbedtls_memory_buffer_alloc_init(w->arena, r.arena_bytes);
    pool = true;
    if (mbedtls_memory_buffer_alloc_verify() != 0) { result = TV4_BOOT_RESOURCE; goto cleanup; }
    pool_valid = true;
    int code = mbedtls_ecp_group_load(&ec.MBEDTLS_PRIVATE(grp), MBEDTLS_ECP_DP_SECP256R1);
    if (code == 0) code = mbedtls_ecp_point_read_binary(&ec.MBEDTLS_PRIVATE(grp), &ec.MBEDTLS_PRIVATE(Q), key.spki + 26, 65);
    if (code == 0) code = mbedtls_ecp_check_pubkey(&ec.MBEDTLS_PRIVATE(grp), &ec.MBEDTLS_PRIVATE(Q));
    if (code != 0) { result = resource_failure(code) ? TV4_BOOT_RESOURCE : TV4_BOOT_KEY; goto cleanup; }
    code = mbedtls_sha256(w->metadata, r.metadata_bytes, candidate.metadata_sha256, 0);
    if (code == 0) code = mbedtls_ecdsa_read_signature(&ec, candidate.metadata_sha256, 32, signature, r.signature_bytes);
    if (code != 0) { result = resource_failure(code) ? TV4_BOOT_RESOURCE : TV4_BOOT_SIGNATURE; goto cleanup; }
    mbedtls_ecdsa_free(&ec); /* return MPI workspace before streaming */
    result = guard(&r, &d);
    if (result != TV4_BOOT_VERIFIED_CANDIDATE) goto cleanup;
    bool parsed = release_policy == NULL ? tv4_boot_parse_metadata(w->metadata, r.metadata_bytes, &metadata) :
        tv4_boot_parse_release_metadata(w->metadata, r.metadata_bytes, &metadata, release_out);
    if (!parsed) { result = TV4_BOOT_METADATA; goto cleanup; }
    result = compatible(&p, &metadata);
    if (result != TV4_BOOT_VERIFIED_CANDIDATE) goto cleanup;
    if (release_policy != NULL) {
        result = tv4_boot_release_compatible(release_policy, release_out);
        if (result != TV4_BOOT_VERIFIED_CANDIDATE) goto cleanup;
    }
    if (mbedtls_sha256_starts(&hash, 0) != 0) { result = TV4_BOOT_CRYPTO; goto cleanup; }
    uint32_t offset = 0;
    while (offset < metadata.image_size) {
        result = guard(&r, &d);
        if (result != TV4_BOOT_VERIFIED_CANDIDATE) goto cleanup;
        uint32_t remaining = metadata.image_size - offset;
        size_t capacity = remaining < TV4_BOOT_CHUNK_BYTES ? remaining : TV4_BOOT_CHUNK_BYTES;
        size_t count = 0;
        bool eof = false;
        bool ok = r.read(r.context, offset, w->chunk, capacity, &count, &eof);
        result = guard(&r, &d);
        if (result != TV4_BOOT_VERIFIED_CANDIDATE) goto cleanup;
        if (!ok || count > capacity || (count == 0 && !eof)) { result = TV4_BOOT_SOURCE; goto cleanup; }
        if (count == 0 || (eof && count < remaining)) { result = TV4_BOOT_LENGTH; goto cleanup; }
        if (mbedtls_sha256_update(&hash, w->chunk, count) != 0) { result = TV4_BOOT_CRYPTO; goto cleanup; }
        offset += (uint32_t)count;
    }
    result = guard(&r, &d);
    if (result != TV4_BOOT_VERIFIED_CANDIDATE) goto cleanup;
    size_t extra = 0;
    bool eof = false;
    bool ok = r.read(r.context, metadata.image_size, w->chunk, 1, &extra, &eof);
    result = guard(&r, &d);
    if (result != TV4_BOOT_VERIFIED_CANDIDATE) goto cleanup;
    if (!ok || extra > 1 || (extra == 0 && !eof)) { result = TV4_BOOT_SOURCE; goto cleanup; }
    if (extra != 0) { result = TV4_BOOT_LENGTH; goto cleanup; }
    if (mbedtls_sha256_finish(&hash, candidate.image_sha256) != 0) { result = TV4_BOOT_CRYPTO; goto cleanup; }
    if (memcmp(candidate.image_sha256, metadata.sha256, 32) != 0) { result = TV4_BOOT_HASH; goto cleanup; }
    candidate.image_bytes = metadata.image_size;
    candidate.image_counter = metadata.image_counter;
    memcpy(candidate.key_id, key.id, strlen(key.id) + 1);
cleanup:
    mbedtls_ecdsa_free(&ec);
    mbedtls_sha256_free(&hash);
    if (pool) {
        if (pool_valid && mbedtls_memory_buffer_alloc_verify() != 0) result = TV4_BOOT_CRYPTO;
        mbedtls_memory_buffer_alloc_free();
        (void)mbedtls_platform_set_calloc_free(unavailable_calloc, unavailable_free);
    }
    mbedtls_platform_zeroize(w, sizeof(*w));
    mbedtls_platform_zeroize(signature, sizeof(signature));
    tv4_boot_result final = guard(&r, &d);
    if (result == TV4_BOOT_VERIFIED_CANDIDATE) result = final;
    active = false;
    if (result == TV4_BOOT_VERIFIED_CANDIDATE) *out = candidate;
    mbedtls_platform_zeroize(&candidate, sizeof(candidate));
    return result;
}

tv4_boot_result tv4_boot_admit(const tv4_boot_policy *policy,
    const tv4_boot_request *request, tv4_boot_workspace *w, tv4_boot_candidate *out)
{
    return admit(policy, request, w, out, NULL, NULL);
}

tv4_boot_result tv4_boot_admit_release(const tv4_boot_release_policy *policy,
    const tv4_boot_request *request, tv4_boot_workspace *w, tv4_boot_release_candidate *out)
{
    _Static_assert(sizeof(tv4_boot_release_candidate) <= 224, "release candidate regression");
    if (active) return TV4_BOOT_BUSY;
    if (out == NULL) return TV4_BOOT_ARGUMENT;
    if (policy == NULL || request == NULL || w == NULL) {
        *out = (tv4_boot_release_candidate){0};
        return TV4_BOOT_ARGUMENT;
    }
    if (!separate(out, sizeof(*out), w, sizeof(*w)) ||
        !separate(out, sizeof(*out), policy, sizeof(*policy)) ||
        !separate(out, sizeof(*out), request, sizeof(*request)) ||
        !separate(w, sizeof(*w), policy, sizeof(*policy)) ||
        !separate(w, sizeof(*w), request, sizeof(*request))) return TV4_BOOT_ARGUMENT;
    tv4_boot_release_policy p = *policy;
    tv4_boot_request r = *request;
    if (aliases(out, sizeof(*out), r.metadata, r.metadata_bytes, TV4_BOOT_METADATA_MAX) ||
        aliases(out, sizeof(*out), r.signature, r.signature_bytes, TV4_BOOT_SIGNATURE_MAX) ||
        aliases(out, sizeof(*out), r.key_id, r.key_id_bytes, TV4_BOOT_ID_BYTES) ||
        (p.image.key_count <= TV4_BOOT_KEYS_MAX &&
         aliases(out, sizeof(*out), p.image.keys, p.image.key_count * sizeof(*p.image.keys),
                 TV4_BOOT_KEYS_MAX * sizeof(*p.image.keys)))) return TV4_BOOT_ARGUMENT;
    *out = (tv4_boot_release_candidate){0};
    if (!tv4_boot_release_policy_valid(&p)) return TV4_BOOT_POLICY;
    tv4_boot_release_candidate candidate = {0};
    tv4_boot_result result = admit(&p.image, &r, w, &candidate.image, &p, &candidate);
    if (result == TV4_BOOT_VERIFIED_CANDIDATE) *out = candidate;
    else *out = (tv4_boot_release_candidate){0};
    mbedtls_platform_zeroize(&candidate, sizeof(candidate));
    return result;
}
