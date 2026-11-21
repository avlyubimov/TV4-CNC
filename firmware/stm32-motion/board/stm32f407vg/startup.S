/* Authored integer-only reset/fault entry for this explicit bench profile.
 * Privileged hardware-reset/MSP entry ONLY; not a live bootloader jump ABI. */
.syntax unified
.cpu cortex-m4
.thumb
.section .text.Reset_Handler,"ax",%progbits
.global Reset_Handler
.type Reset_Handler,%function
.thumb_func
Reset_Handler:
    cpsid i
    ldr r0,=__stack_top__
    mov sp,r0
    movs r0,#0
    msr control,r0
    isb
    bl tv4_bench_entry
    b tv4_bench_halt
.size Reset_Handler,.-Reset_Handler
.ltorg

.section .text.tv4_bench_fault_entry,"ax",%progbits
.global tv4_bench_fault_entry
.type tv4_bench_fault_entry,%function
.thumb_func
tv4_bench_fault_entry:
    cpsid i
    mov r1,lr
    mrs r2,ipsr
    tst lr,#4
    ite eq
    mrseq r0,msp
    mrsne r0,psp
    ldr r3,=__fault_stack_top__
    mov sp,r3
    bl tv4_bench_fault_capture
    b tv4_bench_halt
.size tv4_bench_fault_entry,.-tv4_bench_fault_entry
.ltorg

.section .text.tv4_bench_halt,"ax",%progbits
.global tv4_bench_halt
.type tv4_bench_halt,%function
.thumb_func
tv4_bench_halt:
    cpsid i
1:  wfi
    b 1b
.size tv4_bench_halt,.-tv4_bench_halt
.section .note.GNU-stack,"",%progbits
