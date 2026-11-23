# P1: точные DMA allocation failures в OTA-worker

Код `46cf4000874b1dcd4cc0069e54fb88cbc157ff29`, база main `502a73d` после
PR #73. [Результаты](results.json) получены на чистом checkout: 16/16 ELF/BIN
ESP-IDF 5.3.2 и 6.1.1 прошли проверки. Это программная проверка, без открытия
устройств. Аппаратный результат следующего стенда фиксируется отдельно.

Предыдущий HEAD остановился на SDK hardware-AES allocation с Mbed/PSA132.
Новый diagnostic image сохраняет первые четыре точных bytes/caps/phase,
owner/ISR и allocator ID. До/после SNTP и HTTP init/open/headers/cleanup
снимаются отдельные DMA и DMA|INTERNAL|8BIT heaps. Deferred request heap
измеряется после unwind; это **не** heap в момент отказа. ESP-TLS error,
Mbed/PSA code, certificate flags и int64 raw header result разделены.

## Проверено программно

- Host181/181 и ASan/UBSan/LSan181/181 PASS, включая actual native API shapes
  двух SDK, init failure, AES allocation failure и ledger overflow.
- Actual callback: task/ISR/wrong owner/overflow/oversize/logging exclusion,
  concurrent 8000 events и begin race; disabled mode не регистрирует callback.
  ThreadSanitizer concurrent PASS. Никаких sanitizer suppressions.
- GCC analyzer9 PASS: native worker/standalone двух SDK, existing minimum
  monitor, actual DMA callback и service. Host doubles не являются ESP32 bench.
- Реальные archive/ELF negatives отвергают второго владельца регистрации,
  heap import из callback и callee во Flash. Все SDK archives каждого из16
  вариантов проверены: hook присутствует только в двух worker TLS images.
- Новый ledger136bytes, callback compiler frame32bytes, max frame128bytes;
  ceilings192/192. Existing minimum adapter432bytes/max frame112bytes,
  ceilings512/192, один monitor owner.
- Callback и все вызываемые им RTOS функции находятся в IRAM; ledger/mux —
  в internal DRAM. SDK xTaskGetCurrentTaskHandle обычно во Flash: условный
  linker fragment переносит только эту функцию в диагностическом режиме.
  Callback не обращается к строкам, heap/log/allocator/task waits.
- Для14 вариантов без worker TLS exact SHF_ALLOC sections, compiler frames
  utc_tls_bench/memory_diagnostics/service и весь sdkconfig JSON совпали с
  предыдущими сборками. Hook/state отсутствуют; dependency locks и partition
  unchanged. Новый код default off.

## Воспроизведение без устройств

Для каждого SDK собрать headless, rev12, rev12-memory-bench, rev12-tls-bench,
rev12-ota; дополнительно rev12-ota с trace3072, control8192 и worker TLS8192.
Пины контейнеров и SHA-256 всех16 приложений — в results.json; LVGL9.2.2,
touch1.2.1 и GT9111.1.3 сохраняют прежние lock-файлы.

```bash
./tools/build_idf.sh headless 5.3.2 docker
TV4_IDF_PATH="$HOME/.espressif/v6.1.1/esp-idf" \
  TV4_OTA_MEMORY_DIAGNOSTICS=1 TV4_OTA_WORKER_STACK_BENCH=1 \
  TV4_OTA_WORKER_TLS_BENCH=1 ./tools/build_idf.sh rev12-ota 6.1.1 local
python3 tools/check_ota_memory_diagnostics.py \
  build-idf-6.1.1-rev12-ota-memory-diagnostics-worker-stack-bench-worker-tls-bench \
  rev12-ota 6.1.1 --memory-diagnostics --worker-stack-bench --worker-tls-bench
ctest --test-dir build --output-on-failure
bash tools/analyze_ota_worker_tls.sh build/policy-analysis
```

Host configure и ThreadSanitizer команды — в `.github/workflows/host.yml`;
полная SDK матрица и actual ELF checks — в `firmware.yml`. Результаты не
доказывают runtime stack budget, performance или успешный HTTPS/OTA install.
Приватные BIN/ELF и полные build logs сохранены локально; в Git только метаданные.

## Следующий измеримый результат

Сегодня, 23 ноября 2026 (Asia/Almaty), владелец заранее разрешил необходимые
P1 подключения и стенды без повторных запросов. Подготовлен новый точный пакет:
fresh16MiB/two-NVMe → control touch_fresh → TLS только при полном PASS →
остановка после первого FAIL → весь factory2MiB fecc05d обратно. Ограниченное
одноразовое full recovery только из свежего проверенного снимка. Сеанс3927063
закрыт и не переиспользуется.

SDK callback registration перезаписывает global pointer, без claim/getter.
Sole linked caller проверен; runtime coexistence не обещается. Нет снижения
TLS records16384/4096, DMA reserve64KiB, CA/hostname/date, hardware AES,
touch100/500ms, timeout, clocks/pins/framebuffer или stack8192 ради PASS.
Без owner key/context CHECK/INSTALL остаются закрыты. eFuse/STM32/CAN/motion,
merge и публикация релиза не входят в этот этап.
