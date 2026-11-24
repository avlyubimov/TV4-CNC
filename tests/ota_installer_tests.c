/* Compile the ACTUAL SDK backend against a fake SDK; no alternative writer. */
#include "installer_sdk.h"
#include "ota_sdk_fake.h"
#include "service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
_Static_assert(ESP_OTA_IMG_NEW==0 && ESP_OTA_IMG_PENDING_VERIFY==1 &&
    ESP_OTA_IMG_VALID==2 && ESP_OTA_IMG_INVALID==3 && ESP_OTA_IMG_ABORTED==4 &&
    ESP_OTA_IMG_UNDEFINED==UINT32_MAX,"host SDK state values must match native otadata");
static esp_partition_t parts[8];
static const esp_partition_t *running,*boot,*next;
static esp_ota_img_states_t state;
static unsigned begins,writes,ends,selects,aborts,fail,accepted,rolled_back;
static uint32_t flash_size,physical_size;
static bool allow;
esp_err_t esp_flash_get_size(void *p,uint32_t *n) {(void)p;*n=flash_size;return fail==6?-1:0;}
esp_err_t esp_flash_get_physical_size(void *p,uint32_t *n) {(void)p;*n=physical_size;return fail==8?-1:0;}
const esp_partition_t *esp_partition_find_first(unsigned type,unsigned subtype,const char *label) {
    for(unsigned i=0;i<8;i++)if(parts[i].type==type && parts[i].subtype==subtype && !strcmp(parts[i].label,label))return &parts[i];
    return NULL;
}
const esp_partition_t *esp_ota_get_running_partition(void) {return running;}
const esp_partition_t *esp_ota_get_boot_partition(void) {return boot;}
const esp_partition_t *esp_ota_get_next_update_partition(const esp_partition_t *p) {(void)p;return next;}
esp_err_t esp_ota_get_state_partition(const esp_partition_t *p,esp_ota_img_states_t *out) {(void)p;*out=state;return fail==5?-1:0;}
esp_err_t esp_ota_begin(const esp_partition_t *p,size_t n,esp_ota_handle_t *h) {
    begins++;assert(p==next && p!=running && p!=&parts[2] && n==480);*h=7;return fail==1?-1:0;
}
esp_err_t esp_ota_write(esp_ota_handle_t h,const void *p,size_t n) {writes++;assert(h==7 && p && n<=480);return fail==2?-1:0;}
esp_err_t esp_ota_end(esp_ota_handle_t h) {ends++;assert(h==7);return fail==3?-1:0;}
esp_err_t esp_ota_set_boot_partition(const esp_partition_t *p) {selects++;assert(p==next && p!=running);
        if(fail==4)return -1;
    boot=p;return 0;}
