#include "verification.h"
#include <string.h>
#ifdef ESP_PLATFORM
#include "esp_idf_version.h"
#include "mbedtls/md.h"
#include "mbedtls/pk.h"
#if ESP_IDF_VERSION_MAJOR >= 6
#include "psa/crypto.h"
#endif
#else
#include <openssl/evp.h>
#include <openssl/x509.h>
#endif
bool tv4_ota_p256_key(const uint8_t *der,size_t length) {
    static const uint8_t prefix[]={0x30,0x59,0x30,0x13,0x06,0x07,0x2a,0x86,
        0x48,0xce,0x3d,0x02,0x01,0x06,0x08,0x2a,0x86,0x48,0xce,0x3d,0x03,
        0x01,0x07,0x03,0x42,0x00,0x04};
    return der && length==91 && !memcmp(der,prefix,sizeof(prefix));
}
bool tv4_ota_sha256(const uint8_t *data,size_t length,uint8_t digest[32]) {
    if(!data || !digest)return false;
#ifdef ESP_PLATFORM
#if ESP_IDF_VERSION_MAJOR >= 6
    if(psa_crypto_init()!=PSA_SUCCESS)return false;
#endif
    const mbedtls_md_info_t *md=mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    return md && mbedtls_md(md,data,length,digest)==0;
#else
    unsigned count=0;
    return EVP_Digest(data,length,digest,&count,EVP_sha256(),NULL)==1 && count==32;
#endif
}
bool tv4_ota_verify_manifest(const uint8_t *manifest,size_t length,
                            const uint8_t *signature,size_t signature_length,
                            const uint8_t *trusted_key,size_t key_length) {
    if(!manifest || !length || length>TV4_OTA_MANIFEST_MAX || !signature ||
       signature_length<8 || signature_length>TV4_OTA_SIGNATURE_MAX ||
       !tv4_ota_p256_key(trusted_key,key_length))return false;
#ifdef ESP_PLATFORM
    uint8_t hash[32];if(!tv4_ota_sha256(manifest,length,hash))return false;
    mbedtls_pk_context key;mbedtls_pk_init(&key);
    bool ok=mbedtls_pk_parse_public_key(&key,trusted_key,key_length)==0 &&
            mbedtls_pk_get_bitlen(&key)==256 &&
            mbedtls_pk_verify(&key,MBEDTLS_MD_SHA256,hash,sizeof(hash),signature,signature_length)==0;
    mbedtls_pk_free(&key);return ok;
#else
    const unsigned char *cursor=trusted_key;
    EVP_PKEY *key=d2i_PUBKEY(NULL,&cursor,(long)key_length);
    if(!key)return false;
    EVP_MD_CTX *ctx=EVP_MD_CTX_new();
    bool ok=ctx && EVP_DigestVerifyInit(ctx,NULL,EVP_sha256(),NULL,key)==1 &&
            EVP_DigestVerify(ctx,signature,signature_length,manifest,length)==1;
    EVP_MD_CTX_free(ctx);EVP_PKEY_free(key);return ok;
#endif
}
