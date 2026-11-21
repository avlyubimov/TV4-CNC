# STM32F407VG: первый ST-Link bench и восстановление

**Образ c09a20e ещё не установлен.** 22 ноября выполнены два отдельно
разрешённых SWD-сеанса без записи Flash: первый завершился ошибкой команды,
исправленный повтор прочитал ID/геометрию/защиту и выполнил reset/run текущего ПО.
[Обе попытки и пределы](evidence/hardware/2026-11-22-f407-swd-preflight/README.md).
Это не проверка исполнения нового HSI/watchdog образа и не разрешение на запись.
Build/CI/PR не разрешают новый SWD/read/reset/write/capture сеанс. Прежние
разрешения ESP32 к F407 не относятся. Перед исполнением владелец отдельно
утверждает точный commit/profile/ELF SHA-256 и операции/длительность сеанса;
изолированная плата — только питание/SWD, без CAN, приводов, шпинделя и станка.
Повторные reset/fault/watchdog/restore tests также включаются в явное разрешение.
[Образ и ограничения](STM32_FIRST_IMAGE.md), [owner facts](HARDWARE_STATUS.md).

## До запроса аппаратного сеанса

1. PR reviewed, exact-head CI зелёный. Собрать **clean commit** по STM32_FIRST_IMAGE,
   проверить `image-check.json`: profile, source_commit, source_dirty=false,
   ELF/BIN/map SHA, размеры/vector count и no-motion scope. Сохранить артефакты
   локально вместе с JSON/toolchain versions. Не использовать synthetic probe ELF.
2. Владелец сверяет полную маркировку MCU с STM32F407VGT6 и идентичность платы,
   питание/общую землю/распиновку SWD, отсутствие исполнительных подключений.
   Не подавать одновременно конфликтующие источники питания от ST-Link и USB.
   Сверить исходные BOOT straps/загрузку из main Flash без изменения option bytes.
   HSE frequency пока не нужна. NRST не предполагается подключённым.
3. Владелец указывает существующий полный **1 048 576-byte** F407 backup и его
   ранее независимо сохранённый SHA-256. Размер/vector structure уже сообщены,
   **его SHA-256 нам пока не передан**. Новое вычисление SHA само по себе не
   доказывает соответствие исходному backup. Нужны сверка с исходной записью,
   независимая вторая копия и готовность restore. ESP32 16 МиБ backup сюда не подходит.
   Пути/содержимое дампа/защищённые данные не помещать в Git, PR или public logs.
4. Подготовить preflight без записи Flash: версию probe/OpenOCD, CPUID/DBG ID/Flash size,
   bank/sector geometry, RDP/WRP и option-byte state. Это новые аппаратные чтения,
   поэтому они ещё требуют разрешения. Не угадывать protection по старому SWD report.
   При mismatch/защите/unknown recovery остановиться; **не выполнять unlock,
   mass_erase, option-byte writes, RDP downgrade или стирание всего чипа**.

Локальные build/hash проверки не обращаются к плате:

```bash
python3 tools/check_stm32_bench.py \
  build-stm32f407-bench/tv4_stm32f407_bench.elf \
  --bin build-stm32f407-bench/tv4_stm32f407_bench.bin \
  --map build-stm32f407-bench/tv4_stm32f407_bench.map \
  --build build-stm32f407-bench --output build-stm32f407-bench/image-check.json
```

Backup checks пользователь выполняет с **известными ему проверенными путями**
и исходным checksum file; агент их не подставляет/не открывает без разрешения:

```bash
test "$(stat -c %s "$TV4_F407_BACKUP")" = 1048576
sha256sum --check "$TV4_F407_ORIGINAL_CHECKSUM"
cmp -- "$TV4_F407_BACKUP" "$TV4_F407_SECOND_COPY"
```

## Разрешённый preflight: сначала чтение и решение

Точная версия OpenOCD и probe фиксируются перед сеансом. Подготовленная config
для ST-Link V2/HLA SWD:
[openocd.cfg](../firmware/stm32-motion/board/stm32f407vg/openocd.cfg).
Stock `target/stm32f4x.cfg` имеет reset-init с PLL64MHz; custom config заменяет
его, оставляет SWD1000kHz, запрещает trace pin initialization и задаёт только
IWDG debug-halt freeze. `reset_config none` не предполагает линию NRST;
сброс через SWD/SYSRESETREQ ещё должен быть проверен на стенде.
Не заменять эту config стандартной без повторного review.

После отдельного разрешения, **ещё без Flash write**, возможен такой сеанс:

```bash
openocd -f firmware/stm32-motion/board/stm32f407vg/openocd.cfg \
  -c "gdb_port disabled" -c "tcl_port disabled" -c "telnet_port disabled" \
  -c "init" -c "reset halt" \
  -c "mdw 0xE000ED00 1" -c "mdw 0xE0042000 1" \
  -c "mdh 0x1FFF7A22 1" \
  -c 'echo [flash info 0 sectors]' -c "mdw 0x40023C14 1" \
  -c 'echo [stm32f2x options_read 0]' \
  -c "reset run" -c "shutdown"
```

В OpenOCD0.12 команды `flash protect_check` **нет**. `flash info 0 sectors`
показывает геометрию/защиту bank0; независимое чтение FLASH_OPTCR по
`0x40023C14` даёт RDP[15:8] и nWRP[27:16] согласно RM0090 §3.9.9.
RDP0xAA — level0; nWRP0xFFF — секторы0–11 не write-protected. Иные значения
не исправлять автоматически. `options_read` только читает user option bytes.
В Tcl-блоке результат `flash info`/`options_read` выводить через `echo`, иначе
возвращённая строка может не попасть в лог.

