#include "memory_init.h"
#include <stdint.h>

static bool valid_span(const void *p, size_t words)
{
    if (!words) return true;
    if (!p || (uintptr_t)p % _Alignof(uint32_t) || words > UINTPTR_MAX / sizeof(uint32_t))
        return false;
    return (uintptr_t)p <= UINTPTR_MAX - words * sizeof(uint32_t);
}
static bool overlaps(const void *a, size_t aw, const void *b, size_t bw)
{
    return aw && bw && (uintptr_t)a < (uintptr_t)b + bw * sizeof(uint32_t) &&
           (uintptr_t)b < (uintptr_t)a + aw * sizeof(uint32_t);
}
bool tv4_startup_memory_init(void *data, const void *load, size_t data_words,
                            void *bss, size_t bss_words)
{
    if (!valid_span(data, data_words) || !valid_span(load, data_words) ||
        !valid_span(bss, bss_words) ||
        (data != load && overlaps(data, data_words, load, data_words)) ||
        overlaps(data, data_words, bss, bss_words) ||
        overlaps(load, data_words, bss, bss_words)) return false;
    uint32_t *destination = data;
    const uint32_t *source = load;
    uint32_t *zero = bss;
    for (size_t i = 0; i < data_words; ++i) destination[i] = source[i];
    for (size_t i = 0; i < bss_words; ++i) zero[i] = 0;
    return true;
}
