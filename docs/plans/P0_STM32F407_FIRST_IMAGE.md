# P0 — первая прошиваемая STM32F407VG HSI bench сборка

База main `610f1745d892d5904ea183b50617a20d8ce755bb`, PR #1–55 слиты,
main CI5/5 PASS; открытых PR перед работой нет. Это одна интеграция P0
по IMPLEMENTATION_PRIORITIES, не ещё одна библиотека/модель транспорта.

1. Проверить официальные ST DS8626 Rev12, RM0090 Rev22, PM0214 Rev10 и
   startup STM32F407xx из cmsis-device-f4 v2.6.11/commit0fa0e489e053fa1ca7790bb40b4d76458f64c55d.
   Зафиксировать карту Flash1MiB, SRAM112+16KiB, CCM64KiB,12 Flash sectors,
   98 vectors/512-byte alignment/reserved IRQ79, нужные RCC/SysTick/IWDG/DBG регистры.
   Это документированная модель заявленного VGT6; физических новых измерений нет.
2. Добавить явный opt-in ARM board profile и настоящий standalone ELF/BIN/map.
   HSI16MHz, no FPU instructions/heap/libc/HAL/RTOS/CAN/motion GPIO. Original
   synthetic probe/default libraries/unsupported-profile refusals сохраняются.
   Bench budget16KiB Flash, SRAM main stack4KiB + emergency fault stack1KiB;
   это бюджет этого приложения, не резервирование будущего CAN bootloader.
3. Выполнить integer-only startup/data/BSS/CCM init, полную таблицу векторов,
   bounded clock/watchdog setup, main-only IWDG feed, fault capture на отдельном
   stack без опасного чтения повреждённого exception frame. RAM/SWD diagnostics/
   heartbeat/reset reason + checksum-guarded warm-reset history, без UART/LED.
4. Проверить тот же hardware setup на host MMIO model, ошибки/тайм-ауты/
   watchdog/diagnostic lifetime. Проверить реальный ELF/BIN/map/segments/vectors/
   stacks/no forbidden outputs/negative corruption/link failures и воспроизводимость.
   Добавить actual board build/проверки в существующий CI, точные provenance/
   dependencies/licenses и текстовое evidence; без binary release публикации.
5. Подготовить ST-Link install/verify/diagnostics/full-backup restore runbook.
   Явный OpenOCD profile отменяет vendor reset-init PLL boost; GPIO/trace pins,
   option bytes/unlock/mass erase не включаются. Документационные команды не
   исполнять без отдельного exact-image/операционного разрешения владельца.
6. Обновить статус/инструкции, сохранить настоящие локальные ELF/BIN/map/hash
   и отдельный PR. Не выполнять merge, release или аппаратные операции.

Приёмка: собираемый/программно проверенный образ готов к отдельно разрешённому
изолированному стенду. Фактические boot/HSI/RAM/CCM/watchdog/SWD diagnostics
не объявляются измеренными. Physical inhibit/CAN/motion, full marking/clock/
protection/backup SHA/copy/restore остаются аппаратными gates. Нет SWD/serial/
USB/device/reset/readback/Flash/erase/backup reads/eFuse/option-byte действий.
