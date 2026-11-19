# Реализация: main, открытые PR и аппаратные границы

Актуализация после mainb19a40b / PR #27: owner SWD F407/core/Flash1024KiB/
backup подтверждены в [HARDWARE_STATUS](HARDWARE_STATUS.md). PR-02b добавляет
[startup/sections/no-I/O building blocks](STM32_STARTUP.md), synthetic ARM link
checks и сохраняет board image gate; физического запуска/Flash writer нет.
Следующий этап — [NEXT_STAGE](NEXT_STAGE.md). Ниже сохранены исторические снимки.

Снимок PR-01 от 18 ноября 2026. База: main
`0c4168e864d4c867ba0f49a86eba72c0d59205a1`. Это не утверждение, что все
коммиты рабочей ветки уже слиты. Отчёт: [PR01_REVIEW.md](PR01_REVIEW.md).

## Есть в main

- ESP-IDF 6.1.1 и резерв 5.3.2; headless/rev12, Flash 16 МБ, factory app
  2 МиБ, LVGL 9.2.2, esp_lcd_touch 1.2.1, GT911 1.1.3, точные per-SDK locks.
- Русский simulation-only UI 800×480, ограниченная очередь, модель X/Z,
  инертный transport_send и 4-byte host heartbeat с тайм-аутом 500 мс.
- Прямой RGB/GT911/CH422G адаптер с USB_SEL=0, один framebuffer в PSRAM,
  startup screen/проверка touch, отдельный polling и единственный LVGL owner.
- Главная, библиотека четырёх инструментов с черновиком/валидацией/клавиатурой,
  Диагностика и журнал RAM 32 записи; коррекции инструментов не управляют осью.
- Виртуальные обороты и ограничение подачи, расчёт мм/об, STOP/fault/ACK и
  отдельный явный restart simulation; никакой физической передачи команд.
- Wi-Fi worker, список/профили восьми сетей в NVS, автоподключение/забывание.
  Код есть, но успешное подключение/сохранение на плате ещё не подтверждены.
- Host tests (build validation, core, Wi-Fi profiles, render, RGB565), sanitizers,
  CI docs/host/SDK matrix, runtime health logs и базовые UI метрики.

[Сборка](../firmware/esp32-hmi/README.md), [Wi-Fi](WIFI.md),
[загрузка](BOOT_SCREEN.md), [инструменты](SOFTWARE_STAGE_003.md),
[метрики и пробелы](RUNTIME_DIAGNOSTICS.md). 36 °C/24.1 В и часы/дата в UI —
примерные/фиксированные подписи, не аппаратные датчики или подтверждённое время.

## Изменения PR #5 включены в main

