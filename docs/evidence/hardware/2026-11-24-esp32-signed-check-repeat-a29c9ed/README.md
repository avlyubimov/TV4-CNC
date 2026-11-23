# ESP32: три повторных подписанных CHECK — PASS

24 ноября2026 Asia/Almaty; exact source
`a29c9edde10b8d2359572d04d544d8973fdaa6b3`, PR #80 открыт. ESP-IDF6.1.1,
изолированная Waveshare Rev1.2, питание/USB, без CAN/приводов. Основание —
разрешение владельца на автономные обратимые проверки до08:00. Сеанс закрыт:
23ноября21:20:13–21:39:22 UTC,1148,571s из1800s; использованный пакет не повторять.

Предварительно требовался закрытый PASS [первого CHECK и возврата](../2026-11-24-esp32-signed-check-a29c9ed/README.md).
Пакет проверял точные source/plan/result этого предшественника. Те же app bytes,
237 host/237 ASan/UBSan/LSan, четыре ELF/BIN двух SDK, source CI30/baseline15 PASS;
программные доказательства — [здесь](../../build/esp32-ota-zero-length-check/README.md).

Свежие16MiB прочитаны/device MD5/SHA-256 и две приватные копии на разных NVMe
проверены. Control90s прошёл до записи auto. Auto-app записан ровно один раз,
дальше три reset/UART150s без повторной записи между ними. После каждого CHECK
проверены app bytes/bootloader/table/otadata/неактивные/resource/store области.
При любом FAIL оставшиеся запуски отменялись бы; фактически все три прошли.

| Измерение | Запуск1 | Запуск2 | Запуск3 |
|---|---:|---:|---:|
| Session state |IMAGE_VERIFIED(7)|IMAGE_VERIFIED(7)|IMAGE_VERIFIED(7)|
| error/http/policy/image/resource |0/0/0/0/0|0/0/0/0/0|0/0/0/0/0|
| Resource checks |406|406|406|
| SESSION local minimum internal, bytes |99235|99427|97783|
| Largest internal block, bytes |63488|63488|63488|
| SESSION local minimum PSRAM, bytes |5910032|5910108|5910012|
| Остаток OTA-worker stack, bytes |6896/12288|6896/12288|6896/12288|
| Touch snapshot age, ms |3–18|4–15|1–12|
| Touch errors |0|0|0|
| Cleanup service state/error |READY(5)/0|READY(5)/0|READY(5)/0|

В каждом запуске — ровно один attempted1/pass1/read_only1, без HTTP open failures.
Подпись, совместимость, размер и SHA-256 полного опубликованного bench.2 BIN
проверены прежним guarded session с verify_image=true. Отдельный runtime byte
counter не выводится; IMAGE_VERIFIED требует полного проверенного потока.
BOOT/WORKSPACE/SNTP/SESSION/CLEANUP завершены,3×28 periodic samples CHECK.
Ресурсные пороги, CA/hostname/date/time/timeout/cleanup/touch gates сохранены;
SNTP неаутентифицированный. Это reset-запуски, не cold-power или soak испытание.

Весь исходный свежий factory2MiB `fecc05d` возвращён/verify/preservation/UART30
PASS, return touch3–17ms/errors0. Три обычные записи/пять UART-захватов,
full restore0. На плате `fecc05d`; физический экран/касания владельцем ночью не
наблюдались. STM32/eFuse/CAN/motion и тесты отключением питания не затрагивались.

Общее ограничение: четыре успешных full CHECK в двух конечных сеансах — ещё
не production resource budget, INSTALL, подтверждение OTA-слота или rollback.
Payload bench.2 остаётся unprovisioned/read-only; устанавливать его нельзя.
Далее отдельное исправление допуска local trust при pending confirmation и
native slot bench, затем configured signed installable payload/сетевой INSTALL.

Числовые данные, точные app/plan hashes и флаги сохранности —
[measurements.json](measurements.json). UART, redirects, ключи/context и бэкапы
не публикуются.
