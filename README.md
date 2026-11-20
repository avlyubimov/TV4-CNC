# TV4 CNC

Развиваемая система ЧПУ для ТВ-4-07-20 КА с сохранением ручного управления.
**Инженерный прототип: реальное управление приводами ещё не реализовано
и не введено в эксплуатацию.** Существующий HMI продолжается, не создаётся заново.

## Архитектура и оборудование

- ESP32-S3 Waveshare ESP32-S3-Touch-LCD-7 Rev 1.2: 800×480, RGB LCD/GT911,
  Flash 16 МБ, PSRAM 8 МБ по сообщению ROM/esptool. ESP-IDF 6.1.1, LVGL 9;
  5.3.2 — проверенный резерв. HMI, Wi-Fi, будущие microSD и OTA.
- Motion: **DevEBox STM32F4VX_M Ver. 2.0 получена**, владелец уточнил STM32F407VGT6;
  владелец подтвердил SWD/ST-Link V2, Cortex-M4r0p1, ID0x101f6413, Flash1024KiB,
  3,252V и backup1048576bytes/vector structure PASS. Package/RAM/CCM/HSE/
  секторы/pinout/GPIO/CAN ещё требуют проверки. STEP/DIR будет на таймерах STM32,
  не на ESP32. PR #7 слит: переносимое ядро/host tests; board image пока нет.
- Blue Pill STM32F103C8T6 — отдельный bench: владелец подтвердил ST-Link V2,
  OpenOCD 0.12.0, SWD/64 КиБ Flash и локальный backup. Не замена F407.
- Один closed-loop NEMA23 заказан и ещё не получен; второго двигателя и
  CAN-трансиверов нет. Механика/энкодеры/приводы не введены в эксплуатацию.

Доказательства и неизвестные: [аппаратный статус](docs/HARDWARE_STATUS.md),
[открытые вопросы](docs/OPEN_QUESTIONS.md). Независимый аппаратный E-stop и
блокировки ручного/CNC режима обязательны; сенсорный STOP их не заменяет.

## Текущее состояние

В main есть simulation-only HMI, инструменты, журнал/диагностика, Wi-Fi с
сохранением профилей и стартовый экран. Обе версии SDK × headless/rev12
проверяются CI. Фото владельца подтверждают изображение HMI и список сетей;
на b93cf3b подтверждены базовые касания, удобная клавиатура, показ пароля и
Wi-Fi/DHCP. Полный GT911/FPS/touch-to-display/soak ещё не подтверждены. Фабричный backup 16 МиБ сохранён и SHA-256
проверен; restore не испытан. Бэкапы/пароли/окружения не включены в Git.

В main слиты PR #5 (координаты/Wi-Fi), #6 (архитектура), #7 (STM32 foundation),
#8 (HMI диагностика), #9 (OTA verification) и #10 (native manifest policy), #11 (image stream), #12 (HTTPS/envelope). Их слияние не подтверждает
аппаратные испытания или готовность установки OTA.

## Продолжение разработки

1. Прочитать [AGENTS.md](AGENTS.md), [архитектуру](docs/ARCHITECTURE.md),
   [roadmap](docs/ROADMAP.md) и scoped AGENTS изменяемого модуля.
2. Проверить main/открытые PR/CI, определить короткую задачу и критерии.
3. Изменять в отдельной ветке; PR с проверками, ограничениями и откатом.
   Merge, прошивка, релиз и необратимые настройки — только с соответствующим
   отдельным разрешением владельца.

PR #13 слит: [read-only OTA session](docs/OTA_SESSION.md), без runtime UI/install.
Разрешённая USB-запись b93cf3b и [первый аппаратный baseline](docs/evidence/hardware/2026-11-18-b93cf3b/README.md)
выполнены. [PR-04f: PSRAM workspace](docs/OTA_MEMORY.md) и PR #16 слиты.
[Разрешённый memory bench 1c38d1e](docs/evidence/hardware/2026-11-18-1c38d1e-psram/README.md)
прошёл три цикла allocation/pattern/release; это историческая проба.
PR #18 добавил [проверку времени TLS](docs/TLS_TIME_GATE.md). [PR-04i: SNTP source](docs/SNTP_SOURCE.md) слит. Текущий
[PR-04j: отдельный UTC/TLS bench](docs/UTC_TLS_BENCH.md) запускает проверки
только в специальном diagnostic image, не в обычном HMI.
[19 ноября b085abb испытан](docs/evidence/hardware/2026-11-19-b085abb-utc-tls/README.md):
SNTP PASS, TLS context allocation FAIL (insufficient memory). На плату возвращён
ordinary rev12 b085abb. [Исправление TLS allocator/X.509 dates](docs/TLS_MEMORY_REVIEW.md)
слито в PR #22; [повторный test1bfc7e2](docs/evidence/hardware/2026-11-19-1bfc7e2-tls-psram/README.md)
дал SNTP/verified HEAD200 PASS. На завершение той пробы вернули ordinary rev12 1bfc7e2;
владелец подтвердил HMI/touch/Wi-Fi. Актуальная запись/следующий test — ниже.
Internal heap/stack запас остаётся ограниченным,
production TLS/OTA gate требует review; остальные
[аппаратные проверки](docs/HMI_BENCH_GATE.md) также открыты.

