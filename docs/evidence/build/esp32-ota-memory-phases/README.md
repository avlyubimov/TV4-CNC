# P1 ESP32: программная проверка фазной диагностики

Интеграция от main6f95d27 после PR #61. Исполняемые исходники и все сборки:
**clean `391d42989df8f2b89bffdfdb07947626317ec0e9`**, display version391d429.
Последующий коммит добавляет только документацию и эти результаты; его SHA не
приписывается собранным образам. [Назначение и команды](../../../ESP32_OTA_MEMORY_PHASES.md),
[план](../../../plans/P1_ESP32_MEMORY_PHASES.md). Устройства не открывались:
установленный fecc05d и обе backup-копии не менялись.

## Выполненные проверки

| Проверка | Наблюдённый результат | Доказательство |
|---|---|---|
| Host/LVGL/native | 87/87 PASS,1,42s | [host-tests](host-tests.log) |
| ASan/UBSan/LeakSanitizer | 87/87 PASS,3,20s, `ASAN_OPTIONS=detect_leaks=1` | [sanitizers](sanitizers.log) |
| Actual SDK adapter с fake API | 6 lifecycle/failure/scope cases + disabled mode PASS: nested begin, guards/start/stop failures, scope transition, cleanup без worker, fixed slots и bounded output; heap/logging вне mux | Включены в87 tests |
| Artifact/source guards | 3 tests PASS: writable sections, bounded frames, единственный profile owner | [checks](checks.log) |
| Настройка/context/migration | 15 tests PASS, включая explicit trace flag, Docker forwarding и запрет traced USB migration bundle | [checks](checks.log) |
| ESP-IDF build/config/Flash/PSRAM/partition/ELF/frame | 12/12 вариантов PASS, normal+trace OTA на обоих SDK | [matrix](matrix.json) |
| Python/shell/YAML/ссылки/diff | PASS | [checks](checks.log) |

Первый development sanitizer-запуск в managed ptrace среде отказал с сообщением
LeakSanitizer does not work under ptrace. Приведённый clean-commit запуск
выполнен вне неё без отключения LSan. Test keys только во временных host
fixtures; ни один из12 firmware builds не содержит owner context/ключа.

| SDK | headless | rev12 | memory bench | TLS bench | OTA flag0 | OTA flag1 |
|---|---:|---:|---:|---:|---:|---:|
| 6.1.1 | 1061056 | 1579248 | 1580944 | 1584144 | 1598512 | 1600528 |
| 5.3.2 | 1075472 | 1535040 | 1536736 | 1540032 | 1551040 | 1553088 |

Значения — байты actual app BIN. Во всех случаях образ помещается в factory2MiB;
OTA-варианты дополнительно имеют два слота по3MiB. Полные SHA-256 app/ELF/
bootloader/sdkconfig/table, чистый source/tree, dependency lock hashes и flags —
в matrix.json. Сборки привязаны к чистому коммиту; byte-identical reproducibility
между отдельными сборками не проверялась, APP_REPRODUCIBLE_BUILD не менялся.
Локальные BIN/ELF остаются в соответствующих `build-idf-*` деревьях;
бинарники, дампы и полные SDK-логи в Git не добавлялись.

## Память и сохранённая конфигурация

В traced module **432bytes writable state**, максимальный собственный compiler
frame **112bytes** на обоих SDK; ceilings512/192. В normal flag0 module state0,
SDK minimum monitor отсутствует в OTA ELF. В explicit TLS bench прежний monitor
закономерно остаётся, OTA trace там запрещён. Ordinary profiles не содержат
OTA scope sampler. Main frame1232, worker80, operation432, worker_stack32bytes
одинаковы в четырёх OTA builds. Это compiler/object данные, не общий call-chain
peak или runtime watermark; библиотечные/SDK/TLS frames сюда не суммировались.

SDK6.1.1 local commit051e4c4eb20346e043cdd47bc388fde75ff8af63, tracked changes0;
Xtensa GCC15.2.0/esp-15.2.0_20251204. SDK5.3.2 собран в закреплённом immutable
Docker image, GCC13.2.0/esp-13.2.0_20240530; точные digest/commit — matrix.json.
LVGL9.2.2, touch1.2.1, GT9111.1.3 и оба lock-файла прежние. Host GCC15.2.0,
CMake4.2.3, Ninja1.13.2, Python3.14.4. DMA reserve64KiB, TLS records16384/4096,
single framebuffer, PSRAM80MHz, GPIO, 16MiB Flash map и trust/resource authority
не менялись. CI сохраняет10 SDK jobs, а два OTA jobs дополнительно собирают
trace variant; CI artifacts содержат только текст/метаданные.

## Что ещё не подтверждено

Причина прежнего hardware112, overhead instrumentation, полный runtime stack,
configured TLS/install peak, FPS/latency/soak, OTA-slot confirm/rollback/power-loss
не измерены. Bookkeeping guards не reservations или production floors.
SDK monitor default off; его assert allocation используется только в explicit
диагностической сборке, аппаратный запуск требует нового отдельного разрешения.
Новых Flash/UART/reset, eFuse/STM32/CAN/motion, merge или release не выполнялось.
