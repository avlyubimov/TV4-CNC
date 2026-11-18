#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define TV4_OTA_MANIFEST_MAX 16384u
#define TV4_OTA_SIGNATURE_MAX 72u
/* Canonical DER SPKI, named prime256v1, uncompressed public point (91 bytes).
 * Key comes only from a reviewed local trust store, never from a download.
 * These are verification primitives, not a manifest parser or installer. */
bool tv4_ota_p256_key(const uint8_t *der,size_t length);
bool tv4_ota_sha256(const uint8_t *data,size_t length,uint8_t digest[32]);
bool tv4_ota_verify_manifest(const uint8_t *manifest,size_t length,
                            const uint8_t *signature,size_t signature_length,
                            const uint8_t *trusted_key,size_t key_length);
