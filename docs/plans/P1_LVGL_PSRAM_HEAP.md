# P1: освободить internal RAM для Wi-Fi и AES DMA

База main00a6d86 после слитого PR #74. [Стенд46cf400](../evidence/hardware/2026-11-23-esp32-dma-46cf400/README.md)
показал9 DMA-capable allocation failures при handshake. Dashboard использовал
100516bytes internal RAM; свободная PSRAM почти не участвовала. В основе
решения — этот замер и фактический CLIB allocator LVGL9.2.2, не уменьшение
запасов DMA/стека или ослабление TLS.

Одна интеграция: только explicit rev12-ota выбирает штатный LVGL CUSTOM
allocator; malloc/realloc используют SPIRAM|8BIT, free — SDK, без internal
fallback. Нет нового task/lock/state, глобальная malloc policy прежняя.
LVGL остаётся в app_main; flush_ready вызывается из его task, IRQ не получает
GUI objects. RGB draw buffer/framebuffer/тайминги/пины остаются прежними.

Поддержать весь обязательный CUSTOM API9.2.2. Pool add/remove и monitor,
не поддержанные прежним CLIB, не получают фиктивных собственных метрик;
existing SDK capability diagnostics остаются resource authority. Memory test
проверяет целостность внешнего heap, без печати. При нехватке PSRAM вернуть
NULL; failure realloc сохраняет старый owner. Headless/ordinary CLIB прежние.

Проверки: actual source + pinned LVGL headers, OOM/realloc/caps/integrity;
host и ASan/UBSan/LSan, static analysis,16 SDK variants, actual ELF/archives
с единственным custom core, zero writable state/frame128 ceiling. Прежние
TLS/monitor/crypto/touch gates и pins/dependency locks/Flash map сохраняются.
Проверить10 не-OTA variants на прежние code sections/frames/config.

После CI — новый точный ограниченный пакет: fresh16MiB/two-NVMe, старый
control8192 → новый control8192 → TLS только после PASS, до3 TLS boots,
остановка после FAIL, возврат всего factory fecc05d и одно условное full
recovery из свежего снимка. Заранее данное разрешение23ноября2026 действует;
повторных запросов нет, закрытый пакет46cf400 не используется повторно.
Измерять DMA/internal/PSRAM/stack/touch и доступные UI timings; не выдавать
отсутствующие FPS/physical touch-to-display за подтверждённые.

Изменение Kconfig malloc choice требует однократной регенерации старого
OTA sdkconfig из defaults. Старый generated config сначала сохранить;
несовместимый cached CLIB отклоняется сборкой, молчаливого fallback нет.
Реальные SDK cache/dependency/source версии не смешивать. Это не изменение
таблицы разделов и не новая USB migration.

Критерий — прошиваемые образы обоих SDK и реальный verified HTTPS HEAD с
работающими HMI/GT911/Wi-Fi/resource guards. После него reviewed owner key/
SemVer/time/floors и настоящая signed session; без них CHECK/INSTALL закрыты.
Motion/eFuse/STM32/CAN/merge/releases сюда не входят.
