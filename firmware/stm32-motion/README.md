# STM32F407 Motion — пока документация

Целевая плата DevEBox STM32F4VX_M Ver. 2.0 получена по сообщению владельца.
Точная маркировка MCU, Flash/RAM/секторы, кварц, pinout и SWD именно этой платы
не подтверждены. USB VID/PID 0483:5720 сам по себе не определяет MCU или память.
Сейчас здесь нет startup, linker script, target binary или загрузчика.

Следующий PR-02 добавит host-testable foundation и toolchain/CI; target части
гейтируются подтверждёнными данными, не старым предположением VGT6.
Первое ПО — без активации приводов. STEP/DIR будет timer/DMA/IRQ, не RTOS delay.

Правила: [AGENTS.md](AGENTS.md). Статус: [HARDWARE_STATUS](../../docs/HARDWARE_STATUS.md).
План: [ROADMAP](../../docs/ROADMAP.md). Независимый CAN bootloader и recovery:
[OTA_ARCHITECTURE](../../docs/OTA_ARCHITECTURE.md). Адреса/сектора пока не назначены.
