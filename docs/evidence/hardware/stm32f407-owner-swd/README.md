# STM32F407 — результаты владельца

Источник: новое задание владельца после main `b19a40b` / слияния PR #27.
Дата внесения: 19 ноября2026. Дата и точные команды аппаратной проверки не
переданы. **Это owner-reported evidence; агент не выполнял SWD/read/reset/flash.**

| Сообщено владельцем | Результат |
|---|---|
| Debug access | SWD через ST-Link V2 работает |
| Core | Cortex-M4 r0p1 |
| Device ID (raw, без переинтерпретации) | 0x101f6413 |
| Flash capacity | 1024 КиБ = 1 048 576 байт |
| Напряжение | 3,252 В |
| Резервный дамп | 1 048 576 байт |
| Таблица векторов дампа | Структурно проверена владельцем |

Ранее владелец сообщил STM32F407VGT6 (suffix VGT6) и DevEBox STM32F4VX_M
Ver.2.0. Новое чтение не заменяет сверку физической маркировки/package/pinout.
Версии OpenOCD/другого debugger и ST-Link firmware нового сеанса неизвестны;
Blue Pill/OpenOCD0.12.0 не переносятся автоматически на F407.

## Что остаётся unknown

- HSE/кварц, фактический clock/PLL и тайминг; RAM/CCM availability/bounds.
- Физическая маркировка корпуса, board schematic/pinout/reset/boot routing.
- Sector/erase geometry, protection/option bytes и approved bootloader memory map.
- GPIO, CAN/transceiver/termination/bitrate/IDs, watchdog и физические выходы.
- Backup SHA-256, проверка повторным чтением, независимая копия и restore.
- Версия/работоспособность текущего приложения, новый startup и выполнение probe.

Структурная таблица векторов не устанавливает физический RAM extent, авторство
образа, успешный запуск или восстановление. Нельзя выбирать OTA sectors/app base
по размеру дампа, DEV_ID, suffix или предполагаемому верхнему SP. Core r0p1
не является silicon/package revision. Один voltage sample не является stress test.

Raw dump/UID/приватные пути не публикуются и в этом этапе не читались. Новый
доступ к устройству, запись/стирание/reset/option bytes/drive test требуют отдельного
разрешения. Итог аппаратного состояния — [HARDWARE_STATUS](../../../HARDWARE_STATUS.md).
