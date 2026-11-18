#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* LVGL owner only. Fixed logarithmic bins: p95 is an upper-bound estimate. */
typedef struct { uint32_t bins[17], count, maximum; } diag_histogram_t;
typedef struct {
    uint32_t uptime_ms, window_ms, refresh_hz, render_max_ms, render_p95_ms;
    uint32_t render_samples, event_samples, touch_age_ms;
    uint32_t event_max_ms, event_p95_ms, owner_max_us, owner_busy_permille;
    uint32_t internal_free, internal_min, internal_largest;
    uint32_t psram_free, psram_min, psram_total;
    uint32_t owner_stack, touch_stack, wifi_stack, export_stack;
    uint32_t queue_drops, mock_can_timeouts, display_errors, export_drops;
    uint32_t wifi_attempts, wifi_disconnects, wifi_ip_events, wifi_queue_drops;
    uint32_t touch_errors, reset_reason, watchdog_boot;
    bool board, resources_valid, rev12;
} diag_record_t;
typedef struct {
    diag_record_t record;
    diag_histogram_t render, event;
    uint32_t started_ms, frames, work_max;
    uint64_t work_total;
} diagnostics_t;
void diag_increment(uint32_t *value);
void diag_histogram_add(diag_histogram_t *histogram, uint32_t value);
uint32_t diag_histogram_p95(const diag_histogram_t *histogram);
void diagnostics_init(diagnostics_t *d, uint32_t now_ms);
void diagnostics_render(diagnostics_t *d, uint32_t elapsed_ms);
void diagnostics_event(diagnostics_t *d, uint32_t elapsed_ms);
void diagnostics_work(diagnostics_t *d, uint32_t elapsed_us);
bool diagnostics_window(diagnostics_t *d, uint32_t now_ms);
/* Numeric-only export: no SSID, credentials, paths or private device identifiers.
 * false on truncation; output is cleared instead of exporting partial JSON. */
bool diagnostics_format(const diag_record_t *r, char *out, size_t size);
