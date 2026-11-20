# План последовательной разработки TV4-CNC

Обновлено от main `455ccc6`, 20 ноября 2026; PR #1–33 слиты, пять main workflow PASS.
История и текущее ПО — [IMPLEMENTATION.md](IMPLEMENTATION.md); следующий
STM32 этап — [NEXT_STAGE.md](NEXT_STAGE.md). Рабочий HMI не переписывать заново.
Все новые этапы — отдельные ветки/небольшие PR, merge только с разрешения
владельца. PR-01 слит; PR #13 слит, выполнен первый разрешённый baseline b93cf3b. Таблица не является разрешением
на автоматическое выполнение/прошивку всех следующих этапов.

| PR | Содержание | Приёмка / зависимости | Статус |
|---|---|---|---|
| PR-01 Architecture & Codex Rules | Root/scoped AGENTS, hardware status, OTA/release design, open questions, актуализация документов | Docs-only diff; нет firmware/settings/partition changes; отдельный PR и отчёт | Завершён, PR #6 слит |
| PR-02 STM32 Firmware Foundation | CMake/Ninja/точный ARM GCC, reviewed startup/linker, clock/GPIO/timer/DMA/watchdog abstractions, faults/state machine и host tests | Реальный board profile после package/RAM/vector/clock review; секторная карта нужна для bootloader/Flash writer. Никаких приводов. CI без SWD | PR #7 слит. Owner подтвердил SWD/core/Flash1024KiB/backup; PR-02b: startup objects/no-I/O/sections + synthetic link tests. Board image ещё блокирован, HSE/GPIO/CAN не проверены |
| PR-03 ESP32 Performance Diagnostics | Дополнить существующие метрики, reset/watchdog/reconnect/queue counters, CPU/PSRAM/stacks, export | Не дублировать dashboard/main logs. Измерить физическую touch-to-display отдельно, собрать baseline и soak/fault results | PR #8 слит: instrumentation/ресурсы/worker serial export; per-core CPU и аппаратные baseline/soak ещё открыты |
| PR-04 ESP32 OTA Foundation | Плановая USB-миграция, otadata, A/B, HTTPS, подпись/хеш/compatibility, self-test и rollback | Проверенный backup, trusted key provisioning, migration/recovery review; power-loss/bad-signature/wrong-hardware tests. Разметку не менять app OTA. Firmware install только отдельно разрешён | PR #9–13 слиты; read-only session. Runtime worker/UI/native install/rollback/USB migration ещё не реализованы |
| PR-05 GitHub Release Pipeline | Build matrix, versioned artifacts, signed manifest, checksums, release notes, prerelease/stable | Публикация только после CI и явного owner approval на SHA/tag/канал/артефакты; secrets/protected environment design; никакой фиктивной STM32 bin | PR-05a: metadata-only bundles/exact source/pinned Actions. PR-05b: encoding fixes/shared signed C/Python policy corpus; actual versions/keys/signing/publication ещё открыты |
| PR-06 microSD Support | FAT32, detect/mount/read/write/free-space/eject/error handling; G-code files, logs/settings export, OTA cache | Подтверждённая разводка Rev 1.2 и схема USB/CAN/SD; отсутствие/извлечение/повреждение не блокирует HMI; file I/O только worker | Не реализован |
| PR-07 STM32 CAN Bootloader | Независимый bootloader, sequence/chunks/ACK/retry, SHA/signature, hardware/protocol checks, recovery | Flash geometry и A/B либо staging/recovery решение; сначала host transport. SWD initial install и CAN hardware только по отдельному разрешению | PR #29–33 DATA/crypto/handoff/preflight/completion/lease close и historical outcome/ACK слиты. Production metadata/Flash/recovery/physical CAN/install ещё не реализованы |
| PR-08 Unified Firmware Update UI | Параметры → Обновления: версии HMI/Motion/protocol, compatibility, changelog, размер, progress/result/history, check/download/install/cancel | Worker APIs PR-04/05/07; auto-check можно, auto-install нельзя. STM32 install только после подтверждённого quiescent state | Не реализован |

PR-04 не обязан ждать физического motion: ESP32 обновляется на изолированном
стенде. Но неподтверждённая механика никогда не компенсируется OTA/CI. PR-07
может проектировать и тестировать transport в host до получения трансиверов;
реальное распределение CAN ID/bitrate и GPIO требует отдельного решения.
UI-каркас обновлений допустим в своём PR, но не должен изображать несуществующие
версии motion, проверенные подписи или завершённую установку.

## Сохранённые инженерные milestones

### PR-05b — release metadata policy review от main455ccc6

[Review](RELEASE_POLICY_REVIEW.md): malformed Unicode hex gate и canonical Base64
offline parity;115 signed C/Python scenarios,2809 SemVer comparisons,65536 BMP
escapes и130 Base64 cases. Host29/sanitizers29/native analyzer3units и local
ESP-IDF6.1.1 headless/rev12 PASS; CI matrix8 и STM32 gates сохранены.
Actual local versions/keys, production Motion schema, layout migration/installer
и hardware commissioning остаются открыты. Owner повторил VGT6, HSE не сообщил.

