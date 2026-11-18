#pragma once
#include "manifest.h"
#define TV4_OTA_ENVELOPE_MAX 2048u
/* Local reviewed context only; no downloaded public key accepted.
 * Envelope: exactly key_id/algorithm/signature; canonical base64 DER P-256.
 * Output cleared on error; envelope tree released before native verification. */
tv4_ota_result tv4_ota_signed_manifest_read(const uint8_t *raw,size_t raw_length,
    const uint8_t *envelope,size_t envelope_length,const tv4_ota_context *context,
    tv4_ota_candidate *out);
