#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/* Zero-initialize. Fixed number of crypto contexts; no image-sized or per-chunk
 * buffers in this layer. Finish/close releases backend resources. Worker/single owner only. */
typedef struct { void *context; } tv4_ota_hash;
bool tv4_ota_hash_open(tv4_ota_hash *hash);
bool tv4_ota_hash_update(tv4_ota_hash *hash,const uint8_t *bytes,size_t length);
bool tv4_ota_hash_finish(tv4_ota_hash *hash,uint8_t digest[32]);
void tv4_ota_hash_close(tv4_ota_hash *hash);
