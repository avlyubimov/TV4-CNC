# ESP-IDF 6.1.1: совместимость HMI

> Исторический отчёт PR #1: размеры образов и app 1 МиБ ниже относятся
> к тому этапу. Сейчас factory app — 2 МиБ; актуальные аппаратные результаты
> и границы проверок: [HARDWARE_STATUS.md](HARDWARE_STATUS.md),
> текущее ПО: [IMPLEMENTATION.md](IMPLEMENTATION.md). Отсутствие подключения
> в этом отчёте не отменяет последующую запись HMI владельцем.

## Итог

На установленном Ubuntu SDK `~/.espressif/v6.1.1/esp-idf` успешно собраны
`headless` и `rev12`. Тег SDK **v6.1.1**, commit
`051e4c4eb20346e043cdd47bc388fde75ff8af63`, SDK без локальных изменений.
Оба профиля повторно собраны на ESP-IDF **5.3.2** в ранее проверенном Docker
образе. Оба SDK поддерживаются явно; другие версии CMake отклоняет.
Устройство не подключалось; flash/monitor/erase не выполнялись.

| SDK | Профиль | App bin, байт | Flash header/config | PSRAM |
|---|---|---:|---|---|
| 6.1.1, локальный | headless | 619616 | 16 MB | выключена |
| 6.1.1, локальный | rev12 | 708592 | 16 MB | octal, 80 MHz |
| 5.3.2, Docker | headless | 669296 | 16 MB | выключена |
| 5.3.2, Docker | rev12 | 749088 | 16 MB | octal, 80 MHz |

Размер образа — использование flash, не оценка FPS или потребления RAM.
Сохранена single-factory разметка ESP-IDF: app 1 МиБ по 0x10000, NVS 24 КиБ,
PHY 4 КиБ. Остальная flash пока не размечена. Размер app проверен относительно
раздела; все разделы помещаются в 16 МиБ. Заголовки app **и** bootloader
проверены независимо от sdkconfig: magic, ESP32-S3 chip id и Flash 16 MB.

## Исправления API

- **I²C/GT911/CH422G:** прежний LCD I/O cast номера порта в
  `esp_lcd_i2c_bus_handle_t` удалён из IDF 6. Новый `i2c_new_master_bus`
  используется на обоих SDK. Один bus GPIO8/9 обслуживает GT911 и два
  CH422G адреса-команды 0x24/0x38. Для них создаются постоянные device handles,
  частота всех устройств 400 kHz. Адреса не перепутаны с индексами регистров.
- **RGB:** удалённые поля `bits_per_pixel`, `sram_trans_align`,
  `psram_trans_align` на IDF 6 заменены на RGB565 input/output и DMA burst 64 B.
  Для 5.3.2 сохранены исходные поля. Нет дополнительной конвертации пикселей.
- **GT911:** LCD I/O получает bus handle и 400 kHz. На 6.1.1 явно задан
  `transaction_timeout_ms=20`, чтобы новый default infinite wait не стал
  незаметным поведением. Касание по-прежнему опрашивается отдельной задачей;
  UI читает очередь без ожидания, устаревшее касание отпускается.
- **USB/CAN и подсветка:** sequence CH422G 0x18/0x1a/0x1e и маска USB_SEL
  сохранены. Во всех этих записях EXIO5=0 (USB), SD CS=1. CAN не запускается.
- **Flash:** общий профиль теперь задаёт 16 MB, подтверждённый сохранённым
  бэкапом; см. [BACKUP_STATUS.md](BACKUP_STATUS.md).

