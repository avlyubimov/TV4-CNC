# P1 ESP32: программная проверка фазной диагностики

Интеграция от main6f95d27 после PR #61. [Назначение, команды и ограничения](../../../ESP32_OTA_MEMORY_PHASES.md),
[план](../../../plans/P1_ESP32_MEMORY_PHASES.md). В этом этапе устройства не
открывались: установленный fecc05d и обе backup-копии не менялись.

Предварительные development builds обоих OTA-вариантов на SDK6.1.1 и5.3.2
прошли inspector. В traced module432bytes writable state, максимальный собственный
compiler frame112bytes; ceilings512/192. В normal flag0 state0bytes, SDK minimum
monitor отсутствует в ELF. Это object/compiler наблюдения, не runtime peak.

Host/LVGL:87/87 PASS. ASan/UBSan/LeakSanitizer:87/87 PASS с detect_leaks=1.
Первый sanitizer-запуск внутри managed ptrace среды отказал с сообщением
LeakSanitizer does not work under ptrace; повторён вне неё без отключения LSan.
Actual adapter использует fake SDK только в host tests:6 lifecycle/failure/
scope cases и disabled mode, отдельные artifact/source guards. Конфигурационные
tests15 проверяют ключ/context/migration и explicit diagnostics flag; test keys
остаются только в host temp fixtures и не включаются в firmware builds.

Clean-commit build matrix и точные hashes фиксируются отдельным matrix.json
после завершения сборок. Исходники/SDK/dependency pins, DMA reserve64KiB,
TLS records16384/4096, framebuffer и Flash map не меняются. CI сохраняет10 SDK
jobs, а два OTA jobs дополнительно собирают diagnostic variant; binaries/dumps
не публикуются автоматически, в CI artifacts только текст/метаданные.

Не подтверждены: причина прежнего hardware112, overhead instrumentation,
полный runtime stack, configured TLS/install peak, FPS/latency/soak, OTA-slot
confirm/rollback/power-loss. Bookkeeping guards не являются reservations или
production floors. SDK monitor default off; его assert allocation допускается
только в явно выбранной диагностической сборке с отдельным owner board permission.
Никаких новых Flash/UART/reset, eFuse/STM32/CAN/motion или публикации релиза нет.
