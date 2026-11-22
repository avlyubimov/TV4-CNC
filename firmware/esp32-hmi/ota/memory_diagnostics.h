#pragma once
#include <stdbool.h>
#include <stdint.h>
/* Diagnostic point samples, not reservations, simultaneous peaks or OTA floors. */
typedef enum {TV4_HEAP_HISTORY,TV4_HEAP_WINDOW,TV4_HEAP_UNKNOWN} tv4_heap_scope;
typedef struct {
    uint32_t ms,internal_free,internal_min,internal_largest;
    uint32_t psram_free,psram_min,psram_largest,sampler_stack;
    tv4_heap_scope scope;
} tv4_ota_memory_sample;
typedef enum {TV4_MEM_APP_ENTRY,TV4_MEM_RGB_READY,TV4_MEM_DRAW_READY,
    TV4_MEM_TOUCH_READY,TV4_MEM_WIFI_STARTED,TV4_MEM_DASHBOARD_READY,
    TV4_MEM_UI_READY,TV4_MEM_STARTUP_COUNT} tv4_ota_startup_phase;
typedef enum {TV4_MEM_BOOT=1,TV4_MEM_WORKSPACE,TV4_MEM_SNTP,
    TV4_MEM_SESSION,TV4_MEM_CLEANUP,TV4_MEM_TLS_PROBE} tv4_ota_memory_window;
/* Main posts each of seven fixed startup slots once; no logging/allocation.
 * Sampling is task-context only, never a LVGL callback or ISR. */
void tv4_ota_memory_capture(tv4_ota_memory_sample *);
/* Surround existing capability reads to label transitions as UNKNOWN without
 * adding another heap traversal to the periodic LVGL-owner sample. */
uint32_t tv4_ota_memory_scope_revision(void);
tv4_heap_scope tv4_ota_memory_scope_since(uint32_t);
bool tv4_ota_memory_startup(tv4_ota_startup_phase);
/* Serialized OTA service lifecycle only: boot begin before worker creation;
 * worker owns subsequent begin/end/report. Task-create failure may end without
 * report in main. Start failure never calls SDK stop. Profile exclusion and
 * this serialized lifecycle provide ownership; the pinned SDK does not reject
 * nested start. Nothing here authorizes installation. Unknown stop disables
 * further monitoring. */
bool tv4_ota_memory_window_begin(tv4_ota_memory_window);
bool tv4_ota_memory_window_end(tv4_ota_memory_window);
/* Existing OTA worker only. Fixed numeric records, no identifiers/credentials. */
void tv4_ota_memory_report_window(void);
void tv4_ota_memory_report_startup(void);
