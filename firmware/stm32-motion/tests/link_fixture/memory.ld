/* SYNTHETIC TEST ONLY. These are NOT the STM32F407 memory map or slot sizes.
 * No board memory/Flash sector/bootloader decision may derive from this file. */
MEMORY {
    ROM (rx) : ORIGIN = 0x01000000, LENGTH = 64K
    RAM (rw) : ORIGIN = 0x30000000, LENGTH = 16K
}
__stack_bytes = 2048;