### Исторический снимок PR-07b от main74a7af9 — 20 ноября2026

[Независимый image admission](STM32_IMAGE_ADMISSION.md) в этой ветке:
подпись P-256/raw metadata, strict draft parser/local compatibility/counters,
SHA/exact EOF, bounded arena/deadline/cancel, cleanup и ARM compile checks.
На момент подготовки PR #29 transport был открыт; код admission не зависел от него. Теперь оба PR слиты. Есть только
VERIFIED_CANDIDATE, без board image/Flash/CAN/commit/jump/runtime.
Дальше нужны утверждённый release metadata/keys/version context, интеграционный
session/quiescent contract и verified erase/vector/recovery перед writer.
Аппаратные milestones ниже этим software результатом не закрываются.

- M0: база документов/backup. ESP32 backup завершён; Blue Pill и F407 backups
  сообщены; F407 SWD/core/Flash1024KiB подтверждены владельцем. Restore/SHA F407,
  маркировка/RAM/HSE/секторы и аппаратные unknowns остаются открыты.
- M1: работающий HMI/software simulation и матрица SDK; display наблюдался,
  Wi-Fi input/connect fix — PR #5; commissioning сенсора/метрик ещё не завершён.
- M2: CAN bench и fault injection; сейчас только inert host envelope,
  трансиверов нет. Никаких физических CAN испытаний не заявлять.
- M3: STM32 foundation и bench timer/DMA measurements без приводов.
- M4: квалифицированная safety wiring review, interlocks, homing/limits,
  подтверждённые направления и поосевое commissioning с отдельным разрешением.
- M5: энкодер шпинделя/servo/thread synchronization после механических измерений.
- M6: indexing/C-axis/removable milling — последующие отдельно проверяемые задачи.

OTA, microSD и CI/CD поддерживают эти этапы, но не являются доказательством
безопасности реального станка. Исторический CODEX_TASK_001 больше не активный план.

## Hardware baseline b93cf3b

[18 ноября: разрешённая USB-запись и результаты](evidence/hardware/2026-11-18-b93cf3b/README.md).
Basic HMI/input/Wi-Fi DHCP PASS; internal largest block 12288 bytes блокирует
простое добавление HTTPS/session в internal RAM. PR-04f/04g слиты; [isolated allocation bench 1c38d1e прошёл три цикла](evidence/hardware/2026-11-18-1c38d1e-psram/README.md). Production TLS memory budget ещё не утверждён; Gate 1 soak/negative tests,
UART capture/reset procedure и Gate 2 ещё не завершены.

## PR-04h — время для TLS

[Local provenance/age/skew gate](TLS_TIME_GATE.md) и guard вокруг каждого
HTTP operation, SDK clock sampler. Источник UTC/SNTP service, production policy,
UI и runtime TLS profiling ещё не включены; аппаратных операций нет.

## PR-04i — SNTP source

[Bounded source](SNTP_SOURCE.md) готовится отдельным PR после #18. Нет
startup/UI invocation или production time policy. Следующий шаг — opt-in
UTC/TLS memory profiling с отдельным разрешением на точный image и network test.

## PR-04j — явный UTC/TLS memory bench

[Отдельный профиль](UTC_TLS_BENCH.md), две heap windows, один HEAD без body.
PR #20 слит; [19 ноября b085abb испытан](evidence/hardware/2026-11-19-b085abb-utc-tls/README.md):
SNTP PASS, TLS context allocation FAIL (-141 insufficient memory). Возвращён
ordinary rev12 b085abb. Следующий шаг — review allocator/PSRAM/TLS memory
с host/build проверками; новое аппаратное испытание требует нового разрешения. Production TLS budget не объявлять по одному
снимку/endpoint: нужны ошибки/redirect-hosts/soak и ownership/performance review.

## PR-04k — TLS PSRAM allocator / X.509 dates

[Review и профиль](TLS_MEMORY_REVIEW.md): исправление причины b085abb allocation
failure через штатный SDK PSRAM-only allocator. Только TLS bench; ordinary
profiles/reserves/record sizes не меняются. Включена SDK certificate-date check,
которую local anchor не заменяет. Software matrix/negative/ELF checks; новый
hardware gate отдельно, до него runtime TLS/production budget остаются блокированы.

## Hardware gate PR-04k — 1bfc7e2

[Разрешённый TLS/PSRAM test](evidence/hardware/2026-11-19-1bfc7e2-tls-psram/README.md)
дал SNTP/verified HEAD200/cleanup PASS; возвращён ordinary1bfc7e2,owner HMI PASS.
Production reserve не утверждён: internal min3123/window,2408/history и stack
watermark912bytes. Следующий программный шаг — bounded memory/stack review
перед runtime session/body. Не подключать installer или снижать CA/time/DMA
reserve ради progress; новые hardware/fault/soak tests только отдельно согласовать.

