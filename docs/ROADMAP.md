# План последовательной разработки TV4-CNC

Актуально на 18 ноября 2026; состояние main после слияния PR #12 —
[IMPLEMENTATION.md](IMPLEMENTATION.md). Рабочий HMI не переписывать заново.
Все новые этапы — отдельные ветки/небольшие PR, merge только с разрешения
владельца. PR-01 слит; PR #13 слит, выполнен первый разрешённый baseline b93cf3b. Таблица не является разрешением
на автоматическое выполнение/прошивку всех следующих этапов.

| PR | Содержание | Приёмка / зависимости | Статус |
|---|---|---|---|
| PR-01 Architecture & Codex Rules | Root/scoped AGENTS, hardware status, OTA/release design, open questions, актуализация документов | Docs-only diff; нет firmware/settings/partition changes; отдельный PR и отчёт | Завершён, PR #6 слит |
| PR-02 STM32 Firmware Foundation | CMake/Ninja/точный ARM GCC, CMSIS/HAL/LL, startup/linker, clock/GPIO/timer/DMA/watchdog abstractions, faults/state machine и host tests | Target/startup/linker только после идентификации MCU, Flash/RAM/кварца/секторов; иначе host foundation + явный blocker. Никаких приводов. CI size/static analysis без SWD | PR #7 слит: ядро/host tests/ARM compile-only; board image блокирован HSE/pinout/memory verification |
| PR-03 ESP32 Performance Diagnostics | Дополнить существующие метрики, reset/watchdog/reconnect/queue counters, CPU/PSRAM/stacks, export | Не дублировать dashboard/main logs. Измерить физическую touch-to-display отдельно, собрать baseline и soak/fault results | PR #8 слит: instrumentation/ресурсы/worker serial export; per-core CPU и аппаратные baseline/soak ещё открыты |
| PR-04 ESP32 OTA Foundation | Плановая USB-миграция, otadata, A/B, HTTPS, подпись/хеш/compatibility, self-test и rollback | Проверенный backup, trusted key provisioning, migration/recovery review; power-loss/bad-signature/wrong-hardware tests. Разметку не менять app OTA. Firmware install только отдельно разрешён | PR #9–13 слиты; read-only session. Runtime worker/UI/native install/rollback/USB migration ещё не реализованы |
| PR-05 GitHub Release Pipeline | Build matrix, versioned artifacts, signed manifest, checksums, release notes, prerelease/stable | Публикация только после CI и явного owner approval на SHA/tag/канал/артефакты; secrets/protected environment design; никакой фиктивной STM32 bin | Не реализован; обычный build CI уже есть |
| PR-06 microSD Support | FAT32, detect/mount/read/write/free-space/eject/error handling; G-code files, logs/settings export, OTA cache | Подтверждённая разводка Rev 1.2 и схема USB/CAN/SD; отсутствие/извлечение/повреждение не блокирует HMI; file I/O только worker | Не реализован |
| PR-07 STM32 CAN Bootloader | Независимый bootloader, sequence/chunks/ACK/retry, SHA/signature, hardware/protocol checks, recovery | Flash geometry и A/B либо staging/recovery решение; сначала host transport. SWD initial install и CAN hardware только по отдельному разрешению | Не реализован |
| PR-08 Unified Firmware Update UI | Параметры → Обновления: версии HMI/Motion/protocol, compatibility, changelog, размер, progress/result/history, check/download/install/cancel | Worker APIs PR-04/05/07; auto-check можно, auto-install нельзя. STM32 install только после подтверждённого quiescent state | Не реализован |

PR-04 не обязан ждать физического motion: ESP32 обновляется на изолированном
стенде. Но неподтверждённая механика никогда не компенсируется OTA/CI. PR-07
может проектировать и тестировать transport в host до получения трансиверов;
реальное распределение CAN ID/bitrate и GPIO требует отдельного решения.
UI-каркас обновлений допустим в своём PR, но не должен изображать несуществующие
версии motion, проверенные подписи или завершённую установку.

## Сохранённые инженерные milestones

- M0: база документов/backup. ESP32 backup завершён; Blue Pill backup сообщён;
  restore обоих, идентификация F407 и аппаратные unknowns остаются открыты.
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
