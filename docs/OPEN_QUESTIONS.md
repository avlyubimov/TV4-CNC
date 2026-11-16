# Open questions — do not assume

- Official Waveshare **Rev 1.2** schematic/BSP, LCD and touch driver revisions, and exact USB/CAN multiplexing.
- ESP32 flash size and safe factory firmware backup/restore procedure.
- STM32F407 development board pinout, oscillator and CAN transceiver presence.
- Purchased closed-loop NEMA23 exact driver model, current requirements, encoder resolution and alarm logic.
- Machine travel, ballscrew dimensions, limits, mechanical disengagement and emergency power circuit.
- CAN message allocation, timeouts and heartbeat intervals (to be specified and tested).
- Spindle servo and direct spindle encoder mounting dimensions.
- Actual performance metrics on target board.

## Проверка M1
См. BOARD_VERIFICATION.md: найдены официальная схема и таблица линий семейства;
совместимость BSP LVGL 9 именно Rev 1.2 ещё не подтверждена. Обнаружено расхождение
8/16 MB flash. Реальный CAN ID не назначен; heartbeat 500 мс относится только к тесту.

## Обновление ESP-IDF
- Найден официальный LVGL 9 пример commit bc51a84b30c0cea018fee6e0d7427ec10102060e.
- Линии RGB/CH422G сверены со схемой V1.2; добавлен адаптер с USB_SEL=0.
- ESP-IDF 5.3.2 + LVGL 9.2.2 + GT911 1.1.3 разрешены сборщиком, lock получен.
- Требуются реальные flash_id/PSRAM, фото маркировки, backup заводской flash,
  проверка дисплея/ориентации касания и измерения производительности.
- Антитиринг, homing/лимиты и CAN transport пока не реализованы.
