#include "manifest.h"
#include "envelope.h"
#include "tv4_namespace.h"
#include "cJSON.h"
#include <assert.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static EVP_PKEY *key;
static unsigned char *public_key;
static int key_length;
static tv4_ota_context context;
static tv4_ota_candidate output;
static const char valid[]=
"{\"schema_version\":1,\"release_version\":\"0.2.0\",\"channel\":\"stable\","
"\"source_commit\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
"\"signing\":{\"key_id\":\"test-only\",\"algorithm\":\"ECDSA-P256-SHA256\"},"
"\"protocol\":{\"min\":\"1.0.0\",\"max\":\"1.0.0\"},"
"\"components\":[{\"name\":\"hmi\",\"version\":\"0.2.0\","
"\"hardware_id\":\"waveshare-esp32s3-touch-lcd-7\",\"hardware_revision\":\"1.2\","
"\"chip\":\"esp32s3\",\"profile\":\"rev12\",\"layout_id\":\"tv4-16m-ab-v1\","
"\"minimum_installed_version\":\"0.1.0\",\"minimum_bootloader_version\":\"0.1.0\","
"\"size_bytes\":512,\"sha256\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\","
"\"url\":\"https://github.com/avlyubimov/TV4-CNC/releases/download/v0.2.0/tv4-hmi-esp32s3-v0.2.0.bin\"}],"
"\"release_notes\":\"Проверка\\nJSON\"}";
static size_t signature(const char *raw,size_t length,unsigned char *sig) {
    EVP_MD_CTX *ctx=EVP_MD_CTX_new();assert(ctx);size_t n=80;
    assert(EVP_DigestSignInit(ctx,NULL,EVP_sha256(),NULL,key)==1);
    assert(EVP_DigestSign(ctx,sig,&n,(const unsigned char *)raw,length)==1);
    EVP_MD_CTX_free(ctx);return n;
}
static tv4_ota_result read(const char *raw,size_t length) {
    unsigned char sig[80];size_t n=signature(raw,length,sig);
    return tv4_ota_manifest_read((const uint8_t *)raw,length,sig,n,&context,&output);
}
static void reject(const char *raw,tv4_ota_result expected) {
    memset(&output,0xaa,sizeof(output));assert(read(raw,strlen(raw))==expected);
    const unsigned char *p=(const unsigned char *)&output;
    for(size_t i=0;i<sizeof(output);i++)assert(p[i]==0);
}
static void replace(const char *needle,const char *value,tv4_ota_result expected) {
    char raw[4096];const char *at=strstr(valid,needle);assert(at);
    int n=snprintf(raw,sizeof(raw),"%.*s%s%s",(int)(at-valid),valid,value,at+strlen(needle));
    assert(n>0 && (size_t)n<sizeof(raw));reject(raw,expected);
}
static void envelope_checks(void) {
    unsigned char der[80],base64[112];size_t count=signature(valid,strlen(valid),der);
    int encoded=EVP_EncodeBlock(base64,der,(int)count);assert(encoded>0 && encoded<=96);
    char envelope[1024];
    int n=snprintf(envelope,sizeof(envelope),"{\"key_id\":\"test-only\",\"algorithm\":\"ECDSA-P256-SHA256\",\"signature\":\"%s\"}",base64);
    assert(n>0 && (size_t)n<sizeof(envelope));
    assert(tv4_ota_signed_manifest_read((const uint8_t *)valid,strlen(valid),(const uint8_t *)envelope,(size_t)n,&context,&output)==TV4_OTA_OK);
    assert(tv4_ota_signed_manifest_read((const uint8_t *)valid,strlen(valid)-1,(const uint8_t *)envelope,(size_t)n,&context,&output)==TV4_OTA_BAD_SIGNATURE);
    const char *bad[]={"{}","{\"key_id\":\"test-only\",\"algorithm\":\"none\",\"signature\":\"AAAAAAAAAAA=\"}",
        "{\"key_id\":\"test-only\",\"algorithm\":\"ECDSA-P256-SHA256\",\"signature\":\"AAAAAAAAAAB=\"}",
        "{\"key_id\":\"test-only\",\"algorithm\":\"ECDSA-P256-SHA256\",\"signature\":\"AAAAAAAAAA==\"}",
        "{\"key_id\":\"test-only\",\"key_id\":\"test-only\",\"algorithm\":\"ECDSA-P256-SHA256\",\"signature\":\"AAAAAAAAAAA=\"}",
        "{\"key_id\":\"test-only\",\"algorithm\":\"ECDSA-P256-SHA256\",\"signature\":\"AAAAAAAAAAA=\",\"public_key\":\"downloaded\"}"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {
        assert(tv4_ota_signed_manifest_read((const uint8_t *)valid,strlen(valid),(const uint8_t *)bad[i],strlen(bad[i]),&context,&output)==TV4_OTA_BAD_ENVELOPE);
        assert(!output.size_bytes && !output.url[0]);
    }
    char large[2049];memset(large,' ',sizeof(large));
    assert(tv4_ota_signed_manifest_read((const uint8_t *)valid,strlen(valid),(const uint8_t *)large,sizeof(large),&context,&output)==TV4_OTA_BAD_ENVELOPE);
    const char *saved=context.key_id;context.key_id="unknown";
    assert(tv4_ota_signed_manifest_read((const uint8_t *)valid,strlen(valid),(const uint8_t *)envelope,(size_t)n,&context,&output)==TV4_OTA_NO_TRUST);
    context.key_id=saved;
}
static size_t allocations,fail_at,outstanding,peak;
typedef union { max_align_t alignment;size_t size; } allocation_header;
static void *limited_malloc(size_t size) {
    if(++allocations==fail_at)return NULL;
    allocation_header *p=malloc(sizeof(*p)+size);if(!p)return NULL;
    p->size=size;outstanding+=size;if(outstanding>peak)peak=outstanding;
    return p+1;
}
static void limited_free(void *pointer) {
    if(!pointer)return;
    allocation_header *p=(allocation_header *)pointer-1;
    outstanding-=p->size;free(p);
}
int main(void) {
    key=EVP_PKEY_Q_keygen(NULL,NULL,"EC","prime256v1");assert(key);
    key_length=i2d_PUBKEY(key,&public_key);assert(key_length==91);
    context=(tv4_ota_context){.key_id="test-only",.installed_version="0.1.0",
        .bootloader_version="0.1.0",.protocol_version="1.0.0",.layout_id="tv4-16m-ab-v1",
        .board_rev12=true,.public_key=public_key,.public_key_length=(size_t)key_length};
    assert(read(valid,strlen(valid))==TV4_OTA_OK);
    assert(!output.installable && output.size_bytes==512 && output.sha256[0]==0xaa && strstr(output.notes,"Проверка\n"));

    for(unsigned allowed=0;allowed<2;allowed++) {
        char raw[4096];const char *at=strstr(valid,"\"size_bytes\"");assert(at);
        int count=snprintf(raw,sizeof(raw),"%.*s\"installable\":%s,%s",(int)(at-valid),valid,allowed?"true":"false",at);
        assert(count>0 && (size_t)count<sizeof(raw) && read(raw,(size_t)count)==TV4_OTA_OK);
        assert(output.installable==(bool)allowed);
    }
    replace("\"size_bytes\":512","\"installable\":1,\"size_bytes\":512",TV4_OTA_BAD_SCHEMA);
    replace("\"size_bytes\":512","\"installable\":\"true\",\"size_bytes\":512",TV4_OTA_BAD_SCHEMA);
    replace("\"size_bytes\":512","\"installable\":true,\"installable\":false,\"size_bytes\":512",TV4_OTA_BAD_JSON);
    envelope_checks();
    unsigned char sig[80];size_t n=signature(valid,strlen(valid),sig);sig[n-1]^=1;
    assert(tv4_ota_manifest_read((const uint8_t *)valid,strlen(valid),sig,n,&context,&output)==TV4_OTA_BAD_SIGNATURE);
    replace("\"size_bytes\":512","\"size_bytes\":1.0",TV4_OTA_BAD_JSON);
    replace("\"size_bytes\":512","\"size_bytes\":true",TV4_OTA_BAD_SIZE);
    replace("\"size_bytes\":512","\"size_bytes\":01",TV4_OTA_BAD_JSON);
    replace("\"size_bytes\":512","\"size_bytes\":3145729",TV4_OTA_BAD_SIZE);
    replace("\"schema_version\":1","\"schema_version\":1,\"schema_version\":1",TV4_OTA_BAD_JSON);
    replace("\"schema_version\":1","\"schema_version\":1,\"schema_versi\\u006fn\":1",TV4_OTA_BAD_JSON);
    replace("\"chip\":\"esp32s3\"","\"chip\":\"esp32s3\\u0000evil\"",TV4_OTA_BAD_JSON);
    replace("\"chip\":\"esp32s3\"","\"chip\":\"esp32s3\\u00g0evil\"",TV4_OTA_BAD_JSON);
    replace("Проверка\\nJSON","before\\uX123after",TV4_OTA_BAD_JSON);
    replace("\"chip\":\"esp32s3\"","\"chip\":\"esp32\"",TV4_OTA_WRONG_TARGET);
    replace("\"profile\":\"rev12\"","\"profile\":\"headless\"",TV4_OTA_WRONG_TARGET);
    replace("\"size_bytes\":512","\"unknown\":1,\"size_bytes\":512",TV4_OTA_BAD_SCHEMA);
    replace("releases/download/v0.2.0/","releases/download/v0.1.0/",TV4_OTA_BAD_URL);
    replace("\"key_id\":\"test-only\"","\"key_id\":\"downloaded\"",TV4_OTA_NO_TRUST);
    context.board_rev12=false;reject(valid,TV4_OTA_WRONG_TARGET);context.board_rev12=true;
    replace("\"version\":\"0.2.0\"","\"version\":\"1.2.3-abcdefghijklmnopqrstuvwxyzabcdef\"",TV4_OTA_WRONG_VERSION);
    context.layout_id="factory";reject(valid,TV4_OTA_WRONG_LAYOUT);context.layout_id="tv4-16m-ab-v1";
    context.installed_version="0.2.0";reject(valid,TV4_OTA_WRONG_VERSION);context.installed_version="0.1.0";
    context.bootloader_version=NULL;reject(valid,TV4_OTA_WRONG_VERSION);context.bootloader_version="0.1.0";
    context.protocol_version="2.0.0";reject(valid,TV4_OTA_WRONG_PROTOCOL);context.protocol_version="1.0.0";
    replace("\"size_bytes\":512","\"compatible_peer_versions\":{\"motion_min\":\"1.0.0\",\"motion_max\":\"2.0.0\"},\"size_bytes\":512",TV4_OTA_WRONG_PEER);
    reject("{} trailing",TV4_OTA_BAD_JSON);
    reject("{\"a\":\"\xc0\x80\"}",TV4_OTA_BAD_JSON);
    reject("{\"a\":\"\\ud800\"}",TV4_OTA_BAD_JSON);
    reject("{\"a\":[[[[[[[[[0]]]]]]]]]}",TV4_OTA_BAD_JSON);
    char many[1024]="[";for(unsigned i=0;i<150;i++)strcat(many,"0,");strcat(many,"0]");reject(many,TV4_OTA_BAD_JSON);
    int c;
    assert(tv4_ota_version_compare("1.0.0-alpha","1.0.0-alpha.1",&c) && c<0);
    assert(tv4_ota_version_compare("1.0.0-2","1.0.0-11",&c) && c<0);
    assert(tv4_ota_version_compare("1.0.0-rc.1","1.0.0",&c) && c<0);
    assert(tv4_ota_version_compare("1.9.0","1.10.0",&c) && c<0);
    assert(!tv4_ota_version_compare("1.0.0-01","1.0.0",&c));
    assert(!tv4_ota_version_compare("4294967296.0.0","1.0.0",&c));
    assert(!tv4_ota_version_compare("1.0.0+meta","1.0.0",&c));
    /* Signed malformed-input sweep: encoding/grammar/policy must stay bounded.
     * Some changes in notes/commit can legitimately remain valid. */
    const unsigned char replacements[]={0,0x22,0x5c,0x80};
    for(size_t i=0;i<strlen(valid);i+=3)for(size_t j=0;j<sizeof(replacements);j++) {
        char mutated[sizeof(valid)];memcpy(mutated,valid,sizeof(valid));mutated[i]=(char)replacements[j];
        tv4_ota_result result=read(mutated,strlen(valid));
        assert(result>=TV4_OTA_OK && result<=TV4_OTA_BAD_URL);
        if(result!=TV4_OTA_OK)assert(!output.size_bytes && !output.url[0]);
    }
    /* Inject failure at each cJSON allocation and prove no leaked tree/output. */
    cJSON_Hooks hooks={limited_malloc,limited_free};cJSON_InitHooks(&hooks);
    assert(tv4_ota_manifest_read((const uint8_t *)valid,strlen(valid),sig,n,&context,&output)==TV4_OTA_BAD_SIGNATURE);
    assert(!allocations); /* unauthenticated bytes never reach the parser */
    assert(read(valid,strlen(valid))==TV4_OTA_OK);assert(!outstanding);
    size_t count=allocations;
    for(size_t i=1;i<=count;i++) {
        allocations=0;fail_at=i;
        assert(read(valid,strlen(valid))==TV4_OTA_BAD_JSON);assert(!outstanding);
        assert(!output.size_bytes && !output.url[0]);
    }
    cJSON_InitHooks(NULL);
    printf("Native manifest signature/policy/encoding/OOM scenarios PASS; parse peak=%zu bytes, allocations=%zu\n",peak,count);
    OPENSSL_free(public_key);EVP_PKEY_free(key);
}
