# UTC/TLS bench b085abb — 19 ноября 2026

**SNTP PASS; TLS OPEN FAIL (нехватка памяти). Обычный rev12 возвращён.**

Reviewed source b085abb770eed50539011f30e65c048178c67bfe, main после PR #20.
Waveshare ESP32-S3-Touch-LCD-7 Rev1.2, ESP-IDF6.1.1, LVGL9.2.2. Владелец
отдельно разрешил обе записи этого SHA, security preflight/reset/UTC/network
и один UART capture до90s; подтвердил только USB/питание, без CAN/исполнителей.
Все четыре CI workflows main success. Factory backup16777216bytes повторно
проверен SHA-256 PASS. Полные UART/flash logs остаются локально вне Git.

## Воспроизводимость и записи

Из чистого detached b085abb локально собраны через tools/build_idf.sh профили
rev12-tls-bench и rev12, SDK6.1.1. Здесь сохранены оба validation.json и
build.sha256 (пути хешей относительны соответствующему build directory).
App bench1577024bytes, ordinary1573568bytes; Flash16MB DIO80MHz, PSRAM octal80MHz,
factory2MiB. A/B migration нет. Security preflight: Secure Boot/Flash Encryption
Disabled, flags0, SPI_BOOT_CRYPT_CNT0. eFuse/security настройки не менялись.

Esptool5.6.0 записал bench bootloader/partition/app, exit0, 3/3 hashes verified.
После единственного capture таким же способом вернули ordinary rev12 **того же
b085abb**: exit0, 3/3 hashes verified. NVS отдельно не стирали; не запускали OTA
installer/boot selection, CAN, STM32 или приводы. Возврат ordinary не является
проверкой восстановления factory backup.

Один UART open + согласованный RTS reset, baud115200, timeout0.25s, цикл чтения
с deadline90s. Измеренная длительность90.092s (округление/выход из blocking read),
22014bytes. В окне один rst:0x1/POWERON, app b085abb/SDK6.1.1/PSRAM8MB/RGB/UI ready;
17 numeric diagnostics, uptime7383..87645ms строго растёт. Panic/Guru Meditation
в окне не наблюдались. Open/close могут reset; после close и после возврата
ordinary UART не открывали. Нет утверждения о непрерывной работе вне окна.

## Результат одной пробы

| Этап | Наблюдение |
|---|---|
| SNTP | result0/time_result0,3032ms,cleanup1,source2 (SNTP_UNAUTHENTICATED) |
| UTC policy | Реальная синхронизация принята private2026-only policy; не production trust |
| TLS | result5 (TV4_TLS_OPEN),181ms,status0,header_bytes0,cleanup1 |
| Native | HTTP error28674 (0x7002),TLS detail141; mbedtls_ssl_setup returned -0x008D |
| Final | finished pass=0 stage=tls |
| Monitors | SNTP и TLS: monitor_stop=0,historical_min_restored=1 |

SDK6.1.1 mbedtls ssl.h связывает MBEDTLS_ERR_SSL_ALLOC_FAILED с
PSA_ERROR_INSUFFICIENT_MEMORY, crypto_values.h определяет его как -141.
Следовательно, ошибка при создании TLS context означает нехватку памяти.
Это не неверный пароль и не ошибка сертификата. cert_flags0 при OPEN FAIL
**не подтверждает сертификат**: handshake/HTTP200/verified HEAD не состоялись.
Какая конкретно allocation/область памяти отказала, этим логом не установлено.
SNTP меняет global UTC и не аутентифицирован; никаких HTTP fallback/ослаблений
CA/hostname/time, повторных проб или настройки reserves ради PASS не выполняли.

## Heap snapshots (bytes)

| Snapshot | Internal free | Window min | Largest | PSRAM free | PSRAM min | Worker stack watermark |
|---|---:|---:|---:|---:|---:|---:|
| SNTP window start |20615|20615|7680|7616552|7616552|3636|
| SNTP window end |20175|16615|7680|7616552|7616552|3220|
| TLS window start |20175|20175|7680|7616552|7616552|3220|
| TLS window end |19699|8795|7680|7616552|7616384|3108|
| Worker end, historical min |19699|7796|7680|7616576|7601980|3108|

Окна включают параллельные HMI/Wi-Fi/инструментацию и bookkeeping monitor;
это не точное attribution TLS peak и не production reserve. Largest — sample,
не непрерывный minimum. После self-delete, последний diag: internalfree26231,
largest10240,PSRAMfree7616576,internalmin7796. Разницу concurrent free не считать
доказанным leak/no-leak. Worker6144bytes, sampled minimum watermark3108bytes.

Во всех17 records display_errors/touch_errors/queue_drops/export_drops/
wifi_queue_drops/watchdog_boot/wifi_disconnects=0; последний wifi_ip_events=1.
CPU/physical touch-to-display/RGB scanout не измерены; нулевые idle render_samples
не означают измеренный FPS0. После возврата ordinary владелец подтвердил:
главный экран, касания и Wi-Fi работают нормально. Это qualitative проверка,
не hardware latency/soak test и не независимое UART подтверждение boot ordinary.

## Следующий gate

Runtime TLS заблокирован реальным allocation failure. Нужны отдельный review
allocator capabilities/PSRAM/TLS buffers и host/build checks до нового exact-image
испытания. Не уменьшать DMA reserve/проверки сертификата ради успешного handshake.
Production UTC policy/CA/redirect/failure/soak budget, OTA install/rollback,
factory restore и physical performance targets остаются открытыми.
Текущая запись — **b085abb ordinary rev12, probe off**. Это разрешение исполнено;
новые записи/reset/serial/network испытания требуют отдельного согласования.
