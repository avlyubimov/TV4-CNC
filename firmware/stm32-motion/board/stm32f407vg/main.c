#include "bench.h"
#include "registers.h"
#include "memory_init.h"
#include "bench_build_identity.h"

extern uint32_t __data_load__,__data_start__,__data_end__,__bss_start__,__bss_end__;
extern uint32_t __ccm_start__,__ccm_end__,__stack_bottom__,__stack_top__;
extern uint32_t __fault_stack_top__,__main_guard_start__,__fault_guard_start__;
extern const uint32_t tv4_bench_vectors[];
extern _Noreturn void tv4_bench_halt(void);
_Static_assert(sizeof(uintptr_t)==4,"ARM32 bench only");
volatile tv4_bench_diagnostics tv4_bench_diag;
volatile tv4_bench_record tv4_bench_history __attribute__((section(".noinit.bench_history"),used));
static volatile uint32_t ccm_probe __attribute__((section(".ccm_bss"),used));
static volatile uint32_t data_cookie=UINT32_C(0x40704701);
static volatile uint32_t bss_cookie;
static volatile uint32_t fault_latched;
uint32_t SystemCoreClock=TV4_BENCH_HSI_HZ;
const tv4_bench_build_identity tv4_bench_build __attribute__((section(".bench_build"),used))={
    UINT32_C(0x54563442),TV4_BENCH_ABI,TV4_BENCH_HSI_HZ,1024,TV4_BENCH_SOURCE_DIRTY,
    TV4_BENCH_SOURCE_COMMIT,"stm32f407vg-hsi-no-motion-v1"
};

