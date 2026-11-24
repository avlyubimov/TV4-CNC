# ESP32 native pending confirmation и rollback — PASS

24 ноября2026 Asia/Almaty, exact source
`a74bfcd62ffbc290f6ab5d692a9a40bbed69778b`, открытый PR #81. Изолированная
Waveshare ESP32-S3 Rev1.2, питание/USB, без CAN и приводов. Явное разрешение
владельца на автономные обратимые проверки до08:00 покрывает этот конечный
стенд; отдельное per-hash одобрение владельца не заявляется. Сеанс закрыт за
1031,352s при лимите1800s. Использованный пакет не повторять.

До USB: [265 host/265 ASan/UBSan/LSan, оба pinned SDK,4 ELF/BIN, CI30/30](../../build/esp32-ota-pending-trust/README.md)
и baseline main15/15 PASS. Metadata fixture сверена с реальным методом
официального pinned otatool.py без serial target:8 oracle checks. Процедура и
парсер прошли9 test methods/34 success/fault scenarios, загрузчик пакета8 checks.
Все эти проверки программные, отдельно от наблюдений ниже. Аппаратно — только
ESP-IDF6.1.1;5.3.2 проверен сборкой.

Это **USB boot fixture**, а не подписанный сетевой INSTALL: app bytes и NEW
записи otadata подготовлены через USB. Опубликованный unprovisioned bench.2
не использовался. Configured app version0.1.0-bench.1 повторяется в factory и
ota_0; проверки увеличения SemVer этим стендом нет. Локальный fault image —
новый unprovisioned a74bfcd, а не опубликованный payload c013ddf.

1. Чип/защита/Flash16MiB проверены. Свежие16MiB прочитаны/device MD5 и SHA-256
   сверены, две приватные копии на разных физических NVMe проверены. До записи
   otadata и оба неактивных слота были FF. Bootloader/table совпали с fecc05d guards.
2. Configured factory-control записан/verify/preservation, UART90s PASS:
   PSRAM/XiP, RGB, GT911, UI health, Wi-Fi и семиточечная BOOT trace проходят.
3. Тот же configured BIN записан в ota_0 и проверен **до** выбора NEW. Только
   первый4KiB sector otadata получил sequence1/state NEW0. Native bootloader
   запустил app с0x220000; self-test PASS. Повторное чтение8KiB otadata подтвердило
   sequence1/VALID2/правильный CRC, второй sector FF. Native confirmation — PASS.
4. В ota_1 записан локальный unprovisioned BIN, checksum/ELF/source проверены.
   Только второй sector otadata получил sequence2/NEW0, первый VALID сохранён.
   UART90s содержит ровно два app boots:0x520000 →0x220000, один явный native
   `Rollback to previously worked partition.` и здоровый повторный запуск ota_0.
   Первый app завершил init/BOOT window, но не был подтверждён. Чтение metadata
   подтвердило sequence1/VALID2 + sequence2/INVALID3 с правильными CRC; это не
   ABORTED после неожиданного reset или отказ bootloader проверить испорченный BIN.
5. Из свежего снимка возвращены **полные** factory2MiB, ota_0/ota_1 по3MiB и
   otadata8KiB; все четыре региона verify, выбор factory FF, UART30s PASS.
   На ESP32 `fecc05d`. Девять обычных write calls/четыре capture, full recovery0.

После каждого USB write до запуска проверялись bytes и MD5 всех областей вне
разрешённого региона. Между boots отдельно сохранялись bootloader/table/PHY/
gap/resources/store. Native4KiB записи bootloader/SDK в otadata были ожидаемыми
переходами NEW → VALID и NEW → INVALID, проверенными полным readback sector.
Bootloader и partition table не записывались. Законные runtime NVS updates
сохранены; битовое равенство NVS между загрузками не заявляется.

| UART observation | Factory-control | Confirmed ota_0 | Fallback ota_0 | Returned fecc05d |
|---|---:|---:|---:|---:|
| Минимум sampled internal free, bytes |120639|120195|120587|22731|
| Минимум sampled largest internal, bytes |63488|63488|63488|7680|
| Остаток worker stack, bytes |10880/12288|10880/12288|10784/12288|не измерялся|
| Максимум touch snapshot age, ms |20|19|12|13|
| Resource/touch/display errors |0|0|0|0|

Это samples ограниченного boot-стенда, не полные runtime peaks. Нативная запись
otadata при работающем HMI проверена; запись3MiB приложения при активном RGB,
HTTPS/TLS/подпись одновременно с Flash, потеря Wi-Fi/питания, cold-power, soak
и production install resource budget **не проверены**. Физический экран/касания,
FPS/touch-to-display ночью владельцем не наблюдались. На F407 остаётся ee7d179,
этот USB-сеанс STM32 не открывал. eFuse/option bytes/OTP/CAN/motion не затронуты.

Следующий P1 результат: CHECK-only context должен технически закрывать INSTALL;
затем нужен отдельно согласованный configured signed installable payload и
настоящая guarded HTTPS INSTALL/resource-сессия. Этот PASS подтверждает native
boot/confirmation/refusal/rollback, но не заменяет сетевую установку.

Числовые данные, точные app/ELF/plan hashes, metadata и сохранность —
[measurements.json](measurements.json). UART, context/ключи и полные снимки приватны.
