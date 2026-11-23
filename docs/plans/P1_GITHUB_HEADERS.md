# P1: реальные HTTP-заголовки GitHub Releases

471e109/worker12288: изолированный контроль PASS, CHECK остановлен на
SESSION_TRANSPORT/HTTP_OPEN при resource OK и stack7072–7248. Factory fecc05d
возвращён/verify/UART/owner HMI-touch-Wi-Fi-SETUP PASS. Полный signed CHECK
и install/rollback не приняты. Два запроса видны в UART, хотя владелец сообщил
об одном нажатии; не выдавать этот trace за однократную приёмку.

Прямой GET с теми же User-Agent/Accept-Encoding даёт redirect302:16 headers,
5726bytes total, CSP4178bytes, Location911bytes. Конечный200:25 headers,
859bytes total. Значения/подписанные CDN query не сохраняются публично.
URL уже ограничен2048 и подходит; прежняя гипотеза512 ошибочна, изменения
URL policy не нужны. Один сеанс чтения остановлен до записи Flash.

В официальных esp_http_client.c SDK5.3.2/6.1.1 вся строка GET path+query должна
поместиться в buffer_size_tx. Прежние512bytes также не вмещают CDN redirect.
Подбирать TX по разрешённому URL: минимум512, максимум2080 (URL2048+32),
без увеличения RX4096 или TLS record sizes. Проверить длинную query на обоих SDK.

Результат одной интеграции: actual HTTPS adapter принимает измеренный ответ
GitHub с ограничениями total16KiB/value8KiB, без выделений/копии CSP в callback.
Прежние key128/Location2048/length/body/encoding/duplicate/certificate/hostname/
time/redirect allowlist ограничения сохраняются. Bounded error log выполняется
после callback и не содержит URL/headers/tokens. Новых клиентов/tasks нет.

Приёмка: воспроизвести отказ actual adapter на старом коде через SDK doubles,
проверить исправление и oversized/duplicate/framing/TLS-config/cleanup/time/
cancel cases, host/sanitizers/analyzer, оба pinned SDK/ELF и CI. Затем новый
exact BASE, fresh16MiB/two-NVMe, чистый контроль, один CHECK и возврат factory.
Не снижать resource floors и не ослаблять сертификаты ради PASS.
