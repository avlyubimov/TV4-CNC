#include "wifi_profiles.h"
#include "wifi_service.h"
#include <string.h>
void wifi_profiles_init(wifi_profiles_t *p) { memset(p,0,sizeof(*p));p->version=1; }
int wifi_profiles_find(const wifi_profiles_t *p,const char *ssid) {
    if(!ssid)return -1;
    for(unsigned i=0;i<p->count && i<WIFI_SAVED_MAX;i++)if(!strcmp(p->entries[i].ssid,ssid))return (int)i;
    return -1;
}
bool wifi_profiles_valid(const wifi_profiles_t *p) {
    if(!p || p->version!=1 || p->count>WIFI_SAVED_MAX)return false;
    for(unsigned i=0;i<p->count;i++) {
        const wifi_profile_t *e=&p->entries[i];
        if(!memchr(e->ssid,0,sizeof(e->ssid)) || !memchr(e->password,0,sizeof(e->password)) ||
           !wifi_credentials_valid(e->ssid,e->password))return false;
        for(unsigned j=0;j<i;j++)if(!strcmp(e->ssid,p->entries[j].ssid))return false;
    }
    return true;
}
bool wifi_profiles_remember(wifi_profiles_t *p,const char *ssid,const char *password) {
    if(!wifi_profiles_valid(p) || !wifi_credentials_valid(ssid,password))return false;
    wifi_profile_t entry={0};memcpy(entry.ssid,ssid,strlen(ssid));memcpy(entry.password,password,strlen(password));
    int found=wifi_profiles_find(p,ssid);
    unsigned shift=found>=0?(unsigned)found:p->count<WIFI_SAVED_MAX?p->count:WIFI_SAVED_MAX-1;
    memmove(&p->entries[1],&p->entries[0],shift*sizeof(entry));p->entries[0]=entry;
    if(found<0 && p->count<WIFI_SAVED_MAX)p->count++;
    return true;
}
bool wifi_profiles_forget(wifi_profiles_t *p,const char *ssid) {
    if(!wifi_profiles_valid(p))return false;
    int found=wifi_profiles_find(p,ssid);if(found<0)return false;
    memmove(&p->entries[found],&p->entries[found+1],(p->count-(unsigned)found-1)*sizeof(wifi_profile_t));
    memset(&p->entries[--p->count],0,sizeof(wifi_profile_t));return true;
}

int wifi_profiles_choose(const wifi_profiles_t *p,const char visible[][33],unsigned count) {
    if(!wifi_profiles_valid(p) || !visible)return -1;
    for(unsigned i=0;i<count;i++) {
        if(!memchr(visible[i],0,33))continue;
        int found=wifi_profiles_find(p,visible[i]);if(found>=0)return found;
    }
    return -1;
}
