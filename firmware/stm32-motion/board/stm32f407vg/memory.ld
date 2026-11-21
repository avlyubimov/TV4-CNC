/* Standalone STM32F407VGT6 pinless bench, DS8626/RM0090, NOT CAN OTA layout. */
ENTRY(Reset_Handler)
MEMORY {
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 1024K
    SRAM (rw) : ORIGIN = 0x20000000, LENGTH = 128K
    CCM (rw) : ORIGIN = 0x10000000, LENGTH = 64K
}
__bench_flash_budget__ = 16K;
__main_stack_bytes__ = 4096;
__fault_stack_bytes__ = 1024;
__guard_bytes__ = 32;
__stack_top__ = ORIGIN(SRAM) + LENGTH(SRAM);
__stack_bottom__ = __stack_top__ - __main_stack_bytes__;
__main_guard_start__ = __stack_bottom__ - __guard_bytes__;
__fault_stack_top__ = __main_guard_start__;
__fault_stack_bottom__ = __fault_stack_top__ - __fault_stack_bytes__;
__fault_guard_start__ = __fault_stack_bottom__ - __guard_bytes__;
SECTIONS {
    .isr_vector ORIGIN(FLASH) : ALIGN(512) { KEEP(*(.isr_vector)) } > FLASH
    .text : ALIGN(4) { *(.text .text.*) } > FLASH
    .rodata : ALIGN(4) { KEEP(*(.bench_build)) *(.rodata .rodata.*) . = ALIGN(4); } > FLASH
    __text_end__ = .;
    .data : ALIGN(4) {
        __data_start__ = .; *(.data .data.*) . = ALIGN(4); __data_end__ = .;
    } > SRAM AT> FLASH
    __data_load__ = LOADADDR(.data);
    __image_end__ = LOADADDR(.data) + SIZEOF(.data);
    .bss (NOLOAD) : ALIGN(4) {
        __bss_start__ = .; *(.bss .bss.*) *(COMMON) . = ALIGN(4); __bss_end__ = .;
    } > SRAM
    .noinit (NOLOAD) : ALIGN(4) {
        __noinit_start__ = .; KEEP(*(.noinit.bench_history)) . = ALIGN(4); __noinit_end__ = .;
    } > SRAM
    .ccm_bss (NOLOAD) : ALIGN(4) {
        __ccm_start__ = .; KEEP(*(.ccm_bss)) . = ALIGN(4); __ccm_end__ = .;
    } > CCM
    .stacks __fault_guard_start__ (NOLOAD) : ALIGN(8) {
        . += __guard_bytes__ + __fault_stack_bytes__ + __guard_bytes__ + __main_stack_bytes__;
    } > SRAM
    /* Quarantine unsupported inputs without producing even an empty RWX LOAD.
     * KEEP makes unexpected constructors survive GC and fail the assertion. */
    .forbidden 0 (INFO) : { KEEP(*(.preinit_array* .init_array* .fini_array* .ctors* .dtors* .init .fini .ARM.exidx* .ARM.extab* .got* .iplt* .rel.iplt .igot.plt .glue_7 .glue_7t .stub .plt .vfp11_veneer .v4_bx)) }
    .ARM.attributes 0 : { *(.ARM.attributes) }
    .comment 0 : { *(.comment) }
    .debug_info 0 : { *(.debug_info .gnu.linkonce.wi.*) }
    .debug_abbrev 0 : { *(.debug_abbrev) }
    .debug_aranges 0 : { *(.debug_aranges) }
    .debug_ranges 0 : { *(.debug_ranges) }
    .debug_line 0 : { *(.debug_line) }
    .debug_str 0 : { *(.debug_str) }
    .debug_loc 0 : { *(.debug_loc) }
    .debug_macro 0 : { *(.debug_macro) }
    .debug_frame 0 : { *(.debug_frame) }
    /DISCARD/ : { *(.note.GNU-stack) }
}
ASSERT(ADDR(.isr_vector) == ORIGIN(FLASH) && ADDR(.isr_vector) % 512 == 0, "bench vector origin/alignment")
ASSERT(SIZEOF(.isr_vector) == 98 * 4, "full STM32F407 vector table required")
ASSERT(SIZEOF(.forbidden) == 0, "unsupported runtime/constructors/unwind")
ASSERT(__image_end__ <= ORIGIN(FLASH) + __bench_flash_budget__, "bench Flash budget exceeded")
ASSERT(__data_start__ % 4 == 0 && __data_end__ % 4 == 0 && __data_load__ % 4 == 0, "data alignment")
ASSERT(__bss_start__ % 4 == 0 && __bss_end__ % 4 == 0, "BSS alignment")
ASSERT(__data_end__ <= __bss_start__ && __bss_end__ <= __noinit_start__, "RAM sections overlap")
ASSERT(__noinit_end__ <= __fault_guard_start__, "SRAM data/stack overflow")
ASSERT(__fault_guard_start__ >= ORIGIN(SRAM) && __stack_top__ % 8 == 0 && __fault_stack_top__ % 8 == 0 && __fault_stack_bottom__ % 8 == 0 && __fault_guard_start__ % 8 == 0, "stack bounds/alignment")
ASSERT(ADDR(.stacks) + SIZEOF(.stacks) == __stack_top__, "stack placement")
ASSERT(__ccm_end__ <= ORIGIN(CCM) + LENGTH(CCM), "CCM overflow")
