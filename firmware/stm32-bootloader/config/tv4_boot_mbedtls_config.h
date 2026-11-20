#ifndef TV4_BOOT_MBEDTLS_CONFIG_H
#define TV4_BOOT_MBEDTLS_CONFIG_H

#define MBEDTLS_HAVE_ASM
#define MBEDTLS_BIGNUM_C
#define MBEDTLS_ASN1_PARSE_C
#define MBEDTLS_ASN1_WRITE_C
#define MBEDTLS_ECP_C
#define MBEDTLS_ECP_DP_SECP256R1_ENABLED
#define MBEDTLS_ECP_NIST_OPTIM
#define MBEDTLS_ECDSA_C
#define MBEDTLS_SHA256_C
#define MBEDTLS_MPI_MAX_SIZE 32
/* Keep upstream P-256 window/fixed-point defaults; no speed-for-RAM tweak. */
#define MBEDTLS_PLATFORM_C
#define MBEDTLS_PLATFORM_MEMORY
#define MBEDTLS_PLATFORM_NO_STD_FUNCTIONS
#define MBEDTLS_MEMORY_BUFFER_ALLOC_C
#define MBEDTLS_MEMORY_ALIGN_MULTIPLE 8
#define MBEDTLS_PLATFORM_EXIT_MACRO tv4_boot_crypto_panic
_Noreturn void tv4_boot_crypto_panic(int status);

#endif
