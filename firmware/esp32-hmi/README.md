# HMI ESP32-S3 · симуляция

Поддерживаемые SDK: **ESP-IDF 6.1.1** и проверенный резерв **5.3.2**.
Оба профиля собираются; изображение HMI наблюдалось на плате по фото владельца.
Полная аппаратная проверка GT911/сети/performance ещё впереди: docs/HARDWARE_STATUS.md.
По умолчанию headless выполняет полный UI без GPIO/I²C/RGB/GT911/CAN.
Rev12 включает дисплей, касание и сетевую worker-задачу Wi-Fi. Команд двигателям/реальному шпинделю нет.

## Сборка на Ubuntu установленным SDK

Из корня проекта:

```sh
TV4_IDF_PATH="$HOME/.espressif/v6.1.1/esp-idf" ./tools/build_idf.sh headless 6.1.1 local
TV4_IDF_PATH="$HOME/.espressif/v6.1.1/esp-idf" ./tools/build_idf.sh rev12 6.1.1 local
```

Можно использовать активный IDF_PATH; скрипт проверит точную версию.
Результаты: build-idf-6.1.1-headless/ и build-idf-6.1.1-rev12/.
Скрипт запускает только build, затем проверку файлов. Flash/monitor отсутствуют.

## Docker и резерв 5.3.2

```sh
./tools/build_idf.sh headless 6.1.1 docker
./tools/build_idf.sh rev12 6.1.1 docker
./tools/build_idf.sh headless 5.3.2 docker
./tools/build_idf.sh rev12 5.3.2 docker
```

Официальные image закреплены SHA-256 digest. Версии библиотек: LVGL 9.2.2,
esp_lcd_touch 1.2.1, GT911 1.1.3; CMake выбирает отдельный точный lock для каждого
SDK из locks/. Каждый профиль имеет собственный sdkconfig внутри build tree.
Не переносить старый sdkconfig из другого профиля: сохранённые значения имеют
приоритет над defaults. Старый общий dependencies.lock больше не используется.

## Память и устройства

Все новые сборки нацелены на Flash **16 МБ**, соответствующую сохранённому
заводскому бэкапу. Для Wi-Fi single factory app увеличен до 2 МиБ по адресу 0x10000; остальная flash пока не
размечена под программы/файлы. Инспектор проверяет sdkconfig, заголовки app и
bootloader, ESP32-S3 chip id, размеры разделов и заполнение app partition.
Rev12 требует octal PSRAM 80 MHz; headless не инициализирует PSRAM.

Бэкап заводской Flash **завершён**, 16 777 216 байт, SHA-256 совпал с сохранённым:
см. docs/BACKUP_STATUS.md. Сам образ исключён из Git. Восстановление не
испытывалось. Владелец отдельно выполнил запись HMI и предоставил фото.
Скрипт сборки не исполняет команды flash, которые печатает IDF.
В PR-01 нет подключения/прошивки; для новых операций нужно отдельное разрешение.

Миграция, версии и все результаты: docs/IDF_611_COMPATIBILITY.md.
Схема/адаптер: docs/BOARD_ADAPTER.md; процедура отката: docs/BOARD_VERIFICATION.md.

## Host tests

```sh
git clone https://github.com/lvgl/lvgl.git ../lvgl
git -C ../lvgl checkout 7f07a129e8d77f4984fff8e623fd5be18ff42e74
cmake -S . -B build-host -G Ninja -DLVGL_DIR="$(realpath ../lvgl)"
cmake --build build-host -j 4
ctest --test-dir build-host --output-on-failure
```

Одиннадцать тестов: ядро, инспектор сборок, Wi-Fi profiles, LVGL 32-bit/RGB565,
диагностические окна/экспорт, OTA crypto/candidate/native manifest/image stream и STM32 supervisor.
Есть проверки кириллицы/глифов, границ надписей/кнопок, отказов, локализации,
журнала и редактирования инструмента. CI повторяет тесты с ASan/UBSan/LSan.
Кадры PPM в build-host/ являются рендером ПК, не фото работающей платы.

## Потоки и производительность

app_main — единственный владелец LVGL. GT911 опрашивается отдельной задачей;
UI читает статическую очередь длиной 1 без I²C/ожиданий. 500 мс без корректных
данных защёлкивают ошибку, сброс не перезапускает симуляцию. Лог измеряет кадры,
время рендера, heap и задержку событие→обработчик только на выполняемом устройстве.
На плате эти показатели ещё не измерялись; отсутствие физической регрессии не
утверждается. Один framebuffer по-прежнему допускает tearing.

## Интерфейс и ресурсы

В main работают Главная, Инструмент, Диагностика, Журнал, Wi-Fi в Параметрах
и экран загрузки. Программа и прочие machine settings ещё не реализованы.
Инструменты и журнал хранятся в RAM; коррекции не применяются к координатам,
сохранение/выбор разделены. Детали SOFTWARE_STAGE_002/003.md в docs/.
Шрифты DejaVu с Ё/ё, лицензия в assets/FONT-LICENSE.txt. Процедурные значки:
tools/generate_icons.py; RGB565 иллюстрация: tools/prepare_photo.py и
assets/IMAGE-PROVENANCE.md. Исходный PNG не включается в бинарник.

Настройка сети: [Wi-Fi](../../docs/WIFI.md). Новый Wi-Fi build требует таблицу
разделов с app 2 МиБ. Старый sdkconfig с app 1 МиБ нужно перегенерировать.

Будущая OTA A/B и отдельная USB-миграция описаны в
[OTA_ARCHITECTURE](../../docs/OTA_ARCHITECTURE.md). Эта документация не меняет
действующий factory 2 МиБ. Правила модуля: [AGENTS.md](AGENTS.md).

Диагностика → «Ресурсы» и bounded worker serial export schema=1 добавлены в
PR-03: [HMI_PERFORMANCE](../../docs/HMI_PERFORMANCE.md). Программные окна
не заменяют per-core CPU/физическую latency/scanout; аппаратные замеры открыты.

PR-04a добавляет только OTA verification primitives и неактивный layout candidate:
[OTA_VERIFICATION](../../docs/OTA_VERIFICATION.md). Активный factory 2 МиБ
сохранён; install/HTTPS/rollback пока отсутствуют. Host CMake теперь требует
OpenSSL development library >=3.0 и CLI для crypto/candidate тестов.

PR-04b: [native signed manifest policy](../../docs/OTA_MANIFEST_POLICY.md).
Новый cJSON v1.7.19 vendored с MIT/license/SHA checks и изоляцией символов;
installer и runtime calls в UI/сети не добавлены.

PR-04c: [read-only image stream](../../docs/OTA_IMAGE_STREAM.md). Полный образ
не хранится firmware verifier в RAM; storage/HTTP/installer не добавлены.
