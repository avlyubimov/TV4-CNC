#include "json_guard.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static cJSON *parse(const char *s) {
    return tv4_ota_json_parse((const uint8_t *)s,strlen(s),16384);
}
static void reject(const char *s) { assert(!parse(s)); }

int main(void) {
    unsigned accepted=0;
    /* Independent scalar oracle, not cJSON reserialization. Every BMP escape,
     * including all lone surrogates/NUL, and every ASCII non-hex in each slot. */
    for(unsigned cp=0;cp<65536;cp++) {
        char raw[16];assert(snprintf(raw,sizeof(raw),"\"\\u%04x\"",cp)==8);
        cJSON *s=parse(raw);
        bool valid=cp && (cp<0xd800 || cp>0xdfff);
        assert((s!=NULL)==valid);
        if(s) {
            unsigned char expected[4]={0};
            size_t length;
            if(cp<0x80) {expected[0]=(unsigned char)cp;length=1;}
            else if(cp<0x800) {
                expected[0]=(unsigned char)(0xc0|(cp>>6));
                expected[1]=(unsigned char)(0x80|(cp&63));length=2;
            } else {
                expected[0]=(unsigned char)(0xe0|(cp>>12));
                expected[1]=(unsigned char)(0x80|((cp>>6)&63));
                expected[2]=(unsigned char)(0x80|(cp&63));length=3;
            }
            assert(cJSON_IsString(s) && strlen(s->valuestring)==length);
            assert(!memcmp(s->valuestring,expected,length));++accepted;
        }
        cJSON_Delete(s);
    }
    unsigned invalid_hex=0;
    for(unsigned slot=0;slot<4;slot++)for(unsigned c=1;c<128;c++) {
        if((c>='0' && c<='9') || (c>='a' && c<='f') || (c>='A' && c<='F'))continue;
        char raw[]="\"prefix\\u1234suffix\"";raw[9+slot]=(char)c;
        reject(raw);++invalid_hex;
    }
    const char *bad[]={"\"\\u\"","\"\\u1\"","\"\\u12\"","\"\\u123\"",
        "\"\\x41\"","\"\\q\"","\"\\\n\"","\"\\ud800\\u0000\"",
        "\"\\ud800\\uZZZZ\"","\"\\ud800\\ud800\"","\"\\udc00\\ud800\""};
    for(size_t i=0;i<sizeof(bad)/sizeof(bad[0]);i++)reject(bad[i]);
    const char *good[]={"\"\\ud800\\udc00\"","\"\\udbff\\udfff\"",
        "\"\\uD83D\\uDE80\"","\"\\\\u0000\"","\"\\\\uZZZZ\"",
        "\"\\\"\\\\\\/\\b\\f\\n\\r\\t\"","{\"\\u0061\":1}"};
    for(size_t i=0;i<sizeof(good)/sizeof(good[0]);i++) {
        cJSON *s=parse(good[i]);assert(s);cJSON_Delete(s);
    }
    reject("{\"a\":1,\"\\u0061\":2}");
    /* Actual input length at every truncation boundary, without a terminator
     * in the supplied span. The gate must not read beyond that span. */
    const char *full="\"prefix\\u0041suffix\"";
    for(size_t n=1;n<strlen(full);n++) {
        uint8_t raw[32];memset(raw,0xa5,sizeof(raw));memcpy(raw,full,n);
        assert(!tv4_ota_json_parse(raw,n,16384));
    }
    printf("JSON escape oracle PASS: BMP=65536, accepted=%u, invalid hex=%u; "
           "surrogate/escape/duplicate/truncation cases PASS\n",accepted,invalid_hex);
    assert(accepted==63487);
}
