# Реализация: main, открытые PR и аппаратные границы

Снимок PR-01 от 18 ноября 2026. База: main
`323ab364bde301582f1b9ec8c8bc14e53396cc0c`. Это не утверждение, что все
коммиты рабочей ветки уже слиты. Отчёт: [PR01_REVIEW.md](PR01_REVIEW.md).

## Есть в main

- ESP-IDF 6.1.1 и резерв 5.3.2; headless/rev12, Flash 16 МБ, factory app
  2 МиБ, LVGL 9.2.2, esp_lcd_touch 1.2.1, GT911 1.1.3, точные per-SDK locks.
- Русский simulation-only UI 800×480, ограниченная очередь, модель X/Z,
  инертный transport_send и 4-byte host heartbeat с тайм-аутом 500 мс.
- Прямой RGB/GT911/CH422G адаптер с USB_SEL=0, один framebuffer в PSRAM,
  startup screen/проверка touch, отдельный polling и единственный LVGL owner.
- Главная, библиотека четырёх инструментов с черновиком/валидацией/клавиатурой,
  Диагностика и журнал RAM 32 записи; коррекции инструментов не управляют осью.
- Виртуальные обороты и ограничение подачи, расчёт мм/об, STOP/fault/ACK и
  отдельный явный restart simulation; никакой физической передачи команд.
- Wi-Fi worker, список/профили восьми сетей в NVS, автоподключение/забывание.
  Код есть, но успешное подключение/сохранение на плате ещё не подтверждены.
- Host tests (build validation, core, Wi-Fi profiles, render, RGB565), sanitizers,
  CI docs/host/SDK matrix, runtime health logs и базовые UI метрики.

[Сборка](../firmware/esp32-hmi/README.md), [Wi-Fi](WIFI.md),
[загрузка](BOOT_SCREEN.md), [инструменты](SOFTWARE_STAGE_003.md),
[метрики и пробелы](RUNTIME_DIAGNOSTICS.md). 36 °C/24.1 В и часы/дата в UI —
примерные/фиксированные подписи, не аппаратные датчики или подтверждённое время.

## Отдельно открыт PR #5

[Feat/hmi runtime diagnostics](https://github.com/avlyubimov/TV4-CNC/pull/5), head
`b92c559c304f318e4a76c37f438717af2fac4124`: рабочий/машинный отсчёт с подтверждением
нуля детали, сохранение feed override при выборе шага, исправление возможного
источника 263, крупный password editor/show-hide и текстовые ошибки Wi-Fi.
По снимку Actions зелёные, но PR ещё открыт. PR-01 не копирует эти изменения
и не считает их частью main. Его документы здесь не дублируются; детали — в PR #5.
Merge обоих PR может потребовать согласовать пересекающиеся документационные
правки; исполняемый код и hardware settings в PR-01 неизменны.

## Не реализовано

STM32 target firmware/startup/linker/planner/step/encoder/bootloader; physical
CAN driver/negotiation/production wire protocol; microSD/FAT32/file manager;
GitHub release/signing pipeline; ESP32 A/B/HTTPS installer/health rollback;
F407 CAN update; единый Updates UI. Экран «Программа», непрерывный JOG,
реальные homing/limits и обработка станка тоже не готовы.
Все они запланированы в [ROADMAP.md](ROADMAP.md), а не объявлены работающими
из-за названия каталога или наличия CI сборки ESP32.

## Аппаратные результаты и ограничения

Полный ESP32 backup 16 МиБ с SHA-256 проверен; restore не испытан.
Владелец выполнил запись HMI (esptool hashes verified) и прислал фото физического
изображения/списка Wi-Fi. Пользователь сообщил ошибку 263 и неудобную клавиатуру.
Это подтверждает наблюдавшееся изображение и взаимодействие, но не полный
протокол испытаний GT911, успешный DHCP/reconnect/NVS, FPS/latency или стабильность.
Новые исправления #5 на плате после последней проверки не подтверждены.

DevEBox F407 получена, точная геометрия неизвестна; NEMA23 не получен,
трансиверов нет. Blue Pill/SWD/backup сообщены владельцем, а не испытаны агентом
в PR-01. Полная таблица: [HARDWARE_STATUS.md](HARDWARE_STATUS.md).
Условные ±1000 мм симуляции не являются ходом или soft limits станка.

## Откат и границы PR-01

PR-01 меняет только Markdown-документы/AGENTS. Откат — revert документационного
коммита, без firmware rollback или записи Flash. partitions.csv, sdkconfig,
C/C++, build scripts и workflow YAML не меняются. Бэкапы и окружения исключены
из Git. Испытания/прошивка, публикация релиза и переход к PR-02 требуют
отдельного разрешения/проверки владельца после отчёта PR-01.
