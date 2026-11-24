#include "installer_sdk.h"
#include <string.h>
#ifdef ESP_PLATFORM
#include "sdkconfig.h"
#endif
#if defined(TV4_OTA_SDK_TEST)
#include "ota_sdk_fake.h"
#elif defined(ESP_PLATFORM) && CONFIG_TV4_OTA_ENABLED
#include "esp_ota_ops.h"
#include "esp_flash.h"
#endif
#if defined(TV4_OTA_SDK_TEST) || (defined(ESP_PLATFORM) && CONFIG_TV4_OTA_ENABLED)
static bool part(unsigned type,unsigned subtype,const char *label,uint32_t address,uint32_t size) {
    const esp_partition_t *p=esp_partition_find_first(type,subtype,label);
    return p && p->address==address && p->size==size && !p->encrypted;
}
bool tv4_ota_sdk_layout_valid(void) {
    uint32_t size=0,physical=0;
    return esp_flash_get_size(NULL,&size)==ESP_OK && size==0x1000000 &&
        esp_flash_get_physical_size(NULL,&physical)==ESP_OK && physical==size &&
        part(1,2,"nvs",0x9000,0x6000) && part(1,1,"phy_init",0xf000,0x1000) &&
        part(0,0,"factory",0x10000,0x200000) && part(1,0,"otadata",0x210000,0x2000) &&
        part(0,0x10,"ota_0",0x220000,0x300000) && part(0,0x11,"ota_1",0x520000,0x300000) &&
        part(1,0x40,"resources",0x820000,0x300000) && part(1,0x41,"store",0xb20000,0x4e0000);
}
static bool known_app(const esp_partition_t *p) {
    return p && p->type==ESP_PARTITION_TYPE_APP && !p->encrypted &&
        ((p->subtype==0 && p->address==0x10000 && p->size==0x200000) ||
         (p->subtype==0x10 && p->address==0x220000 && p->size==0x300000) ||
         (p->subtype==0x11 && p->address==0x520000 && p->size==0x300000));
}
bool tv4_ota_sdk_finish_boot(bool healthy) {
    const esp_partition_t *p=esp_ota_get_running_partition();
    if(!known_app(p))return false;
    if(p->subtype==ESP_PARTITION_SUBTYPE_APP_FACTORY)return true;
    esp_ota_img_states_t state;
    if(esp_ota_get_state_partition(p,&state)!=ESP_OK)return false;
    if(state==ESP_OTA_IMG_VALID)return true;
    if(state==ESP_OTA_IMG_PENDING_VERIFY) {
        if(healthy && tv4_ota_sdk_layout_valid() && esp_ota_mark_app_valid_cancel_rollback()==ESP_OK)return true;
        esp_ota_mark_app_invalid_rollback_and_reboot();
    }
    return false;
}
static const esp_partition_t *target(void) {
    if(!tv4_ota_sdk_layout_valid())return NULL;
    const esp_partition_t *r=esp_ota_get_running_partition(),*b=esp_ota_get_boot_partition();
    if(!known_app(r) || !known_app(b) || r->address!=b->address)return NULL; /* no second install before reboot */
    if(r->subtype!=ESP_PARTITION_SUBTYPE_APP_FACTORY) {
        esp_ota_img_states_t state;
        if(esp_ota_get_state_partition(r,&state)!=ESP_OK || state!=ESP_OTA_IMG_VALID)return NULL;
    }
    const esp_partition_t *p=esp_ota_get_next_update_partition(NULL);
    if(!known_app(p) || p->address==r->address || p->address==0x10000 ||
       (p->subtype!=0x10 && p->subtype!=0x11) || p->size!=0x300000)return NULL;
    return p;
}
bool tv4_ota_sdk_begin(void *context,const tv4_ota_candidate *c) {
    tv4_ota_sdk *s=context;
    if(!s || !c || !c->installable || !s->approval || s->opened || s->sealed)return false;
    const tv4_install_approval *a=s->approval;
    if(!c->size_bytes || c->size_bytes!=a->size || c->size_bytes>0x300000 ||
       strcmp(a->version,c->version) || strcmp(a->release,c->release) ||
       strcmp(a->url,c->url) || memcmp(a->sha256,c->sha256,32))return false;
    const esp_partition_t *p=target();if(!p)return false;
    s->partition=p;s->expected=c->size_bytes;s->written=0;
    s->native_error=esp_ota_begin(p,c->size_bytes,&s->handle);
    s->opened=s->native_error==ESP_OK;return s->opened;
}
bool tv4_ota_sdk_write(void *context,const uint8_t *p,size_t n) {
    tv4_ota_sdk *s=context;
    if(!s || !s->opened || !p || !n || s->written>s->expected || n>s->expected-s->written)return false;
    s->native_error=esp_ota_write(s->handle,p,n);if(s->native_error!=ESP_OK)return false;
    s->written+=(uint32_t)n;return true;
}
bool tv4_ota_sdk_seal(void *context) {
    tv4_ota_sdk *s=context;if(!s || !s->opened || s->written!=s->expected)return false;
    s->native_error=esp_ota_end(s->handle);s->opened=false; /* end consumes handle on failure too */
    s->sealed=s->native_error==ESP_OK;return s->sealed;
}
bool tv4_ota_sdk_activate(void *context) {
    tv4_ota_sdk *s=context;
    if(!s || !s->sealed || !s->commit_allowed || target()!=s->partition || !s->commit_allowed(s->owner))return false;
    s->native_error=esp_ota_set_boot_partition(s->partition);
    if(s->native_error!=ESP_OK)return false;
    s->sealed=false;return true;
}
void tv4_ota_sdk_abort(void *context) {
    tv4_ota_sdk *s=context;if(!s)return;
    if(s->opened) {s->native_error=esp_ota_abort(s->handle);s->opened=false;}
    s->sealed=false; /* never erase factory/active or select a partial image */
}
#else
bool tv4_ota_sdk_layout_valid(void) {return false;}
bool tv4_ota_sdk_finish_boot(bool healthy) {(void)healthy;return false;}
bool tv4_ota_sdk_begin(void *s,const tv4_ota_candidate *c) {(void)s;(void)c;return false;}
bool tv4_ota_sdk_write(void *s,const uint8_t *p,size_t n) {(void)s;(void)p;(void)n;return false;}
bool tv4_ota_sdk_seal(void *s) {(void)s;return false;}
bool tv4_ota_sdk_activate(void *s) {(void)s;return false;}
void tv4_ota_sdk_abort(void *s) {(void)s;}
#endif