esp_err_t esp_ota_abort(esp_ota_handle_t h) {aborts++;assert(h==7);return 0;}
esp_err_t esp_ota_mark_app_valid_cancel_rollback(void) {accepted++;return fail==7?-1:0;}
esp_err_t esp_ota_mark_app_invalid_rollback_and_reboot(void) {rolled_back++;return -1;}
static bool commit(void *p) {(void)p;return allow;}
static void reset(void) {
    const esp_partition_t init[]={
        {1,2,0x9000,0x6000,"nvs",false},{1,1,0xf000,0x1000,"phy_init",false},
        {0,0,0x10000,0x200000,"factory",false},{1,0,0x210000,0x2000,"otadata",false},
        {0,0x10,0x220000,0x300000,"ota_0",false},{0,0x11,0x520000,0x300000,"ota_1",false},
        {1,0x40,0x820000,0x300000,"resources",false},{1,0x41,0xb20000,0x4e0000,"store",false}};
    memcpy(parts,init,sizeof(parts));running=boot=&parts[2];next=&parts[4];state=ESP_OTA_IMG_VALID;
    flash_size=physical_size=0x1000000;begins=writes=ends=selects=aborts=fail=accepted=rolled_back=0;allow=true;
}
static tv4_ota_candidate candidate(void) {
    tv4_ota_candidate c={0};strcpy(c.version,"0.2.0");strcpy(c.release,"0.2.0");strcpy(c.url,"https://example.test/fixture-only.bin");c.size_bytes=480;c.installable=true;memset(c.sha256,0x42,32);return c;
}
static tv4_install_approval approval(const tv4_ota_candidate *c) {
    tv4_install_approval a={0};strcpy(a.version,c->version);strcpy(a.release,c->release);strcpy(a.url,c->url);memcpy(a.sha256,c->sha256,32);a.size=c->size_bytes;return a;
}
int main(void) {
    for(unsigned wrap=0;wrap<2;wrap++) {
        uint32_t base=wrap?UINT32_MAX-1500:0;
        tv4_ota_boot_window w={.started=base};
        assert(tv4_ota_boot_observe(&w,base,false,base)==TV4_BOOT_WAIT);
        for(uint32_t t=0;t<3000;t+=50)assert(tv4_ota_boot_observe(&w,base+t,true,base+t)==TV4_BOOT_WAIT);
        assert(tv4_ota_boot_observe(&w,base+3000,true,base+3000)==TV4_BOOT_CHECK_MEMORY);
        w=(tv4_ota_boot_window){.started=base};
        assert(tv4_ota_boot_observe(&w,base,true,base)==TV4_BOOT_WAIT);
        assert(tv4_ota_boot_observe(&w,base+3000,true,base)==TV4_BOOT_WAIT && !w.observing); /* stale single pulse */
        assert(tv4_ota_boot_observe(&w,base+3100,true,base+3100)==TV4_BOOT_WAIT);
        assert(tv4_ota_boot_observe(&w,base+6000,true,base+6000)==TV4_BOOT_WAIT);
        assert(tv4_ota_boot_observe(&w,base+6100,false,base+6100)==TV4_BOOT_WAIT); /* resets continuous window */
        assert(tv4_ota_boot_observe(&w,base+6200,true,base+6200)==TV4_BOOT_WAIT);
        for(uint32_t t=6250;t<9200;t+=50)assert(tv4_ota_boot_observe(&w,base+t,true,base+t)==TV4_BOOT_WAIT);
        assert(tv4_ota_boot_observe(&w,base+9200,true,base+9200)==TV4_BOOT_CHECK_MEMORY);
        assert(tv4_ota_boot_observe(&w,base+30000,true,base+30000)==TV4_BOOT_TIMEOUT);
    }
    tv4_ota_candidate c=candidate();tv4_install_approval a=approval(&c);uint8_t data[480]={0};
    for(unsigned from=0;from<3;from++) {
        reset();running=boot=&parts[from==0?2:from==1?4:5];next=&parts[from==1?5:4];
        tv4_ota_sdk s={.approval=&a,.commit_allowed=commit};assert(tv4_ota_sdk_begin(&s,&c));
        assert(!tv4_ota_sdk_activate(&s));assert(!tv4_ota_sdk_seal(&s));
        assert(tv4_ota_sdk_write(&s,data,17) && tv4_ota_sdk_write(&s,data+17,463));
        assert(!tv4_ota_sdk_write(&s,data,1));assert(tv4_ota_sdk_seal(&s));
        assert(!tv4_ota_sdk_write(&s,data,1));assert(tv4_ota_sdk_activate(&s));
        assert(boot==next && begins==1 && writes==2 && ends==1 && selects==1);
        tv4_ota_sdk_abort(&s);assert(!aborts); /* completed handle not aborted */
        s=(tv4_ota_sdk){.approval=&a,.commit_allowed=commit};assert(!tv4_ota_sdk_begin(&s,&c)); /* reboot required */
    }
    for(unsigned n=0;n<8;n++)for(unsigned kind=0;kind<4;kind++) {
        reset();tv4_ota_sdk s={.approval=&a,.commit_allowed=commit};
        if(kind==0)parts[n].address+=0x1000;
        if(kind==1)parts[n].size-=0x1000;
        if(kind==2)parts[n].encrypted=true;
        if(kind==3)parts[n].subtype=0xff;
        assert(!tv4_ota_sdk_layout_valid() && !tv4_ota_sdk_begin(&s,&c) && !begins && !selects);
    }
    for(unsigned n=0;n<15;n++) {
        reset();tv4_ota_sdk s={.approval=&a,.commit_allowed=commit};esp_partition_t unknown={0,0,0x900000,0x200000,"unknown",false};
        if(n==0)running=NULL;
        if(n==1)boot=NULL;
        if(n==2)next=NULL;
        if(n==3)next=running;
        if(n==4) {running=boot=&parts[4];next=&parts[2];}
        if(n==5) {running=boot=&parts[4];state=ESP_OTA_IMG_PENDING_VERIFY;}
        if(n==6) {running=boot=&parts[4];state=ESP_OTA_IMG_INVALID;}
        if(n==7)running=boot=&unknown;
        if(n==8)flash_size=0x800000;
        if(n==9)fail=6;
        if(n==10)physical_size=0x800000;
        if(n==11)fail=8;
        if(n==12) {running=boot=&parts[4];state=ESP_OTA_IMG_ABORTED;}
        if(n==13) {running=boot=&parts[4];state=ESP_OTA_IMG_NEW;}
        if(n==14) {running=boot=&parts[4];state=ESP_OTA_IMG_UNDEFINED;}
        assert(!tv4_ota_sdk_begin(&s,&c) && !begins);
    }
    for(unsigned n=0;n<6;n++) {
        reset();tv4_ota_candidate changed=c;
        if(n==0)changed.sha256[31]^=1;
        if(n==1)strcpy(changed.version,"0.3.0");
        if(n==2)strcpy(changed.release,"0.3.0");
        if(n==3)strcat(changed.url,"x");
        if(n==4)changed.size_bytes--;
        if(n==5)changed.installable=false;
        tv4_ota_sdk s={.approval=&a,.commit_allowed=commit};assert(!tv4_ota_sdk_begin(&s,&changed) && !begins);
    }
    for(unsigned phase=1;phase<=4;phase++) {
        reset();fail=phase;tv4_ota_sdk s={.approval=&a,.commit_allowed=commit};
        bool ok=tv4_ota_sdk_begin(&s,&c);
        if(ok)ok=tv4_ota_sdk_write(&s,data,sizeof(data));
        if(ok)ok=tv4_ota_sdk_seal(&s);
        if(ok)ok=tv4_ota_sdk_activate(&s);
        assert(!ok && boot==running);tv4_ota_sdk_abort(&s);tv4_ota_sdk_abort(&s);assert(aborts==(phase==2));
    }
    for(unsigned n=0;n<3;n++) {
        reset();tv4_ota_sdk s={.approval=&a,.commit_allowed=commit};
        assert(tv4_ota_sdk_begin(&s,&c) && tv4_ota_sdk_write(&s,data,sizeof(data)) && tv4_ota_sdk_seal(&s));
        if(n==0)allow=false;
        if(n==1)next=&parts[5];
        if(n==2)boot=&parts[5];
        assert(!tv4_ota_sdk_activate(&s) && !selects);tv4_ota_sdk_abort(&s);
    }
    reset();assert(tv4_ota_sdk_finish_boot(false) && !accepted && !rolled_back); /* factory is retained */
    for(unsigned n=0;n<6;n++) {
        reset();running=boot=&parts[4];state=ESP_OTA_IMG_PENDING_VERIFY;
        if(n==1)fail=7; /* mark valid failure -> rollback */
        if(n==2)parts[3].size=0;
        if(n==3)state=ESP_OTA_IMG_INVALID;
        if(n==4)fail=5; /* unreadable OTA state */
        bool result=tv4_ota_sdk_finish_boot(n!=5);
        assert(result==(n==0));assert(accepted==((n==0 || n==1)?1u:0u));
        assert(rolled_back==((n==1 || n==2 || n==5)?1u:0u));
    }
    /* An already rejected/unconfirmed record is never promoted by finish_boot.
     * Native bootloader rollback is qualified separately on the real board. */
    const esp_ota_img_states_t rejected[]={ESP_OTA_IMG_NEW,ESP_OTA_IMG_INVALID,
        ESP_OTA_IMG_ABORTED,ESP_OTA_IMG_UNDEFINED};
    for(unsigned n=0;n<sizeof(rejected)/sizeof(rejected[0]);n++)for(unsigned healthy=0;healthy<2;healthy++) {
        reset();running=boot=&parts[4];state=rejected[n];
        assert(!tv4_ota_sdk_finish_boot(healthy!=0) && !accepted && !rolled_back);
    }
    reset();running=boot=&parts[5];
    assert(tv4_ota_sdk_finish_boot(true) && !accepted && !rolled_back);
    puts("Actual SDK installer: factory/alternating slots, layout/approval/state gates, SDK failures, cancellation, boot acceptance/rollback and no active/factory writes PASS (mock SDK, no hardware)");
}
