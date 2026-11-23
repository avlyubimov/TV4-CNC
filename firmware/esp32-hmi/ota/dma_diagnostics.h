#pragma once
#include <stdbool.h>
#include <stdint.h>
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif
#if defined(TV4_OTA_DMA_DIAG_TEST)
#define TV4_DMA_TRACE TV4_OTA_DMA_DIAG_ENABLED
#elif defined(ESP_PLATFORM) && CONFIG_TV4_OTA_WORKER_TLS_BENCH
#define TV4_DMA_TRACE 1
#else
#define TV4_DMA_TRACE 0
#endif

/* Only the existing opt-in OTA-worker TLS probe. No resource floor or
 * reservation. A complete trace can describe a failed HTTPS operation. */
typedef enum {
    TV4_DMA_NONE = 0, TV4_DMA_SNTP, TV4_DMA_HTTP_INIT, TV4_DMA_HTTP_OPEN,
    TV4_DMA_HTTP_HEADERS, TV4_DMA_HTTP_CLEANUP
} tv4_dma_phase;
#if TV4_DMA_TRACE
/* The pinned SDK registration overwrites its global callback; ELF/archive
 * checks MUST establish this is the sole linked registration caller. There is
 * no runtime getter/claim API and no coexistence promise for arbitrary code.
 * Init is once/idempotent for one existing task; never resets event history. */
bool tv4_ota_dma_init(void);
bool tv4_ota_dma_begin(tv4_dma_phase);
bool tv4_ota_dma_end(tv4_dma_phase);
bool tv4_ota_dma_finish(void);
#else
static inline bool tv4_ota_dma_init(void) {return false;}
static inline bool tv4_ota_dma_begin(tv4_dma_phase p) {(void)p;return false;}
static inline bool tv4_ota_dma_end(tv4_dma_phase p) {(void)p;return false;}
static inline bool tv4_ota_dma_finish(void) {return false;}
#endif
