#pragma once
#include "session.h"
/* Single worker owner, initialized {0}; no concurrent acquire/run/release.
 * Policy comes from local reviewed measured headroom, never downloaded metadata.
 * No arbitrary production default.
 * Admission is a snapshot, not a reservation of TLS/internal memory. */
typedef struct { size_t internal_free,internal_largest,psram_free,psram_largest; } tv4_ota_heap;
typedef struct {
    bool reviewed;
    size_t internal_free_floor,internal_largest_floor,psram_free_floor;
} tv4_ota_memory_policy;
typedef enum { TV4_MEMORY_OK,TV4_MEMORY_ARGUMENT,TV4_MEMORY_BUSY,
    TV4_MEMORY_NOT_REVIEWED,TV4_MEMORY_UNAVAILABLE,TV4_MEMORY_HEADROOM,
    TV4_MEMORY_PSRAM,TV4_MEMORY_ALLOCATION,TV4_MEMORY_WRONG_REGION } tv4_ota_memory_result;
typedef struct {
    void *context;
    bool (*snapshot)(void *,tv4_ota_heap *);
    void *(*allocate_external)(void *,size_t);
    bool (*external)(void *,const void *);
    void (*release)(void *,void *);
} tv4_ota_memory_ops;
typedef struct {
    tv4_ota_session *session;
    tv4_ota_memory_ops owner; /* context must outlive lease */
} tv4_ota_memory;
/* Exact target sizeof exported for ELF validation without allocating a BSS buffer. */
extern const uint32_t tv4_ota_workspace_bytes;
tv4_ota_memory_result tv4_ota_memory_acquire(tv4_ota_memory *,const tv4_ota_memory_policy *,const tv4_ota_memory_ops *);
/* SDK rev12: MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT only, no internal fallback.
 * Host/headless/no PSRAM: UNAVAILABLE; never starts a task/network operation. */
tv4_ota_memory_result tv4_ota_memory_acquire_psram(tv4_ota_memory *,const tv4_ota_memory_policy *);
/* Outside run only; refuses active stages, resets/closes hashes before freeing.
 * Empty release succeeds. Owner must drop all borrowed session pointers. */
tv4_ota_memory_result tv4_ota_memory_release(tv4_ota_memory *);
