# ESP32 HMI — специализированные правила

От main `ef3b47d` готовится [операторская трассировка после INSTALL](../../docs/plans/P1_POST_INSTALL_RESTART_TRACE.md).
`TV4_OTA_RESTART_TRACE` default-off: тот же configured worker, маркеры до
штатного SDK reset, без auto-CHECK/auto-reboot и новых проверок после commit.
CI fixture только software-only; для платы нужен прежний reviewed owner context.
Следующие сведения о f819955 и bench.4 остаются подтверждённой аппаратной базой;
новая post-INSTALL трассировка ещё не квалифицирована.

Код диагностики [PR #93](https://github.com/avlyubimov/TV4-CNC/pull/93)
слит владельцем в main `d541baf`; source `f819955`. Обе сборки 5.3.2/6.1.1,
313 host/313 sanitizers, source CI 34 и merged-main CI 17 PASS.
Диагностика выключена по умолчанию, ключей/релизов не добавляет.

[Один software restart без INSTALL](../../docs/evidence/hardware/2026-11-25-esp32-restart-f819955/README.md) на SDK 6.1.1 PASS:
36/36 guards на каждом из двух запусков; reset_reason=1→3, watchdog_boot=0,
request/shutdown_hook core0 → noos core1, 90 секунд после сброса без host reset.
Свежие и финальные реальные 16 МиБ/verify/две NVMe-копии проверены. Исходные factory
и otadata возвращены; все байты вне NVS совпали, full recovery 0. Сеанс закрыт
за 1103,269 с, UART закрыт; оставлен bench.4 OTA1 VALID. Финальные UART HMI/GT911/
Wi-Fi guards PASS; владелец подтвердил главный экран, касания и Wi-Fi.

Причина RTCWDT после bench.5 не установлена, его FAIL сохраняется. Один
unprovisioned factory PASS с маркерами не квалифицирует post-INSTALL restart.
Следующий P1 — сопоставимые shutdown-маркеры в настоящем настроенном пути
после подписанного CHECK/INSTALL, затем независимые reset/slot/self-test/readback
проверки. Прежние TLS/signature/SHA/compatibility/resource/time/confirmation
и watchdog guards не ослаблять; не менять драйверы/частоты без измерений.
Закрытые одноразовые пакеты не повторять, опубликованные assets не менять.

На bench.4 прежний период OTA истёк. Стендовая policy .4→.5 утверждена только
до 27 ноября 2026 08:00 Asia/Almaty; production policy/budget/discovery, RF/AP/power
loss, runtime 5.3.2 и physical FPS/latency открыты. F407 ee7d179 не подключался.
Standing isolated-board grant не разрешает CAN/движение/eFuse/option bytes.
Самостоятельный merge и новый release/tag без соответствующего review запрещены.
Ниже сохранена история; активная очередь — docs/NEXT_STAGE.md.

Закрыто PR #90 от main 10cd8cd / source 104fb06: понятный отказ OTA по
разрешённому времени проверен. Только TIME_REJECTED + TIME_RANGE + cleanup OK
даёт error24 / «Дата вне разрешённого периода OTA». До/после диапазона одинаковы;
unauthenticated SNTP не доказывает действительную дату. 290 host/290 sanitizers,
16 новых worker-сценариев, оба pinned SDK и source CI 15 PASS.

Первый стенд 396.821 с: SNTP timeout, целевая квалификация FAIL сохранена. Один
отдельно разрешённый повтор 792.933 с: точный error24/cleanup и все 45 guards PASS;
исходные OTA0/metadata восстановлены, новые actual 16 МиБ/MD5/две NVMe-копии и
финальный boot PASS. Оставлен неизменяемый bench.4 OTA1 VALID, владелец подтвердил
экран/касания/Wi-Fi. Новый UI и cancellation text ещё не входят в этот релиз.
F407 ee7d179 не открывался. Закрытые одноразовые пакеты не повторять.

Прежние contexts истекли 25 ноября 2026 08:00 Asia/Almaty; не продлевать и не
переписывать их автоматически. Для положительных OTA нужны новое решение
владельца о policy и отдельный точный пакет; published assets/key/floors прежние.
Далее P1 independent RF/AP/power loss, production policy/budget/discovery; P2
reproducibility. Никаких новых release/tag/self-merge/CAN/motion/eFuse/option bytes.
Читать docs/NEXT_STAGE.md и docs/evidence/hardware/2026-11-25-esp32-time-policy/
от корня. Ниже история закрытых этапов; она не задаёт текущую очередь.

Закрытый стенд PR #89 от main bf5c325 после PR #88: signed STA Disconnect → transport
failure/cleanup → новое подключение/CHECK → полный INSTALL в одной загрузке `.3`
PASS на неизменяемом source6c7b1d5 / SDK6.1.1. Штатный reboot/self-test/VALID,
новое чтение всех16МиБ/сверка вне NVS/две NVMe-копии PASS; оставлен bench.4 OTA1
VALID, владелец подтвердил HMI/touch/Wi-Fi. Одна host metadata write/full restore0.
Первый сеанс остановлен на раннем переподключении, восстановлен; причина пока
не установлена, FAIL сохранён. Повтор прошёл с исходным строгим guard.

274 host/274 sanitizers/24 procedure PASS.56 частичных отказов дополнены полным
authenticated retry в том же процессе; actual SDK abort/rebegin проверен.
Firmware/assets/context прежние; UI cancellation text fix ещё не установлен.
Читать docs/NEXT_STAGE.md и docs/evidence/hardware/2026-11-24-esp32-wifi-retry/
от корня. Не выдавать добровольный STA disconnect за потерю RF/AP.

Далее P1 independent RF/AP/power loss, production policy/budget/version discovery;
P2 reproducibility. Контексты только до25ноября2026 08:00 Asia/Almaty, immutable
assets/context не менять. Новые release/tag требуют точного owner approval,
self-merge запрещён. Standing isolated-board grant не разрешает движение/CAN/
eFuse/option bytes. F407 ee7d179 не открывался; одноразовые пакеты не повторять.
Ниже исторические контракты, активный этап — NEXT_STAGE.

Текущий отдельный результат: local trust перед pending confirmation (docs/plans/P1_PENDING_TRUST.md от корня).
Не подтверждать новый OTA-slot без рабочего локального context; factory recovery
и ранее VALID без настроек сохраняются. AP/DHCP/SNTP не являются boot prerequisites.
265 host/265 sanitizers, четыре ELF обоих pinned SDK и CI30 PASS. Native a74bfcd
confirmation/INVALID rollback/полный возврат PASS; это USB fixture, не сетевой INSTALL.

Текущий P1 — docs/plans/P1_AUTONOMOUS_SIGNED_CHECK.md от корня. a29c9ed:
control/один full signed CHECK/возврат всего fecc05d PASS. IMAGE_VERIFIED/error0,
406 resource checks, SESSION local min97399bytes, largest63488, stack6896/12288,
touch0–20ms/errors0. Physical UI ночью не наблюдался.237 host/sanitizers,
analyzer/четыре ELF обоих SDK/CI30 PASS; аппаратно только6.1.1.

Finite repeatability также закрыт PASS: fresh16MiB/two-NVMe/control90s,
три150s reset CHECK без повторной записи auto-app, весь factory fecc05d/verify/
UART30.3×IMAGE_VERIFIED/error0/406checks, SESSION minima97783–99427, stack6896,
touch1–18ms/errors0. Не повторять пакет. Далее PR81 local context до pending
confirmation/native boot rollback, затем configured installable payload/INSTALL.
Published unprovisioned bench.2 read-only, не устанавливать. Production budget
и физический UI/FPS/latency ещё не приняты.

Default-off signed bench только configured rev12-ota+trace, один worker,
45s healthy Wi-Fi/2s settle/один full read-only CHECK. External CHECK/INSTALL/
REBOOT закрыты, CANCEL сохранён; ordinary profiles/metadata CHECK прежние.
Known-zero/no-TE SDK framing fix не отменяет empty200/unknown/CL+TE/duplicates/
compression/other mismatch refusals. Headers/TX bounds, CA/time/floors/monitor/
cleanup/timeouts сохраняются. Owner разрешил автономные обратимые isolated-board
checks до24ноября08:00 Asia/Almaty без вопросов о доступе; finite1800s/recovery
reserve, без eFuse/option bytes/CAN/motion/merge/нового release/INSTALL bench.2.
Key/context вне Git/CI. На F407 прежний ee7d179, readback/health/resume PASS.

Ниже исторические контракты; актуальные hardware facts — в корневых документах.

После стенда a5f8b7e: control touch_fresh FAIL (один UINT32_MAX age), TLS image
не записан, весь factory fecc05d возвращён/owner PASS, разрешение закрыто.
Читать docs/plans/P1_TOUCH_SNAPSHOT_CLOCK.md от корня. В actual main freshness
и diagnostics копировать touch snapshot до consumer clock; не clamp age и не
ослаблять100/500ms. Исправление ещё не hardware-qualified; новых devices нет.

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