## Обязательные следующие подсистемы

PR #29–33 DATA/admission/handoff/model preflight/completion слиты в main `455ccc6`.
[PR-07e completion/outcome](docs/STM32_UPDATE_OUTCOME.md) связывает
проверку signed image с clean lease close и bounded доставкой исторического
результата/ACK отправителю. Pending cleanup не даёт success, quarantine не
забывает resource. Peer report не аутентифицирован и не разрешает install/motion.

В PR #34 слит [PR-05b release policy review](docs/RELEASE_POLICY_REVIEW.md): strict
Unicode/Base64, общий signed C/Python corpus, версии/каналы/component/local-context
проверки. Это не production Motion manifest, назначение версий/ключей или installer.

Текущая ветка [PR-08a: сведения о ПО](docs/FIRMWARE_INFO_UI.md), база main `b56986f`:
«Параметры» → Wi-Fi / «ПО и обновления», local build/SDK/profile с явным
provenance, неизвестные STM32/CAN версии. Проверка/установка обновлений ещё не
настроены; сетевого updater или записи Flash этот экран не запускает.

[OTA ESP32 A/B и F407 по CAN](docs/OTA_ARCHITECTURE.md),
[GitHub Releases/подписи/owner approval](docs/RELEASE_PROCESS.md),
STM32 foundation, microSD/FAT32 и единый экран «Параметры → Обновления»
запланированы отдельно. Пока не заявляются реализованными. Автопроверка
обновлений допустима, автоматическая установка без оператора — нет.

Пользовательский интерфейс MVP — русский ru-RU, централизованные строки;
kk-KZ/en-US предусмотрены архитектурно, но не включены частично.

[PR-04l: TLS memory/stack headroom](docs/TLS_HEADROOM.md) уменьшает только
HTTP RX и собственный frame diagnostic worker.
[Headroom test8f0d899](docs/evidence/hardware/2026-11-19-8f0d899-headroom/README.md)
дал HEAD200 PASS,internal window min5307/worker watermark1840bytes. Теперь на
плате **ordinary rev12 8f0d899**,owner HMI/touch/Wi-Fi PASS; production budget открыт.

[PR-04m: resource-checked OTA session](docs/OTA_RESOURCE_GUARD.md) добавляет
проверки local heap/stack policy на границах read-only сессии, без runtime
caller/production floors. На плате остаётся ordinary8f0d899.

[PR-05a: review-кандидаты сборок](docs/BUILD_CANDIDATES.md) связывают clean
source SHA/tree, app/ELF/config hashes и точный SDK/profile; CI сохраняет
проверяемый metadata-only artifact каждого из8 вариантов. Actions закреплены
полными SHA. Подпись/публикация binary/релиза и установка ещё не включены.

[STM32 startup foundation](docs/STM32_STARTUP.md) продолжает mainb19a40b после
PR #1–27: owner SWD facts, generic init/reset/no-I/O и synthetic link/fault tests.
Реальная board memory/vector/clock map и CAN bootloader ещё не утверждены;
следующая последовательность — [NEXT_STAGE](docs/NEXT_STAGE.md). Плата не прошивается.

PR #28–30 со startup, [update transport](docs/STM32_UPDATE_TRANSPORT.md) и
crypto admission слиты. Host/LVGL27 suites и sanitizers27 проверяют текущую
ветку с completion/outcome; [evidence](docs/evidence/build/stm32-update-outcome/README.md).
Это software foundation, без физического CAN, Flash writer и board bootloader.
