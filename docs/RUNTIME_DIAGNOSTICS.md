# Диагностика HMI: instrumentation и границы измерений

## 22 ноября: контроль a5f8b7e остановлен, fecc05d возвращён

[Стенд](evidence/hardware/2026-11-22-esp32-worker-tls-control/README.md) завершён
за608.667s: fresh16MiB/device MD5/SHA-256/two-NVMe PASS, control8192 app-only
write/verify/UART90.101s. Одно touch_age_ms=4294967295 на uptime26335ms из16
samples при touch_errors0; остальные0–19ms. touch_fresh FAIL сохранён,
TLS image не записывался, SNTP/HEAD не запускались. Весь factory2MiB fecc05d
возвращён из свежего снимка/verify/UART30.046s/owner HMI-touch-Wi-Fi-SETUP PASS.
Две writes/два captures, полный16MiB restore не понадобился. На ESP32 fecc05d,
F407 ee7d179 не открывался; разрешение использовано, новых аппаратных действий нет.

В actual app_main найдены два clock-before-snapshot paths. Host injections
воспроизводят underflow/false-health, исправление копирует snapshot прежде clock.
Пороги100/500ms, polling20ms и строгий hardware parser сохраняются. Исправление
ещё не испытывалось на плате; TLS/crypto/install/floors/rollback остаются открыты.

## P1: контроль3072 и три reset-запуска8192

[Стенд dfa9f28](evidence/hardware/2026-11-22-esp32-worker-stack/README.md) завершён:
unprovisioned worker8192 действительно создан, watermark6608–6784bytes,
BOOT local15767–15863, periodic largest7680. Контроль3072 дал watermark1568,
BOOT local20815/largest12288 в periodic. В первой8192 пробе владелец проверял UI,
поэтому runtime нагрузка различалась. Scope0/IP-event samples и per-heap history
не являются production floor, нагрузкой BOOT Wi-Fi или full configured peak.
Весь factory2MiB fecc05d возвращён/verify/owner HMI PASS, разрешение закрыто.
Ни TLS/crypto/install, ни FPS/latency/soak этим не проверены. Ниже история.

## История: реальные startup/BOOT samples fc90200

[Стенд22ноября](evidence/hardware/2026-11-22-esp32-ota-memory/README.md) завершён, fecc05d/factory возвращён и owner HMI PASS.
7startup slots/mask127, BOOT start0/stop0,16diag+4return samples, explicit
factory self-test checks_pass1/sdk_finish1. На APP_ENTRY historical202308,
в BOOT local minimum1887 к UI_READY после dashboard, после stop history112.
Гипотеза112 до HMI данным образом не подтверждена; конкретный allocator не найден.
SDK restore min(saved,local) по каждому heap объясняет, почему последующая
сумма может быть меньше обеих прежних сумм; simultaneous peak/OOM/floor не измерен.

OTA-worker watermark1488из3072bytes во всех16valid samples; configured8192/
TLS/install не тестировались. Scope0(history) во всех periodic records.
Startup sample_stack относится к main, worker_stack_at_emit — к более позднему
выводу; ни compiler frame112, ни эти числа не полный call-chain peak.
Main10580/touch минимум2504/Wi-Fi1104/export1220bytes; render/event0 не FPS/latency.

TV4_OTA_MEMORY_DIAGNOSTICS default off; только explicit flag1 включает SDK
minimum monitor и ledger. Normal OTA сохраняет global history/stack validity
и boot marker. Обычный fecc05d после возврата не содержит этой instrumentation.
Следующий результат — startup RAM transient review без урезания DMA/TLS/буферов.
Новых устройств/UART/reset/Flash после закрытого сеанса нет.

## Исторический baseline PR-03

PR-03 расширяет код ниже: [HMI_PERFORMANCE.md](HMI_PERFORMANCE.md).
Добавлены heap/PSRAM minima, task stacks, reset/counters/histograms и bounded
worker serial JSON export. Per-core CPU и аппаратная latency/soak не завершены.
Следующий раздел сохраняет baseline до PR-03; старый printf заменён worker export.

