# F407 — первый SWD preflight агента, без установки образа

22 ноября 2026, Asia/Almaty. База main
`4fe84f10a70228b9f440f41c6ccc50869919042a` после PR #56;
[main CI5/5 PASS](base-main-ci.json). Владелец подтвердил изолированную F407 через ST-Link V2,
только питание/SWD, без CAN и приводов. Разрешены **два отдельных** сеанса
до60сек каждый: discovery/reset/halt/ID/Flash geometry/protection/options reads,
reset/run текущего ПО. Разрешения на Flash write/erase/option changes/backup
read или установку c09a20e не было; эти операции не выполнялись.

[Результат JSON](result.json), [команды/версии](commands.md),
[план](../../../plans/F407_SWD_PREFLIGHT.md),
[актуальный runbook](../../../STM32_STLINK_RUNBOOK.md).

| Проверка | Фактический результат / предел |
|---|---|
| Адаптер | STLINK V2J29S7/API2, OpenOCD0.12.0; один обнаруженный0483:3748 |
| Питание | Probe samples3.257257V и3.260437V; не calibrated/load/stability measurement |
| SWD | Requested1000kHz, OpenOCD выбрал950kHz;6breakpoints/4watchpoints |
| Core/CPUID | Cortex-M4r0p1, `0x410fc241`; совпадает с прежним owner report |
| DBGMCU_IDCODE | Raw `0x101f6413`, DEV_ID low12=`0x413`, совпадает с owner report |
| Silicon REV_ID | `0x101f`; OpenOCD пишет unknown. ST RM0090 §38.6.1 и ES0182 table2 определяют **5 или6**, не один точный marking |
| Flash | Size register1024KiB; driver bank0 base`0x08000000`,size`0x00100000`; все12sector offsets/sizes совпали с bench profile |
| Protection snapshot | FLASH_OPTCR=`0x0fffaaed`,RDP=`0xaa`/level0,nWRP=`0xfff`; driver сообщает sectors0–11 not protected; user_options=`0xec` |
| Reset/run | Обе попытки после reads/error выполнили cleanup reset/run текущего ПО; completion markers в обоих логах |
| Образ c09a20e | **Не устанавливался и не исполнялся**; HSI/tick/RAM/CCM/watchdog/fault tests по-прежнему не выполнены |
| Flash/OTP/backup | Нет write/erase/unlock/option writes/OTP contents access/backup reads; listing OTP bank не означает чтение его содержимого |

Первый сеанс завершился exit1: подготовленный runbook ошибочно содержал
`flash protect_check`, которого в установленном OpenOCD0.12 нет.
ID/core/Flash size были считаны, но geometry output внутри Tcl catch не был
явно выведен, protection до отказа не прочитана. Это **partial**, не PASS.
Cleanup выполнил reset/run; [полный log](attempt1-partial.txt) сохранён.

Повтор выполнен только после нового явного разрешения владельца. Команда
удалена; `echo [flash info 0 sectors]` сохраняет возвратную строку, FLASH_OPTCR
читается32бит без записи, `echo [stm32f2x options_read 0]` фиксирует user options.
[Log](attempt2-complete.txt) содержит READS_COMPLETE, RESET_RUN_COMPLETE и
SESSION_COMPLETE_NO_FLASH_WRITE; process exit0. Обе попытки ограничены внешним
timeout50s + kill-after2s. Debug servers отключены; custom cfg не включает
stock PLL64MHz reset-init или trace GPIO. В config есть только ожидаемая
volatile debug IWDG-halt-freeze policy, не option-byte изменение.

Геометрия здесь — прочитанный Flash size и **отчёт драйвера** для распознанного
MCU; секторы не стирались, физическая erase/program/restore операция не проверена.
`result.json` построен из actual sector log и сравнён со всеми12entries
профиля `stm32f407vg-hsi-no-motion-v1`. Snapshot защиты не изменяет разрешения
на будущую запись и должен быть актуальным для нового разрешённого сеанса.

Ревизии5/6 подтверждаются [ST ES0182 Rev19, July2026, table2](https://www.st.com/resource/en/errata_sheet/dm00037591.pdf)
и [RM0090 Rev22 §38.6.1](https://www.st.com/resource/en/reference_manual/rm0090-stm32f407-advanced-armbased-32bit-mcus-stmicroelectronics.pdf).
Нельзя выбрать5 или6 из одного ID; full physical marking/package не получены.
RDP/nWRP расшифрованы по RM0090 §3.9.9; readout опирается на installed
[OpenOCD command documentation](https://openocd.org/doc/html/Flash-Commands.html).
`openocd -c noinit ... help ... shutdown` проверял command help **без target init**;
[отдельный log](offline-command-help.txt) не выдаётся за аппаратный сеанс.

Исходный F407 backup SHA/разрешение на чтение backup/независимая вторая копия
ещё не предоставлены. После их проверки и маркировки требуется отдельное
exact-image разрешение на c09a20e ELF SHA
`d6bb0cc3bde04b37315d583ae040867f0ccfc2e4238ecb4a4422f9bbee542e0e`.
BIN SHA `7563947b101452cd99b46478acd05b7003d954f541871cf76d3a2a73e7558a2e`,2596bytes.
Повторных прошивок/сбросов для bench, HSE/CAN/GPIO/drive tests не выполняли.
Обычный ESP32 8f0d899 не затрагивался. Записанный полный F407 backup ранее
остаётся owner fact, новое чтение Flash dump этим отчётом не подтверждается.

Raw logs сохранены локально; здесь только текст, абсолютный private evidence
path в failed trace заменён на `<local-evidence>`, trailing whitespace убран.
Дампов/ключей/паролей/бинарников нет. Набор фактов не доказывает physical inhibit,
HSI/RAM/CCM/clock/timer/motion readiness. CAN loader addresses не назначены.
