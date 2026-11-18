#include "verification.h"
#include <assert.h>
#include <openssl/evp.h>
#include <openssl/x509.h>
#include <string.h>
#include <stdio.h>
static EVP_PKEY *generate(const char *curve) {
    EVP_PKEY *key=EVP_PKEY_Q_keygen(NULL,NULL,"EC",curve);assert(key);return key;
}
int main(void) {
    /* Ephemeral test key in RAM only: never a device trust anchor. */
    EVP_PKEY *key=generate("prime256v1");
    unsigned char *der=NULL;int n=i2d_PUBKEY(key,&der);assert(n==91);
    const uint8_t message[]="{\"schema_version\":1}\n";
    uint8_t signature[80];size_t length=sizeof(signature);
    EVP_MD_CTX *ctx=EVP_MD_CTX_new();assert(ctx);
    assert(EVP_DigestSignInit(ctx,NULL,EVP_sha256(),NULL,key)==1);
    assert(EVP_DigestSign(ctx,signature,&length,message,sizeof(message)-1)==1);
    assert(tv4_ota_verify_manifest(message,sizeof(message)-1,signature,length,der,(size_t)n));
    assert(!tv4_ota_verify_manifest(message,sizeof(message)-2,signature,length,der,(size_t)n));
    signature[length-1]^=1;
    assert(!tv4_ota_verify_manifest(message,sizeof(message)-1,signature,length,der,(size_t)n));
    signature[length-1]^=1;
    assert(!tv4_ota_verify_manifest(message,TV4_OTA_MANIFEST_MAX+1,signature,length,der,(size_t)n));
    assert(!tv4_ota_verify_manifest(message,sizeof(message)-1,signature,length,NULL,0));
    assert(!tv4_ota_verify_manifest(message,sizeof(message)-1,signature,73,der,(size_t)n));
    uint8_t hash[32];const uint8_t abc[]="abc";
    const uint8_t expected[]={0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
        0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    assert(tv4_ota_sha256(abc,3,hash) && !memcmp(hash,expected,32));
    EVP_PKEY *other=generate("prime256v1");unsigned char *other_der=NULL;
    int other_n=i2d_PUBKEY(other,&other_der);assert(other_n==91);
    assert(!tv4_ota_verify_manifest(message,sizeof(message)-1,signature,length,other_der,(size_t)other_n));
    EVP_PKEY *wrong=generate("secp256k1");unsigned char *wrong_der=NULL;
    int wrong_n=i2d_PUBKEY(wrong,&wrong_der);assert(wrong_n>0);
    assert(!tv4_ota_p256_key(wrong_der,(size_t)wrong_n));
    OPENSSL_free(wrong_der);OPENSSL_free(other_der);OPENSSL_free(der);
    EVP_PKEY_free(wrong);EVP_PKEY_free(other);EVP_PKEY_free(key);EVP_MD_CTX_free(ctx);
    puts("OTA P-256 signature, exact-byte mutation, unknown key, curve and SHA-256 PASS");
}
