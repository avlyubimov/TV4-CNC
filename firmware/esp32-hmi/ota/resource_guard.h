#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Current capability-heap samples plus this worker's lifetime stack watermark
 * (ESP-IDF bytes). Not an atomic snapshot, reservation, or transient-peak trace. */
typedef struct {
    size_t internal_free,internal_largest,psram_free,psram_largest,stack_watermark;
} tv4_ota_resource_sample;
typedef struct {
    bool reviewed;
    size_t internal_free_floor,internal_largest_floor,psram_free_floor,psram_largest_floor,stack_floor;
} tv4_ota_resource_policy;
typedef enum { TV4_RESOURCE_OK,TV4_RESOURCE_ARGUMENT,TV4_RESOURCE_NOT_REVIEWED,
    TV4_RESOURCE_UNAVAILABLE,TV4_RESOURCE_INVALID_SAMPLE,TV4_RESOURCE_INTERNAL_FREE,
    TV4_RESOURCE_INTERNAL_LARGEST,TV4_RESOURCE_PSRAM_FREE,TV4_RESOURCE_PSRAM_LARGEST,
    TV4_RESOURCE_STACK } tv4_ota_resource_result;
/* Same worker only. Reader must be bounded, nonallocating and do no I/O. */
typedef bool (*tv4_ota_resource_reader)(void *,tv4_ota_resource_sample *);
typedef struct {
    tv4_ota_resource_result result;
    uint32_t checks; /* Saturating attempts; zero means no observation. */
    tv4_ota_resource_sample sample;
} tv4_ota_resource_report;
/* Immutable local measured/reviewed policy; no default or downloaded floors. */
tv4_ota_resource_result tv4_ota_resource_policy_validate(const tv4_ota_resource_policy *);
tv4_ota_resource_result tv4_ota_resource_check(const tv4_ota_resource_policy *,
    tv4_ota_resource_reader,void *,tv4_ota_resource_sample *);
/* SDK rev12/PSRAM only; no tasks, heap-min monitor, network, or allocations.
 * Host/headless/unavailable PSRAM: false with zero output. Worker context only. */
bool tv4_ota_resource_sample_current(void *,tv4_ota_resource_sample *);
