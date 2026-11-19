# ESP32 HMI — специализированные правила

Наследовать корневой AGENTS.md. Прочитать README.md этого каталога,
docs/BOARD_ADAPTER.md, docs/RUNTIME_DIAGNOSTICS.md и docs/OTA_ARCHITECTURE.md.

- Сохранять существующий ESP-IDF/LVGL 9 HMI, локализацию, инструменты,
  журнал, startup screen и Wi-Fi. Не заменять их новым шаблоном/BSP.
- Базовый SDK 6.1.1, проверять также 5.3.2; LVGL 9.2.2, touch 1.2.1,
  GT911 1.1.3 закреплены manifest/отдельными lock-файлами. Не смешивать sdkconfig.
- headless не запускает GPIO/RGB/GT911/PSRAM/радио; rev12 — стенд, не motion.
  В rev12 есть worker Wi-Fi, поэтому это уже не только «дисплей и сенсор».
- LVGL принадлежит app_main. I2C touch polling, сеть, NVS, microSD и будущий
  OTA выполняются вне LVGL callbacks, через ограниченные сообщения/snapshots.
- Сохранять проверенную разводку RGB/GT911/CH422G. USB_SEL=0; не переключать
  общий USB/CAN тракт автоматически. Не назначать SD/CAN пины по памяти.
- Single framebuffer допускает tearing; двойной буфер/PSRAM/DMA менять
  только с измерениями, профилем памяти и откатом. Не снижать производительность
  ради удобства реализации без объяснения/согласования.
- Метрики подписывать: LVGL refresh/render, программная event latency,
  отдельные аппаратные touch-to-display и RGB scanout. Экспорт не должен
  блокировать UI и раскрывать Wi-Fi пароли/ключи или содержимое бэкапов.
- microSD опциональна для boot/диагностики; ошибка/извлечение не зависают UI.
  Пути файлов/размеры валидировать; NVS и схемы хранения версионировать.
- OTA: штатные esp_https_ota/esp_ota_ops/otadata, неактивный слот, проверка
  подписи манифеста доверенным ключом, хеша/длины/совместимости до активации.
  Обычный app OTA не пишет bootloader, таблицу разделов или активный слот.
  Первый boot подтверждать после self-test; Wi-Fi/AP или microSD не обязательны
  для успешной загрузки HMI. Установка — только после подтверждения оператора.
- Не включать eFuse/Secure Boot/Flash Encryption в рамках настройки OTA.
  Текущий factory 2 МиБ не менять до отдельно проверенной USB-миграции.
- PR-04a/04b/04c добавляют verification/native policy/read-only stream;
  PR-04d добавляет envelope/read-only HTTPS primitives; PR-04e — bounded
  read-only session coordinator; PR-04f — PSRAM-only workspace/admission.
  Нет runtime OTA worker/UI или installer. PR-04g — отдельный explicit
  rev12-memory-bench с finite allocation probe через low-priority worker;
  обычные headless/rev12 probe не запускают. Никакого HTTPS/session_run в probe.
  Неактивный candidate CSV не подключать к sdkconfig/build/flash. Никаких
  production ключей, сетевого OTA/write/boot partition activation без review.
  Для вызовов verifier нужен reviewed local trust store, не downloaded key.
- CPU/hardware/soak часть PR-03 остаётся открытой; не выдавать software windows
  за физическую latency или достижение целевых показателей.

- PR-04h: native HTTPS требует fresh local time anchor, одного boolean hint
  недостаточно. Clock sampler не устанавливает provenance; SNTP не включать
  молча и не считать authenticated. Time/anchor меняет один worker, не callback.

- PR-04i SNTP source — explicit worker call, не из startup/LVGL callback. Только
  reviewed local policy с allow_unauthenticated_sntp; source не authenticated.
  Нет параллельных TLS/UTC updates или других владельцев global SNTP service.

- PR-04j rev12-tls-bench — explicit opt-in, mutual exclusion с memory probe.
  Единственный owner SDK local heap minimum monitor; во время окон existing
  resources diagnostics видят window minima, после stop history восстанавливается.
  No body/auth/redirects; SNTP может менять UTC, source не authenticated.

- PR-04k: TLS bench использует штатный SDK PSRAM-only allocator и X.509 date
  checks. Choice глобальна для mbedTLS image, не per-client. Ordinary internal
  allocator/DMA reserve64KiB/records16384/4096 не менять ради PASS. Source/ELF
  validation не является hardware TLS/performance evidence; после отдельного
  теста1bfc7e2 получен HEAD200, но RAM/stack budget открыт. На конец той пробы
  возвращён1bfc7e2 ordinary; текущий статус — docs/HARDWARE_STATUS.md.

- PR-04l: TLS worker использует nonblocking wifi_service_online(), не UI list
  snapshot в своём стеке. HTTP RX2048 только для HEAD bench, TLS records прежние.
  Phase logging только в worker, не callbacks. Не выдавать compiler frame/RX
  request size за runtime stack/heap budget или performance измерение.

- Headroom test8f0d899: HEAD200 PASS,window min5307/history4204/stack1840bytes.
  Это не production/worst-case/load approval; после одного90s capture возвращён
  ordinary8f0d899,owner HMI PASS. Новые device operations только отдельно согласовать.

- PR-04m: будущий runtime entry — tv4_ota_session_run_guarded с reviewed local
  policy/reader/report; legacy run не выполняет resource admission. Пока новых
  callers нет. Boundary samples не reservation/peak trace/overflow protection;
  не выводить production floors из HEAD minimum. Workspace34000 сохраняется.
