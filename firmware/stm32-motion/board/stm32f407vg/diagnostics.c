#include "bench.h"
#include <limits.h>
_Static_assert(sizeof(tv4_bench_fault)==64,"SWD fault ABI");
_Static_assert(sizeof(tv4_bench_record)==84,"warm-reset record ABI");
_Static_assert(sizeof(tv4_bench_diagnostics)==168,"SWD diagnostics ABI");

static uint32_t crc_word(uint32_t crc,uint32_t word)
{
    crc^=word;
    for(unsigned bit=0;bit<32;++bit)crc=(crc>>1)^(UINT32_C(0xedb88320)&(0u-(crc&1u)));
    return crc;
}
static uint32_t record_crc(const volatile tv4_bench_record *r)
{
    uint32_t c=UINT32_MAX;c=crc_word(c,r->abi);c=crc_word(c,r->boot_count);c=crc_word(c,r->reason);
    /* Named fields avoid object-padding/alias assumptions in persistent CRC. */
#define WORD(x) c=crc_word(c,r->fault.x)
    WORD(exception);WORD(exc_return);WORD(stack_pointer);WORD(frame_valid);
    WORD(r0);WORD(r1);WORD(r2);WORD(r3);WORD(r12);WORD(lr);WORD(pc);WORD(xpsr);
    WORD(cfsr);WORD(hfsr);WORD(mmfar);WORD(bfar);
#undef WORD
    return ~c;
}
bool tv4_bench_record_valid(const volatile tv4_bench_record *r)
{return r!=NULL && r->magic==TV4_BENCH_RECORD_MAGIC && r->abi==TV4_BENCH_ABI && r->boot_count!=0 && r->crc==record_crc(r);}
void tv4_bench_record_save(volatile tv4_bench_record *r,uint32_t boots,uint32_t reason,const tv4_bench_fault *fault)
{
    r->magic=0;r->abi=TV4_BENCH_ABI;r->boot_count=boots;r->reason=reason;
    r->fault=fault!=NULL?*fault:(tv4_bench_fault){0};r->crc=record_crc(r);
    /* RAM diagnostic integrity only, not durable/authenticated identity.
     * Completed record is committed last; interrupted write is discarded. */
#if !defined(TV4_BENCH_TEST_MMIO)
    __asm volatile("dmb":::"memory");
#endif
    r->magic=TV4_BENCH_RECORD_MAGIC;
}
void tv4_bench_diag_begin(volatile tv4_bench_diagnostics *d,volatile tv4_bench_record *r)
{
    *d=(tv4_bench_diagnostics){0};d->abi=TV4_BENCH_ABI;d->bytes=(uint32_t)sizeof(*d);d->state=TV4_BENCH_BOOT;
    bool valid=tv4_bench_record_valid(r);d->history_valid=valid?1u:0u;
    d->boot_count=valid?(r->boot_count==UINT32_MAX?UINT32_MAX:r->boot_count+1u):1u;
    if(valid){d->previous_reason=r->reason;d->previous_fault=r->fault;}
    tv4_bench_record_save(r,d->boot_count,0,NULL);d->magic=TV4_BENCH_DIAG_MAGIC;
}
bool tv4_bench_frame_span(uint32_t sp,uint32_t exc_return,uint32_t cfsr,
    uint32_t bottom,uint32_t top,uint32_t *core)
{
    if(core==NULL)return false;
    *core=0;
    /* Stacking/lazy-stacking errors make the memory frame untrustworthy.
     * Only our normal MSP stack is an acceptable source, never fault stack. */
    if((cfsr&UINT32_C(0x3838))!=0 || (sp&3u)!=0 || bottom>=top ||
        (exc_return!=UINT32_C(0xfffffff1) && exc_return!=UINT32_C(0xfffffff9) &&
         exc_return!=UINT32_C(0xffffffe1) && exc_return!=UINT32_C(0xffffffe9)))return false;
    uint32_t extra=(exc_return&16u)!=0?0u:72u;
    if(sp<bottom || sp>top || top-sp<extra+32u)return false;
    *core=sp+extra;return true;
}
bool tv4_bench_feed_due(uint32_t now,uint32_t *last,tv4_bench_state state,bool guards_ok)
{
    if(last==NULL || state!=TV4_BENCH_RUNNING || !guards_ok)return false;
    uint32_t elapsed=now-*last;
    if(elapsed<TV4_BENCH_FEED_MS || elapsed>UINT32_C(0x7fffffff))return false;
    *last=now;return true;
}
