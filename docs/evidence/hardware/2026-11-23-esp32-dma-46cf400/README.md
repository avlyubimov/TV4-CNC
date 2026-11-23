# 23 ноября: DMA trace46cf400, полный restore fecc05d

[Числовые результаты](measurements.json). Источник46cf400, ESP-IDF6.1.1,
rev12-ota/worker8192, без owner key/context. Перед стендом source CI15/15 PASS;
PR #74 с build evidence —30/30 PASS. Действовало заранее данное владельцем
разрешение на необходимые P1 подключения/стенды23ноября (Asia/Almaty).

## Подтверждено на плате

Сеанс15:06:40–15:20:11UTC,810,754s. Проверены identity/security/Flash16MiB,
свежий полный snapshot/device MD5/SHA-256 и две приватные копии на разных NVMe.
Control и TLS app-only writes/verify с проверкой MD5 вне factory до запуска.

Control PASS:16 samples, touch age1–20ms/errors0, BOOT/self-test/PSRAM8MiB/XiP/
RGB/GT911/Wi-Fi и stack marker8192. Это UART evidence, не физические нажатия.

SNTP PASS:1020ms, cleanup1, source2 (unauthenticated). Первая TLS-проба FAIL
при HTTP open: native28674, ESP-TLS32794 (handshake failed), Mbed32640
(`-0x7F80`, HW_ACCEL_FAILED), certificate flags0; headers не вызывались.
Нулевые certificate flags не доказывают успешную проверку сертификата.

Failure callback реально сработал: total9, сохранены первые4, lost5, valid0.
Все сохранённые события — HTTP_OPEN, другая задача (`owner0/isr0`),
heap_caps_malloc, bytes190/190/190/136, caps2060 (`DMA|INTERNAL|8BIT`).
После unwind request heap free3479–3719/largest2944 — это **не** heap в момент
отказа. Собственный failed request AES среди пяти потерянных событий неизвестен.
До open DMA free4499/largest4352, generic internal free12287/largest7680:
общий internal8BIT largest нельзя использовать вместо DMA headroom.

Проба остановлена без обхода ownership/loss/resource/TLS/touch gates;
остальные два TLS boots отменены. Cleanup1, global windows закрыты, watchdog
и touch error counters0. Runtime minimum internal2415/history2408 и remaining
stack3936/8192 — диагностические замеры, без production resource qualification.

Весь factory2MiB fecc05d возвращён и verify PASS. В его30s UART один sample
touch_age_ms4294967295 при errors0, остальные13/17/0: строгий touch_fresh FAIL,
совпадающий с известным clock-before-snapshot race старого fecc05d. По плану
выполнена **одна полная запись свежего16MiB snapshot**, verify и reset PASS.
Bootloader/table/otadata записывались при этом условном полном восстановлении;
они возвращены из проверенного снимка. Повторного restore не было.

Отдельная разрешённая сегодняшней инструкцией проверка после восстановления:
один reset/UART30,057s без Flash operations, все boot/resource checks PASS,
touch age3–19ms. На плате оставлен fecc05d/factory/tv4-16m-ab-v1. Всего основной
сеанс сделал4 Flash writes (включая full restore),3 captures; дополнительный
capture один. Открытие/закрытие порта может давать дополнительные reset.
Визуальное подтверждение владельца для сегодняшних образов не получено.

## Инженерный вывод и следующий PR

На этом запуске подтверждён дефицит **DMA-capable internal memory** уже во
время handshake; по сохранённым четырём событиям нельзя назначить конкретную
задачу/вызывающий участок SDK или точный запрос AES. Потеря пяти событий явно
сохранена, trace не объявлен полным. Прежний HEAD132 остаётся отдельным фактом.

Создание dashboard использовало100516bytes internal RAM в control и столько
же в TLS variant; PSRAM почти не менялась. LVGL9.2.2 использует CLIB malloc,
SDK предпочитает internal для запросов до16384bytes. Следующий отдельный PR —
официальный CUSTOM allocator LVGL на PSRAM **только explicit OTA profile**,
с real ELF/BIN обоих SDK, host/fault tests и новым ограниченным сравнением
control → новый control → TLS. Это обоснованное направление, ещё не доказанный
фикс и не разрешение ослабить TLS records16384/4096/DMA64KiB/stack/touch gates.

## Оставшиеся блокеры

Успешный HTTPS/подписанный OTA CHECK/install/rollback ещё не подтверждены.
Нужны reviewed owner key/SemVer/time/resource settings, затем настоящая signed
session и inactive-slot/confirmation/rollback acceptance. Физические FPS/
touch-to-display/tearing/soak и power-loss не измерены. Полный USB restore
проверен для текущего snapshot, а не OTA rollback или испытанием обрыва питания.
F407/CAN/motion/eFuse не затрагивались; PR и релиз не сливались/не публиковались.
Raw UART/NVS/dumps/идентификаторы остаются приватными.
