/* Host-only adapter for independent Python/C policy conformance tests.
 * Private key exists only in this process; no signing API is linked into HMI.
 * JSON lines are a test harness protocol, not CAN/release/firmware metadata. */
#include "envelope.h"
#include "json_guard.h"
#include <assert.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char line[65536];
static uint8_t raw[TV4_OTA_MANIFEST_MAX+1],envelope[TV4_OTA_ENVELOPE_MAX+1];
static EVP_PKEY *key;
static const char *text(const cJSON *obj,const char *name) {
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(obj,name);
    assert(!v || cJSON_IsNull(v) || cJSON_IsString(v));
    return cJSON_IsString(v)?v->valuestring:NULL;
}
static bool flag(const cJSON *obj,const char *name) {
    const cJSON *v=cJSON_GetObjectItemCaseSensitive(obj,name);
    assert(cJSON_IsBool(v));return cJSON_IsTrue(v);
}
static int digit(char c) {
    if(c>='0' && c<='9')return c-'0';
    if(c>='a' && c<='f')return c-'a'+10;
    return -1;
}
static size_t unhex(const char *s,uint8_t *out,size_t capacity) {
    assert(s);size_t n=strlen(s);assert(n%2==0 && n/2<=capacity);
    for(size_t i=0;i<n/2;i++) {
        int a=digit(s[2*i]),b=digit(s[2*i+1]);assert(a>=0 && b>=0);
        out[i]=(uint8_t)((a<<4)|b);
    }
    return n/2;
}
static void add_hex(cJSON *obj,const char *name,const uint8_t *s,size_t n) {
    static const char digits[]="0123456789abcdef";
    char *hex=malloc(n*2+1);assert(hex);
    for(size_t i=0;i<n;i++) {hex[2*i]=digits[s[i]>>4];hex[2*i+1]=digits[s[i]&15];}
    hex[2*n]=0;assert(cJSON_AddStringToObject(obj,name,hex));free(hex);
}
static void emit(cJSON *obj) {
    char *s=cJSON_PrintUnformatted(obj);assert(s);puts(s);fflush(stdout);
    cJSON_free(s);cJSON_Delete(obj);
}
static void compare(const cJSON *request,cJSON *reply) {
    int order=777;
    bool ok=tv4_ota_version_compare(text(request,"a"),text(request,"b"),&order);
    assert(cJSON_AddBoolToObject(reply,"ok",ok));
    if(ok)assert(cJSON_AddNumberToObject(reply,"order",order));
}
static void sign_raw(const cJSON *request,cJSON *reply) {
    size_t length=unhex(text(request,"raw"),raw,sizeof(raw));
    uint8_t signature[80],encoded[112];size_t n=sizeof(signature);
    EVP_MD_CTX *ctx=EVP_MD_CTX_new();assert(ctx);
    assert(EVP_DigestSignInit(ctx,NULL,EVP_sha256(),NULL,key)==1);
    assert(EVP_DigestSign(ctx,signature,&n,raw,length)==1);
    EVP_MD_CTX_free(ctx);
    assert(EVP_EncodeBlock(encoded,signature,(int)n)>0);
    assert(cJSON_AddStringToObject(reply,"signature",(const char *)encoded));
}
static void verify(const cJSON *request,cJSON *reply,const uint8_t *pub,size_t pub_n) {
    size_t n=unhex(text(request,"raw"),raw,sizeof(raw));
    size_t en=unhex(text(request,"envelope"),envelope,sizeof(envelope));
    uint8_t other_pub[128];const char *override=text(request,"public_key");
    if(override) {pub_n=unhex(override,other_pub,sizeof(other_pub));pub=other_pub;}
    const cJSON *c=cJSON_GetObjectItemCaseSensitive(request,"context");
    tv4_ota_context ctx={0};
    if(cJSON_IsObject(c)) {
        ctx=(tv4_ota_context){.key_id=text(c,"key_id"),
            .installed_version=text(c,"installed_version"),
            .bootloader_version=text(c,"bootloader_version"),
            .protocol_version=text(c,"protocol_version"),
            .motion_version=text(c,"motion_version"),.layout_id=text(c,"layout_id"),
            .prerelease=flag(c,"prerelease"),.board_rev12=flag(c,"board_rev12"),
            .public_key=pub,.public_key_length=pub_n};
    } else assert(cJSON_IsNull(c));
    tv4_ota_candidate out;memset(&out,0xa5,sizeof(out));
    tv4_ota_result result=tv4_ota_signed_manifest_read(raw,n,envelope,en,
        cJSON_IsObject(c)?&ctx:NULL,&out);
    assert(cJSON_AddNumberToObject(reply,"result",result));
    if(result==TV4_OTA_OK) {
        assert(cJSON_AddStringToObject(reply,"release",out.release));
        assert(cJSON_AddStringToObject(reply,"version",out.version));
        assert(cJSON_AddStringToObject(reply,"url",out.url));
        assert(cJSON_AddStringToObject(reply,"notes",out.notes));
        assert(cJSON_AddNumberToObject(reply,"size_bytes",out.size_bytes));
        assert(cJSON_AddBoolToObject(reply,"installable",out.installable));
        add_hex(reply,"sha256",out.sha256,sizeof(out.sha256));
    } else {
        const uint8_t *bytes=(const uint8_t *)&out;
        for(size_t i=0;i<sizeof(out);i++)assert(bytes[i]==0);
        assert(cJSON_AddBoolToObject(reply,"output_zero",true));
    }
}
int main(void) {
    key=EVP_PKEY_Q_keygen(NULL,NULL,"EC","prime256v1");assert(key);
    uint8_t pub[91],*cursor=pub;int pub_n=i2d_PUBKEY(key,&cursor);
    assert(pub_n==91);
    cJSON *hello=cJSON_CreateObject();assert(hello);
    add_hex(hello,"public_key",pub,sizeof(pub));emit(hello);
    while(fgets(line,sizeof(line),stdin)) {
        size_t length=strlen(line);assert(length && line[length-1]=='\n');
        cJSON *request=cJSON_Parse(line),*reply=cJSON_CreateObject();
        assert(request && cJSON_IsObject(request) && reply);
        const char *op=text(request,"op");assert(op);
        if(!strcmp(op,"compare"))compare(request,reply);
        else if(!strcmp(op,"sign"))sign_raw(request,reply);
        else {assert(!strcmp(op,"verify"));verify(request,reply,pub,(size_t)pub_n);}
        emit(reply);cJSON_Delete(request);
    }
    assert(!ferror(stdin));EVP_PKEY_free(key);
}
