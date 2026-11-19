/* Core-only synthetic vectors. F407 external IRQs intentionally NOT supplied.
 * This is not a board vector table and must never become a deployable artifact. */
.syntax unified
.cpu cortex-m4
.thumb
.section .isr_vector,"a",%progbits
.word __stack_top__, Reset_Handler
.word tv4_startup_halt, tv4_startup_halt, tv4_startup_halt
.word tv4_startup_halt, tv4_startup_halt
.word 0, 0, 0, 0
.word tv4_startup_halt, tv4_startup_halt, 0
.word tv4_startup_halt, tv4_startup_halt
.section .note.GNU-stack,"",%progbits
