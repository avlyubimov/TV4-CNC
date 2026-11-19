# Host tests

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
с LVGL содержит19 CTest suites; Python suite имеет15 tests. Реальные8 SDK images
дополнительно проверяются build inspector/candidate verifier в firmware CI.
