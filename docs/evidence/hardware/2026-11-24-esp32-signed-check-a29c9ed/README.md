# ESP32: полный подписанный read-only CHECK — PASS

24 ноября 2026, Asia/Almaty. Точный source
`a29c9edde10b8d2359572d04d544d8973fdaa6b3`, открытый PR #80, baseline main
`c3b82f0ef8db764f9f242ad09caec015f58ce19a`. Source CI30/30 и baseline15/15
прошли до подключения. [Software evidence](../../build/esp32-ota-zero-length-check/README.md)
отдельно подтверждает237 host/237 ASan/UBSan/LSan и четыре ELF/BIN обоих SDK.
Аппаратно здесь проверен только ESP-IDF6.1.1;5.3.2 проверен сборкой.

Изолированная Waveshare ESP32-S3 Rev1.2: питание/USB, без CAN и приводов.
Основание — явное разрешение владельца на автономные обратимые проверки
подключённых плат до24ноября08:00. Этот отдельный сеанс закрыт:21:00:12–21:18:28
UTC23ноября,1095,492s при лимите1800s. Повторять использованный пакет нельзя.

1. Чип, защита и Flash16MiB проверены. Свежие16MiB прочитаны и сверены с
   device MD5; SHA-256 и две приватные копии на разных физических NVMe проверены.
2. Configured control12288, trace1/auto0: app-only write/verify, проверка
   сохранности Flash вне factory и UART90s — PASS. Сенсор11–14ms в контрольных
   samples, ошибок нет, startup/self-test/PSRAM/RGB/GT911 проходят.
3. Auto1 записан один раз: app-only write/verify/preservation, UART420s — PASS.
   Ровно один `attempted=1 pass=1 read_only=1`, без внешнего нажатия CHECK.
   Полная сессия `IMAGE_VERIFIED(7)`, все error/http/policy/image/resource=0,
   406 resource checks, итог service READY(5)/error0. Native HTTP failures нет.
4. Весь свежий factory-раздел2MiB `fecc05d` возвращён, verify/preservation и
   UART30s — PASS. Три обычные записи, три захвата; полный restore не понадобился.
   Otadata, неактивные слоты, resources/store не изменены CHECK. На плате `fecc05d`.

Проверен опубликованный read-only payload
[v0.1.0-bench.2](https://github.com/avlyubimov/TV4-CNC/releases/tag/v0.1.0-bench.2),
source `c013ddf1346bf1aaf3c60eacf14e07fe4846e2c8`: signed manifest, локальный
доверенный P-256 public key, совместимость, размер и SHA-256 полного BIN.
Ожидаемый BIN1598560bytes,
SHA-256 `017236a2abec4f71b8737a396a9601fe603d61931babc05405a9df874d1663de`.
Runtime не выводит отдельный счётчик прочитанных байтов: доказательство полного
потока — guarded session с `verify_image=true` и состоянием IMAGE_VERIFIED,
которое требует проверенных длины и digest. Это не установка образа.

| Показатель | Наблюдение | Область измерения |
|---|---:|---|
| Internal local minimum |97399bytes|SESSION4 SDK minimum monitor|
| Internal free |119331bytes|минимум среди16 сетевых boundary points|
| Largest internal block |63488bytes|сетевые samples и periodic samples|
| PSRAM local minimum |5910092bytes|SESSION4 SDK minimum monitor|
| Largest PSRAM block |5898240bytes|сетевые boundary points|
| Остаток OTA-worker stack |6896/12288bytes|high-water mark, включая cleanup|
| Длительность SESSION4 |45413ms|между post-start/pre-stop samples|
| Сенсор snapshot age |0–20ms|periodic samples CHECK; touch_errors0|
| Итог после cleanup |internal119679/PSRAM5982116bytes|service READY/error0|

BOOT/WORKSPACE/SNTP/SESSION/CLEANUP окна завершены, start/stop0. Пороги
internal64KiB/largest32KiB, PSRAM1MiB/largest128KiB и stack3072 сохранены.
CA, hostname, certificate dates, anchor age/skew и прежние сетевые тайм-ауты
не ослаблены. SNTP остаётся неаутентифицированным и ограничен согласованным
стендовым календарным окном. Ключи, context, URL redirects, UART и снимки приватны.

Это один успешный ограниченный CHECK, ещё не производственный ресурсный бюджет,
soak-test, INSTALL, подтверждение OTA-слота или rollback. Физические касания,
изображение, FPS и touch-to-display ночью владельцем не наблюдались; выводы
о UI ограничены успешной инициализацией/health и UART snapshots. Неактивный
payload bench.2 не содержит provisioning и не разрешён для установки.
eFuse/STM32/CAN/motion/потеря питания не испытывались. Отдельная проверка
повторяемости требует нового свежего снимка и завершённого PASS этого сеанса.

Полные числовые результаты и точные app hashes — [measurements.json](measurements.json).