Официальные источники и локальные заголовки:
[миграция LCD/I²C/RGB](https://docs.espressif.com/projects/esp-idf/en/v6.1.1/esp32s3/migration-guides/release-6.x/6.0/peripherals.html),
`components/esp_lcd/rgb/include/esp_lcd_panel_rgb.h`,
`components/esp_lcd/include/esp_lcd_io_i2c.h`,
`components/esp_driver_i2c/include/driver/i2c_master.h` из SDK v6.1.1.

## Производительность и границы проверки

Сохранены 800×480 RGB565, PCLK 16 MHz, прежние porches, один framebuffer в
PSRAM, bounce 800×10 пикселей, partial draw buffer 800×20×2 байт, CPU 240 MHz,
FreeRTOS 1 kHz, UI loop 10 ms и touch poll 20 ms. Нет новых копирований кадров,
перехода в 32-bit framebuffer или выделений памяти в цикле опроса. LVGL остаётся
у единственного владельца. Отрисовка не перемещена в I²C task/callback.

Это подтверждение совместимости API, конфигурации и сохранения структуры пути
рендера. Физическое отсутствие регрессии FPS/задержки/tearing, фактический запуск
PSRAM, RGB и CH422G на плате без испытаний не подтверждаются. В соответствии
с заданием никаких аппаратных тестов не выполнялось. Блокирующих ошибок сборки
6.1.1 не осталось. Производительность на железе ещё нужно измерить отдельно.

## Точные зависимости

| Компонент | Версия | Хеш компонента |
|---|---|---|
| LVGL | 9.2.2 | 096c69af22eaf8a2b721e3913da91918c5e6bf1a762a113ec01f401aa61337a0 |
| esp_lcd_touch | 1.2.1 | 3f85a7d95af876f1a6ecca8eb90a81614890d0f03a038390804e5a77e2caf862 |
| esp_lcd_touch_gt911 | 1.1.3 | acc1c184358aa29ef72506f618c9c76a8cc2bf12af38a2bff3d44d84f3a08857 |

Манифест закрепляет все три библиотеки напрямую. CMake выбирает
`firmware/esp32-hmi/locks/idf-<version>.lock`: 5.3.2 использует lock format 2,
6.1.1 — format 3. Старый общий dependencies.lock удалён, чтобы SDK не
перезаписывали один файл друг друга. Требование IDF в манифесте — диапазон
из-за синтаксиса SimpleSpec; разрешены только 5.3.2 и 6.1.1 через CMake/script.

Локально: Python 3.14.4, xtensa GCC 15.2.0 (esp-15.2.0_20251204),
idf-component-manager 3.1.2, esptool 5.6.0, esp-idf-kconfig 3.14.0.
Docker 5.3.2: Python 3.10.12, xtensa GCC 13.2.0,
idf-component-manager 2.1.0, esptool 4.8.1. Оба Docker image закреплены digest
в tools/build_idf.sh и CI. LVGL host source закреплён commit
7f07a129e8d77f4984fff8e623fd5be18ff42e74 (v9.2.2).

## Команды и свидетельства

```sh
TV4_IDF_PATH="$HOME/.espressif/v6.1.1/esp-idf" ./tools/build_idf.sh headless 6.1.1 local
TV4_IDF_PATH="$HOME/.espressif/v6.1.1/esp-idf" ./tools/build_idf.sh rev12 6.1.1 local
./tools/build_idf.sh headless 5.3.2 docker
./tools/build_idf.sh rev12 5.3.2 docker
```

Скрипт разрешает только build и чтение полученных файлов. Он не принимает
пользовательскую строку idf-команд, не передаёт USB/serial в Docker.
`tools/check_idf_build.py` проверяет target/SDK, конфигурацию, образы и разметку.

- [6.1.1 headless](evidence/build/idf-6.1.1/headless.log),
  [rev12](evidence/build/idf-6.1.1/rev12.log), соответствующие `*.json` содержат
  SHA-256 app/bootloader, размеры и проверенные настройки.
- [5.3.2 headless](evidence/build/idf-5.3.2/headless.log),
  [rev12](evidence/build/idf-5.3.2/rev12.log) — проверенная резервная сборка.
- [Host tests](evidence/build/host-tests.log): **4/4** — state/protocol/tools,
  inspection validator (6 negative/positive cases), LVGL render 32-bit и RGB565.
- [ASan/UBSan/LSan](evidence/build/sanitizers-tests.log): **4/4**. Первичная
  попытка в sandbox упала из-за ptrace-ограничения LeakSanitizer; сохранена
  отдельно, повтор вне sandbox успешен без отключения детектора утечек.
- `initial-rev12.log` сохраняет исходные ошибки удалённых RGB/I²C API.
  `initial.log` — отказ сети реестра в sandbox; retry с сетью сохранён отдельно.
  Исходный headless также компилировался; это не покрывало выключенный адаптер.

CI собирает все 4 сочетания SDK/профиля в фиксированных image, проверяет отсутствие
изменений lock-файлов и сохраняет отчёты. Host CI включает рендеры и sanitizer
матрицу. Никаких подключений к устройству CI не выполняет.

## Откат

Исходный проверенный 5.3.2 baseline остаётся в main, commit 20b8114. Ветка
совместимости также собирается на 5.3.2. Откат Git не затрагивает локальные
backups/ и не требует записи устройства. Собственный PR не сливается автоматически.
