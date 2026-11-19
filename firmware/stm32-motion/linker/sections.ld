/* Sections contract ONLY. No MEMORY/default board addresses/bootloader reserve.
 * A future reviewed board profile must supply ROM/RAM, stack and full vectors.
 * The sole current caller is a synthetic link-test fixture, not STM32 firmware. */
ENTRY(Reset_Handler)
SECTIONS
{
    .isr_vector ORIGIN(ROM) : {
        __vectors_start__ = .;
        KEEP(*(.isr_vector))
        __vectors_end__ = .;
    } > ROM
    .text : {
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
        __text_end__ = .;
    } > ROM
    .ARM.extab : { *(.ARM.extab*) } > ROM
    .ARM.exidx : { *(.ARM.exidx*) } > ROM
    .unsupported_glue : { *(.glue_7 .glue_7t .vfp11_veneer .v4_bx) } > ROM
    .iplt : { *(.iplt) } > ROM
    .rel.iplt : { *(.rel.iplt) } > ROM
    .igot.plt : { *(.igot.plt) } > ROM
    .init_array : { KEEP(*(.init_array*)) KEEP(*(.fini_array*)) KEEP(*(.preinit_array*)) } > ROM
    .data : {
        . = ALIGN(4);
        __data_start__ = .;
        *(.data .data.*)
        . = ALIGN(4);
        __data_end__ = .;
    } > RAM AT > ROM
    __data_load__ = LOADADDR(.data);
    __rom_end__ = LOADADDR(.data) + SIZEOF(.data);
    .bss (NOLOAD) : {
        . = ALIGN(4);
        __bss_start__ = .;
        *(.bss .bss.*) *(COMMON)
        . = ALIGN(4);
        __bss_end__ = .;
    } > RAM
    __stack_top__ = ORIGIN(RAM) + LENGTH(RAM);
    __stack_bottom__ = __stack_top__ - __stack_bytes;
    .ARM.attributes 0 : { *(.ARM.attributes) }
    /DISCARD/ : { *(.comment) *(.note*) }
}
ASSERT(DEFINED(__stack_bytes) && __stack_bytes > 0, "explicit nonzero stack required")
ASSERT((ORIGIN(ROM) % 128) == 0 && SIZEOF(.isr_vector) >= 64, "core vector alignment/size")
ASSERT((__stack_top__ % 8) == 0 && (__stack_bottom__ % 8) == 0, "stack alignment")
ASSERT(__stack_bottom__ >= ORIGIN(RAM), "stack reserve exceeds RAM")
ASSERT(__bss_end__ <= __stack_bottom__, "RAM data/BSS collide with stack")
ASSERT(__rom_end__ <= ORIGIN(ROM) + LENGTH(ROM), "ROM load image overflow")
ASSERT(ORIGIN(ROM) + LENGTH(ROM) <= ORIGIN(RAM) ||
       ORIGIN(RAM) + LENGTH(RAM) <= ORIGIN(ROM), "ROM/RAM overlap")
ASSERT((__data_load__ % 4) == 0, "data load alignment")
ASSERT(SIZEOF(.init_array) == 0, "constructors/destructors unsupported by minimal startup")
ASSERT(SIZEOF(.unsupported_glue) + SIZEOF(.iplt) + SIZEOF(.rel.iplt) + SIZEOF(.igot.plt) == 0,
       "interworking/dynamic-link stubs unsupported by minimal startup")
