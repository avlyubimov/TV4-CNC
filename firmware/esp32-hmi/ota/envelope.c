#include "envelope.h"
#include "json_guard.h"
#include <string.h>
static int b64(unsigned char c) {
    if(c>='A' && c<='Z')return c-'A';
    if(c>='a' && c<='z')return c-'a'+26;
    if(c>='0' && c<='9')return c-'0'+52;
    if(c=='+')return 62;
    if(c=='/')return 63;
    return -1;
}
static bool decode(const char *text,uint8_t out[72],size_t *written) {
    size_t n=strlen(text);if(n<12 || n>96 || n%4)return false;
    size_t used=0;
    for(size_t i=0;i<n;i+=4) {
        int a=b64((unsigned char)text[i]),b=b64((unsigned char)text[i+1]);
        bool pad2=text[i+2]=='=',pad3=text[i+3]=='=';
        if(a<0 || b<0 || (pad2 && !pad3) || ((pad2 || pad3) && i+4!=n))return false;
        int c=pad2?0:b64((unsigned char)text[i+2]),d=pad3?0:b64((unsigned char)text[i+3]);
        if(c<0 || d<0 || (pad2 && (b&15)) || (pad3 && !pad2 && (c&3)))return false;
        out[used++]=(uint8_t)((a<<2)|(b>>4));
        if(!pad2)out[used++]=(uint8_t)((b<<4)|(c>>2));
        if(!pad3)out[used++]=(uint8_t)((c<<6)|d);
    }
    if(used<8 || used>72)return false;
    *written=used;return true;
}
tv4_ota_result tv4_ota_signed_manifest_read(const uint8_t *raw,size_t raw_length,
    const uint8_t *envelope,size_t envelope_length,const tv4_ota_context *ctx,tv4_ota_candidate *out) {
    if(!out)return TV4_OTA_BAD_SCHEMA;
    memset(out,0,sizeof(*out));
    bool id_ok=false;
    if(ctx && ctx->key_id)for(size_t i=0;i<=64;i++)if(!ctx->key_id[i]) {id_ok=i>0;break;}
    if(!ctx || !id_ok ||
       !tv4_ota_p256_key(ctx->public_key,ctx->public_key_length))return TV4_OTA_NO_TRUST;
    cJSON *root=tv4_ota_json_parse(envelope,envelope_length,TV4_OTA_ENVELOPE_MAX);
    if(!root)return TV4_OTA_BAD_ENVELOPE;
    tv4_ota_result result=TV4_OTA_BAD_ENVELOPE;
    const cJSON *id=NULL,*algorithm=NULL,*signature=NULL;unsigned fields=0;
    if(cJSON_IsObject(root))for(const cJSON *p=root->child;p;p=p->next) {
        ++fields;
        if(!strcmp(p->string,"key_id"))id=p;
        else if(!strcmp(p->string,"algorithm"))algorithm=p;
        else if(!strcmp(p->string,"signature"))signature=p;
    }
    uint8_t der[72];size_t der_length=0;
    if(fields==3 && id && algorithm && signature &&
       cJSON_IsString(id) && cJSON_IsString(algorithm) && cJSON_IsString(signature) &&
       id->valuestring && algorithm->valuestring && signature->valuestring) {
        if(strcmp(id->valuestring,ctx->key_id))result=TV4_OTA_NO_TRUST;
        else if(!strcmp(algorithm->valuestring,"ECDSA-P256-SHA256") && decode(signature->valuestring,der,&der_length))result=TV4_OTA_OK;
    }
    cJSON_Delete(root);
    if(result!=TV4_OTA_OK)return result;
    return tv4_ota_manifest_read(raw,raw_length,der,der_length,ctx,out);
}
