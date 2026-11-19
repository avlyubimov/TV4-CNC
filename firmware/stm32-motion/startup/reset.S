/* Authored Cortex-M4 reset building block. No board vector table or MMIO.
 * Requires privileged reset/MSP entry; arbitrary bootloader handoff is not ready.
 * Built/linked only by the opt-in synthetic probe, never a board image. */
.syntax unified
.cpu cortex-m4
.thumb
.section .text.Reset_Handler,"ax",%progbits
.global Reset_Handler
.type Reset_Handler,%function
.thumb_func
Reset_Handler:
    cpsid i
    ldr r0, =__stack_top__
    mov sp, r0
    bl tv4_startup_entry
    b tv4_startup_halt
.size Reset_Handler, .-Reset_Handler
.ltorg

.section .text.tv4_startup_halt,"ax",%progbits
.global tv4_startup_halt
.type tv4_startup_halt,%function
.thumb_func
tv4_startup_halt:
    cpsid i
1:  b 1b
.size tv4_startup_halt, .-tv4_startup_halt
.section .note.GNU-stack,"",%progbits
