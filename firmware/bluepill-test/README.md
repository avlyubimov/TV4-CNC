# STM32F103 Blue Pill — отдельный стенд

Владелец подтвердил STM32F103C8T6, работающий ST-Link V2/SWD,
OpenOCD 0.12.0 (Cortex-M3, Flash 64 КиБ) и локальный backup со структурно
корректной таблицей векторов. Сам backup в Git не включается.
Это не доказательство восстановления или работы CAN/таймеров/bootloader.

Только bench-протокол, таймеры/IRQ и эксперименты с recovery. Контроллер станка
остаётся STM32F407. Сейчас target firmware здесь нет. Трансиверов пока нет:
до их получения host tests, без подключения приводов. GPIO/remap/USB conflicts,
CAN ID и bitrate определяются отдельным решением, а не этим README.

[Правила](AGENTS.md), [аппаратный статус](../../docs/HARDWARE_STATUS.md),
[план](../../docs/ROADMAP.md), [CAN-заглушка](../../protocol/can/README.md).
SWD/запись/erase требуют отдельного разрешения владельца.