[PR #5](https://github.com/avlyubimov/TV4-CNC/pull/5) слит в main коммитом
`0c4168e864d4c867ba0f49a86eba72c0d59205a1`. Сохранены рабочий/машинный отсчёт с подтверждением
нуля детали и журналом, feed override при выборе шага, исправление возможного
источника 263, полноэкранный password editor/show-hide и текстовые ошибки Wi-Fi.
Координаты действуют только в симуляции, без перемещения физических осей.
Подробности: [WORK_COORDINATES.md](WORK_COORDINATES.md), [WIFI.md](WIFI.md).
Аппаратная проверка новых исправлений ещё требуется. PR-01 согласован с этим
main; исполняемый код и hardware settings относительно main неизменны.

## Не реализовано

STM32 target firmware/startup/linker/planner/step/encoder/bootloader; physical
CAN driver/negotiation/production wire protocol; microSD/FAT32/file manager;
GitHub release/signing pipeline; ESP32 A/B/HTTPS installer/health rollback;
F407 CAN update; единый Updates UI. Экран «Программа», непрерывный JOG,
реальные homing/limits и обработка станка тоже не готовы.
Все они запланированы в [ROADMAP.md](ROADMAP.md), а не объявлены работающими
из-за названия каталога или наличия CI сборки ESP32.

## Аппаратные результаты и ограничения

Полный ESP32 backup 16 МиБ с SHA-256 проверен; restore не испытан.
Владелец выполнил запись HMI (esptool hashes verified) и прислал фото физического
изображения/списка Wi-Fi. Пользователь сообщил ошибку 263 и неудобную клавиатуру.
Это подтверждает наблюдавшееся изображение и взаимодействие, но не полный
протокол испытаний GT911, успешный DHCP/reconnect/NVS, FPS/latency или стабильность.
Новые исправления #5 на плате после последней проверки не подтверждены.

DevEBox F407 получена, точная геометрия неизвестна; NEMA23 не получен,
трансиверов нет. Blue Pill/SWD/backup сообщены владельцем, а не испытаны агентом
в PR-01. Полная таблица: [HARDWARE_STATUS.md](HARDWARE_STATUS.md).
Условные ±1000 мм симуляции не являются ходом или soft limits станка.

## Откат и границы PR-01

PR-01 меняет только Markdown-документы/AGENTS. Откат — revert документационного
коммита, без firmware rollback или записи Flash. partitions.csv, sdkconfig,
C/C++, build scripts и workflow YAML не меняются. Бэкапы и окружения исключены
из Git. Испытания/прошивка, публикация релиза и переход к PR-02 требуют
отдельного разрешения/проверки владельца после отчёта PR-01.

## PR-02 — следующий этап

В отдельной ветке добавлены portable STM32 supervisor, host/fault tests и
compile-only Cortex-M4F библиотека. Нет startup/linker/board image, HAL/STEP
или физического CAN. Владелец сообщил STM32F407VGT6; HSE/pinout и фактическая
геометрия остаются блокерами. [Контракт и проверки](STM32_FOUNDATION.md).

## PR-03 — программная диагностика

Добавлены fixed-window histograms, ресурсы/стек/counters/reset, панель ресурсов
и worker serial JSON export. Перенесён printf из UI owner; render callback не
логирует. [Детали и оставшиеся hardware/CPU/soak задачи](HMI_PERFORMANCE.md).

## PR-04a — verification foundation

Добавлены host/SDK P-256/SHA-256 primitives, локальный валидатор подписанного
релиза и неактивный candidate A/B CSV с проверкой обоих SDK. Нет production
trust anchors, downloader/installer/rollback или записи устройства.
[Контракт, evidence и следующие части](OTA_VERIFICATION.md).

## PR-04b — native manifest policy

Общий C parser/policy проверяет exact-byte подпись до JSON allocations,
затем bounded schema/target/layout/versions/protocol/URL. Namespaced cJSON
v1.7.19 MIT закреплён SHA-256. Installer/context provisioning ещё отсутствуют.
[Описание и evidence](OTA_MANIFEST_POLICY.md).

## PR-04c — read-only image stream

Добавлены incremental dual SHA-256, ESP image format/descriptor/checksum checks,
receive/cancel/error/deadline состояния и cleanup. Нет sink/HTTP/Flash/boot commit.
[Описание и evidence](OTA_IMAGE_STREAM.md).

## PR-04d — signature envelope / HTTPS

Добавлены bounded конверт подписи и read-only HTTPS transport primitives.
Не подключены к app_main/UI, нет discovery/OTA Manager/installer/Flash writes.
[Контракт, проверки и ограничения](OTA_HTTPS.md).

## PR-04e — read-only session

Единая worker-only операция связывает metadata/envelope/policy/image checks,
общий timeout/cancel, cleanup и terminal result. Нет runtime task/UI/install.
[Контракт, evidence и board gate](OTA_SESSION.md).

## Baseline b93cf3b на Waveshare — 18 ноября 2026

По отдельному разрешению владельца записана ESP-IDF 6.1.1 rev12; 3/3 hashes
verified. Boot log подтверждает версию/PSRAM/RGB, владелец подтвердил
экран/касания/клавиатуру/показ пароля/Wi-Fi IP. [Evidence, memory blocker и
ограничение UART capture](evidence/hardware/2026-11-18-b93cf3b/README.md).
Это не OTA install/soak/commissioning и не разрешение на следующую запись.

## PR-04f — PSRAM-only session workspace

Добавлены bounded allocation/lifetime, explicit reviewed headroom policy и
ELF target sizeof check. Нет internal fallback, runtime calls или installer.
[Контракт, evidence и оставшиеся ограничения](OTA_MEMORY.md).

## PR-04g — explicit memory bench profile

Добавлен rev12-memory-bench: три bounded allocation/pattern/release cycles в
worker после 15 секунд работы UI. Обычные headless/rev12 отключены от probe.
Нет HTTPS/session_run/Flash/boot operations. [Контракт и board procedure](OTA_MEMORY_BENCH.md).

## Hardware memory bench 1c38d1e

Отдельно разрешённая USB-запись rev12-memory-bench и один60s capture выполнены.
[completed=3 pass=1, timings/heap и ограничения](evidence/hardware/2026-11-18-1c38d1e-psram/README.md).
На плате остаётся этот bench image. Runtime TLS budget/installer не включены.

## PR-04h — time gate для native HTTPS

Свежая local provenance/age/skew anchor проверяется вокруг backend I/O и final
cleanup; boolean hint не даёт native transport разрешения без anchor. Нет
SNTP/clock updates/runtime UI. [Контракт и evidence](TLS_TIME_GATE.md).

## PR-04i — bounded SNTP source

Worker-only lifecycle с deadline/cancel/cleanup и SDK TCP/IP ownership.
Mark SNTP_UNAUTHENTICATED после stop/sample/policy; нет startup/UI/TLS trigger.
[Контракт, evidence и ограничения](SNTP_SOURCE.md).

## PR-04j — отдельный UTC/TLS bench

rev12-tls-bench выполняет one-shot SNTP/verified HEAD и heap-window snapshots
в worker. Ordinary profiles off, без response body/OTA install.
[Policy, scope, evidence и hardware gate](UTC_TLS_BENCH.md).

19 ноября [разрешённый b085abb bench](evidence/hardware/2026-11-19-b085abb-utc-tls/README.md)
подтвердил actual SNTP/private policy, но TLS OPEN FAIL (-141 insufficient memory).
Нет успешного verified HEAD/HTTP200. Ordinary rev12 того же SHA возвращён,
3/3 hashes verified; владелец подтвердил HMI/touch/Wi-Fi. Это измерение отказа,
не allocator fix или разрешение runtime HTTPS/OTA; следующий gate — memory review.

## PR-04k — изолированный TLS memory fix

[TLS_MEMORY_REVIEW.md](TLS_MEMORY_REVIEW.md): native SDK external allocator,
X.509 date checks и строгая artifact validation только для TLS diagnostic image.
Остальные профили/internal reserve/record sizes сохранены. Нового аппаратного
PASS нет; на плате ordinary b085abb. Это не runtime OTA/installer integration.

## Аппаратная проверка PR-04k — 1bfc7e2

[SNTP/verified HEAD200 PASS](evidence/hardware/2026-11-19-1bfc7e2-tls-psram/README.md),
оба cleanup/monitor PASS. Ordinary rev12 того же SHA возвращён,3/3 hashes verified,
владелец подтвердил HMI/touch/Wi-Fi. Internal min3123/window,2408/history,stack912
ограничивают дальнейшее runtime OTA: нужен отдельный review запаса и failure paths.
Успешного manifest/image download/install/rollback и performance измерений нет.

## PR-04l — собственный stack frame / HTTP RX

[TLS_HEADROOM.md](TLS_HEADROOM.md): read-only try-lock Wi-Fi online getter,
HEAD HTTP RX2048 и пять worker-phase snapshots. Только diagnostic caller;
UI snapshot/NVS API и buffers TLS/DMA/stacks прежние. Нет runtime OTA integration
или нового измерения на плате. Hardware record1bfc7e2 остаётся актуальным.

## Аппаратная проверка PR-04l — 8f0d899

[Сравнение с1bfc7e2](evidence/hardware/2026-11-19-8f0d899-headroom/README.md):
SNTP/HEAD200/cleanup PASS,window internal minimum5307/history4204,stack1840bytes.
Phase snapshots показывают recorded low уже после open. Production budget/CPU/
worst-case не подтверждены. Ordinary rev12 того же8f0d899 возвращён,3/3 hashes
verified,владелец подтвердил HMI/touch/Wi-Fi. Runtime body/install/rollback нет.

## PR-04m — guarded read-only session entry

[OTA_RESOURCE_GUARD.md](OTA_RESOURCE_GUARD.md): pure policy/checker, read-only SDK
heap/stack sampler и session boundary checks с отдельным report. Проверки не
резервируют память/не прерывают SDK и не дают production budget. Нет caller
в app_main/UI/network tasks, новых native operations или изменения платы.

## PR-05a — CI review-кандидаты

[BUILD_CANDIDATES.md](BUILD_CANDIDATES.md): offline проверка полного ESP app
image и ELF digest, clean source/SDK/profile provenance, deterministic metadata
bundle/checksums и независимый verifier. Новая host suite, интеграция в8 SDK jobs,
все Actions закреплены SHA. Docker получает Git display version linked worktree.
Реализована подготовка кандидатов; signing, binary Releases, production version/
trust provisioning и установка остаются открыты. Плата ordinary8f0d899 прежняя.
