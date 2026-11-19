#include "memory_init.h"
#include "noio.h"
#include <stdint.h>
extern uint32_t __data_start__, __data_end__, __data_load__;
extern uint32_t __bss_start__, __bss_end__;
extern _Noreturn void tv4_startup_halt(void);
_Static_assert(sizeof(uintptr_t) == 4, "ARM32 link probe only");
_Noreturn void tv4_startup_entry(void)
{
    /* Linker assertions own ranges/word alignment, not inferred MCU RAM sizes. */
    size_t data_words = ((uintptr_t)&__data_end__ - (uintptr_t)&__data_start__) / 4u;
    size_t bss_words = ((uintptr_t)&__bss_end__ - (uintptr_t)&__bss_start__) / 4u;
    if (!tv4_startup_memory_init(&__data_start__, &__data_load__, data_words,
                                &__bss_start__, bss_words)) tv4_startup_halt();
    tv4_motion_platform platform = tv4_motion_noio_platform();
    (void)platform.inhibit_outputs(platform.context); /* unavailable, never ready */
    tv4_startup_halt(); /* No application/IRQs/clock/physical driver activation. */
}
