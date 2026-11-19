# Следующий этап: STM32F407 startup и board contract

База: mainb19a40b, PR #1–27 слиты, четыре workflow main PASS. ESP32 HMI,
Wi-Fi/LVGL/SDK matrix, read-only OTA и CI candidates уже существуют; заново
реализовывать их вместо STM32 foundation не нужно. План ниже не разрешает
доступ к устройству или прошивку.

## Принятые аппаратные результаты

[Владелец подтвердил](evidence/hardware/stm32f407-owner-swd/README.md):
ST-Link V2/SWD, Cortex-M4r0p1, raw device ID0x101f6413, Flash1024KiB,
напряжение3,252V, backup1048576bytes и structural vector check PASS.
Агент их не перечитывал. Путь/дамп/непереданный SHA не публикуются.

## Текущая программная часть PR-02b

[Контракт](STM32_STARTUP.md): portable data/BSS init и no-I/O platform,
Cortex-M4 reset object, linker sections без MEMORY/default board addresses,
opt-in synthetic ARM link probe и positive/negative checks. Настоящего board
vector table, MCU memory map, clock/GPIO/CAN driver, Flash writer и .bin нет.
No-I/O adapter возвращает failure inhibit и не предоставляет watchdog feed;
supervisor остаётся FAULT даже после healthy inputs/self-test/ACK.

Synthetic addresses/stack/64-byte core vectors — только test fixture. Их нельзя
использовать как1MiB layout платы, пример резервирования bootloader или OTA slot.
BOARD_IMAGE=ON продолжает fail. Compile/link PASS не означает аппаратный запуск.

## Что требуется для следующего board-only build

1. Сверить физическую маркировку/package/board revision с документацией;
   уточнить RAM/CCM ranges/availability и полный IRQ vector target. DEV_ID и
   suffix не заменяют эту проверку. Определить, какие reference facts остаются
   инженерными предположениями, и закрепить CMSIS/HAL/LL только если они нужны.
2. Утвердить clock/reset entry policy. Возможен минимальный HSI-only путь без
   внешнего HSE/PLL. Неизвестный HSE блокирует HSE/PLL-конфигурацию; пригодность
   источника/частоты для CAN проверяется отдельно. Текущий код clock не устанавливает/не измеряет.
3. Сформировать reviewable standalone no-I/O memory/vector profile с явными
   источниками и stack budget, без пинов приводов и без bootloader reserve.
   Проверить ABI/startup/linker/размеры/deadline/failure на ПК/CI до hardware.
4. Для последующего стенда сначала уточнить backup SHA/независимую копию и
   recovery, затем отдельно согласовать точный image и SWD/write/reset/capture.
   На этом этапе никаких hardware действий не предлагается исполнять автоматически.
5. GPIO/CAN backend и физические уровни/полярности — отдельный board decision
   и commissioning, не разрешение двигаться из software IDLE или ACK.

## Будущий CAN Bootloader / ESP32 OTA

Архитектура [OTA](OTA_ARCHITECTURE.md) сохраняется: GitHub signed component →
ESP32 bounded transport/cache → независимый F407 bootloader, который сам
проверяет подпись/длину/SHA/hardware/protocol и recovery до jump/commit.
Нужны проверенные erase sectors/protection/RAM и согласованная metadata policy;
размер1MiB сам по себе не выбирает A/B или staging, app offset или адрес metadata.
Сейчас ни один Flash адрес/сектор под bootloader/OTA не резервируется.

Повторное reset/reconnect/update не возобновляет motion. Перед установкой STM32
нужно fresh session-bound подтверждение остановленных осей/шпинделя/программы;
no-I/O stub, SIMULATION и ACK доставки его не заменяют. Hardware E-stop независим.

Приёмка текущего PR: docs согласованы с owner facts/unknowns; host/sanitizer,
ARM/static/link/fault tests и CI PASS; нет hardware операций, firmware binary
публикации, релиза или самостоятельного merge.
