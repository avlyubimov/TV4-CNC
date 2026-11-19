#include "resource_guard.h"
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#if CONFIG_TV4_BOARD_REV12 && CONFIG_SPIRAM
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
#endif
tv4_ota_resource_result tv4_ota_resource_policy_validate(const tv4_ota_resource_policy *p) {
    if(!p)return TV4_RESOURCE_ARGUMENT;
    if(!p->reviewed)return TV4_RESOURCE_NOT_REVIEWED;
    if(!p->internal_free_floor || !p->internal_largest_floor || !p->psram_free_floor ||
       !p->psram_largest_floor || !p->stack_floor ||
       p->internal_largest_floor>p->internal_free_floor || p->psram_largest_floor>p->psram_free_floor)
        return TV4_RESOURCE_ARGUMENT;
    return TV4_RESOURCE_OK;
}
tv4_ota_resource_result tv4_ota_resource_check(const tv4_ota_resource_policy *p,
    tv4_ota_resource_reader reader,void *context,tv4_ota_resource_sample *out) {
    if(!out)return TV4_RESOURCE_ARGUMENT;
    *out=(tv4_ota_resource_sample){0};
    tv4_ota_resource_result valid=tv4_ota_resource_policy_validate(p);
    if(valid!=TV4_RESOURCE_OK)return valid;
    if(!reader)return TV4_RESOURCE_ARGUMENT;
    tv4_ota_resource_sample sample={0};
    if(!reader(context,&sample))return TV4_RESOURCE_UNAVAILABLE;
    *out=sample;
    if(sample.internal_largest>sample.internal_free || sample.psram_largest>sample.psram_free)
        return TV4_RESOURCE_INVALID_SAMPLE;
    if(sample.internal_free<p->internal_free_floor)return TV4_RESOURCE_INTERNAL_FREE;
    if(sample.internal_largest<p->internal_largest_floor)return TV4_RESOURCE_INTERNAL_LARGEST;
    if(sample.psram_free<p->psram_free_floor)return TV4_RESOURCE_PSRAM_FREE;
    if(sample.psram_largest<p->psram_largest_floor)return TV4_RESOURCE_PSRAM_LARGEST;
    if(sample.stack_watermark<p->stack_floor)return TV4_RESOURCE_STACK;
    return TV4_RESOURCE_OK;
}
bool tv4_ota_resource_sample_current(void *context,tv4_ota_resource_sample *out) {
    (void)context;if(!out)return false;
    *out=(tv4_ota_resource_sample){0};
#if defined(ESP_PLATFORM) && CONFIG_TV4_BOARD_REV12 && CONFIG_SPIRAM
    const uint32_t internal=MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT,external=MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT;
    if(!heap_caps_get_total_size(external))return false;
    out->internal_free=heap_caps_get_free_size(internal);
    out->internal_largest=heap_caps_get_largest_free_block(internal);
    out->psram_free=heap_caps_get_free_size(external);
    out->psram_largest=heap_caps_get_largest_free_block(external);
    out->stack_watermark=uxTaskGetStackHighWaterMark(NULL);
    return true;
#else
    return false;
#endif
}
