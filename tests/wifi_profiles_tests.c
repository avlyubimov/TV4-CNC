#include "wifi_profiles.h"
#include "wifi_service.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    wifi_profiles_t p;wifi_profiles_init(&p);assert(wifi_profiles_valid(&p));
    assert(!wifi_profiles_remember(&p,"","12345678"));
    assert(!wifi_profiles_remember(&p,"test","short"));
    assert(wifi_profiles_remember(&p,"TEST_HOME","correctpass"));
    assert(wifi_profiles_remember(&p,"TEST_OPEN",""));
    assert(p.count==2 && wifi_profiles_find(&p,"TEST_HOME")==1);
    assert(wifi_profiles_remember(&p,"TEST_HOME","updatedpass"));
    assert(p.count==2 && !strcmp(p.entries[0].password,"updatedpass"));
    /* Same first profile does not produce a changed persistence image. */
    wifi_profiles_t before=p;assert(wifi_profiles_remember(&p,"TEST_HOME","updatedpass"));
    assert(!memcmp(&before,&p,sizeof(p)));
    const char visible[][33]={"TEST_UNKNOWN","TEST_OPEN","TEST_HOME"};
    assert(wifi_profiles_choose(&p,visible,3)==wifi_profiles_find(&p,"TEST_OPEN"));
    assert(wifi_profiles_choose(&p,visible,1)==-1);
    assert(wifi_profiles_choose(&p,NULL,0)==-1);
    unsigned char blob[sizeof(p)];memcpy(blob,&p,sizeof(p));wifi_profiles_init(&p);
    memcpy(&p,blob,sizeof(p));assert(wifi_profiles_valid(&p) && p.count==2);
    assert(!strcmp(p.entries[0].ssid,"TEST_HOME") && !strcmp(p.entries[0].password,"updatedpass"));
    assert(wifi_profiles_forget(&p,"TEST_HOME"));assert(wifi_profiles_find(&p,"TEST_HOME")<0);
    assert(!wifi_profiles_forget(&p,"missing") && p.count==1);
    wifi_profile_t zero={0};assert(!memcmp(&p.entries[p.count],&zero,sizeof(zero)));
    for(unsigned i=0;i<12;i++) {
        char name[16];snprintf(name,sizeof(name),"TEST_%u",i);
        assert(wifi_profiles_remember(&p,name,"12345678"));
    }
    assert(p.count==WIFI_SAVED_MAX && !strcmp(p.entries[0].ssid,"TEST_11"));
    assert(wifi_profiles_find(&p,"TEST_0")<0 && wifi_profiles_find(&p,"TEST_4")>=0);
    before=p;p.version=999;assert(!wifi_profiles_valid(&p));p=before;
    p.count=WIFI_SAVED_MAX+1;assert(!wifi_profiles_valid(&p));p=before;
    memset(p.entries[0].ssid,'a',sizeof(p.entries[0].ssid));assert(!wifi_profiles_valid(&p));p=before;
    memset(p.entries[0].password,'a',sizeof(p.entries[0].password));assert(!wifi_profiles_valid(&p));p=before;
    p.entries[1]=p.entries[0];assert(!wifi_profiles_valid(&p));
    puts("Wi-Fi profile validation, persistence image, updates, eviction and forgetting passed");
}
