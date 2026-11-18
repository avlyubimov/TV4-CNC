#include "manifest.h"
#include "json_guard.h"
#include <stdio.h>
#include <string.h>
#include <limits.h>

typedef struct { uint32_t number[3];const char *pre; } version_t;
static bool length_ok(const char *s,size_t max) {
    if(!s)return false;
    for(size_t i=0;i<=max;i++)if(!s[i])return true;
    return false;
}
static bool digit(char c) { return c>='0' && c<='9'; }
static bool ident(char c) { return digit(c) || (c>='a' && c<='z') || (c>='A' && c<='Z') || c=='-'; }
static bool parse_version(const char *s,version_t *v) {
    if(!length_ok(s,64) || !*s)return false;
    for(unsigned i=0;i<3;i++) {
        const char *start=s;uint32_t number=0;
        while(digit(*s)) {
            unsigned d=(unsigned)(*s++-'0');
            if(number>(UINT32_MAX-d)/10)return false;
            number=number*10+d;
        }
        if(s==start || (s-start>1 && *start=='0'))return false;
        v->number[i]=number;
        if(i<2 && *s++!='.')return false;
    }
    v->pre=NULL;if(!*s)return true;
    if(*s++!='-' || !*s)return false;
    v->pre=s;
    for(;;) {
        const char *start=s;bool numeric=true;
        while(ident(*s)) { if(!digit(*s))numeric=false;++s; }
        if(s==start || (numeric && s-start>1 && *start=='0'))return false;
        if(!*s)return true;
        if(*s++!='.' || !*s)return false;
    }
}
static int compare_pre(const char *a,const char *b) {
    if(!a || !b)return !a?(!b?0:1):-1;
    for(;;) {
        const char *ae=a,*be=b;bool an=true,bn=true;
        while(*ae && *ae!='.') {if(!digit(*ae))an=false;++ae;}
        while(*be && *be!='.') {if(!digit(*be))bn=false;++be;}
        size_t al=(size_t)(ae-a),bl=(size_t)(be-b);
        if(an!=bn)return an?-1:1;
        if(an && al!=bl)return al<bl?-1:1;
        int c=memcmp(a,b,al<bl?al:bl);
        if(c)return c<0?-1:1;
        if(al!=bl)return al<bl?-1:1;
        if(!*ae || !*be)return !*ae?(!*be?0:-1):1;
        a=ae+1;b=be+1;
    }
}
bool tv4_ota_version_compare(const char *a,const char *b,int *comparison) {
    version_t av,bv;
    if(!comparison || !parse_version(a,&av) || !parse_version(b,&bv))return false;
    for(unsigned i=0;i<3;i++)if(av.number[i]!=bv.number[i]) {
        *comparison=av.number[i]<bv.number[i]?-1:1;return true;
    }
    *comparison=compare_pre(av.pre,bv.pre);return true;
}
static bool at_least(const char *current,const char *minimum) {
    int c;return tv4_ota_version_compare(current,minimum,&c) && c>=0;
}
static bool range(const char *current,const char *min,const char *max) {
    int c;return at_least(current,min) && tv4_ota_version_compare(current,max,&c) && c<=0;
}
static bool shape(const cJSON *obj,const char *const *required,size_t count,const char *optional) {
    if(!cJSON_IsObject(obj))return false;
    size_t found=0;
    for(const cJSON *p=obj->child;p;p=p->next) {
        bool known=false;
        for(size_t i=0;i<count;i++)if(!strcmp(p->string,required[i])) {known=true;++found;break;}
        if(!known && (!optional || strcmp(p->string,optional)))return false;
    }
    return found==count;
}
static const char *text(const cJSON *obj,const char *key,size_t limit) {
    const cJSON *p=cJSON_GetObjectItemCaseSensitive(obj,key);
    return cJSON_IsString(p) && length_ok(p->valuestring,limit)?p->valuestring:NULL;
}
static bool equal(const char *a,const char *b) {return a && b && !strcmp(a,b);}
static bool hex(const char *s,size_t n,uint8_t *out) {
    if(!s || strlen(s)!=n)return false;
    unsigned value=0;
    for(size_t i=0;i<n;i++) {
        unsigned d;if(s[i]>='0' && s[i]<='9')d=(unsigned)(s[i]-'0');
        else if(s[i]>='a' && s[i]<='f')d=(unsigned)(s[i]-'a'+10);else return false;
        value=(value<<4)|d;if(out && i%2)out[i/2]=(uint8_t)value;
    }
    return true;
}
static tv4_ota_result policy(const cJSON *root,const tv4_ota_context *ctx,tv4_ota_candidate *out) {
    static const char *const fields[]={"schema_version","release_version","channel","source_commit","signing","protocol","components","release_notes"};
    static const char *const signing_fields[]={"key_id","algorithm"};
    static const char *const range_fields[]={"min","max"};
    static const char *const component_fields[]={"name","version","hardware_id","hardware_revision","chip","profile","layout_id","minimum_installed_version","minimum_bootloader_version","size_bytes","sha256","url"};
    static const char *const peer_fields[]={"motion_min","motion_max"};
    if(!shape(root,fields,8,NULL))return TV4_OTA_BAD_SCHEMA;
    const cJSON *schema=cJSON_GetObjectItemCaseSensitive(root,"schema_version");
    if(!cJSON_IsNumber(schema) || schema->valuedouble!=1)return TV4_OTA_BAD_SCHEMA;
    const cJSON *signing=cJSON_GetObjectItemCaseSensitive(root,"signing");
    if(!shape(signing,signing_fields,2,NULL) || !equal(text(signing,"key_id",64),ctx->key_id) ||
       !equal(text(signing,"algorithm",32),"ECDSA-P256-SHA256"))return TV4_OTA_NO_TRUST;
    const char *release=text(root,"release_version",64),*notes=text(root,"release_notes",4096);
    version_t release_version;
    if(!parse_version(release,&release_version) || !notes || !hex(text(root,"source_commit",40),40,NULL))return TV4_OTA_BAD_SCHEMA;
    if(!equal(text(root,"channel",10),ctx->prerelease?"prerelease":"stable") ||
       ((release_version.pre!=NULL)!=ctx->prerelease))return TV4_OTA_WRONG_VERSION;
    const cJSON *protocol=cJSON_GetObjectItemCaseSensitive(root,"protocol");
    if(!shape(protocol,range_fields,2,NULL) || !range(ctx->protocol_version,text(protocol,"min",64),text(protocol,"max",64)))return TV4_OTA_WRONG_PROTOCOL;
    const cJSON *components=cJSON_GetObjectItemCaseSensitive(root,"components");
    if(!cJSON_IsArray(components) || cJSON_GetArraySize(components)!=1)return TV4_OTA_BAD_SCHEMA;
    const cJSON *app=components->child;
    if(!shape(app,component_fields,sizeof(component_fields)/sizeof(component_fields[0]),"compatible_peer_versions"))return TV4_OTA_BAD_SCHEMA;
    if(!ctx->board_rev12 || !equal(text(app,"name",8),"hmi") || !equal(text(app,"hardware_id",64),"waveshare-esp32s3-touch-lcd-7") ||
       !equal(text(app,"hardware_revision",8),"1.2") || !equal(text(app,"chip",16),"esp32s3") ||
       !equal(text(app,"profile",16),"rev12"))return TV4_OTA_WRONG_TARGET;
    if(!equal(ctx->layout_id,"tv4-16m-ab-v1") || !equal(text(app,"layout_id",32),ctx->layout_id))return TV4_OTA_WRONG_LAYOUT;
    const char *version=text(app,"version",31);version_t app_version;int c;
    if(!parse_version(version,&app_version) || (!ctx->prerelease && app_version.pre) ||
       !at_least(ctx->installed_version,text(app,"minimum_installed_version",64)) ||
       !at_least(ctx->bootloader_version,text(app,"minimum_bootloader_version",64)) ||
       !tv4_ota_version_compare(version,ctx->installed_version,&c) || c<=0)return TV4_OTA_WRONG_VERSION;
    const cJSON *peer=cJSON_GetObjectItemCaseSensitive(app,"compatible_peer_versions");
    if(peer && (!shape(peer,peer_fields,2,NULL) || !range(ctx->motion_version,text(peer,"motion_min",64),text(peer,"motion_max",64))))return TV4_OTA_WRONG_PEER;
    const cJSON *size=cJSON_GetObjectItemCaseSensitive(app,"size_bytes");
    if(!cJSON_IsNumber(size) || size->valuedouble<1 || size->valuedouble>0x300000)return TV4_OTA_BAD_SIZE;
    const char *sha=text(app,"sha256",64),*url=text(app,"url",512);
    if(!hex(sha,64,NULL))return TV4_OTA_BAD_SCHEMA;
    char expected[513];int n=snprintf(expected,sizeof(expected),"https://github.com/avlyubimov/TV4-CNC/releases/download/v%s/tv4-hmi-esp32s3-v%s.bin",release,version);
    if(n<0 || (size_t)n>=sizeof(expected) || !equal(url,expected))return TV4_OTA_BAD_URL;
    /* Only commit validated output after every check succeeds. */
    strcpy(out->release,release);strcpy(out->version,version);strcpy(out->url,url);strcpy(out->notes,notes);
    out->size_bytes=(uint32_t)size->valuedouble;(void)hex(sha,64,out->sha256);return TV4_OTA_OK;
}
tv4_ota_result tv4_ota_manifest_read(const uint8_t *raw,size_t length,
    const uint8_t *signature,size_t signature_length,const tv4_ota_context *ctx,tv4_ota_candidate *out) {
    if(!out)return TV4_OTA_BAD_SCHEMA;
    memset(out,0,sizeof(*out));
    if(!ctx || !length_ok(ctx->key_id,64) || !*ctx->key_id ||
       !tv4_ota_p256_key(ctx->public_key,ctx->public_key_length))return TV4_OTA_NO_TRUST;
    if(!tv4_ota_verify_manifest(raw,length,signature,signature_length,ctx->public_key,ctx->public_key_length))return TV4_OTA_BAD_SIGNATURE;
    cJSON *root=tv4_ota_json_parse(raw,length,TV4_OTA_MANIFEST_MAX);
    if(!root)return TV4_OTA_BAD_JSON;
    tv4_ota_result result=policy(root,ctx,out);
    cJSON_Delete(root);return result;
}
