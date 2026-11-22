# OTA-worker8192: программные сборки и проверки

Чистый source commit `f655e56b16ec09a86c0b0b9eb9163bd4c517e11a`, tree `2711d0dec8f9b6798bf7281e2ce03c682e742328`.
Матрица выполнена 2026-11-22T14:39:53.938576+00:00 — 2026-11-22T14:43:06.550820+00:00 (UTC).
Позднейший коммит с этим evidence меняет только документацию; приведённые
локальные ELF/BIN имеют app version `f655e56`. CI собирает собственный точный head.
Зависимости/locks и контейнеры закреплены; SHA-256 — в JSON ниже.

## Результат

14/14 реальных ELF/BIN: по пять прежних профилей, OTA trace-control3072 и
opt-in worker-stack8192 на ESP-IDF 6.1.1 и 5.3.2. SDK6 — установленный local,
SDK5 — официальный pinned Docker. Для всех проверены source cleanliness,
app header/checksum/appended SHA/ELF digest/SDK/Flash16MiB/config/partition и
unchanged lock files. Стендовый вариант остаётся unprovisioned, SETUP;
CHECK/INSTALL закрыты. Бинарники локальны, релиз не публиковался.

| SDK | Вариант | App bytes | Worker requested bytes | Проверки |
| --- | --- | ---: | ---: | --- |
| 6.1.1 | headless | 1061072 | 0 | PASS |
| 6.1.1 | rev12 | 1579296 | 0 | PASS |
| 6.1.1 | rev12-memory-bench | 1580992 | 0 | PASS |
| 6.1.1 | rev12-tls-bench | 1584176 | 0 | PASS |
| 6.1.1 | rev12-ota | 1598560 | 3072 | PASS |
| 6.1.1 | rev12-ota + trace | 1600576 | 3072 | PASS |
| 6.1.1 | rev12-ota + trace + stack bench | 1600624 | 8192 | PASS |
| 5.3.2 | headless | 1075504 | 0 | PASS |
| 5.3.2 | rev12 | 1535088 | 0 | PASS |
| 5.3.2 | rev12-memory-bench | 1536784 | 0 | PASS |
| 5.3.2 | rev12-tls-bench | 1540064 | 0 | PASS |
| 5.3.2 | rev12-ota | 1551088 | 3072 | PASS |
| 5.3.2 | rev12-ota + trace | 1553120 | 3072 | PASS |
| 5.3.2 | rev12-ota + trace + stack bench | 1553184 | 8192 | PASS |

[Полная матрица](matrix.json), [краткий журнал](matrix-run.log),
[сравнение вариантов](variant-comparison.json), [среда](environment.json).
В trace-control/bench отличается ровно `TV4_OTA_WORKER_STACK_BENCH=false→true`.
Partition binary byte-identical; diagnostics writable432bytes и compiler frames
обоих модулей совпадают. App delta +48bytes на6.1.1/+64bytes на5.3.2.
Compiler own frame — не полный стек вызовов и не runtime watermark.

## Исполненные тесты

- [Host/LVGL](host-tests.log):108/108 PASS, включая десять actual ESP service
  случаев (control/bench × success/task-fail/pending-fail/probe-fail/health-timeout).
  Исполняется настоящий service.c с SDK doubles: xTaskCreate argument3072/8192,
  single owner, watchdog/self-test/reset paths и блокировка SETUP commands.
- [ASan/UBSan/LSan](sanitize-tests.log):108/108 PASS, `detect_leaks=1`.
  Первая попытка внутри ptrace sandbox не поддерживалась LSan; повторена вне
  sandbox, без отключения проверки утечек или suppressions.
- [Build/compile guards](guard-tests.log):6 unittest methods PASS: wrong profile,
  missing trace, provisioning/private-key claim, migration rejection, Docker
  flags/default-off, actual service compile negatives. Это не аппаратные тесты.
- [GCC analyzer](service-analysis.log): actual ESP branch modes0/1 PASS.
  Shell syntax, Python compile и changed-doc relative links проверены отдельно.

Первая матрица на720f643 остановилась на14-м варианте с compiler EMFILE;
13 вариантов прошли. Повтор этого варианта с container nofile4096 PASS.
Настройка явно закреплена в build script/CI; окончательная матрица наf655e56
полностью PASS. Причина исчерпания FD глубже не доказана; это не объявляется
несовместимостью firmware API. Исходные неудачные журналы сохранены локально.

## Аппаратные границы

Устройств/UART/SWD/reset/Flash не открывали. На ESP32 остаётся fecc05d/factory/
tv4-16m-ab-v1, F407 ee7d179. DMA reserve64KiB/TLS16384/4096/framebuffer/clocks,
PSRAM/pins/trust/time/resource policy и monitor ownership не менялись.
Requested8192 и host SDK doubles не измеряют target heap/stack/PSRAM/RTOS.
Hardware allocation/repeatability/UI/Wi-Fi/SETUP и configured TLS/install ещё
не квалифицированы; floors/keys/context не назначены. Для нового exact-image
стенда требуется отдельное разрешение; прежнее разрешение израсходовано.

[Контракт и команды](../../../ESP32_OTA_WORKER_STACK_BENCH.md),
[план](../../../plans/P1_OTA_WORKER_STACK_BENCH.md),
[следующий этап](../../../NEXT_STAGE.md).
