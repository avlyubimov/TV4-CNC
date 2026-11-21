/* Vector layout derived from STMicroelectronics cmsis-device-f4 v2.6.11.
 * Copyright (c) 2017 STMicroelectronics. Apache-2.0; see
 * third_party/stm32f4-reference/LICENSE.md. Authored handler routing below.
 * Full F407:98 words; core reserved entries and IRQ79 remain zero. */
.syntax unified
.cpu cortex-m4
.thumb
.section .isr_vector,"a",%progbits
.balign 512
.global tv4_bench_vectors
.type tv4_bench_vectors,%object
tv4_bench_vectors:
    .word __stack_top__ /* 0: _estack */
    .word Reset_Handler /* 1: Reset_Handler */
    .word tv4_bench_fault_entry /* 2: NMI_Handler */
    .word tv4_bench_fault_entry /* 3: HardFault_Handler */
    .word tv4_bench_fault_entry /* 4: MemManage_Handler */
    .word tv4_bench_fault_entry /* 5: BusFault_Handler */
    .word tv4_bench_fault_entry /* 6: UsageFault_Handler */
    .word 0 /* 7: 0 */
    .word 0 /* 8: 0 */
    .word 0 /* 9: 0 */
    .word 0 /* 10: 0 */
    .word tv4_bench_fault_entry /* 11: SVC_Handler */
    .word tv4_bench_fault_entry /* 12: DebugMon_Handler */
    .word 0 /* 13: 0 */
    .word tv4_bench_fault_entry /* 14: PendSV_Handler */
    .word SysTick_Handler /* 15: SysTick_Handler */
    .word tv4_bench_fault_entry /* 16: WWDG_IRQHandler */
    .word tv4_bench_fault_entry /* 17: PVD_IRQHandler */
    .word tv4_bench_fault_entry /* 18: TAMP_STAMP_IRQHandler */
    .word tv4_bench_fault_entry /* 19: RTC_WKUP_IRQHandler */
    .word tv4_bench_fault_entry /* 20: FLASH_IRQHandler */
    .word tv4_bench_fault_entry /* 21: RCC_IRQHandler */
    .word tv4_bench_fault_entry /* 22: EXTI0_IRQHandler */
    .word tv4_bench_fault_entry /* 23: EXTI1_IRQHandler */
    .word tv4_bench_fault_entry /* 24: EXTI2_IRQHandler */
    .word tv4_bench_fault_entry /* 25: EXTI3_IRQHandler */
    .word tv4_bench_fault_entry /* 26: EXTI4_IRQHandler */
    .word tv4_bench_fault_entry /* 27: DMA1_Stream0_IRQHandler */
    .word tv4_bench_fault_entry /* 28: DMA1_Stream1_IRQHandler */
    .word tv4_bench_fault_entry /* 29: DMA1_Stream2_IRQHandler */
    .word tv4_bench_fault_entry /* 30: DMA1_Stream3_IRQHandler */
    .word tv4_bench_fault_entry /* 31: DMA1_Stream4_IRQHandler */
    .word tv4_bench_fault_entry /* 32: DMA1_Stream5_IRQHandler */
    .word tv4_bench_fault_entry /* 33: DMA1_Stream6_IRQHandler */
    .word tv4_bench_fault_entry /* 34: ADC_IRQHandler */
    .word tv4_bench_fault_entry /* 35: CAN1_TX_IRQHandler */
    .word tv4_bench_fault_entry /* 36: CAN1_RX0_IRQHandler */
    .word tv4_bench_fault_entry /* 37: CAN1_RX1_IRQHandler */
    .word tv4_bench_fault_entry /* 38: CAN1_SCE_IRQHandler */
    .word tv4_bench_fault_entry /* 39: EXTI9_5_IRQHandler */
    .word tv4_bench_fault_entry /* 40: TIM1_BRK_TIM9_IRQHandler */
    .word tv4_bench_fault_entry /* 41: TIM1_UP_TIM10_IRQHandler */
    .word tv4_bench_fault_entry /* 42: TIM1_TRG_COM_TIM11_IRQHandler */
    .word tv4_bench_fault_entry /* 43: TIM1_CC_IRQHandler */
    .word tv4_bench_fault_entry /* 44: TIM2_IRQHandler */
    .word tv4_bench_fault_entry /* 45: TIM3_IRQHandler */
    .word tv4_bench_fault_entry /* 46: TIM4_IRQHandler */
    .word tv4_bench_fault_entry /* 47: I2C1_EV_IRQHandler */
    .word tv4_bench_fault_entry /* 48: I2C1_ER_IRQHandler */
    .word tv4_bench_fault_entry /* 49: I2C2_EV_IRQHandler */
    .word tv4_bench_fault_entry /* 50: I2C2_ER_IRQHandler */
    .word tv4_bench_fault_entry /* 51: SPI1_IRQHandler */
    .word tv4_bench_fault_entry /* 52: SPI2_IRQHandler */
    .word tv4_bench_fault_entry /* 53: USART1_IRQHandler */
    .word tv4_bench_fault_entry /* 54: USART2_IRQHandler */
    .word tv4_bench_fault_entry /* 55: USART3_IRQHandler */
    .word tv4_bench_fault_entry /* 56: EXTI15_10_IRQHandler */
    .word tv4_bench_fault_entry /* 57: RTC_Alarm_IRQHandler */
    .word tv4_bench_fault_entry /* 58: OTG_FS_WKUP_IRQHandler */
    .word tv4_bench_fault_entry /* 59: TIM8_BRK_TIM12_IRQHandler */
    .word tv4_bench_fault_entry /* 60: TIM8_UP_TIM13_IRQHandler */
    .word tv4_bench_fault_entry /* 61: TIM8_TRG_COM_TIM14_IRQHandler */
    .word tv4_bench_fault_entry /* 62: TIM8_CC_IRQHandler */
    .word tv4_bench_fault_entry /* 63: DMA1_Stream7_IRQHandler */
    .word tv4_bench_fault_entry /* 64: FSMC_IRQHandler */
    .word tv4_bench_fault_entry /* 65: SDIO_IRQHandler */
    .word tv4_bench_fault_entry /* 66: TIM5_IRQHandler */
    .word tv4_bench_fault_entry /* 67: SPI3_IRQHandler */
    .word tv4_bench_fault_entry /* 68: UART4_IRQHandler */
    .word tv4_bench_fault_entry /* 69: UART5_IRQHandler */
    .word tv4_bench_fault_entry /* 70: TIM6_DAC_IRQHandler */
    .word tv4_bench_fault_entry /* 71: TIM7_IRQHandler */
    .word tv4_bench_fault_entry /* 72: DMA2_Stream0_IRQHandler */
    .word tv4_bench_fault_entry /* 73: DMA2_Stream1_IRQHandler */
    .word tv4_bench_fault_entry /* 74: DMA2_Stream2_IRQHandler */
    .word tv4_bench_fault_entry /* 75: DMA2_Stream3_IRQHandler */
    .word tv4_bench_fault_entry /* 76: DMA2_Stream4_IRQHandler */
    .word tv4_bench_fault_entry /* 77: ETH_IRQHandler */
    .word tv4_bench_fault_entry /* 78: ETH_WKUP_IRQHandler */
    .word tv4_bench_fault_entry /* 79: CAN2_TX_IRQHandler */
    .word tv4_bench_fault_entry /* 80: CAN2_RX0_IRQHandler */
    .word tv4_bench_fault_entry /* 81: CAN2_RX1_IRQHandler */
    .word tv4_bench_fault_entry /* 82: CAN2_SCE_IRQHandler */
    .word tv4_bench_fault_entry /* 83: OTG_FS_IRQHandler */
    .word tv4_bench_fault_entry /* 84: DMA2_Stream5_IRQHandler */
    .word tv4_bench_fault_entry /* 85: DMA2_Stream6_IRQHandler */
    .word tv4_bench_fault_entry /* 86: DMA2_Stream7_IRQHandler */
    .word tv4_bench_fault_entry /* 87: USART6_IRQHandler */
    .word tv4_bench_fault_entry /* 88: I2C3_EV_IRQHandler */
    .word tv4_bench_fault_entry /* 89: I2C3_ER_IRQHandler */
    .word tv4_bench_fault_entry /* 90: OTG_HS_EP1_OUT_IRQHandler */
    .word tv4_bench_fault_entry /* 91: OTG_HS_EP1_IN_IRQHandler */
    .word tv4_bench_fault_entry /* 92: OTG_HS_WKUP_IRQHandler */
    .word tv4_bench_fault_entry /* 93: OTG_HS_IRQHandler */
    .word tv4_bench_fault_entry /* 94: DCMI_IRQHandler */
    .word 0 /* 95: 0 */
    .word tv4_bench_fault_entry /* 96: HASH_RNG_IRQHandler */
    .word tv4_bench_fault_entry /* 97: FPU_IRQHandler */
.size tv4_bench_vectors,.-tv4_bench_vectors
.section .note.GNU-stack,"",%progbits
