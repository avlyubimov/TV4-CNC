#include "bench.h"
/* GCC emits memset for aggregate initialization even with -fno-builtin.
 * This one freestanding byte primitive needs no libc, heap or initialized FPU.
 * Volatile stores prevent compiler recognition/recursive memset generation. */
void *tv4_bench_memset(void *destination,int value,size_t bytes)
{
    volatile unsigned char *out=destination;
    for(size_t i=0;i<bytes;++i)out[i]=(unsigned char)value;
    return destination;
}
#if !defined(TV4_BENCH_TEST_MMIO)
void *memset(void *destination,int value,size_t bytes) __attribute__((alias("tv4_bench_memset")));
#endif
