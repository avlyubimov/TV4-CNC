# PR-04j — isolated UTC/TLS diagnostic image

**rev12-tls-bench** — новый профиль, flag TV4_UTC_TLS_BENCH default n. Только
его sdkconfig.tls-bench overlay включает запросы. Mutually exclusive с
rev12-memory-bench, зависит от Rev1.2/PSRAM/CA bundle. Existing defaults/rev12
не меняются, validator отвергает неверные flags/отсутствие CA. CI: четыре
профиля × SDK6.1.1/5.3.2. Это не runtime OTA Manager и не обычный HMI.

## Последовательность

Через15s после UI ready main только schedules one-shot priority1 worker,
stack6144bytes. До создания нужны internal free>=24576/largest>=8192. Это guard
для isolated test, не доказанный TLS production reserve. При отказе HMI работает.

Worker ждёт существующий Wi-Fi до15s, затем:

1. SDK local heap minimum window для SNTP, sync timeout10s, stop, clock sample.
2. Private **2026-only bench policy**: UTC2026-01-01..2026-12-31, age60s/skew2s,
   allow_unauthenticated_sntp=true. Actual SNTP success обязателен; это acceptance
   window, не build date/fallback и не production policy. За пределами года — fail.
3. Только при source/cleanup/monitor success — отдельное TLS heap window.
4. Один verified HTTPS **HEAD https://github.com/avlyubimov/TV4-CNC**. Штатный
   CA bundle, hostname/date verification и fresh time anchor, auth NONE,
   automatic redirects off, no cookie/custom credentials, keepalive off.
   Buffer RX4096/TX512, SDK I/O<=5s, logical request budget15s.
5. Только headers/status, expected200. Не читает/не хранит response body,
   не парсит release/manifest/image, не следует redirect и не вызывает OTA session.
6. Cleanup client и heap monitor, revoke anchor, final pass/stage + worker exit.
   No retry. Новый boot снова допускает одну такую пробу.

SNTP использует pool.ntp.org/DNS/UDP123, меняет global UTC, provenance остаётся
SNTP_UNAUTHENTICATED. Это не NTS/authenticated UTC. Не запускать рядом с другим
clock updater/TLS client, SDK SNTP/heap monitor имеют одного owner. Обычный
Wi-Fi HMI продолжает свою работу; UTC в UI пока не отображается как telemetry.

## Что измеряется

Используется heap_caps_monitor_local_minimum_free_size_start/stop из обоих SDK.
Две windows отделяют startup historical low от текущих lows. Во время window
существующая панель/JSON diagnostics тоже видит **window minimum**, не startup
minimum; после stop возвращается historical tracking. Логи явно размечают окна.
SDK monitor имеет небольшой bookkeeping allocation, входит в overhead пробы.
Других пользователей global monitor в этом image быть не должно.

Snapshot: internal free/min/largest, PSRAM free/min, stack watermark. SDK minimum
агрегируется по heaps и включает **все параллельные** HMI/Wi-Fi/инструментацию.
Это window observation, не точное attribution одного TLS allocation и не
production memory budget. Largest block — только sample в конце/начале, не
continuous minimum. Не объявлять leak по разнице free при concurrent activity.

HEAD backend проверяет headers budget16KiB/key128/value8192, их значения не
логирует. SDK может обрабатывать headers до callback/check; лимит не гарантирует
pre-allocation cap внутреннего parser. Время/логический deadline проверяется
до/после I/O и после cleanup; нет hard preemption DNS/TLS/SDK call. Не обходить
CA/hostname/time или переходить на HTTP ради PASS. HTTP non200 фиксируется как
отказ, redirects не выполняются, числовые SDK/TLS/cert flags — diagnostic data. Для6.1.1 читаются client getter,
для5.3.2 — DISCONNECTED error handle по официальному примеру SDK. Если5.3.2
не дал details, tls_error/cert_flags=-1 означают UNKNOWN, не успешную проверку.

Endpoint/root HEAD выбран только для handshake/header memory probe, не расширяет
allowlist OTA downloader. Он не проверяет GitHub asset/CDN redirect chains,
подпись/совместимость/установку. HEAD PASS не разрешает OTA или приводы.

