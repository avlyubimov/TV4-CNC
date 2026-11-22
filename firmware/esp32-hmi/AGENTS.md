# ESP32 HMI — специализированные правила

P1 после main53b0715/PR #68: opt-in UTC/HEAD в том же OTA-worker8192 описан в
docs/ESP32_OTA_WORKER_TLS_BENCH.md от корня. Новый TV4_OTA_WORKER_TLS_BENCH
требует unprovisioned OTA+trace+stack flags, default off. Native backend нового
режима использует только existing diagnostic adapter, не SDK monitor напрямую
и не второй task. SNTP3/TLS_PROBE6 после BOOT закрываются последовательно;
SETUP/CHECK/INSTALL gates прежние. Software image не hardware разрешение.

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
  Legacy ordinary factory 2 МиБ и explicit A/B profile не смешивать. После
  отдельно разрешённой миграции fecc05d на плате tv4-16m-ab-v1/factory; ordinary
  flash не является разрешённым откатом. Актуальный статус — корневой AGENTS.md
  и docs/HARDWARE_STATUS.md.
- P1 от main d1317a8: только explicit rev12-ota включает installer/worker,
  rollback/watchdog и ota/partitions.ab.csv. Ordinary profiles/factory2MiB
  прежние; candidate CSV остаётся историческим read-only input. Читайте
  docs/ESP32_OTA_INSTALLATION.md. Отсутствующий owner-local trust/context
  блокирует установку; проверочные ключи только host. Подготовка USB-пакета
  не разрешает его запись. Текущие установленные образы этим PR не меняются.
- P1 после PR #61: читать docs/ESP32_OTA_MEMORY_PHASES.md. TV4_OTA_MEMORY_DIAGNOSTICS
  default off; explicit flag1 только rev12-ota и другой build tree. SDK minimum
  start выделяет/asserts bookkeeping и не отклоняет nested start: один serialized
  service owner, запрет concurrent TLS bench, source/ELF gates обязательны.
  Ни startup callbacks, ни per-chunk путь не логируют/не начинают окна. Main
  публикует семь фиксированных samples, существующий worker выводит ограниченно.
  После main973a9ba UI_READY следует за первым dashboard refresh, optional
  export/Wi-Fi запускаются после него: ID стабильны, time-order0,1,2,3,5,6,4.
  Не сортировать историю по ID как хронологию и не переносить host heap на ESP32.
  Не приписывать late worker_stack_at_emit старой startup-точке; не выдавать
  own compiler frames за full runtime stack. Install/crypto/resource authority,
  heap clocks/DMA reserve/TLS records/framebuffer и installed fecc05d прежние.

Стенд main dfa9f28 после PR #67 завершён за928.998s: контроль3072/три
unprovisioned reset boots8192 PASS, fresh16MiB/two-NVMe и возврат factory2MiB
fecc05d/verify/owner HMI-touch-Wi-Fi-SETUP PASS. Читать от корня
 docs/evidence/hardware/2026-11-22-esp32-worker-stack/README.md. Worker watermark
6608–6784/largest7680 не configured TLS/install/floor. Первый boot включал UI
проверку; нагрузки различались. На ESP32 fecc05d, разрешение закрыто, новых
hardware действий нет. Семь startup IDs/monitor ownership/пины/клоки прежние.

`TV4_OTA_WORKER_STACK_BENCH` default off: только unprovisioned rev12-ota с
memory diagnostics1 и отдельным build tree. Читать docs/ESP32_OTA_WORKER_STACK_BENCH.md
от корня. Тот же worker получает8192, SETUP/CHECK/INSTALL прежние; requested
stack/log/host SDK doubles не runtime resource qualification. Не подставлять
test keys/context и не включать новые network/Flash operations ради замера.

Исторические контракты ниже описывают слитые read-only этапы; P1 расширяет только
explicit OTA profile, не включает installer в прежних профилях и не снимает
аппаратные/ключевые/ресурсные gates.

- PR-04a/04b/04c добавляют verification/native policy/read-only stream;
  PR-04d добавляет envelope/read-only HTTPS primitives; PR-04e — bounded
  read-only session coordinator; PR-04f — PSRAM-only workspace/admission.
  Нет runtime OTA worker или installer. PR-08a даёт только local info UI ниже.
  PR-04g — отдельный explicit
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

- PR-05b: читать docs/RELEASE_POLICY_REVIEW.md. Strict Unicode hex/short escape
  gate до pinned cJSON; canonical envelope Base64 native/offline parity.
  Host-only signing/conformance helper не собирать в firmware. Corpus не назначает
  local production versions/keys/layout и не разрешает Motion schema/installer.

- PR-08a: читать docs/FIRMWARE_INFO_UI.md. Local descriptor/build/SDK/profile
  только для отображения, не installed SemVer/trust store. Неизвестные STM32/CAN
  остаются unknown. New panels lazy-create/delete в LVGL owner, не callbacks;
  no fake discovery/check/download/install/progress/history и no OTA callers.
  New snapshot origin SIMULATOR явно маркирует host fixtures; DEVICE означает
  local SDK descriptor, не commissioned hardware или release verification.
