#include "bench.h"
#include "registers.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

enum injection {NONE,BAD_CORE,BAD_DEVICE,BAD_FLASH,HSI_STUCK,SWITCH_STUCK,STOP_STUCK,
    LSI_STUCK,IWDG_INITIAL_PENDING,IWDG_PVU_STUCK,IWDG_RVU_STUCK,IWDG_BAD_PR,IWDG_BAD_RLR};
static struct {
    uint32_t cr,cfgr,csr,ahb,debug,pr,rlr,sr,ctrl,load,val,reads,writes;
    unsigned updates,feeds,starts,sync_reads;
    bool unlocked;
    enum injection fail;
} mock;

uint16_t tv4_bench_test_read16(uint32_t address)
{assert(address==TV4_FLASH_KIB);++mock.reads;return mock.fail==BAD_FLASH?512u:1024u;}
uint32_t tv4_bench_test_read32(uint32_t address)
{
    ++mock.reads;
    switch(address) {
    case TV4_SCB_CPUID:return mock.fail==BAD_CORE?UINT32_C(0x410fc231):UINT32_C(0x410fc241);
    case TV4_DBG_IDCODE:return mock.fail==BAD_DEVICE?UINT32_C(0x10000419):UINT32_C(0x10016413);
    case TV4_RCC_CR:return mock.cr;
    case TV4_RCC_CFGR:return mock.cfgr;
    case TV4_RCC_CSR:return mock.csr;
    case TV4_RCC_AHB1ENR:return mock.ahb;
    case TV4_DBG_APB1_FZ:return mock.debug;
    case TV4_IWDG_PR:return mock.pr^(mock.fail==IWDG_BAD_PR?1u:0u);
    case TV4_IWDG_RLR:return mock.rlr^(mock.fail==IWDG_BAD_RLR?1u:0u);
    case TV4_IWDG_SR: {
        /* An inactive IWDG cannot complete a pending clock-domain update.
         * The old wait-before-start sequence must fail this actual-source
         * model; no claim is made about uncaptured SR bits on the board. */
        uint32_t result=mock.sr;
        if(mock.starts!=0 && ++mock.sync_reads>=6) {
            mock.sr=mock.fail==IWDG_PVU_STUCK?1u:mock.fail==IWDG_RVU_STUCK?2u:0u;
        }
        return result;
    }
    default:assert(!"read outside exact bench register set");return 0;
    }
}
void tv4_bench_test_write32(uint32_t address,uint32_t value)
{
    ++mock.writes;
    switch(address) {
    case TV4_DBG_APB1_FZ:
        assert(value==(mock.debug|(UINT32_C(1)<<12)));mock.debug=value;break;
    case TV4_RCC_CR: {
        uint32_t enable=(UINT32_C(1)<<16)|(UINT32_C(1)<<19)|(UINT32_C(1)<<24)|(UINT32_C(1)<<26);
        assert(((value&enable)&~mock.cr)==0); /* never enable HSE/CSS/PLL */
        assert((value&UINT32_C(0xfff8))==(mock.cr&UINT32_C(0xfff8))); /* trim/calibration retained */
        if((value&(UINT32_C(1)<<18))!=(mock.cr&(UINT32_C(1)<<18)))assert((mock.cr&(UINT32_C(1)<<17))==0);
        mock.cr=value&~((UINT32_C(1)<<1)|(UINT32_C(1)<<17)|(UINT32_C(1)<<25)|(UINT32_C(1)<<27));
        if((value&1u)!=0 && mock.fail!=HSI_STUCK)mock.cr|=2;
        if((value&(UINT32_C(1)<<16))!=0)mock.cr|=UINT32_C(1)<<17;
        if((value&(UINT32_C(1)<<24))!=0 || mock.fail==STOP_STUCK)mock.cr|=UINT32_C(1)<<25;
        if((value&(UINT32_C(1)<<26))!=0)mock.cr|=UINT32_C(1)<<27;
        break;
    }
    case TV4_RCC_CFGR:
        assert((value&3u)==0 && (mock.cr&2u)!=0);
        mock.cfgr=(value&~12u)|(mock.fail==SWITCH_STUCK?8u:0u);break;
    case TV4_RCC_CIR:assert(value==0);break;
    case TV4_RCC_AHB1ENR:assert(value==(mock.ahb|(UINT32_C(1)<<20)));mock.ahb=value;break;
    case TV4_RCC_CSR:
        if((value&(UINT32_C(1)<<24))!=0)assert(mock.starts==1);
        mock.csr=(value&~UINT32_C(0x01000002));
        if((value&(UINT32_C(1)<<24))!=0)mock.csr&=~UINT32_C(0xfe000000);
        if((value&1u)!=0 && mock.fail!=LSI_STUCK)mock.csr|=2;
        break;
    case TV4_IWDG_KR:
        if(value==UINT32_C(0x5555))mock.unlocked=true;
        else if(value==UINT32_C(0xaaaa)){assert(mock.starts==1 && mock.pr==4 && mock.rlr==1023 && mock.sr==0);mock.unlocked=false;++mock.feeds;}
        else {assert(value==UINT32_C(0xcccc) && mock.starts==0);++mock.starts;}
        break;
    case TV4_IWDG_PR:assert(mock.unlocked && value==4);mock.pr=value;mock.sr|=1;++mock.updates;break;
    case TV4_IWDG_RLR:assert(mock.unlocked && value==1023);mock.rlr=value;mock.sr|=2;++mock.updates;break;
    case TV4_SYSTICK_CTRL:assert(value==0 || value==7);mock.ctrl=value;break;
    case TV4_SYSTICK_LOAD:assert(value==15999);mock.load=value;break;
    case TV4_SYSTICK_VAL:assert(value==0);mock.val=value;break;
    default:assert(!"forbidden write: no GPIO/CAN/timer/DMA/Flash/option byte register allowed");
    }
}
static void reset_mock(enum injection fail,bool warm)
{
    memset(&mock,0,sizeof(mock));mock.fail=fail;mock.cr=UINT32_C(0x5583);
    mock.csr=UINT32_C(0x20000000);mock.ahb=UINT32_C(1)<<20;mock.debug=UINT32_C(1)<<5;
    if(fail==IWDG_INITIAL_PENDING)mock.sr=3;
    if(warm){mock.cr|=(UINT32_C(1)<<16)|(UINT32_C(1)<<17)|(UINT32_C(1)<<18)|
        (UINT32_C(1)<<19)|(UINT32_C(1)<<24)|(UINT32_C(1)<<25)|(UINT32_C(1)<<26)|(UINT32_C(1)<<27);
        mock.cfgr=UINT32_C(0xcfe1fc0a);}
}
static void hardware_paths(void)
{
    const tv4_bench_error expected[]={TV4_BENCH_OK,TV4_BENCH_BAD_CORE,TV4_BENCH_BAD_DEVICE,
        TV4_BENCH_BAD_FLASH,TV4_BENCH_HSI_TIMEOUT,TV4_BENCH_CLOCK_SWITCH_TIMEOUT,TV4_BENCH_CLOCK_STOP_TIMEOUT,
        TV4_BENCH_LSI_TIMEOUT,TV4_BENCH_OK,TV4_BENCH_IWDG_TIMEOUT,TV4_BENCH_IWDG_TIMEOUT,
        TV4_BENCH_IWDG_READBACK,TV4_BENCH_IWDG_READBACK};
    unsigned cases=0;
    for(unsigned warm=0;warm<2;++warm)for(unsigned i=0;i<sizeof(expected)/sizeof(expected[0]);++i) {
        reset_mock((enum injection)i,warm!=0);tv4_bench_diagnostics d={0};
        assert(tv4_bench_hardware_init(&d)==expected[i]);assert(d.reset_flags==UINT32_C(0x20000000));
        assert(mock.reads<=TV4_BENCH_WAIT_LOOPS+100u && mock.writes<24 && d.motion_outputs==0);
        if(expected[i]==TV4_BENCH_OK) {
            assert(d.watchdog_started==1 && mock.starts==1 && mock.feeds==1 && d.debug_halt_freeze==1);
            assert(mock.updates==2 && mock.sync_reads==7);
            assert(d.hsi_hz==16000000 && d.rcc_cfgr==0 && (d.rcc_cr&UINT32_C(0x0fdf0000))==0);
            assert(d.rcc_csr==3 && mock.debug==(UINT32_C(1)<<5 | UINT32_C(1)<<12));
            tv4_bench_tick_start();assert(mock.ctrl==7 && mock.load==15999 && mock.val==0);
            tv4_bench_watchdog_feed();assert(mock.feeds==2);
        }else {
            bool started=i>IWDG_INITIAL_PENDING;
            assert(d.watchdog_started==(started?1u:0u) && mock.starts==(started?1u:0u) && mock.feeds==0);
            assert((mock.csr&UINT32_C(0x20000000))!=0); /* no premature reset-flag clear */
            if(i<=3)assert(mock.writes==0);
        }
        ++cases;
    }
    printf("PASS%u actual-source MMIO cases: cold/debug-PLL entry, pending IWDG sync requires start, independent PVU/RVU timeout/readback, active watchdog on init fault/no feed, bounded waits, HSI-only writes/no motion registers\n",cases);
}
static void diagnostic_lifetime(void)
{
    tv4_bench_record r={0};tv4_bench_diagnostics d;assert(!tv4_bench_record_valid(NULL) && !tv4_bench_record_valid(&r));
    tv4_bench_diag_begin(&d,&r);assert(d.magic==TV4_BENCH_DIAG_MAGIC && d.bytes==168 && d.boot_count==1 && d.history_valid==0);
    assert(tv4_bench_record_valid(&r) && r.reason==0 && d.motion_outputs==0 && d.request==0);
    tv4_bench_fault f={.exception=6,.exc_return=UINT32_C(0xfffffff9),.pc=UINT32_C(0x08000101),.frame_valid=1};
    tv4_bench_record_save(&r,1,TV4_BENCH_EXCEPTION,&f);assert(tv4_bench_record_valid(&r));
    unsigned mutations=0;unsigned char *raw=(unsigned char *)&r;
    for(size_t i=0;i<sizeof(r);++i)for(unsigned bit=0;bit<8;++bit) {
        raw[i]^=(unsigned char)(1u<<bit);assert(!tv4_bench_record_valid(&r));
        raw[i]^=(unsigned char)(1u<<bit);++mutations;
    }
    tv4_bench_diag_begin(&d,&r);assert(d.boot_count==2 && d.history_valid==1 && d.previous_reason==TV4_BENCH_EXCEPTION && d.previous_fault.pc==f.pc);
    assert(tv4_bench_record_valid(&r) && r.boot_count==2 && r.reason==0);
    r.magic=0;tv4_bench_diag_begin(&d,&r);assert(d.history_valid==0 && d.boot_count==1 && d.previous_reason==0);
    tv4_bench_record_save(&r,UINT32_MAX,TV4_BENCH_TEST_WATCHDOG,NULL);tv4_bench_diag_begin(&d,&r);
    assert(d.boot_count==UINT32_MAX && d.previous_reason==TV4_BENCH_TEST_WATCHDOG && tv4_bench_record_valid(&r));
    tv4_bench_record_save(&r,0,0,NULL);assert(!tv4_bench_record_valid(&r));
    printf("PASS diagnostic warm-reset/fault/torn-record/boot saturation +%u independent single-bit CRC refusals; no durable identity/physical authority\n",mutations);
}
static void frame_and_watchdog_guards(void)
{
    uint32_t bottom=UINT32_C(0x2001f000),top=UINT32_C(0x20020000),core=0;
    const uint32_t exc[]={UINT32_C(0xfffffff1),UINT32_C(0xfffffff9),UINT32_C(0xffffffe1),UINT32_C(0xffffffe9)};
    for(unsigned i=0;i<4;++i) {
        uint32_t extra=(exc[i]&16u)!=0?0u:72u;
        assert(tv4_bench_frame_span(bottom,exc[i],0,bottom,top,&core) && core==bottom+extra);
        assert(tv4_bench_frame_span(top-extra-32u,exc[i],0,bottom,top,&core));
        assert(!tv4_bench_frame_span(top-extra-28u,exc[i],0,bottom,top,&core) && core==0);
    }
    const uint32_t bad_sp[]={0,UINT32_MAX,bottom-4u,bottom+1u,top+4u};
    for(unsigned i=0;i<sizeof(bad_sp)/sizeof(bad_sp[0]);++i)assert(!tv4_bench_frame_span(bad_sp[i],exc[0],0,bottom,top,&core) && core==0);
    for(unsigned bit=0;bit<32;++bit)if((UINT32_C(0x3838)&(1u<<bit))!=0)
        assert(!tv4_bench_frame_span(bottom,exc[0],1u<<bit,bottom,top,&core) && core==0);
    assert(!tv4_bench_frame_span(bottom,UINT32_C(0xfffffffd),0,bottom,top,&core)); /* PSP unowned */
    assert(!tv4_bench_frame_span(bottom,0,0,bottom,top,&core) && core==0);
    assert(!tv4_bench_frame_span(bottom,exc[0],0,top,bottom,&core) && core==0);
    assert(!tv4_bench_frame_span(bottom,exc[0],0,bottom,top,NULL));
    uint32_t last=0;for(unsigned i=0;i<100;++i)assert(!tv4_bench_feed_due(i,&last,TV4_BENCH_RUNNING,true) && last==0);
    assert(tv4_bench_feed_due(100,&last,TV4_BENCH_RUNNING,true) && last==100);
    for(unsigned state=TV4_BENCH_BOOT;state<=TV4_BENCH_WATCHDOG_TEST;++state)if(state!=TV4_BENCH_RUNNING)
        assert(!tv4_bench_feed_due(200,&last,(tv4_bench_state)state,true) && last==100);
    assert(!tv4_bench_feed_due(200,&last,TV4_BENCH_RUNNING,false) && last==100);
    assert(!tv4_bench_feed_due(99,&last,TV4_BENCH_RUNNING,true) && last==100);
    last=UINT32_MAX-20u;assert(tv4_bench_feed_due(79,&last,TV4_BENCH_RUNNING,true) && last==79);
    assert(!tv4_bench_feed_due(200,NULL,TV4_BENCH_RUNNING,true));
    puts("PASS valid basic/extended MSP frames; invalid/stack-fault/out-of-range/PSP never dereferenced; main-progress+guard gated watchdog feed/wrap/fault no-feed");
}
static void runtime_fill(void)
{
    for(unsigned n=0;n<=16;++n)for(int value=-1;value<=255;++value) {
        unsigned char a[20],b[20];memset(a,0x55,sizeof(a));memset(b,0x55,sizeof(b));
        assert(tv4_bench_memset(a+2,value,n)==a+2);memset(b+2,value,n);assert(memcmp(a,b,sizeof(a))==0);
    }
    assert(tv4_bench_memset(NULL,0,0)==NULL);
    puts("PASS freestanding compiler memset primitive vs host oracle, guards/zero length/all-byte values; no libc/heap needed by ARM image");
}
int main(void)
{hardware_paths();diagnostic_lifetime();frame_and_watchdog_guards();runtime_fill();return 0;}
