# P1: LVGL CUSTOM heap в PSRAM

[Результаты](results.json): source38f70b9, clean checkout, база main00a6d86
после PR #74. Причина — измеренные100516bytes internal RAM на dashboard и
DMA allocation failures из [стенда46cf400](../../hardware/2026-11-23-esp32-dma-46cf400/README.md).

Программная приёмка:16/16 реальных ELF/BIN двух pinned SDK,183 host и183
ASan/UBSan/LSan tests, новый allocator GCC analyzer PASS. Тестируется actual
adapter с pinned LVGL9.2.2 headers: exact caps/no fallback, OOM, сохранность
старого owner при failed realloc, zero-size/NULL, integrity failure и build
refusal stale CLIB/unsupported board/PSRAM/XiP. Tests не открывают устройства.

Actual SDK archives/ELF: только main предоставляет CUSTOM core, LVGL archive
не содержит второго core. Zero writable state, собственные frames32bytes
против ceiling128; это не полный runtime stack. IDF6 использует архивы без
rescan group: linker anchor lv_malloc_core загружает единственный adapter.
SDK5.3.2 также проверен. Existing TLS/heap-monitor/IRAM guards проходят.

10 non-OTA вариантов сохранили SHF_ALLOC sections/frames трёх прежних OTA
модулей и полный sdkconfig JSON. В OTA меняется только выбор CLIB→CUSTOM;
allocator requests используют SPIRAM|8BIT. Global malloc policy16384,
internal reserve65536, RGB/draw/framebuffer, pins/clocks/TLS16384/4096,
stack8192 и dependency locks прежние. ELF/BIN hashes и exact dependencies
записаны в JSON; новые закрытые/непроверенные библиотеки не добавлены.

Повторить: `tools/build_idf.sh` для пяти profiles каждого SDK, дополнительно
rev12-ota trace3072/control8192/workerTLS8192, затем
`tools/check_ota_memory_diagnostics.py` с соответствующими флагами. Inspector
включает actual GUI core check. Host/ASan/analyzer команды — в host.yml.
Старый generated OTA sdkconfig сохранить и регенерировать из defaults;
несовместимый cached CLIB отклоняется, Flash map не меняется.

Это software evidence. Реальный сравнительный стенд и ограничения измерений
фиксируются отдельно; отсутствие keys/context сохраняет SETUP/CHECK/INSTALL.
Физические FPS/touch-to-display и signed OTA этим отчётом не подтверждаются.
