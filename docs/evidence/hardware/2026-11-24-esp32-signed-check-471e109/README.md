# 471e109: контроль PASS, HTTPS open FAIL, factory возвращён

Профиль rev12-ota/SDK6.1.1, configured BASE0.1.0-bench.1/worker12288,
SHA-256 `726d419fdf5f665f2a8d49d2769703bbd8d36121ca82f664aad2b1eedf9e5919`.
Владелец разрешил необходимые P1 стенды и явно продлил время проверки.
CI source30/main15 PASS, реальные [два SDK](software.json), host189/sanitizers189.

[Сеанс](measurements.json)1052,432s: fresh16MiB/device MD5/SHA/two-NVMe PASS;
app-only BASE/verify, контроль90s PASS, CHECK window420s FAIL, весь свежий
factory2MiB fecc05d/verify/UART30 PASS. Две writes/три captures, full restore0.
MD5 вне factory проверены, bootloader/table/otadata/слоты сохранены. Владелец
подтвердил экран, касания, saved Wi-Fi и OTA SETUP после возврата. На F407
ee7d179 изменений нет; eFuse/CAN/motion/INSTALL не затрагивались.

Контроль:16 samples, touch7–10ms/errors0, internal120587–120639bytes,
largest63488, worker watermark10704/12288. Self-test/PSRAM8MiB/XiP/RGB/GT911/
Wi-Fi PASS. Это UART freshness, не physical FPS/touch-to-display.

Во время CHECK видны два запроса, хотя владелец сообщил об одном нажатии;
происхождение второго не установлено. Оба SESSION_FAILED/error2/HTTP_OPEN4,
resource OK0/checks2, internal119355/119363, largest63488, PSRAM~5940924,
PSRAM largest5898240, remaining stack7248/7072. Final UI stage9/error6.
Полный manifest/image stream не проверен; нельзя объявлять SESSION4 или
production resource qualification. Для одноразовой приёмки также требуется
ровно один запрос. Старый bridge не логировал стадию open/SDK/cert error.

[Прямой GET с теми же UA/Accept-Encoding](http-header-shape.json) показал
302:5726bytes headers/CSP4178/Location911, final200:859bytes. Старый adapter
ограничивает total/value4096, значит не принимает такой ответ; actual-source
SDK-double test воспроизводит этот отказ. Это подтверждённая несовместимость,
согласующаяся с HTTP_OPEN; единственную причину на самой плате старый лог
не различал. [Следующее исправление](../../../plans/P1_GITHUB_HEADERS.md).

Отдельное [чтение отменено до любой записи Flash](interrupted-read.json)
из-за ошибочной гипотезы агента о URL512. Фактический redirect limit2048;
native URL allowlist принял реальные903–907bytes. Плата reset/run fecc05d,
владелец подтвердил UI. URL policy не меняется, закрытые пакеты не повторяются.
Сырые UART, снимки Flash, ключи, context и подписанные CDN query не публикуются.
