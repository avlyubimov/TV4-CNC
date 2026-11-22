# STM32F407 — первый успешный стенд ee7d179

**22 ноября 2026, 11:33:43–11:33:59 Asia/Almaty (UTC+5): PASS.**
Один отдельно разрешённый сеанс завершён за **16,196 с**, exit0.
На изолированной F407 оставлен работающим **ee7d179435f3f9e7598c7646b85ff64f4d5e9a32**,
профиль `stm32f407vg-hsi-no-motion-v1`, BIN2540bytes. Это первый успешный
базовый запуск; CAN, GPIO/приводы и безопасность реального станка не проверялись.

[Фактический OpenOCD log](openocd.txt), [result JSON](result.json),
[пять snapshots](snapshots.md), [identity/hash/размеры](identity.json),
[выполненные команды](commands.md), [redacted Tcl](executed-sequence.tcl.txt).
База отчётной ветки main `07fb406e37ebd1f1e019dbee0b54e8bb5571be1a`, PR #57 слит
владельцем. Установлен именно проверенный clean PR head **ee7d179**, а не новая
сборка из коммита отчёта. [Предыдущий неудачный c09 и restore](../2026-11-22-f407-swd-preflight/README.md)
сохранены как история; их ошибки не удалены.

## Подтверждено на плате

| Проверка | Фактический результат |
|---|---|
| Разрешение | Владелец: «Разрешаю весь стенд ee7d179»; одна изолированная F407, питание/SWD, ≤180s, conditional restore при отказе |
| Предварительная проверка | Exact ELF/BIN SHA/source_dirty=false; три локальных snapshot files по1MiB одинаковы, сверены с исходным SHA. Новых full-Flash reads с MCU не было |
| ID/защита | Cortex-M4r0p1, CPUID0x410fc241, ID0x101f6413, Flash1024KiB, OPTCR0x0fffaaed/RDP0/nWRP0xFFF |
| Probe | STLINK V2J29S7/API2, OpenOCD0.12.0, requested1000→950kHz; voltage sample3.255666V, не calibrated/load test |
| Debug | DBGMCU_CR0→1, доступ при WFI сохранён; ручной RESET и подключение NRST не понадобились |
| Flash | Один program exact ELF в sector0, **Verified OK**; clean104-byte build identity совпала до/после тестов |
| Boot | Во всех пяти snapshots state4/RUNNING, error0; data/BSS, один CCM probe word и оба stack guards PASS |
| Прогресс main | Boot count1 неизменен, nominal ticks1499→2997, heartbeat/feed14→29 |
| Watchdog | Один RAM request0x57444731, running window5s; boot count1→2, IWDG reset flag установлен, previous_reason0x57444731 |
| UsageFault | Один RAM request0x464c5431, running window5s; boot count2→3, previous_reason11, exception6, frame_valid1, CFSR0x10000/UNDEFINSTR |
| Fault PC | 0x0800044c в `.text` точного ELF, opcode00de соответствует UDF #0; MMFAR/BFAR valid flags не установлены, их значения не объявляются адресом ошибки |
| После fault | Boot count3 неизменен; ticks3005→4501, heartbeat/feed30→45; все self-tests PASS, exact identity повторно совпала |
| Warm history | Пять21-word records из лога отдельно проверены CRC32 локально; previous fault сохранён в diagnostic после reset |
| Завершение | Resume, marker SESSION_COMPLETE_EE7D179_RUNNING, shutdown, exit0; **ee7d179 оставлен работающим**. Conditional snapshot restore не использован |

Полные SHA опубликованы только для собственных build artifacts. Snapshot dumps,
их SHA/private paths не публикуются; проверенные копии сохраняются на двух
физических NVMe. В предыдущем сеансе restore этого снимка уже прошёл verify.
ESP32 ordinary8f0d899, option bytes/OTP/CAN/motion не менялись. Успешный watchdog
reset не разрешает новое движение, чтение или программирование; этот scope завершён.

## Инженерные выводы и границы

Наблюдаемый новый RUNNING и два штатных reset подтверждают устранение прежнего
отказа в проверенном сценарии после исправления IWDG sequence и Sleep debug.
Отдельное значение IWDG_SR не снималось. HSI16MHz/SystemCoreClock и divider1 —
номинал/прочитанная конфигурация, а не независимое измерение частоты.

Это cookies/init, один32-bit CCM word и guards, **не полный SRAM/CCM memtest**,
не runtime stack high-water, soak/thermal/load test или испытание power loss.
Два running windows по5s подтверждают reset/history, не точный watchdog timeout.
CRC history — целостность RAM после warm reset, не durable/authenticated state.

Полная маркировка STM32F407VGT6 ранее сверена владельцем; REV_ID0x101f по ST
означает5/6, точная silicon revision ещё неизвестна. HSE, полная схема выводов,
physical GPIO/CAN/termination/трансиверы/inhibit, RAM stress и точные частоты
остаются открытыми. Sector0 — standalone bench, не резерв будущего CAN loader.
P4 loader/app/staging addresses не назначены.

## Программные проверки и следующий результат

До записи ee7d179 прошёл host77/sanitizers77, actual ARM contract12groups,
negative/repro/static checks и CI26/26 (push+PR), main07fb406 CI5/5.
[Software evidence](../../build/stm32f407-bringup-fix/README.md) сохраняет working-tree
результаты; [source CI](source-ci.json) привязан к точному ee7d179. В этом отчётном
PR firmware/tools/dependency/CI configs не меняются; новые локальные сборки не
заявляются. Проверены parsed snapshots/CRC/ELF opcode, ссылки/Bash/JSON, privacy,
text-only scope и соответствие source hashes. [Validation](validation.txt).

P0 достигнут: настоящий программно проверенный образ работает на базовом стенде.
Следующий приоритет — P1 ESP32 OTA на существующих HTTPS/crypto/verification,
с review16MiB layout и отдельной USB migration procedure; не новая библиотека
вместо инсталлятора. Новая аппаратная операция требует нового точного разрешения.