## Проверки без платы

[Evidence](evidence/build/utc-tls-bench/README.md). Host mock tests для HEAD lifecycle:
time/cancel/deadline/open/header/status/cleanup/final clock change, no redirects.
Profile gate/CA bundle/mutual exclusion проверяются Python negative tests.
Остальные UI/STM32/OTA tests сохранены. Native SDK compiled/linked; [аппаратный результат b085abb — ниже](#аппаратный-результат-19-ноября-2026).

Сборка: ./tools/build_idf.sh rev12-tls-bench 6.1.1 local
(установленный SDK) либо 5.3.2 docker (pinned image). Build script не прошивает.
App/layout остаются factory2MiB/Flash16MB/PSRAM8MB, без OTA A/B migration.

## Следующий hardware gate

После review и зелёного CI подготовить точный SHA/hash профиля и повторно
проверить backup. Получить отдельное разрешение на **новую USB запись, UTC update,
DNS/UDP/HEAD traffic, reset и один UART capture до90s** на стенде только USB/
питание, без CAN/исполнителей. Прежнее разрешение на1c38d1e не действует здесь.

UART open/close может reset: не обещать read-only/no-reset, не переоткрывать
ради второй выборки, не объединять разные boot. Собрать stage markers/source/
HTTP status/TLS/cert errors, monitor_stop=0, final pass и monotonic diag sequence.
Проверить HMI/touch/Wi-Fi у владельца. При source/time/OOM/CA/status/monitor failure
сохранить результат и остановиться; не менять reserves/security автоматически.

До теста 19 ноября на плате оставался memory-bench1c38d1e. Bench и возврат
ordinary b085abb отдельно согласованы и выполнены; текущий статус ниже. Defaults всегда probe off.
Нет Flash/eFuse/boot-selection/installer/motion/private keys/release/auto-merge.
Откат PR — revert кода, не изменение уже прошитого устройства.

## Аппаратный результат — 19 ноября 2026

[Evidence b085abb](evidence/hardware/2026-11-19-b085abb-utc-tls/README.md):
SNTP PASS; TLS OPEN FAIL, mbedtls_ssl_setup -0x008D (insufficient memory).
Verified HEAD/HTTP200/certificate success не достигнуты. Оба monitor cleanup PASS.
Нужен отдельный allocator/PSRAM/memory review; не уменьшать проверки/reserves
ради PASS. После одного90s capture возвращён ordinary rev12 того же b085abb,
3/3 hashes verified; владелец подтвердил HMI/touch/Wi-Fi. Новых аппаратных
операций это evidence не разрешает, production TLS budget остаётся блокирован.

## Исправление после allocation failure

[PR-04k: review allocator/проверки дат](TLS_MEMORY_REVIEW.md) выбирает PSRAM-only
allocator и включает X.509 notBefore/notAfter только в TLS overlay. TLS records
16384/4096 и internal DMA reserve64KiB сохранены. Исходный b085abb имел local
UTC anchor, но SDK certificate-date check был выключен; старую формулировку
«time-checked» не считать доказательством X.509 validity check. На момент
software PR новый image не испытывался; последующая проба — ниже.
Production TLS gate остаётся открытым.

## Повторная аппаратная проба — 1bfc7e2

[19 ноября, после PR #22](evidence/hardware/2026-11-19-1bfc7e2-tls-psram/README.md):
SNTP PASS, TLS HEAD HTTP200/cleanup/monitors PASS. Запас ограничен: internal
min3123/window,2408/history,stack watermark912bytes. Это не production budget
и не body/OTA install acceptance. Возвращён ordinary rev12 1bfc7e2,owner HMI PASS;
новых аппаратных операций отчёт не разрешает.

## PR-04l — HTTP/stack headroom

[Новый software review](TLS_HEADROOM.md) уменьшает HTTP RX до2048 и убирает
полный Wi-Fi snapshot из стека TLS worker. Добавлены http_* phase snapshots,
не из SDK callbacks. Старый transport PASS1bfc7e2 и его minima остаются
историческими измерениями; нового runtime headroom/performance результата нет.
