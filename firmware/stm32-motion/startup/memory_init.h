#ifndef TV4_STARTUP_MEMORY_INIT_H
#define TV4_STARTUP_MEMORY_INIT_H
#include <stdbool.h>
#include <stddef.h>
/* Trusted linker-owned, 4-byte word spans, not downloaded image addresses.
 * Validates arithmetic/alignment/overlap before any write. Cannot establish
 * physical RAM/ROM availability: the reviewed board linker must do that.
 * Zero-length spans may be NULL. Exact in-place data copy is allowed. */
bool tv4_startup_memory_init(void *data, const void *load, size_t data_words,
                            void *bss, size_t bss_words);
#endif
