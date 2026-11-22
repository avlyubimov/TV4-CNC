# История F407 — c09 FAIL и проверенное восстановление

**Исторический отчёт до успешного стенда ee7d179.**
Актуальное установленное ПО/аппаратный результат —
[ee7d179 PASS](../2026-11-22-f407-ee7d179-bench/README.md).
Сохранённые здесь failed attempts, result.json и hashes относятся к прежним сеансам.

22 ноября 2026, Asia/Almaty. База main `4fe84f10a70228b9f440f41c6ccc50869919042a`
после PR #56; [main CI](base-main-ci.json). Владелец подтвердил изолированную
F407 (питание/SWD, без CAN/приводов), затем сверил надпись **STM32F407VGT6**.
Все пять отдельно разрешённых аппаратных сеансов завершены. **По завершении этих прежних попыток восстановлен
полный снимок Flash, снятый до стенда; reset/run выполнен. c09a20e не оставлен на плате.**
NRST к ST-Link не подключён; подписанная кнопка RESET доступна и отпущена.

[Машиночитаемые факты](result.json), [команды](commands.md),
[план исправления](../../../plans/F407_SWD_PREFLIGHT.md),
[runbook](../../../STM32_STLINK_RUNBOOK.md).

## Подтверждённые характеристики

| Проверка | Результат и предел |
|---|---|
| Probe/core | STLINK V2J29S7/API2, OpenOCD0.12.0; Cortex-M4r0p1, CPUID `0x410fc241` |
| MCU | Надпись STM32F407VGT6 сверена владельцем; raw ID `0x101f6413`, DEV_ID `0x413` |
| Revision | REV_ID `0x101f` означает silicon **5 или 6** по ST, точная ревизия не определена; core r0p1 — другое поле |
| Flash | 1024KiB; bank0 `0x08000000`, 12 секторов driver report совпали с profile |
| Protection | FLASH_OPTCR `0x0fffaaed`, RDP `0xaa`/level0, nWRP `0xfff`, user_options `0xec`; snapshot перед операциями, не будущая гарантия |
| Питание/SWD | Probe samples3.257257/3.260437V, requested1000→950kHz; recovery100kHz. Не измерение точности/нагрузки |
| Backup | Два полных чтения по1048576bytes, byte/SHA/basic vectors PASS; независимая SHA-проверенная копия на другом физическом NVMe |
| c09 Flash write | ELF SHA `d6bb0cc3bde04b37315d583ae040867f0ccfc2e4238ecb4a4422f9bbee542e0e`: program/verify и exact clean identity PASS |
| c09 boot | **FAIL**: state5/FAULT, error8/IWDG_TIMEOUT, watchdog_started0, ticks/heartbeat/feed0. RAM/CCM/guard self-tests и запросы watchdog/UsageFault не достигнуты |
| Recovery | DBGMCU_CR0→1, debug access восстановлен после ручного RESET. Полный снимок1MiB записан, **Verified OK**, reset/run, marker VERIFIED_SNAPSHOT_RESTORED |

## Выполненные сеансы и отказы

1. Первый preflight ≤60s: неподдерживаемая команда `flash protect_check`
   привела к exit1; cleanup reset/run завершён. [Partial log](attempt1-partial.txt).
2. Отдельно разрешённый повтор ≤60s: `echo [flash info 0 sectors]`, чтение
   FLASH_OPTCR и `options_read`; полный ID/geometry/protection report, exit0,
   reset/run. [Log](attempt2-complete.txt). Отсутствующая команда удалена из runbook.
3. Отдельный backup-сеанс ≤180s: два полных чтения current Flash, reset/run;
   [redacted log](backup-session-redacted.txt), [receipt](backup-receipt.json).
   По поручению владельца выбрана папка на другом физическом NVMe: исходные
   reads на nvme1n1, копия на nvme0n1. SHA совпал, files0600/dirs0700.
4. Точный c09 install/bench ≤180s: program/verify/identity прошли; после run
   потерян SWD. Условное восстановление остановилось на reset/halt, **до записи
   снимка**. [Log](install-c09-failed.txt). Весь bench завершился exit1.
5. Новое разрешение на recovery ≤180s: владелец удерживал RESET до настройки
   debug, затем подтвердил отпускание. Исправленная config установила только
   DBG_SLEEP и IWDG-halt freeze. Доступ восстановлен; прочитаны clean c09 identity
   и FAULT/error8. Условный full snapshot restore прошёл program/verify/reset-run.
   [Log](recovery-c09-and-restore.txt). **Exit1 относится к неудачному bench;
   успешное восстановление подтверждено отдельными строками, не exit0 всего сеанса.**

Ни один сеанс не менял option bytes/OTP/CAN/motion или ESP32 ordinary8f0d899.
Успешная запись/restore не являются испытанием power-loss recovery или независимым
измерением erase boundaries. Snapshot снят с текущего ПО перед стендом, его
происхождение как оригинального factory image не доказано. Старый owner backup
не открывался. Дампы, их SHA и полные private пути не опубликованы.

## Инженерные выводы и программное исправление

Изначальная config заменила stock examine-end и пропустила DBG_SLEEP. После
установки bit0 DBGMCU_CR доступ к WFI восстановился. Это согласуется с
[RM0090 §38.16.1/3](https://www.st.com/resource/en/reference_manual/rm0090-stm32f407-advanced-armbased-32bit-mcus-stmicroelectronics.pdf).
Stop/Standby/trace bits и GPIO не включаются. Полный набор условий Sleep erratum
[ES0182 §2.2.5](https://www.st.com/resource/en/errata_sheet/dm00037591.pdf) у c09
отсутствует: FLASH_ACR=0, обе WFI имеют адрес2mod4. NOP workaround не добавлялся.

Error8 подтверждает timeout ожидания IWDG, **raw IWDG_SR не захвачен**.
Проверка кода выявила ожидание синхронизации до старта watchdog. Исправленный
код запускает IWDG перед настройкой/ожиданием, сразу отмечает started и при
последующем отказе не feed. Последовательность сверена с
[ST HAL F4 v1.8.5, pinned commit](https://github.com/STMicroelectronics/stm32f4xx-hal-driver/blob/b6f0ed3829f3829eb358a2e7417d80bba1a42db7/Src/stm32f4xx_hal_iwdg.c#L163).
HAL не добавлен в runtime. Причина предполагается на основании наблюдений и
официального кода; её устранение ещё нужно подтвердить новым аппаратным запуском.
[Программные проверки](../../build/stm32f407-bringup-fix/README.md).

## Оставшиеся аппаратные блокеры

На момент этого исторического отчёта исправленный образ ещё не был записан;
позже отдельное разрешение и успешный ee7d179 bench описаны по ссылке выше. RUNNING/progress, CCM/guards, watchdog reset и UsageFault capture пока
не подтверждены. Полные SRAM/CCM stress/high-water, частоты HSI/LSI/HSE, схема
пинов, физические GPIO/CAN/inhibit неизвестны. CAN loader layout не выбран.
Raw logs/scripts/артефакты остаются локально; опубликован только текст с заменой
private путей и удалением trailing whitespace. Эти результаты не разрешают движение.
