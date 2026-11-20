#include "firmware_info.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void unknown(const firmware_info_t *info) {
    const unsigned char *p=(const unsigned char *)info;
    for(size_t i=0;i<sizeof(*info);i++)assert(p[i]==0);
}
static void reject(const char *build,size_t bn,const char *sdk,size_t sn,
    firmware_info_source_t source,firmware_info_profile_t profile) {
    firmware_info_t info;memset(&info,0xa5,sizeof(info));
    assert(!firmware_info_set(&info,build,bn,sdk,sn,source,profile));unknown(&info);
}
int main(void) {
    firmware_info_t info;
    firmware_info_reset(&info);unknown(&info);firmware_info_reset(NULL);
    const char build[]="git-build-dirty",sdk[]="v6.1.1";
    for(unsigned source=FIRMWARE_INFO_DEVICE;source<=FIRMWARE_INFO_SIMULATOR;source++)
        for(unsigned profile=FIRMWARE_PROFILE_HEADLESS;profile<=FIRMWARE_PROFILE_TLS_BENCH;profile++) {
            assert(firmware_info_set(&info,build,sizeof(build),sdk,sizeof(sdk),
                (firmware_info_source_t)source,(firmware_info_profile_t)profile));
            assert(!strcmp(info.build,build) && !strcmp(info.sdk,sdk));
            assert(info.source==(firmware_info_source_t)source && info.profile==(firmware_info_profile_t)profile);
        }
    assert(!firmware_info_set(NULL,build,sizeof(build),sdk,sizeof(sdk),FIRMWARE_INFO_DEVICE,FIRMWARE_PROFILE_REV12));
    char longest[32];memset(longest,'W',31);longest[31]=0;
    assert(firmware_info_set(&info,longest,sizeof(longest),longest,sizeof(longest),FIRMWARE_INFO_DEVICE,FIRMWARE_PROFILE_REV12));
    memset(longest,'a',31);assert(info.build[0]=='W' && info.sdk[0]=='W');
    assert(firmware_info_set(&info,info.build,sizeof(info.build),info.sdk,sizeof(info.sdk),info.source,info.profile));
    assert(info.build[0]=='W' && strlen(info.build)==31);
    /* Each unterminated span must fail without reading past its supplied size. */
    char raw[33];memset(raw,'a',sizeof(raw));
    for(size_t size=0;size<=sizeof(raw);size++) {
        reject(raw,size,sdk,sizeof(sdk),FIRMWARE_INFO_DEVICE,FIRMWARE_PROFILE_REV12);
        reject(build,sizeof(build),raw,size,FIRMWARE_INFO_DEVICE,FIRMWARE_PROFILE_REV12);
    }
    raw[32]=0;reject(raw,sizeof(raw),sdk,sizeof(sdk),FIRMWARE_INFO_DEVICE,FIRMWARE_PROFILE_REV12);
    for(unsigned c=1;c<256;c++)if(c<32 || c>126) {
        char text[]={'a',(char)c,0};
        reject(text,sizeof(text),sdk,sizeof(sdk),FIRMWARE_INFO_DEVICE,FIRMWARE_PROFILE_REV12);
    }
    const char *bad[]={NULL,"","   ","line\nline","é"};
    for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++)
        reject(bad[i],bad[i]?strlen(bad[i])+1:0,sdk,sizeof(sdk),FIRMWARE_INFO_DEVICE,FIRMWARE_PROFILE_REV12);
    reject(build,sizeof(build),sdk,sizeof(sdk),FIRMWARE_INFO_UNKNOWN,FIRMWARE_PROFILE_REV12);
    reject(build,sizeof(build),sdk,sizeof(sdk),(firmware_info_source_t)99,FIRMWARE_PROFILE_REV12);
    reject(build,sizeof(build),sdk,sizeof(sdk),FIRMWARE_INFO_DEVICE,FIRMWARE_PROFILE_UNKNOWN);
    reject(build,sizeof(build),sdk,sizeof(sdk),FIRMWARE_INFO_DEVICE,(firmware_info_profile_t)99);
    printf("Firmware display metadata PASS: local origins/profiles,31-byte bounds, "
        "unterminated/invalid spans, independent copies/alias, atomic UNKNOWN; sizeof=%zu\n",sizeof(info));
}