static void barrier(void){__asm volatile("dsb\nisb":::"memory");}
static void init_guards(void)
{
    volatile uint32_t *a=&__main_guard_start__,*b=&__fault_guard_start__;
    for(unsigned i=0;i<8;++i){a[i]=UINT32_C(0xa55a4070);b[i]=UINT32_C(0xa55a4070);}
}
static bool guards_ok(void)
{
    volatile const uint32_t *a=&__main_guard_start__,*b=&__fault_guard_start__;
    for(unsigned i=0;i<8;++i)if(a[i]!=UINT32_C(0xa55a4070) || b[i]!=UINT32_C(0xa55a4070))return false;
    return true;
}
static void core_prepare(void)
{
    tv4_bench_write32(TV4_SYSTICK_CTRL,0);
    tv4_bench_write32(TV4_SCB_ICSR,(UINT32_C(1)<<25)|(UINT32_C(1)<<27));
    for(unsigned i=0;i<3;++i) {
        uint32_t mask=i==2?UINT32_C(0x3ffff):UINT32_MAX;
        tv4_bench_write32(TV4_NVIC_ICER+4u*i,mask);tv4_bench_write32(TV4_NVIC_ICPR+4u*i,mask);
    }
    tv4_bench_write32(TV4_SCB_VTOR,(uint32_t)(uintptr_t)tv4_bench_vectors);barrier();
    tv4_bench_write32(TV4_SCB_CCR,tv4_bench_read32(TV4_SCB_CCR)|UINT32_C(0x218)); /* STKALIGN, DIV0/UNALIGNED traps */
    tv4_bench_write32(TV4_SCB_SHCSR,tv4_bench_read32(TV4_SCB_SHCSR)|UINT32_C(0x70000));
    barrier();
}
static _Noreturn void fail(tv4_bench_error error,const tv4_bench_fault *fault)
{
    __asm volatile("cpsid i":::"memory");
    tv4_bench_diag.state=TV4_BENCH_FAULT;tv4_bench_diag.error=(uint32_t)error;
    tv4_bench_diag.motion_outputs=0;
    tv4_bench_record_save(&tv4_bench_history,tv4_bench_diag.boot_count,(uint32_t)error,fault);
    barrier();tv4_bench_halt(); /* no watchdog feed/reset-to-motion path */
}
void SysTick_Handler(void)
{++tv4_bench_diag.ticks_ms;}
_Noreturn void tv4_bench_fault_capture(uint32_t sp,uint32_t exc_return,uint32_t exception)
{
    if(fault_latched!=0)tv4_bench_halt();
    fault_latched=1;
    tv4_bench_fault f={.exception=exception,.exc_return=exc_return,.stack_pointer=sp,
        .cfsr=tv4_bench_read32(TV4_SCB_CFSR),.hfsr=tv4_bench_read32(TV4_SCB_HFSR),
        .mmfar=tv4_bench_read32(TV4_SCB_MMFAR),.bfar=tv4_bench_read32(TV4_SCB_BFAR)};
    uint32_t core;
    if(tv4_bench_frame_span(sp,exc_return,f.cfsr,(uint32_t)(uintptr_t)&__stack_bottom__,
        (uint32_t)(uintptr_t)&__stack_top__,&core)) {
        volatile const uint32_t *frame=(volatile const uint32_t *)(uintptr_t)core;
        f.r0=frame[0];f.r1=frame[1];f.r2=frame[2];f.r3=frame[3];f.r12=frame[4];
        f.lr=frame[5];f.pc=frame[6];f.xpsr=frame[7];f.frame_valid=1;
    }
    fail(TV4_BENCH_EXCEPTION,&f);
}
_Noreturn void tv4_bench_entry(void)
{
    /* Normal SRAM only. CCM is initialized after its clock is enabled. */
    size_t data=((uintptr_t)&__data_end__-(uintptr_t)&__data_start__)/4u;
    size_t bss=((uintptr_t)&__bss_end__-(uintptr_t)&__bss_start__)/4u;
    if(!tv4_startup_memory_init(&__data_start__,&__data_load__,data,&__bss_start__,bss))tv4_bench_halt();
    tv4_bench_diag_begin(&tv4_bench_diag,&tv4_bench_history);init_guards();core_prepare();
    tv4_bench_error error=tv4_bench_hardware_init(&tv4_bench_diag);if(error!=TV4_BENCH_OK)fail(error,NULL);
    for(volatile uint32_t *p=&__ccm_start__;p<&__ccm_end__;++p)*p=0;
    if(SystemCoreClock!=TV4_BENCH_HSI_HZ || data_cookie!=UINT32_C(0x40704701) || bss_cookie!=0 || ccm_probe!=0)fail(TV4_BENCH_MEMORY,NULL);
    tv4_bench_diag.data_bss_pass=1;data_cookie=UINT32_C(0x55aa55aa);bss_cookie=UINT32_C(0xaa55aa55);
    ccm_probe=UINT32_C(0x4070cc01);barrier();if(ccm_probe!=UINT32_C(0x4070cc01))fail(TV4_BENCH_MEMORY,NULL);
    tv4_bench_diag.ccm_probe_pass=1;tv4_bench_diag.stack_guards_pass=guards_ok()?1u:0u;
    if(tv4_bench_diag.stack_guards_pass==0)fail(TV4_BENCH_STACK,NULL);
    tv4_bench_diag.state=TV4_BENCH_RUNNING;tv4_bench_tick_start();barrier();__asm volatile("cpsie i":::"memory");
    uint32_t last=0;
    for(;;) {
        uint32_t request=tv4_bench_diag.request;
        if(request==TV4_BENCH_TEST_WATCHDOG) {
            tv4_bench_diag.request=0;tv4_bench_diag.state=TV4_BENCH_WATCHDOG_TEST;
            tv4_bench_record_save(&tv4_bench_history,tv4_bench_diag.boot_count,TV4_BENCH_TEST_WATCHDOG,NULL);
        }else if(request==TV4_BENCH_TEST_FAULT){tv4_bench_diag.request=0;__asm volatile("udf #0");}
        else if(request!=0)tv4_bench_diag.request=0; /* unknown request never acts */
        bool guards=guards_ok();if(!guards)fail(TV4_BENCH_STACK,NULL);
        uint32_t now=tv4_bench_diag.ticks_ms;
        if(tv4_bench_feed_due(now,&last,(tv4_bench_state)tv4_bench_diag.state,guards)) {
            tv4_bench_watchdog_feed();++tv4_bench_diag.watchdog_feeds;++tv4_bench_diag.heartbeat;
        }
        __asm volatile("wfi");
    }
}