Это halt/reset/read, не пассивное наблюдение. На успешном пути выполняется
reset/run текущего ПО. Ошибка до него может оставить CPU halted; в выполненных
сеансах cleanup через Tcl `catch` отдельно выполнил reset/run даже после ошибки.
Проверять marker/exit, не объявлять cleanup успешным без лога. Новый сеанс или
повторное reset требует разрешения в пределах согласованного scope. Проверить
device ID low12=0x413, Flash size1024KiB, ожидаемые12sectors и отсутствие
препятствующей записи защиты. Полный raw ID сравнить с owner0x101f6413;
неожиданное отличие расследовать. Option values не изменять. Перед записью
сопоставить реальный отчёт с документированной картой; при сомнении остановиться.

## Отдельно разрешённая установка и verify

Первый образ standalone по `0x08000000`. Он **заменяет код в sector0**,
не сохраняет прежнее приложение пригодным для запуска. Небольшой BIN не
является гарантией сохранности остальных Flash contents или возможности
запуска прежнего firmware. Точный full backup/recovery обязателен.
Не пользоваться автоматическим `erase_address`/unlock/mass_erase fallback.

После успешного preflight и утверждения **конкретного clean ELF SHA**:

```bash
openocd -f firmware/stm32-motion/board/stm32f407vg/openocd.cfg \
  -c "program {build-stm32f407-bench/tv4_stm32f407_bench.elf} verify reset exit"
```

По [официальной OpenOCD процедуре](https://openocd.org/doc/html/Flash-Programming.html)
`program` делает reset/init, erase/write, `verify`, затем reset/run и exit.
ELF программируется по Flash LOAD addresses, `.bss/.noinit/stacks/CCM` — NOLOAD.
Зафиксировать полный exit/log/verify result. Ошибка verify/питания не разрешает
повторную запись или изменение защиты автоматически: сохранить ошибку и
действовать в пределах отдельно утверждённого recovery.

## Проверка исполнения без неизвестных пинов

В разрешённом SWD-сеансе запустить OpenOCD с той же config, открыть отдельный
GDB с **точным ELF**. GDB connection останавливает CPU и может freeze IWDG;
не выдавать остановленный tick за отсутствие heartbeat.

```text
target extended-remote localhost:3333
monitor reset halt
print tv4_bench_build
continue
```

После примерно2 секунд running прервать GDB по Ctrl-C и прочитать:

```text
print tv4_bench_build
print tv4_bench_diag
print tv4_bench_history
```

Сверить exact source_commit/dirty0/profile. Ожидаются magic0x54563444/ABI1/
bytes168/state4/error0, hsi_hz16000000, RCC SWS=HSI/dividers1, self-test/guards1,
watchdog_started1, motion_outputs0. После повторного continue/паузы/interrupt
ticks/heartbeat/feed counters должны увеличиться. Динамические адреса брать из
ELF symbols/JSON, не из прежнего образа. Это подтвердит базовое исполнение,
**не измерит точность clock, worst-case stack, все SRAM/CCM или безопасность GPIO**.

Только если разрешены отдельные fault/watchdog tests, из halted GDB:

```text
set var tv4_bench_diag.request = 0x57444731
continue
```

CPU должен **работать** больше верхнего ожидаемого LSI timeout (предварительно
дать ≥5 секунд), потом interrupt/read. IWDG reset flag bit29, history_valid1,
previous_reason0x57444731 и увеличенный boot_count проверяются вместе. Freeze
при halt намеренный; reset cause/record не заменяют измерение timeout.
Для отдельной UsageFault пробы аналогично request0x464c5431; после reset ожидаются
previous_reason11, previous_fault.exception6/CFSR.UNDEFINSTR, валидный frame/PC
в текущем `.text`. При повторном сбросе/порче стека/неверном frame сохранить
сырой diagnostic и прекратить test; не объявлять fault recovery успешным.
Точное время, running duration и влияние debugger заносятся в hardware evidence.

## Отдельно разрешённое восстановление полного дампа

Вернуть исходное содержимое можно только **проверенной полной копией этой платы**.
Снова проверить размер/исходный SHA/независимую копию и тот же read-only protection
preflight. Любая RDP/WRP проблема требует отдельного решения, не auto-unlock.
Вместо имени ниже владелец передаёт свой проверенный путь; эту строку не запускать
с буквальным placeholder и не подставлять ESP32 backup:

```text
openocd -f firmware/stm32-motion/board/stm32f407vg/openocd.cfg \
  -c "program {/absolute/verified/f407-factory-1mib.bin} verify reset exit 0x08000000"
```

Offset для raw BIN — последний аргумент OpenOCD `program`, **0x08000000**,
а не0x0; ELF собственного offset не требует. Полный1MiB restore стирает/записывает
Flash sectors0–11; option bytes/OTP в этот dump не входят и не восстанавливаются.
Команды опираются на [OpenOCD Flash documentation](https://openocd.org/doc/html/Flash-Commands.html).
Проверить exit/verify/reset и поведение заводского приложения. При согласованном
readback можно отдельно сравнить полный readback SHA с исходным SHA; это новая
операция чтения, не выполняемая автоматически данной процедурой.

Разрыв питания при записи может оставить MCU без приложения. SWD recovery
требует пригодного probe/питания и разрешённой повторной операции; образ не
содержит независимый recovery loader. Восстановление пока **не испытано**.
Будущий CAN loader/разделение Flash/OTA относится к P4 после P3; sector0 данного
bench не означает утверждения его будущего размера/адресов приложений.
