# P1: запас стека configured OTA worker

После PR #77 опубликован согласованный read-only payload v0.1.0-bench.2.
Первый configured BASE c013ddf/SDK6.1.1 загрузился с PSRAM/RGB/GT911,
Wi-Fi и self-test PASS. Во время контрольного окна владелец дважды нажал CHECK;
оба запроса завершились stage9/error11 (SESSION_RESOURCE). Остаток стека
3024/8192 ниже утверждённого floor3072. Изолированный контроль поэтому FAIL;
его нельзя переименовать в принятый SESSION4. Весь factory fecc05d возвращён,
verify/UART/owner UI-touch-Wi-Fi-SETUP PASS; full restore не понадобился.

Одна необходимая интеграция: добавить configured worker 4096 байт стека
(12288 всего), сохранив ordinary3072 и unprovisioned stack/TLS bench8192.
Это инженерный запас для нового замера, не production qualification.
Положительные resource floors, CA/hostname/date, подпись/SHA/размер/совместимость,
таймауты, память LVGL/PSRAM/DMA и единственный worker остаются прежними.
Добавить ограниченный лог результата resource guard после SESSION window,
чтобы отличать stack/internal/largest ошибки без их обхода.

Программная приёмка: фактический service под SDK doubles для всех трёх
размеров, fail-closed invalid trust, task/probe/watchdog fault paths, sanitizer,
оба pinned SDK/actual ELF/frames и весь CI. Затем новый exact BASE-пакет,
fresh16MiB/two-NVMe, чистый контроль без CHECK, один CHECK по команде и возврат
factory. Календарное окно одного стенда продлено владельцем; таймауты и резерв
восстановления не продлевать по ответу сети. Никаких INSTALL/eFuse/STM32/CAN/motion.
