#include "hash.h"
#include <stdlib.h>
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_idf_version.h"
#include "mbedtls/md.h"
#if ESP_IDF_VERSION_MAJOR >= 6
#include "psa/crypto.h"
#endif
#else
#include <openssl/evp.h>
#endif
void tv4_ota_hash_close(tv4_ota_hash *h) {
    if(!h || !h->context)return;
#ifdef ESP_PLATFORM
    mbedtls_md_context_t *ctx=h->context;mbedtls_md_free(ctx);free(ctx);
#else
    EVP_MD_CTX_free(h->context);
#endif
    h->context=NULL;
}
bool tv4_ota_hash_open(tv4_ota_hash *h) {
    if(!h || h->context)return false;
#ifdef ESP_PLATFORM
#if ESP_IDF_VERSION_MAJOR >= 6
    if(psa_crypto_init()!=PSA_SUCCESS)return false;
#endif
    mbedtls_md_context_t *ctx=calloc(1,sizeof(*ctx));if(!ctx)return false;
    mbedtls_md_init(ctx);h->context=ctx;
    const mbedtls_md_info_t *info=mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if(info && !mbedtls_md_setup(ctx,info,0) && !mbedtls_md_starts(ctx))return true;
#else
    EVP_MD_CTX *ctx=EVP_MD_CTX_new();if(!ctx)return false;h->context=ctx;
    if(EVP_DigestInit_ex(ctx,EVP_sha256(),NULL)==1)return true;
#endif
    tv4_ota_hash_close(h);return false;
}
bool tv4_ota_hash_update(tv4_ota_hash *h,const uint8_t *bytes,size_t length) {
    if(!h || !h->context || (!bytes && length))return false;
    if(!length)return true;
#ifdef ESP_PLATFORM
    return mbedtls_md_update(h->context,bytes,length)==0;
#else
    return EVP_DigestUpdate(h->context,bytes,length)==1;
#endif
}
bool tv4_ota_hash_finish(tv4_ota_hash *h,uint8_t digest[32]) {
    if(!h || !h->context || !digest) {tv4_ota_hash_close(h);return false;}
#ifdef ESP_PLATFORM
    bool ok=mbedtls_md_finish(h->context,digest)==0;
#else
    unsigned count=0;bool ok=EVP_DigestFinal_ex(h->context,digest,&count)==1 && count==32;
#endif
    tv4_ota_hash_close(h);if(!ok)memset(digest,0,32);return ok;
}
