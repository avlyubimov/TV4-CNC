#include "bench.h"
#include "registers.h"

static bool wait_bits(uint32_t address,uint32_t mask,uint32_t expected)
{
    for(uint32_t n=0;n<TV4_BENCH_WAIT_LOOPS;++n)
        if((tv4_bench_read32(address)&mask)==expected)return true;
    return false; /* iteration bound, not a measured time budget */
}
static void update(uint32_t address,uint32_t set,uint32_t clear)
{tv4_bench_write32(address,(tv4_bench_read32(address)&~clear)|set);}
tv4_bench_error tv4_bench_hardware_init(volatile tv4_bench_diagnostics *d)
{
    d->reset_flags=tv4_bench_read32(TV4_RCC_CSR);
    d->core_id=tv4_bench_read32(TV4_SCB_CPUID);
    if(((d->core_id>>4)&UINT32_C(0xfff))!=UINT32_C(0xc24))return TV4_BENCH_BAD_CORE;
    d->device_id=tv4_bench_read32(TV4_DBG_IDCODE);
    if((d->device_id&UINT32_C(0xfff))!=UINT32_C(0x413))return TV4_BENCH_BAD_DEVICE;
    d->flash_kib=tv4_bench_read16(TV4_FLASH_KIB);
    if(d->flash_kib!=1024u)return TV4_BENCH_BAD_FLASH;
    /* Freeze only IWDG during CPU HALT. Running/host-disconnected CPU still
     * has an active watchdog. No trace pins or peripheral GPIO are touched. */
    update(TV4_DBG_APB1_FZ,UINT32_C(1)<<12,0);d->debug_halt_freeze=1;
    d->state=TV4_BENCH_CLOCK;update(TV4_RCC_CR,1,0);
    if(!wait_bits(TV4_RCC_CR,2,2))return TV4_BENCH_HSI_TIMEOUT;
    update(TV4_RCC_CFGR,0,3);
    if(!wait_bits(TV4_RCC_CFGR,12,0))return TV4_BENCH_CLOCK_SWITCH_TIMEOUT;
    tv4_bench_write32(TV4_RCC_CFGR,0); /* HCLK/PCLK1/PCLK2 = HSI, no MCO */
    update(TV4_RCC_CR,0,(UINT32_C(1)<<16)|(UINT32_C(1)<<19)|(UINT32_C(1)<<24)|(UINT32_C(1)<<26));
    if(!wait_bits(TV4_RCC_CR,(UINT32_C(1)<<17)|(UINT32_C(1)<<25)|(UINT32_C(1)<<27),0))return TV4_BENCH_CLOCK_STOP_TIMEOUT;
    update(TV4_RCC_CR,0,UINT32_C(1)<<18); /* HSEBYP writable only with HSE off */
    tv4_bench_write32(TV4_RCC_CIR,0);update(TV4_RCC_AHB1ENR,UINT32_C(1)<<20,0);
    (void)tv4_bench_read32(TV4_RCC_AHB1ENR);d->hsi_hz=TV4_BENCH_HSI_HZ;
    d->rcc_cr=tv4_bench_read32(TV4_RCC_CR);d->rcc_cfgr=tv4_bench_read32(TV4_RCC_CFGR);
    d->state=TV4_BENCH_WATCHDOG;update(TV4_RCC_CSR,1,0);
    if(!wait_bits(TV4_RCC_CSR,2,2))return TV4_BENCH_LSI_TIMEOUT;
    if(!wait_bits(TV4_IWDG_SR,3,0))return TV4_BENCH_IWDG_TIMEOUT;
    tv4_bench_write32(TV4_IWDG_KR,UINT32_C(0x5555));
    tv4_bench_write32(TV4_IWDG_PR,TV4_BENCH_IWDG_PR);
    tv4_bench_write32(TV4_IWDG_RLR,TV4_BENCH_IWDG_RLR);
    if(!wait_bits(TV4_IWDG_SR,3,0))return TV4_BENCH_IWDG_TIMEOUT;
    if(tv4_bench_read32(TV4_IWDG_PR)!=TV4_BENCH_IWDG_PR || tv4_bench_read32(TV4_IWDG_RLR)!=TV4_BENCH_IWDG_RLR)return TV4_BENCH_IWDG_READBACK;
    tv4_bench_write32(TV4_IWDG_KR,UINT32_C(0xaaaa));
    tv4_bench_write32(TV4_IWDG_KR,UINT32_C(0xcccc));d->watchdog_started=1;
    update(TV4_RCC_CSR,UINT32_C(1)<<24,0); /* RMVF only after captured flags */
    d->rcc_csr=tv4_bench_read32(TV4_RCC_CSR);return TV4_BENCH_OK;
}
void tv4_bench_tick_start(void)
{
    tv4_bench_write32(TV4_SYSTICK_CTRL,0);tv4_bench_write32(TV4_SYSTICK_LOAD,TV4_BENCH_TICK_RELOAD);
    tv4_bench_write32(TV4_SYSTICK_VAL,0);tv4_bench_write32(TV4_SYSTICK_CTRL,7);
}
void tv4_bench_watchdog_feed(void)
{tv4_bench_write32(TV4_IWDG_KR,UINT32_C(0xaaaa));}
