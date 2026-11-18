# PR-03 — HMI instrumentation и bounded serial export

План: дополнить существующую диагностику без смены BSP/таймингов, отделить
callback/owner от вывода, проверить окна/counters/export и сборки обоих SDK.
Приёмка программной части: host/RGB565/sanitizers, 4 сборки, snapshot UI без
клиппинга, ограниченная память/очередь, нет credentials в diagnostic record.
Аппаратная часть PR-03 (baseline/soak/физическая latency) остаётся открытой.

## Что добавлено

- core/diagnostics: окна около 1 с, LVGL refresh Hz, max и оценка p95 для
  refresh/event-to-owner. 17 фиксированных логарифмических bins; p95 — верхняя
  оценка с границами 1/2/4/8/.../32768 мс, последний bin ограничен максимумом.
  Число samples экспортируется отдельно. Пустое окно даёт 0 (нет samples); это не доказательство нулевой latency.
- Owner max_work_us и busy_permille: сумма wall time работы LVGL owner / время
  окна, исключая sleep и отдельную выборку ресурсов. Это **не CPU utilization**.
- Раз в ~1 с: internal heap free/min/largest, PSRAM free/min/total,
  owner/touch/Wi-Fi stack high-water (байт по ESP-IDF), reset reason и признак
  watchdog reset текущей загрузки. Не persistent watchdog history.
- UI queue overflow, mock CAN heartbeat timeouts, ошибки RGB flush/GT911,
  Wi-Fi connection attempts/disconnect events/IP events/полная command queue.
  Wi-Fi disconnect events включают намеренное отключение; это не только сбои.
  Нет SSID, паролей, MAC, NVS или dumps в diagnostic record.
- Диагностика → «Ресурсы»: переключаемая панель памяти/стеков/потерь/reset.
  Недоступные host ресурсы помечаются «На плате не измерено»; тестовый render
  использует искусственный fixture, не аппаратные результаты.

## Экспорт и конкурентность

app_main остаётся единственным владельцем LVGL/diagnostics_t. Коллектор и
histograms не выделяют память, не выполняют I/O. Wifi snapshots защищены
существующим mutex; UI читает с timeout 0. Метрики touch передаются в уже
существующем single-element snapshot, без доступа к I2C из UI. Stack sampling
worker-задач ограничен приблизительно 1 Гц.

Каждые ~5 с owner копирует один diag_record_t в очередь длиной 1 с ожиданием 0.
Задача tv4_export (4096 байт stack, priority 1) форматирует до 1535 байт JSON и
пишет строку `tv4_diag {...}` в stdout. Она добавляет свой stack high-water.
При занятой очереди sample пропускается, export_drops возрастает; heap не растёт.
При ошибке создания worker HMI продолжает работу, export_drops показывает
неотправленные snapshots. При усечении formatter очищает выход, не выдаёт
частичный JSON. schema=1; только числовые/фиксированные platform и profile_rev12 поля.
Stack watermark 0 означает отсутствие выборки/worker, а не измеренный запас;
сопоставлять со startup log. touch_age_ms — возраст последнего snapshot, не
задержка физического касания. Версии SDK/app берутся из boot log, сохранять
его вместе с JSONL и исходным SHA для аппаратного отчёта.

Старый printf раз в секунду из dashboard_update удалён; RGB flush callback
теперь только отмечает ошибку/счётчик и завершает flush. Owner применяет fault;
ошибка не логируется из render callback. UART/USB, stdout backend и его
планирование всё ещё могут влиять на систему: отсутствие деградации нужно
измерить на плате. PCLK/framebuffer/PSRAM/DMA/GT911 timing не менялись.

Сбор потока (только после отдельного разрешения на serial доступ) выполняется
обычным monitor/терминалом; отфильтровать строки с префиксом tv4_diag и сохранить
JSONL локально. Сохранение на microSD и export-кнопка файлового менеджера
отложены до PR-06. Этот PR к serial не подключается.

## Чего эти значения не измеряют

cpu_percent, touch_to_display_ms, rgb_scanout_hz экспортируются как null.
FreeRTOS runtime stats сейчас не включаются ради неподтверждённой точности/
overhead; per-core CPU load остаётся незавершённой частью PR-03. Программный
owner duty не переименовывается в CPU load. Refresh Hz не RGB scanout; event
latency начинается в LVGL callback, не при физическом касании. Текущие часы,
36 °C и 24.1 В в HMI остаются статичными подписями, не telemetry.

Нет физических performance/soak/pressure/reset/reconnect испытаний. До закрытия
PR-03 нужны hardware/profile/version в отчёте, длительность, метод/погрешность
для touch-to-display, baseline до/после сети/экспорта и проверка без AP/карты.
Сам факт сборки не закрывает цели <50 мс, 30 FPS и координаты 20–30 Гц.

## Проверки и откат

Логи/validation: [evidence/build/hmi-performance](evidence/build/hmi-performance/).
Проверки охватывают wraparound окна, p95 bounds/outliers, reset окон без потери
counters, saturation, worst-case JSON size/truncation и layout панели ресурсов
в XRGB8888/RGB565. Существующие core/Wi-Fi/STM32 tests сохраняются.
Откат — revert PR; NVS/schema/Flash partitions/GPIO/dependencies не изменены.
Плата не прошивалась, приводов/CAN/OTA операций нет.
