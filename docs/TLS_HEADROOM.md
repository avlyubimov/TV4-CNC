# PR-04l — запас памяти диагностического TLS worker

## Основание

[1bfc7e2: HEAD200](evidence/hardware/2026-11-19-1bfc7e2-tls-psram/README.md)
подтвердил isolated TLS connect, но internal minimum3123bytes/window и
2408/history, worker stack watermark912/6144bytes слишком тесны для объявления
production OTA budget. Эти измерения не заменяются оценкой нового исходного кода.

## Изменения

1. Worker больше не держит wifi_snapshot_t со списками16сетей/профилей в своём
   стеке во время SNTP/TLS. Новый wifi_service_online() читает только NET_ONLINE
   под тем же mutex, try-lock0. False при отсутствии lock/конкуренции, как прежний
   отказ snapshot; NET_SCANNING/старый IP не означают online. Это advisory check,
   не гарантия следующего I/O. Нет allocation, ожидания mutex или копии списка.
   UI/full snapshot API, сеть/NVS/профили/очереди и их размеры не меняются.
2. Только HEAD bench HTTP RX4096→2048bytes, TX512 прежний. Штатный SDK malloc
   просит на2048bytes меньше под receive buffer; точное улучшение internal minimum
   не выводится из этой разницы. Small malloc обычно предпочитает internal при
   текущем ALWAYSINTERNAL16384, но placement/fallback/concurrency определяются SDK.
   **TLS records16384/4096 не уменьшены**; это отдельные буферы mbedTLS в PSRAM.
3. Пять конечных worker-phase snapshots: http_before_init/http_after_init/
   http_after_open/http_after_headers/http_after_cleanup. Они вне HTTP-event/LVGL
   callbacks, не печатают headers/SSID/IP/credentials. У неисполненных стадий
   нет snapshot. Ошибки init/open/header/cleanup сохраняют прежний fail/cleanup path.
   Новая строка http_rx/http_tx обозначает именно HTTP buffer, не TLS fragment.

Стек worker остаётся6144bytes **во внутренней памяти**, как и owner/Wi-Fi stacks.
Не увеличиваем его за счёт ограниченного heap, не переносим stack в PSRAM.
DMA/internal reserve64KiB, PSRAM/cache/PCLK, board pins, partitions/dependencies,
CA bundle/hostname/X.509 dates/UTC policy/headers bounds/timeouts/no redirects/
auth/body/install — прежние. Другие профили по-прежнему не запускают probe.
Новый getter нужен сейчас только TLS bench, UI продолжает пользоваться snapshot.

## Почему HTTP RX не является пределом размера заголовка

В обоих закреплённых SDK esp_http_client выделяет RX через malloc(buffer_size_rx)
и передаёт прочитанные части http_parser_execute. http_on_header_field/value
накапливают фрагменты через http_utils_append_string/realloc до header event.
Поэтому2048 не новый лимит header value; прежние128/8192/16384 post-parser
ограничения сохранены. SDK может аллоцировать/увеличивать header strings до нашего
callback: pre-allocation cap по-прежнему не гарантируется, OOM возможен.
Длинный заголовок пересекающий2048 boundary нужен в следующей hardware проверке.

HEAD не читает response body. Меньший HTTP buffer может увеличить число reads/
parser calls; это экономия памяти с ещё не измеренным runtime CPU/timing эффектом,
не обещание нулевой регрессии. Нельзя уменьшать TLS input record без доказанной
fragment negotiation или отключать certificate validation ради экономии.

## Software evidence

[Результаты](evidence/build/tls-bench-headroom/README.md): host17tests/sanitizers,
оба SDK ×4profiles, ELF gates/target workspace, direct frame comparison и
неизменность конфигураций/dependencies/layout. Native compiler frame — только
собственный кадр worker, **не** весь call graph, не maximum TLS stack usage.
Новый boolean accessor проверен на всех состояниях, stale IP, full network list,
reset fixture и отсутствие изменения snapshot. Native lock contention/радио
host fixture не моделирует; соответствующий код скомпилирован обоими SDK.

## Следующий аппаратный gate

На момент software PR на плате был **ordinary rev12 1bfc7e2**,owner HMI PASS.
Сам software PR не делал device/serial/reset/Flash/SNTP/HEAD операций.
Последующая отдельно согласованная проба — ниже. Старое разрешение на1bfc7e2 исполнено. После review/CI собрать
exact SHA и ordinary возврат, проверить hashes/backup и отдельно согласовать
новое испытание. Сопоставить SNTP/verified HEAD200/cleanup, phase snapshots,
local/history minima, largest samples/stack watermarks и HMI/Wi-Fi под нагрузкой.
Не складывать разные boots/окна; дополнительное логирование входит в измерения.

Ни экономия RX request2048, ни меньший native frame не утверждают production
reserve, worst-case stack, отсутствие OOM/утечек/latency regression. Не включать
runtime OTA body/session/installer до budget/fault/soak review; production UTC/
CA/redirect/CDN/security и physical performance gates остаются открыты.
Откат кода — revert PR; устройство этим PR не меняется. SDK/ключи/eFuse/релизы,
движение/CAN и активная разметка не меняются.

## Последующая аппаратная проба — 8f0d899

[19 ноября](evidence/hardware/2026-11-19-8f0d899-headroom/README.md):
SNTP/HEAD200/cleanup/monitors PASS; observed window min3123→5307,history2408→4204,
stack watermark912→1840bytes. Новый runtime результат относится к одной пробе,
не production/CPU/worst-case budget. Phase low уже после TLS open; headers не
уменьшили minimum. Controlled single-field fragmentation/negative/load/soak ещё
открыты. Возвращён ordinary8f0d899,owner HMI/touch/Wi-Fi PASS,разрешение исполнено.
