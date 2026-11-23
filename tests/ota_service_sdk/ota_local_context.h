/* Host-only missing-context fixture. Invalid zero key, never firmware trust. */
#pragma once
#include <stdint.h>
#ifndef SERVICE_TEST_PROVISIONED
#define SERVICE_TEST_PROVISIONED 0
#endif
#define TV4_OTA_PROVISIONED SERVICE_TEST_PROVISIONED
#ifndef SERVICE_TEST_INSTALL_REVIEWED
#define SERVICE_TEST_INSTALL_REVIEWED 0
#endif
#define OTA_INSTALL_REVIEWED SERVICE_TEST_INSTALL_REVIEWED
#ifndef SERVICE_TEST_VALID_TRUST
#define SERVICE_TEST_VALID_TRUST 0
#endif
#ifndef SERVICE_TEST_INVALID_POLICY
#define SERVICE_TEST_INVALID_POLICY 0
#endif
#if SERVICE_TEST_VALID_TRUST
/* Ephemeral host-only key generated in RAM, never a firmware trust anchor. */
extern uint8_t service_public_key[91];
#define ota_public_key service_public_key
#define OTA_KEY_ID "host-service-test-only"
#define OTA_INSTALLED_VERSION "0.1.0-bench.1"
#define OTA_BOOTLOADER_VERSION "0.1.0-bench.1"
#define OTA_PROTOCOL_VERSION "1.0.0"
#define OTA_MANIFEST_URL "https://github.com/avlyubimov/TV4-CNC/releases/download/v0.1.0-bench.2/manifest.json"
#define OTA_PRERELEASE 1
#define OTA_FLOOR_INTERNAL_FREE 65536u
#define OTA_FLOOR_INTERNAL_LARGEST (SERVICE_TEST_INVALID_POLICY==2?0u:32768u)
#define OTA_FLOOR_PSRAM_FREE 1048576u
#define OTA_FLOOR_PSRAM_LARGEST 131072u
#define OTA_FLOOR_STACK 3072u
#define OTA_TIME_EARLIEST_EPOCH 1700000000u
#define OTA_TIME_LATEST_EPOCH (SERVICE_TEST_INVALID_POLICY==1?1700000000u:2300000000u)
#define OTA_TIME_MAXIMUM_AGE_MS 60000u
#define OTA_TIME_MAXIMUM_SKEW_MS 2000u
#else
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
#endif
