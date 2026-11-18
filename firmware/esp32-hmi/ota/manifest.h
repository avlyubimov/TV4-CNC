#pragma once
#include "verification.h"
/* Caller supplies local, trusted context, not values copied from a download.
 * Empty/unknown versions or keys fail closed. Single HMI component only. */
typedef struct {
    const char *key_id, *installed_version, *bootloader_version, *protocol_version;
    const char *motion_version; /* NULL unless a real peer version is known. */
    const char *layout_id;
    bool prerelease, board_rev12;
    const uint8_t *public_key;
    size_t public_key_length;
} tv4_ota_context;
typedef struct {
    char release[65],version[65],url[513],notes[4097];
    uint32_t size_bytes;
    uint8_t sha256[32];
} tv4_ota_candidate;
typedef enum {
    TV4_OTA_OK, TV4_OTA_NO_TRUST, TV4_OTA_BAD_SIGNATURE, TV4_OTA_BAD_JSON,
    TV4_OTA_BAD_SCHEMA, TV4_OTA_WRONG_TARGET, TV4_OTA_WRONG_LAYOUT,
    TV4_OTA_WRONG_VERSION, TV4_OTA_WRONG_PROTOCOL, TV4_OTA_WRONG_PEER,
    TV4_OTA_BAD_SIZE, TV4_OTA_BAD_URL, TV4_OTA_BAD_ENVELOPE
} tv4_ota_result;
/* DER signature; envelope decoding remains in the future transport layer.
 * Verifies exact bytes BEFORE bounded JSON parsing. No network/Flash/LVGL.
 * Output is cleared on every rejection. Requires a worker, never UI callback. */
tv4_ota_result tv4_ota_manifest_read(const uint8_t *raw,size_t length,
    const uint8_t *signature,size_t signature_length,const tv4_ota_context *context,
    tv4_ota_candidate *out);
/* Strict SemVer subset: no build metadata, uint32 core numbers, <=64 bytes. */
bool tv4_ota_version_compare(const char *a,const char *b,int *comparison);
