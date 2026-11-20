# Host tests

Текущий набор от main28fe7bc + PR-07c: **25 suites** (в main после #29/30 было24).
Новая boot_update_handoff suite:10 групп,54 signed end-to-end RAM transfers с
loss/duplicate и real Mbed TLS admission; boot/policy/source/RX invalidation,
clock/cancel/cleanup/reentry/release/quarantine. Root normal/sanitizers25/25,
standalone bootloader5/5. [Контракт](../docs/STM32_UPDATE_HANDOFF.md),
[evidence](../docs/evidence/build/stm32-update-handoff/README.md).
Далее сохранены исторические counts отдельных независимых PR.

PR-07b от main74a7af9: root набор22 suites =20 ранее существовавших +
boot_image_admission и boot_crypto_source. Отдельный PR #29 сюда не копируется.
Admission содержит14 групп с реальными ephemeral OpenSSL подписями против
Mbed TLS3.6.7: key/DER/metadata/compatibility/counter/stream/hash/deadline/cancel/
OOM/reentry/snapshot/alias failures; source gate —5 positive/tamper cases.
Это software tests, без actual STM32 execution/Flash или install approval.
[Контракт](../docs/STM32_IMAGE_ADMISSION.md),
[evidence](../docs/evidence/build/stm32-image-admission/README.md).

CMake + CTest, см. firmware/esp32-hmi/README.md.
Тест core проверяет старт, инертный транспорт, сброс без движения, аварийную защёлку,
таймаут 500 мс, переполнение часов/последовательности, неверные длины/версии/значения,
повтор пакета, полноту RU и fallback локали. Это стендовый формат heartbeat,
не согласованный протокол станка.
Тест render при LVGL_DIR проверяет настоящий шрифт и создаёт кадр 800×480.

`layout_checks.c` проверяет геометрию настоящего дерева LVGL: границы и переносы
подписей, пересечения текста и кнопок. Проверки выполняются на начальном экране
и после переходов в FAULT/HOLD, а также с длинной надписью отсутствия связи.

Этап ПО-002: кольцевой журнал 32 записи, вытеснение и порядок новых записей;
повтор отказа без засорения журнала; причины касания/дисплея; ручной запуск
новой симуляции после ACK. Render проверяет настоящие клики навигации и
диагностики, отказ/timeout, все страницы журнала и возврат к JOG.

ПО-003: строгий парсер mm→µm, границы int32/геометрии, атомарность и запреты
изменений; тесты настоящей клавиатуры/отмены/сохранения/выбора резца, отсутствие
изменения координат и обновление карточки Главной. Проверка глифов применяется
ко всем видимым подписям: placeholder-квадраты приводят к ошибке теста.

SDK-миграция: четвёртый тест build_validation проверяет положительный пример
инспектора и отказы для Flash, профиля, PSRAM, image header/chip id и
переполнения раздела. Используются синтетические файлы, не прошивка устройства.
Host CI и локальная проверка также проходят ASan/UBSan/LSan.

PR-05a: build_candidate — отдельная Python suite для offline review pipeline:
оба SDK ×4profiles на synthetic images, checksum/digest/segment/descriptor/ELF
faults, stale outputs/source, scope/profile mixups, strict JSON/file limits,
symlink/FIFO/extra files, deterministic inventory и CLI. Docker version forwarding
проверяется mock command без запуска container/SDK/device. Полная host матрица
с LVGL содержит19 CTest suites; Python suite имеет16 tests. Реальные8 SDK images
дополнительно проверяются build inspector/candidate verifier в firmware CI.

PR-02b: motion_startup_noio проверяет data/BSS/zero/alignment/overflow/overlap,
write-free errors и запрет readiness реального supervisor с incomplete no-I/O
adapter. Полный root набор теперь20 CTest suites. Opt-in ARM motion_link_contract
отдельно выполняет11 unittest cases: positive ELF/SP/Thumb/LMA/VMA/stack и
negative memory/stack/constructor/corruption; ARM-код нигде не исполняется.

PR-07a: update_session / update_loopback добавляют portable sender/receiver и
RAM-only fault injection. Полный root набор теперь 22 CTest suites (включая
LVGL и sanitizer variants). Есть 17 unit groups, CRC known vectors и collision,
identity/cursor/length/retry/deadline/abort/clock checks; 2049 memory transfers
с потерями DATA/ACK/final ACK, corruption/reorder/pending duplicates; 10 volatile
reset cuts и truncated/partial consumer failures. Стандартный OpenSSL 3 SHA
oracle проверяет полученные байты и отвергает изменённый payload с валидной CRC.
Это не embedded signature verifier, physical CAN или power-loss Flash test.
Конечный статус только COMPLETE_UNVERIFIED; runtime/install caller отсутствует.
Контракт — [STM32_UPDATE_TRANSPORT](../docs/STM32_UPDATE_TRANSPORT.md),
результаты — [evidence](../docs/evidence/build/stm32-update-transport/README.md).
