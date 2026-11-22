#include "firmware_info.h"
#include <string.h>
_Static_assert(sizeof(firmware_info_t)<=72,"Local display snapshot budget changed");

void firmware_info_reset(firmware_info_t *info) { if(info)memset(info,0,sizeof(*info)); }
static bool copy_text(char out[FIRMWARE_INFO_TEXT_CAPACITY],const char *text,size_t capacity) {
    if(!text || !capacity)return false;
    bool nonspace=false;
    for(size_t i=0;i<capacity && i<FIRMWARE_INFO_TEXT_CAPACITY;i++) {
        unsigned char c=(unsigned char)text[i];
        if(!c)return i>0 && nonspace;
        if(c<32 || c>126)return false;
        out[i]=(char)c;if(c!=' ')nonspace=true;
    }
    return false;
}
bool firmware_info_set(firmware_info_t *out,const char *build,size_t build_capacity,
    const char *sdk,size_t sdk_capacity,firmware_info_source_t source,
    firmware_info_profile_t profile) {
    if(!out)return false;
    firmware_info_t next={0};
    bool valid=(source==FIRMWARE_INFO_DEVICE || source==FIRMWARE_INFO_SIMULATOR) &&
        profile>=FIRMWARE_PROFILE_HEADLESS && profile<=FIRMWARE_PROFILE_OTA &&
        copy_text(next.build,build,build_capacity) && copy_text(next.sdk,sdk,sdk_capacity);
    if(!valid) { firmware_info_reset(out);return false; }
    next.source=source;next.profile=profile;*out=next;return true;
}
