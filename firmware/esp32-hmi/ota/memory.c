#include "memory.h"
#include <string.h>
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#include "esp_heap_caps.h"
#include "esp_memory_utils.h"
#endif
_Static_assert(sizeof(tv4_ota_session)<=36u*1024u,"Review bounded OTA workspace growth");
const uint32_t tv4_ota_workspace_bytes=sizeof(tv4_ota_session);
static bool headroom(const tv4_ota_heap *h,const tv4_ota_memory_policy *p) {
    return h->internal_free>=p->internal_free_floor && h->internal_largest>=p->internal_largest_floor;
}
tv4_ota_memory_result tv4_ota_memory_acquire(tv4_ota_memory *lease,const tv4_ota_memory_policy *policy,const tv4_ota_memory_ops *ops) {
    if(!lease)return TV4_MEMORY_ARGUMENT;
    if(lease->session)return TV4_MEMORY_BUSY;
    if(!policy || !ops || !ops->snapshot || !ops->allocate_external || !ops->external || !ops->release)
        return TV4_MEMORY_ARGUMENT;
    if(!policy->reviewed)return TV4_MEMORY_NOT_REVIEWED;
    if(!policy->internal_free_floor || !policy->internal_largest_floor || !policy->psram_free_floor ||
       policy->internal_largest_floor>policy->internal_free_floor)return TV4_MEMORY_ARGUMENT;
    tv4_ota_heap before={0};
    if(!ops->snapshot(ops->context,&before))return TV4_MEMORY_UNAVAILABLE;
    if(!headroom(&before,policy))return TV4_MEMORY_HEADROOM;
    /* Subtraction avoids overflow for hostile/unmeasured size_t floors. */
    if(before.psram_free<policy->psram_free_floor || before.psram_free-policy->psram_free_floor<sizeof(tv4_ota_session) ||
       before.psram_largest<sizeof(tv4_ota_session))return TV4_MEMORY_PSRAM;
    tv4_ota_session *session=ops->allocate_external(ops->context,sizeof(*session));
    if(!session)return TV4_MEMORY_ALLOCATION;
    if(!ops->external(ops->context,session) || !ops->external(ops->context,(uint8_t *)session+sizeof(*session)-1)) {
        ops->release(ops->context,session);return TV4_MEMORY_WRONG_REGION;
    }
    tv4_ota_heap after={0};
    if(!ops->snapshot(ops->context,&after)) {ops->release(ops->context,session);return TV4_MEMORY_UNAVAILABLE;}
    if(!headroom(&after,policy) || after.psram_free<policy->psram_free_floor) {
        ops->release(ops->context,session);return TV4_MEMORY_HEADROOM;
    }
    memset(session,0,sizeof(*session));lease->session=session;lease->owner=*ops;
    return TV4_MEMORY_OK;
}
tv4_ota_memory_result tv4_ota_memory_release(tv4_ota_memory *lease) {
    if(!lease)return TV4_MEMORY_ARGUMENT;
    if(!lease->session)return TV4_MEMORY_OK;
    switch(lease->session->result.state) {
        case TV4_SESSION_IDLE:case TV4_SESSION_CANDIDATE_READY:case TV4_SESSION_IMAGE_VERIFIED:
        case TV4_SESSION_FAILED:case TV4_SESSION_CANCELLED:case TV4_SESSION_INSTALLED:break;
        default:return TV4_MEMORY_BUSY;
    }
    /* Only a successfully acquired lease may be released. */
    if(!lease->owner.release)return TV4_MEMORY_ARGUMENT;
    tv4_ota_session_reset(lease->session);
    lease->owner.release(lease->owner.context,lease->session);memset(lease,0,sizeof(*lease));
    return TV4_MEMORY_OK;
}
#if defined(ESP_PLATFORM) && CONFIG_TV4_BOARD_REV12 && CONFIG_SPIRAM
static bool snapshot(void *context,tv4_ota_heap *h) {
    (void)context;
    const uint32_t internal=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT,external=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
    h->internal_free=heap_caps_get_free_size(internal);h->internal_largest=heap_caps_get_largest_free_block(internal);
    h->psram_free=heap_caps_get_free_size(external);h->psram_largest=heap_caps_get_largest_free_block(external);
    return true;
}
static void *allocate(void *context,size_t bytes) {(void)context;return heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
static bool external(void *context,const void *p) {(void)context;return esp_ptr_external_ram(p);}
static void release(void *context,void *p) {(void)context;heap_caps_free(p);}
#endif
tv4_ota_memory_result tv4_ota_memory_acquire_psram(tv4_ota_memory *lease,const tv4_ota_memory_policy *policy) {
#if defined(ESP_PLATFORM) && CONFIG_TV4_BOARD_REV12 && CONFIG_SPIRAM
    const tv4_ota_memory_ops ops={NULL,snapshot,allocate,external,release};
    return tv4_ota_memory_acquire(lease,policy,&ops);
#else
    (void)lease;(void)policy;return TV4_MEMORY_UNAVAILABLE;
#endif
}
