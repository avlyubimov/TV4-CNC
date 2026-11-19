#include "resource_guard.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
typedef struct { tv4_ota_resource_sample sample;unsigned reads;bool unavailable; } fixture;
/* Synthetic unit-test floors only; not a production policy or measured budget. */
static const tv4_ota_resource_policy policy={true,32768,8192,65536,32768,2048};
static bool read_sample(void *context,tv4_ota_resource_sample *out) {
    fixture *f=context;f->reads++;*out=f->sample;return !f->unavailable;
}
static fixture healthy(void) {
    return (fixture){{32768,8192,65536,32768,2048},0,false};
}
int main(void) {
    fixture f=healthy();tv4_ota_resource_sample out;
    assert(tv4_ota_resource_check(&policy,read_sample,&f,&out)==TV4_RESOURCE_OK && f.reads==1);
    struct { size_t offset,floor;tv4_ota_resource_result reason; } cases[]={
        {offsetof(tv4_ota_resource_sample,internal_free),policy.internal_free_floor,TV4_RESOURCE_INTERNAL_FREE},
        {offsetof(tv4_ota_resource_sample,internal_largest),policy.internal_largest_floor,TV4_RESOURCE_INTERNAL_LARGEST},
        {offsetof(tv4_ota_resource_sample,psram_free),policy.psram_free_floor,TV4_RESOURCE_PSRAM_FREE},
        {offsetof(tv4_ota_resource_sample,psram_largest),policy.psram_largest_floor,TV4_RESOURCE_PSRAM_LARGEST},
        {offsetof(tv4_ota_resource_sample,stack_watermark),policy.stack_floor,TV4_RESOURCE_STACK}};
    for(size_t i=0;i<sizeof(cases)/sizeof(cases[0]);i++) {
        f=healthy();*(size_t *)((unsigned char *)&f.sample+cases[i].offset)=cases[i].floor-1;
        assert(tv4_ota_resource_check(&policy,read_sample,&f,&out)==cases[i].reason && f.reads==1);
    }
    f=healthy();f.sample.internal_largest=f.sample.internal_free+1;
    assert(tv4_ota_resource_check(&policy,read_sample,&f,&out)==TV4_RESOURCE_INVALID_SAMPLE);
    f=healthy();f.sample.psram_largest=f.sample.psram_free+1;
    assert(tv4_ota_resource_check(&policy,read_sample,&f,&out)==TV4_RESOURCE_INVALID_SAMPLE);
    f=healthy();f.unavailable=true;
    assert(tv4_ota_resource_check(&policy,read_sample,&f,&out)==TV4_RESOURCE_UNAVAILABLE);
    assert(!out.internal_free && !out.psram_free && !out.stack_watermark);
    tv4_ota_resource_policy p=policy;p.reviewed=false;f=healthy();
    assert(tv4_ota_resource_check(&p,read_sample,&f,&out)==TV4_RESOURCE_NOT_REVIEWED && !f.reads);
    assert(tv4_ota_resource_check(NULL,read_sample,&f,&out)==TV4_RESOURCE_ARGUMENT && !f.reads);
    assert(tv4_ota_resource_check(&policy,NULL,&f,&out)==TV4_RESOURCE_ARGUMENT && !f.reads);
    assert(tv4_ota_resource_check(&policy,read_sample,&f,NULL)==TV4_RESOURCE_ARGUMENT && !f.reads);
    for(unsigned i=0;i<5;i++) {
        p=policy;
        switch(i) {
            case 0:p.internal_free_floor=0;break;case 1:p.internal_largest_floor=0;break;
            case 2:p.psram_free_floor=0;break;case 3:p.psram_largest_floor=0;break;default:p.stack_floor=0;break;
        }
        assert(tv4_ota_resource_check(&p,read_sample,&f,&out)==TV4_RESOURCE_ARGUMENT && !f.reads);
    }
    p=policy;p.internal_largest_floor=p.internal_free_floor+1;
    assert(tv4_ota_resource_policy_validate(&p)==TV4_RESOURCE_ARGUMENT);
    p=policy;p.psram_largest_floor=p.psram_free_floor+1;
    assert(tv4_ota_resource_policy_validate(&p)==TV4_RESOURCE_ARGUMENT);
    p=policy;p.stack_floor=SIZE_MAX;f=healthy();
    assert(tv4_ota_resource_check(&p,read_sample,&f,&out)==TV4_RESOURCE_STACK);
    p=policy;p.psram_free_floor=SIZE_MAX;
    assert(tv4_ota_resource_check(&p,read_sample,&f,&out)==TV4_RESOURCE_PSRAM_FREE);
    assert(!tv4_ota_resource_sample_current(NULL,&out) && !out.internal_free && !out.stack_watermark);
    assert(!tv4_ota_resource_sample_current(NULL,NULL));
    puts("Resource policy bounds/review/current heap/stack/unavailable/fail-closed sampler PASS; no allocation/network");
}
