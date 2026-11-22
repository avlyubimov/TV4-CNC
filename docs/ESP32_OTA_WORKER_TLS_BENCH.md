# Один UTC/HEAD проход в OTA-worker8192

Программное продолжение [стенда worker8192](evidence/hardware/2026-11-22-esp32-worker-stack/README.md),
база main `53b0715` после PR #68. Подготовлен отдельный диагностический image;
он не записан на устройство. На ESP32 остаётся fecc05d/factory/tv4-16m-ab-v1,
F407 ee7d179. Прежнее аппаратное разрешение закрыто.

## Выбор и поведение

`TV4_OTA_WORKER_TLS_BENCH=1` принимается только вместе с
`TV4_OTA_MEMORY_DIAGNOSTICS=1` и `TV4_OTA_WORKER_STACK_BENCH=1` для rev12-ota,
без owner-local context. Все флаги default off; shell/CMake/C/config/ELF guards
отклоняют неверные сочетания и provisioning. Migration packager этот image
не принимает. Normal/trace/stack-only/standalone TLS профили сохраняют поведение.

Тот же OTA-worker8192 после успешного self-test, закрытия BOOT window,
`finish_boot`, снятия своей boot-WDT subscription и публикации SETUP выполняет:

1. 15 секунд settling с проверкой свежести HMI health каждые 250 мс.
2. До 15 секунд ожидания доступного Wi-Fi; offline завершает пробу без SNTP/TLS.
3. Один existing SNTP sync к pool.ntp.org с logical deadline 10 секунд.
4. Один existing verified HTTPS HEAD к публичному репозиторию TV4-CNC на GitHub,
   logical deadline 15 секунд, каждый I/O timeout не более 5 секунд.
5. Очистку HTTP/SNTP ресурсов, отзыв локального time anchor и возврат в обычное
   ожидание команд SETUP. Ошибка optional probe сама по себе не перезагружает HMI.

Это логические ограничения: DNS/SDK I/O/cleanup не имеют hard-preemption и могут
занять больше времени. Новых tasks/timers/retries нет. Во время SDK вызова отмена
проверяется на следующей границе. Потеря/устаревание health отменяет дальнейший
проход; существующий idle watchdog и UI owner сохраняются.

SNTP **не аутентифицирован** и может изменить system UTC. Изолированная политика
2026 года прежнего standalone bench не является production time authority;
после её диапазона проба закрывается с ошибкой. Anchor локален, не передаётся
installer. Ключ/context не добавляются, CHECK/INSTALL остаются закрытыми.
Нет response body, auth, redirects, download/install/Flash/boot-selection APIs.

Native HTTP backend повторно использует CA bundle, hostname и X.509 validity-date
checks, PSRAM-only mbedTLS allocator, HTTP RX2048/TX512. TLS records16384/4096,
DMA64KiB, framebuffer, PSRAM/RGB clocks, pins и A/B layout сохраняются. Сверка API:
[ESP-IDF 6.1.1 HTTP client](https://docs.espressif.com/projects/esp-idf/en/v6.1.1/esp32s3/api-reference/protocols/esp_http_client.html),
[ESP-IDF 5.3.2 HTTP client](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/api-reference/protocols/esp_http_client.html).

## Измерения и единственный monitor owner

Native backend нового режима вызывает только существующий
`tv4_ota_memory_window_begin/end/report_window`. В его object нет прямых SDK
minimum start/stop или xTaskCreate/vTaskDelete imports; ELF inspector это проверяет.
Старый standalone bench остаётся взаимно исключённым владельцем в своём профиле.

Семь startup IDs и BOOT1/WORKSPACE2/SNTP3/SESSION4/CLEANUP5 не меняются.
Добавлен только diagnostic TLS_PROBE6, принимаемый adapter лишь под новым флагом.
Окна SNTP3 и TLS_PROBE6 последовательны; четыре boundary samples и start/stop
result выводятся существующим adapter после закрытия каждого окна. Failed start
не вызывает SDK stop, failed stop оставляет scope UNKNOWN и отключает монитор.
При невозможности открыть/закрыть окно следующий сетевой этап не выполняется.
Guards — не reservations или production admission floors.

Snapshots `http_before_init/after_init/after_open/after_headers/after_cleanup` и
`worker_end` сняты из того же worker. Runtime watermark и per-heap minima требуют
отдельного стенда; compiler frames не full call-stack bound, minima не
simultaneous peaks. Даже HEAD200 не квалифицирует crypto/workspace/installer,
подписанную установку, rollback, cold boot, soak или power-loss recovery.

## Сборка без устройств

```bash
TV4_IDF_PATH="$HOME/.espressif/v6.1.1/esp-idf" \
  TV4_OTA_MEMORY_DIAGNOSTICS=1 TV4_OTA_WORKER_STACK_BENCH=1 \
  TV4_OTA_WORKER_TLS_BENCH=1 ./tools/build_idf.sh rev12-ota 6.1.1 local
TV4_OTA_MEMORY_DIAGNOSTICS=1 TV4_OTA_WORKER_STACK_BENCH=1 \
  TV4_OTA_WORKER_TLS_BENCH=1 ./tools/build_idf.sh rev12-ota 5.3.2 docker
python3 tools/check_ota_memory_diagnostics.py \
  build-idf-6.1.1-rev12-ota-memory-diagnostics-worker-stack-bench-worker-tls-bench \
  rev12-ota 6.1.1 --memory-diagnostics --worker-stack-bench --worker-tls-bench
```

Build trees разделены. CI собирает прежние 14 вариантов и два новых на pinned
SDK images, проверяет dependency locks/config/ELF/frames/monitor ownership.
Host tests выполняют actual service/native backend/adapter с SDK doubles обеих
версий, включая отмену, offline, ошибку/тайм-аут SNTP/TLS, oversized headers,
redirect status, cleanup и monitor failures. Они не обращаются к сети/плате.

Следующий аппаратный этап требует отдельного exact-image разрешения с описанием
SNTP/HTTPS, свежим full backup/two-NVMe и возвратом factory fecc05d. Этот документ
и программная сборка такого разрешения не дают. eFuse/STM32/CAN/motion не затрагиваются.
