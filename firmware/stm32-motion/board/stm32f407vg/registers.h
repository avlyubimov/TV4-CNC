#ifndef TV4_STM32F407_BENCH_REGISTERS_H
#define TV4_STM32F407_BENCH_REGISTERS_H
#include <stdint.h>
/* Authored register subset, reviewed against RM0090 Rev22 section7/21/38,
 * PM0214 Rev10 and ST stm32f407xx.h v2.6.11. No GPIO/timer/Flash write registers.
 * No HAL/CMSIS implementation linked. Host substitution is tests only. */
#define TV4_RCC_CR UINT32_C(0x40023800)
#define TV4_RCC_CFGR UINT32_C(0x40023808)
#define TV4_RCC_CIR UINT32_C(0x4002380c)
#define TV4_RCC_AHB1ENR UINT32_C(0x40023830)
#define TV4_RCC_CSR UINT32_C(0x40023874)
#define TV4_IWDG_KR UINT32_C(0x40003000)
#define TV4_IWDG_PR UINT32_C(0x40003004)
#define TV4_IWDG_RLR UINT32_C(0x40003008)
#define TV4_IWDG_SR UINT32_C(0x4000300c)
#define TV4_DBG_IDCODE UINT32_C(0xe0042000)
#define TV4_DBG_APB1_FZ UINT32_C(0xe0042008)
#define TV4_FLASH_KIB UINT32_C(0x1fff7a22)
#define TV4_SYSTICK_CTRL UINT32_C(0xe000e010)
#define TV4_SYSTICK_LOAD UINT32_C(0xe000e014)
#define TV4_SYSTICK_VAL UINT32_C(0xe000e018)
#define TV4_SCB_CPUID UINT32_C(0xe000ed00)
#define TV4_SCB_ICSR UINT32_C(0xe000ed04)
#define TV4_SCB_VTOR UINT32_C(0xe000ed08)
#define TV4_SCB_CCR UINT32_C(0xe000ed14)
#define TV4_SCB_SHCSR UINT32_C(0xe000ed24)
#define TV4_SCB_CFSR UINT32_C(0xe000ed28)
#define TV4_SCB_HFSR UINT32_C(0xe000ed2c)
#define TV4_SCB_MMFAR UINT32_C(0xe000ed34)
#define TV4_SCB_BFAR UINT32_C(0xe000ed38)
#define TV4_NVIC_ICER UINT32_C(0xe000e180)
#define TV4_NVIC_ICPR UINT32_C(0xe000e280)

#if defined(TV4_BENCH_TEST_MMIO)
uint32_t tv4_bench_test_read32(uint32_t address);
uint16_t tv4_bench_test_read16(uint32_t address);
void tv4_bench_test_write32(uint32_t address,uint32_t value);
#define tv4_bench_read32 tv4_bench_test_read32
#define tv4_bench_read16 tv4_bench_test_read16
#define tv4_bench_write32 tv4_bench_test_write32
#else
static inline uint32_t tv4_bench_read32(uint32_t a)
{return *(volatile const uint32_t *)(uintptr_t)a;}
static inline uint16_t tv4_bench_read16(uint32_t a)
{return *(volatile const uint16_t *)(uintptr_t)a;}
static inline void tv4_bench_write32(uint32_t a,uint32_t v)
{*(volatile uint32_t *)(uintptr_t)a=v;}
#endif
#endif
