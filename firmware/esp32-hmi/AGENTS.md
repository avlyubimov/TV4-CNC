# ESP32 HMI — специализированные правила

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
  Текущий factory 2 МиБ не менять до отдельно проверенной USB-миграции.
- PR-03 дополняет instrumentation/worker export. Не менять GPIO, PCLK,
  framebuffer/PSRAM timing и Flash-разметку без измерений. Аппаратная часть
  и per-core CPU load остаются открытыми; не заявлять выполнение targets.
