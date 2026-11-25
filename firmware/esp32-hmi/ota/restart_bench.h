#pragma once
#include <stdbool.h>
/* Explicit diagnostic image only. No persistent writes or reset override. */
bool tv4_restart_bench_cold_boot(void);
bool tv4_restart_bench_prepare(void);
_Noreturn void tv4_restart_bench_execute(void);
