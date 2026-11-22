# Стенд 3927063: touch_fresh PASS, HTTPS остановлен на AES allocation

22 ноября 2026, 18:25:29–18:37:52 UTC (23:25–23:37 Asia/Almaty).
Один отдельно разрешённый сеанс **743,183 с** из1800; он закрыт, разрешение
использовано. [Точный пакет](../../../ESP32_OTA_BENCH_3927063.md),
[числа/проверки/SHA-256](measurements.json). Firmware source — чистый
`392706355193b7544132e2296f7ef8572e1f3c10`, SDK6.1.1, trace/worker8192.
PR #71/#72 слиты владельцем; отчёт подготовлен от main70eaf1d.

## Фактически выполнено

Чип/защита/Flash16MiB PASS. Свежие16777216bytes прочитаны с платы, device MD5/
SHA-256/two-independent-NVMe/fsync PASS. SHA вновь
`ce6af4e88d755254f65ca2cf952d17ad9892a8ec7f487b0ed4391eccc3355e5a`.
Это новое чтение и новые файлы, прежние копии не перезаписывались. После стенда
обе host-копии повторно сверены по SHA/размеру/modes0600/0700; NVS не декодировалась.

| Этап | UART, с | Samples | Touch age, мс | Результат |
|---|---:|---:|---:|---|
| control8192 | 90,146 | 16 | 2–19 | Все проверки PASS |
| worker-tls-1 | 90,100 | 16 | 2–19 | Touch/boot/monitor PASS, HEAD FAIL |
| возвращённый fecc05d | 30,043 | 4 | 3–18 | Все UART checks PASS; owner UI PASS |

Touch errors0 во всех36 samples; underflow UINT32_MAX не повторился. Seven
startup IDs, BOOT window, PSRAM8MiB/XiP, RGB/GT911/self-test, saved Wi-Fi IP
и worker8192 checks прошли. Это подтверждает две reset-пробы исправления clock
на SDK6.1.1; SDK5.3.2 здесь проверен только программно, не на устройстве.
FPS/latency, cold-power и длительный soak не измерены.

Одна control app write, одна TLS app write, один возврат **всего factory2MiB**
из нового снимка, все verify PASS; после каждой записи MD5 вне factory проверен
до запуска. Worker-tls-2/3 не запускались после FAIL. Три captures, полный16MiB
restore не понадобился. Bootloader/table/otadata отдельно не писались.
Нет полного Flash readback после возврата или сравнения NVS между всеми boots.

## Конкретный отказ HTTPS

SNTP/time/cleanup PASS, source2 (неаутентифицированный SNTP), elapsed1603ms.
HTTP init/open выполнены; чтение заголовков вернуло `TV4_TLS_HEADERS=6`,
status-1, native_error-1, tls_error132, cert_flags0, header_bytes0,
elapsed2805ms, cleanup1. HTTP200 не получен: **verified HEAD FAIL**.
Флаги сертификата0 сами по себе не являются успешной HTTPS-пробой.

В private UART непосредственно перед отказом:

```text
esp-aes: Failed to allocate memory
esp-tls-mbedtls: read error :-0x0084
transport_base: esp_tls_conn_read error, errno=I/O error
```

В pinned SDK6.1.1 positive132 соответствует захваченному отрицательному
`PSA_ERROR_GENERIC_ERROR=-132`, не POSIX errno132. AES log подтверждает отказ
аллокации в SDK hardware-AES path; **какой именно allocation site, размер,
caps и transient DMA largest — не снято**. Не объявлять их известными по
общему internal_free или generic TLS code.

Control worker остаток6736из8192. TLS worker minimum remaining3792из8192;
после open internal_free6847/largest5120, TLS snapshot local minimum2571,
history после окна2408. PSRAM minimum6055088; after-cleanup PSRAM6088912,
internal15531/largest6656. Per-heap minima не simultaneous free/OOM floor.
Window start/stop/cleanup и touch checks сохранены; TLS failure не исключён.
Это первый реальный TLS memory blocker в этом OTA worker, не production budget.

## Возврат и следующий шаг

На ESP32 оставлен **fecc05d/rev12-ota/factory/tv4-16m-ab-v1**. Полный factory
SHA `081d99e707f578824cc9058d1d18c4f87700b2e60a09afa907e98a9c33bffd1a`.
Владелец подтвердил экран/касания/Wi-Fi/OTA SETUP временного3927063 и уже
возвращённого fecc05d; установка недоступна. F407 ee7d179 не открывался.
eFuse, STM32, CAN и motion не затрагивались; испытаний power-loss не было.

Следующая работа — [разбор AES/DMA памяти](../../../plans/P1_AES_DMA_MEMORY.md),
затем новый reviewed exact-image пакет и отдельное разрешение. Не повторять
использованный пакет и не продолжать оставшиеся TLS boots по старому согласию.
Hardware AES, CA/hostname/date, TLS16384/4096, DMA reserve64KiB, clocks/pins/
framebuffer/monitor-owner/touch thresholds не ослаблять ради PASS.
Настоящая signed SESSION4, install/confirm/rollback и release acceptance ещё
не выполнены; ключ/context/floors отсутствуют, CHECK/INSTALL закрыты.
