#ifndef TV4_STM32F407_BENCH_H
#define TV4_STM32F407_BENCH_H
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Private to the first pinless bench image and its actual-source host tests.
 * No motion/CAN/Flash writer/bootloader/physical inhibit authority. */
#define TV4_BENCH_HSI_HZ UINT32_C(16000000)
#define TV4_BENCH_TICK_RELOAD UINT32_C(15999)
#define TV4_BENCH_IWDG_PR UINT32_C(4) /* /64 */
#define TV4_BENCH_IWDG_RLR UINT32_C(1023)
#define TV4_BENCH_FEED_MS UINT32_C(100)
#define TV4_BENCH_WAIT_LOOPS UINT32_C(100000)
#define TV4_BENCH_DIAG_MAGIC UINT32_C(0x54563444)
#define TV4_BENCH_RECORD_MAGIC UINT32_C(0x54563452)
#define TV4_BENCH_ABI UINT32_C(1)
#define TV4_BENCH_TEST_WATCHDOG UINT32_C(0x57444731)
#define TV4_BENCH_TEST_FAULT UINT32_C(0x464c5431)

typedef enum {
    TV4_BENCH_BOOT=1,TV4_BENCH_CLOCK,TV4_BENCH_WATCHDOG,TV4_BENCH_RUNNING,
    TV4_BENCH_FAULT,TV4_BENCH_WATCHDOG_TEST
} tv4_bench_state;
typedef enum {
    TV4_BENCH_OK,TV4_BENCH_BAD_CORE,TV4_BENCH_BAD_DEVICE,TV4_BENCH_BAD_FLASH,
    TV4_BENCH_HSI_TIMEOUT,TV4_BENCH_CLOCK_SWITCH_TIMEOUT,TV4_BENCH_CLOCK_STOP_TIMEOUT,
    TV4_BENCH_LSI_TIMEOUT,TV4_BENCH_IWDG_TIMEOUT,TV4_BENCH_IWDG_READBACK,
    TV4_BENCH_MEMORY,TV4_BENCH_EXCEPTION,TV4_BENCH_STACK
} tv4_bench_error;
typedef struct {
    uint32_t exception,exc_return,stack_pointer,frame_valid;
    uint32_t r0,r1,r2,r3,r12,lr,pc,xpsr;
    uint32_t cfsr,hfsr,mmfar,bfar;
} tv4_bench_fault;
typedef struct {
    uint32_t magic,abi,boot_count,reason;
    tv4_bench_fault fault;
    uint32_t crc;
} tv4_bench_record;
typedef struct {
    uint32_t magic,abi,bytes,state,error,boot_count,reset_flags;
    uint32_t core_id,device_id,flash_kib,hsi_hz,ticks_ms,heartbeat,watchdog_feeds;
    uint32_t rcc_cr,rcc_cfgr,rcc_csr,watchdog_started,debug_halt_freeze,motion_outputs;
    uint32_t data_bss_pass,ccm_probe_pass,stack_guards_pass,history_valid,previous_reason;
    tv4_bench_fault previous_fault;
    uint32_t request; /* Only local, separately authorized SWD bench tests. */
} tv4_bench_diagnostics;
typedef struct {
    uint32_t magic,abi,hsi_hz,flash_kib,dirty;
    char source_commit[41],profile[40];
} tv4_bench_build_identity;

extern volatile tv4_bench_diagnostics tv4_bench_diag;
extern volatile tv4_bench_record tv4_bench_history;
extern const tv4_bench_build_identity tv4_bench_build;
extern uint32_t SystemCoreClock;
void *tv4_bench_memset(void *destination,int value,size_t bytes);

bool tv4_bench_record_valid(const volatile tv4_bench_record *r);
void tv4_bench_record_save(volatile tv4_bench_record *r,uint32_t boots,uint32_t reason,
    const tv4_bench_fault *fault);
void tv4_bench_diag_begin(volatile tv4_bench_diagnostics *d,volatile tv4_bench_record *r);
bool tv4_bench_frame_span(uint32_t sp,uint32_t exc_return,uint32_t cfsr,
    uint32_t stack_bottom,uint32_t stack_top,uint32_t *core_frame);
bool tv4_bench_feed_due(uint32_t now,uint32_t *last,tv4_bench_state state,bool guards_ok);
tv4_bench_error tv4_bench_hardware_init(volatile tv4_bench_diagnostics *d);
void tv4_bench_tick_start(void);
void tv4_bench_watchdog_feed(void);
#endif
