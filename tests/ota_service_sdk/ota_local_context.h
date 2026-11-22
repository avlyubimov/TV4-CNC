/* Host-only missing-context fixture. Invalid zero key, never firmware trust. */
#pragma once
#include <stdint.h>
#ifndef SERVICE_TEST_PROVISIONED
#define SERVICE_TEST_PROVISIONED 0
#endif
#define TV4_OTA_PROVISIONED SERVICE_TEST_PROVISIONED
static const uint8_t ota_public_key[] = {0};
#define OTA_KEY_ID ""
#define OTA_INSTALLED_VERSION ""
#define OTA_BOOTLOADER_VERSION ""
#define OTA_PROTOCOL_VERSION ""
#define OTA_MANIFEST_URL ""
#define OTA_PRERELEASE 0
#define OTA_FLOOR_INTERNAL_FREE 0u
#define OTA_FLOOR_INTERNAL_LARGEST 0u
#define OTA_FLOOR_PSRAM_FREE 0u
#define OTA_FLOOR_PSRAM_LARGEST 0u
#define OTA_FLOOR_STACK 0u
#define OTA_TIME_EARLIEST_EPOCH 0u
#define OTA_TIME_LATEST_EPOCH 0u
#define OTA_TIME_MAXIMUM_AGE_MS 0u
#define OTA_TIME_MAXIMUM_SKEW_MS 0u
