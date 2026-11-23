# P1 — точный DMA allocation trace после PR #73

База main502a73d. Цель одной интеграции — настоящий ELF/BIN обоих SDK,
который различает общий internal8BIT heap и DMA caps и сохраняет точный
failed request существующего SNTP/HEAD в OTA-worker8192.

`dma_diagnostics.c` включён только `TV4_OTA_WORKER_TLS_BENCH`; остальные
профили, control и standalone TLS не регистрируют hook и не содержат ledger.
Четыре первых события, saturating counters/loss marker, запрет reinit/другого
owner. Callback в IRAM, данные в DRAM; ни heap traversal, ни logging, string
access, allocation/free или task wait в нём нет. Deferred emit — в том же worker.
ISR и чужая задача помечаются отдельно; pointer/имя задачи не выводятся.

SDK5.3.2/6.1.1 callback registration перезаписывает global pointer и не имеет
claim/getter API. Поэтому проверять **все реальные SDK archives**: только один
reference из dma_diagnostics.c.obj. Не обещать runtime coexistence. Секция
callback и четыре RTOS callee проверяются в ELF; только в этом режиме linker
fragment переносит xTaskGetCurrentTaskHandle из Flash в IRAM. Другие profiles
сохраняют штатное размещение. Отказы ownership/overflow делают trace invalid.

Фазы1..5: SNTP, HTTP init/open/headers/cleanup. До/после — free/largest точных
MALLOC_CAP_DMA и DMA|INTERNAL|8BIT. Callback захватывает bytes/caps, sequence,
phase, owner/isr и allocator ID. request_free/largest снимаются **после unwind**,
это не heap в момент failed allocation. Positive132 остаётся Mbed/PSA кодом;
отдельно сохраняются известность и значение ESP-TLS layer error/raw headers.

Приёмка: actual-source task/ISR/multi-producer/overflow/init failure tests,
обе native SDK API shapes, ASan/UBSan/LSan/TSan/static analysis; ELF imports/
IRAM/DRAM/sole registration/192-byte ledger+frame bounds, прежние monitor
owner и512-byte/192-frame bounds. Старые14 вариантов без новых hook imports/
state; конфиги, locks, partition и функции control/обычных режимов сохраняются.
TLS/AES/CA/date, records16384/4096, DMA reserve64KiB, clocks/pins/display,
стек8192, touch100/500ms и тайм-ауты не изменяются.

Владелец23ноября2026 заранее разрешил необходимые подключения и P1 стендовые
операции сегодня (Asia/Almaty), велел не спрашивать повторно. Это новое
действующее разрешение; старый сеанс3927063 закрыт и не повторяется. После CI —
новый точный одноразовый пакет, fresh16MiB/two-NVMe, control touch_fresh,
TLS только при PASS, остановка после отказа, весь factory fecc05d обратно и
ограниченное recovery. Устройство должно соответствовать pinned identity/map/
security preflight. eFuse/option bytes, motion, merge и публикации сюда не входят.