## PR-04l — TLS worker headroom

[Изолированное исправление](TLS_HEADROOM.md): compact Wi-Fi online accessor
вместо UI snapshot в стеке, HTTP RX2048, finite stage diagnostics. Stack6144,
TLS16384/4096 и DMA reserve64KiB прежние. Software checks не закрывают runtime
memory/stack/CPU gate; следующее exact-image испытание отдельно согласовать.

## Hardware gate PR-04l — 8f0d899

[Разрешённый test](evidence/hardware/2026-11-19-8f0d899-headroom/README.md):
SNTP/HEAD200/cleanup PASS,window internal min5307/stack watermark1840bytes,
против3123/912 у1bfc7e2. Вернули ordinary8f0d899,owner HMI/touch/Wi-Fi PASS.
Следующий программный шаг — review remaining handshake/internal allocations/
call stack и bounded resource admission/failure перед runtime OTA session/body.
Не превращать один положительный test в production reserve/soak approval.

## PR-04m — bounded resource admission

[Resource guard](OTA_RESOURCE_GUARD.md): reviewed local policy/current heap/stack,
обязательные inputs нового guarded session entry, sticky failure/candidate revoke
на phase/chunk boundaries. Legacy API и target workspace34000 сохранены.
Нет runtime worker/body/install activation; native budget/load/fault gates открыты.

## PR-05a — сразу несколько частей подготовки релизов

[Build candidates](BUILD_CANDIDATES.md): полный offline ESP app parser, связь
app↔ELF/clean source/display version, детерминированный metadata-only bundle,
независимый verifier и negative tests, интеграция в8 CI jobs и SHA-pinned Actions.
Обычная factory2MiB layout остаётся; SemVer/key/license/release approval и runtime
OTA/migration gates не подменяются успешным review artifact. Аппаратных операций нет.

## PR-02b — после owner SWD F407

[Owner evidence](evidence/hardware/stm32f407-owner-swd/README.md) закрывает прежнее
UNKNOWN по SWD/core/device ID/Flash capacity. [Startup foundation](STM32_STARTUP.md)
проверяет portable init/no-I/O и Cortex-M4 reset/link sections на synthetic map,
сохраняя блокировку board image. Следом нужен reviewed RAM/vector/clock/board
contract; HSE можно отложить при отдельно принятом HSI-only решении. CAN bootloader
и OTA sectors не резервируются до geometry/recovery review; motion запрещён.

## PR-07a — две стороны транспорта до Flash и CAN hardware

[Ограниченный logical session](STM32_UPDATE_TRANSPORT.md): stop-and-wait,
двухфазный consumer/ACK, exact duplicate с проверкой payload, CRC-32C,
explicit retries/total/progress deadlines, identity/cursor checks и sticky faults.
Host/sanitizer tests проверяют потери DATA/final ACK, reorder/corruption,
partial consumer/volatile resets и отсутствие automatic resume. Новый модуль
ARM compile-only, независим от motion. Нет runtime caller или installer.
Этот transport gate был продолжен независимым admission в PR #30;
hardware/Flash layout/recovery/quiescent/wire decisions остаются открытыми.

## PR-07c — transport/source lifetime

[Read-only handoff](STM32_UPDATE_HANDOFF.md) от main28fe7bc связывает слитые
PR #29/30: immutable lease, policy/boot/source guards, descriptor↔signature/SHA,
receipt revoke и bounded cleanup/quarantine. Root25/sanitizer25/standalone5,
ARM/static/image/output gates; hardware milestones не закрываются.
Следующий software review — production metadata/context и protocol/source
transition после bounded RX completion. Writer/jump ждут board/erase/vector/
recovery/fresh physical quiescent/operator gates; Flash addresses не резервируются.

## PR-07d — проверка модели образа до board map

[SP/PC/vector checker](STM32_IMAGE_PREFLIGHT.md) от main4b7631e использует только
explicit local model; источник остаётся подписанным immutable snapshot. Новая
profile suite, signed vector/fault tests, ARM/static/import/frame и прежние
8 SDK builds. MODEL_MATCH не закрывает hardware/Flash/recovery milestones и
не разрешает writer/jump. Следующие review: production release/context/profile
и verified board memory/vector/clock/erase/protection без предположения адресов.

## PR-07e — завершение software-проверки и доставка истории

[Completion/outcome](STM32_UPDATE_OUTCOME.md) от maine0bd84e: actual crypto/model
checks → lease close → historical report/ACK, bounded repeats/linger/deadline,
context/reboot/conflict и quarantine. Host27/sanitizers27/standalone7, ARM/static/
source/output gates и8 SDK builds. Next: reviewed production negotiation/codec/
identity и metadata/context, actual board/Flash/recovery; не activation по peer report.
