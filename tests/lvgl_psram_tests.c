/* Actual allocator with the pinned LVGL9 headers and a host SDK boundary. */
#include "lvgl.h"
#include "lvgl_psram_sdk.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>

static unsigned allocations,reallocations,frees,integrity;
static bool fail_allocation,corrupt;
static void caps(uint32_t value) {assert(value==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));}
void *heap_caps_malloc(size_t n,uint32_t c)
{
    caps(c);++allocations;return fail_allocation||!n?NULL:malloc(n);
}
void *heap_caps_realloc(void *p,size_t n,uint32_t c)
{
    caps(c);++reallocations;
    if(!n) {free(p);return NULL;}
    return fail_allocation?NULL:realloc(p,n);
}
void heap_caps_free(void *p) {++frees;free(p);}
bool heap_caps_check_integrity(uint32_t c,bool print_errors)
{
    caps(c);assert(!print_errors);++integrity;return !corrupt;
}
int main(void)
{
    lv_mem_init();assert(!allocations && !reallocations && !frees);
    unsigned char *p=lv_malloc_core(32);assert(p);memset(p,0x5a,32);
    unsigned char *q=lv_realloc_core(p,64);assert(q);
    for(unsigned i=0;i<32;i++)assert(q[i]==0x5a);
    fail_allocation=true;
    assert(!lv_malloc_core(SIZE_MAX));assert(!lv_realloc_core(q,SIZE_MAX));
    for(unsigned i=0;i<32;i++)assert(q[i]==0x5a); /* Failed realloc retains owner. */
    lv_free_core(q);lv_free_core(NULL);
    assert(allocations==2 && reallocations==2 && frees==2); /* No fallback. */
    fail_allocation=false;
    p=lv_realloc_core(NULL,8);assert(p);assert(!lv_realloc_core(p,0));
    assert(lv_mem_test_core()==LV_RESULT_OK);corrupt=true;
    assert(lv_mem_test_core()==LV_RESULT_INVALID && integrity==2);
    lv_mem_monitor_t monitor;memset(&monitor,0xff,sizeof(monitor));
    lv_mem_monitor_core(&monitor);lv_mem_monitor_core(NULL);
    const unsigned char *bytes=(const unsigned char *)&monitor;
    for(size_t i=0;i<sizeof(monitor);i++)assert(!bytes[i]);
    assert(!lv_mem_add_pool(&monitor,sizeof(monitor)));lv_mem_remove_pool(NULL);
    lv_mem_deinit();assert(allocations==2 && reallocations==4 && frees==2);
    return 0;
}
