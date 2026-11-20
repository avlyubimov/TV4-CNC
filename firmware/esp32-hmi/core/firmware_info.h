#pragma once
#include <stdbool.h>
#include <stddef.h>

enum { FIRMWARE_INFO_TEXT_CAPACITY=32 };
typedef enum { FIRMWARE_INFO_UNKNOWN, FIRMWARE_INFO_DEVICE, FIRMWARE_INFO_SIMULATOR } firmware_info_source_t;
typedef enum {
    FIRMWARE_PROFILE_UNKNOWN, FIRMWARE_PROFILE_HEADLESS, FIRMWARE_PROFILE_REV12,
    FIRMWARE_PROFILE_MEMORY_BENCH, FIRMWARE_PROFILE_TLS_BENCH
} firmware_info_profile_t;
/* Local display metadata only: never SemVer/trust/peer/install authority.
 * No candidate, network data, secrets or update readiness flags. */
typedef struct {
    char build[FIRMWARE_INFO_TEXT_CAPACITY],sdk[FIRMWARE_INFO_TEXT_CAPACITY];
    firmware_info_source_t source;
    firmware_info_profile_t profile;
} firmware_info_t;

void firmware_info_reset(firmware_info_t *info);
/* Copies printable ASCII C strings from explicit bounded spans; at most31bytes.
 * No silent truncation. One invalid field clears the entire snapshot to UNKNOWN.
 * Inputs may alias the previous info strings; callers own all supplied spans.
 * Source/profile describe local origin/configuration, not physical readiness. */
bool firmware_info_set(firmware_info_t *out,const char *build,size_t build_capacity,
    const char *sdk,size_t sdk_capacity,firmware_info_source_t source,
    firmware_info_profile_t profile);
