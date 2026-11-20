#ifndef TV4_BOOT_METADATA_H
#define TV4_BOOT_METADATA_H
#include "admission.h"
typedef struct {
    char hardware_id[TV4_BOOT_ID_BYTES], hardware_revision[TV4_BOOT_ID_BYTES];
    char memory_profile[TV4_BOOT_ID_BYTES];
    uint32_t image_counter, minimum_bootloader_counter, protocol_min, protocol_max, image_size;
    uint8_t sha256[32];
} tv4_boot_metadata;
/* Private, flat ASCII draft schema. Called only after signature verification. */
bool tv4_boot_parse_metadata(const uint8_t *bytes, size_t length, tv4_boot_metadata *out);
bool tv4_boot_identifier(const char value[TV4_BOOT_ID_BYTES]);
#endif