Существующий код main 323ab36, без изменений в PR-01. Физическое изображение
HMI наблюдалось, но числовых performance результатов с платы в проверенных
материалах нет. Повторно создавать instrumentation вместо анализа кода не нужно.

| Показатель | Существующий источник/смысл | Ограничение |
|---|---|---|
| LVGL refresh rate | dashboard.c считает LV_EVENT_REFR_READY за окно ~1 с | Перерисовки LVGL, не RGB scanout/VSYNC и не обещание 30 FPS |
| LVGL render/refresh time | Последний REFR_START → REFR_READY, мс | Не полный physical input-to-pixel, не распределение p95/p99 |
| Event processing latency | Время queue enqueue callback → обработка owner | Не включает физический контакт/GT911 sample/видимость пикселя |
| Free heap | esp_get_free_heap_size в dashboard | Общая оценка, не полный профиль всех capability heaps |
| Internal memory | main: free/minimum/largest block, каждые 5 с | Минимум с boot и fragmentation proxy, не измеренная длительная устойчивость |
| PSRAM | total до UI, free в main health log | Нет графика/пиков/сквозного bandwidth measurement |
| Owner work duration | max_work_us за 5 с | Исключает sleep и отдельный health log; включает dashboard/LVGL и их работу/логи, не CPU utilization |
| Stack high-water | uxTaskGetStackHighWaterMark(NULL) владельца, ESP-IDF bytes | Не все worker stacks и не доказанный безопасный запас |
| Touch freshness | Возраст последнего valid sample/защёлкнутый fault | Не corner calibration и не physical latency |
| Queue overflow | bounded queue/drop/fault logic есть | Нет отдельного полного counter/export на все очереди |
| CAN timeout | Тестовый heartbeat 500 мс, journal | Только mock envelope, не физическая CAN performance |

UI показывает примерные 36 °C/24.1 В и фиксированные часы/дату; это не измерение
температуры/питания и не источник времени TLS. Driver init messages подтверждают
возврат API, а не качество физического изображения/касания.

## Дополнение PR-03 (ещё не сделано)

- Runtime CPU load по ядрам, memory capability stats, min/free PSRAM и
  все worker stack watermarks с ограниченной частотой публикации.
- Reset reason, watchdog cause/count, Wi-Fi reconnect attempts/success/failure,
  queue overflow по источнику и отдельно mock/будущий physical CAN timeouts.
- Показатели на диагностическом экране с корректными названиями/единицами,
  sampled windows/percentiles, hardware/profile/version/uptime и error context.
- Export versioned bounded records через worker (сначала serial/file abstraction,
  microSD backend после PR-06). Пароли, signing keys и private dumps исключить.
- Физическую touch-to-display измерять отдельной воспроизводимой методикой:
  момент физического касания и появления изменения пикселей, разрешение и
  погрешность измерителя; не переименовывать существующий event_ms.
- Снять baseline до/после Wi-Fi scan/reconnect, без AP/карты, при ошибке touch,
  под memory pressure и при длительном прогоне. RGB frame/scanout считать отдельно.
- Проверить неблокирующее поведение callbacks, включая существующий error log
  внутри flush: при необходимости перенести логирование в owner/worker.
- Не менять PCLK/framebuffer/DMA/PSRAM в рамках документационного PR-01.

Цели <50 мс feedback, 30+ FPS где достижимо, координаты 20–30 Гц, отсутствие
зависаний на timeout — **цели, не достигнутые показатели**. Для каждой записи
указать host/board, метод, версии, длительность, worst case/погрешность и ограничения.
Частоты single framebuffer/PCLK 16 МГц и polling 20 мс не доказывают их выполнение.

## Чтение serial — только отдельно разрешённый стенд

После отдельного разрешения на подключение, выбора порта и активации SDK,
из ASCII-path корня репозитория пример monitor (без Flash):

```bash
idf.py -C firmware/esp32-hmi -B "$PWD/build-idf-6.1.1-rev12" -p "$PORT" monitor
```

PR-01 не запускает эту команду. Аппаратный статус и оставшиеся gates:
[HARDWARE_STATUS.md](HARDWARE_STATUS.md), [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md).
