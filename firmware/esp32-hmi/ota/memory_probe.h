#pragma once
#include "session.h"
/* Bench-only CPU/cache-visible pattern check of metadata and HTTP chunk buffers.
 * Idle, owned session only. Clears tested buffers; not a whole PSRAM memtest. */
bool tv4_ota_memory_probe_buffers(tv4_ota_session *session);
/* Called once by startup only when explicit memory-bench profile is enabled.
 * Schedules finite low-priority worker, never HTTPS/session_run/Flash/boot ops. */
bool tv4_ota_memory_bench_start(void);
