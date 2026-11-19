# TLS/PSRAM bench 1bfc7e2 — 19 ноября 2026

**SNTP PASS, verified HTTPS HEAD HTTP200 PASS. Ordinary rev12 возвращён.**
Это одна isolated probe, не production TLS memory budget или OTA install approval.

Source1bfc7e277fd54899b3e4b16b8fb57867917526ab, main после review/merge PR #22.
Waveshare ESP32-S3-Touch-LCD-7 Rev1.2, ESP-IDF6.1.1, LVGL9.2.2.
Владелец отдельно разрешил этот SHA: security preflight, обе USB-записи,
UTC/SNTP/network/reset и один UART capture около90s; подтвердил изолированный
стенд только USB/питание, без CAN/исполнителей. Все4 workflows CI main success.
Factory backup16777216bytes повторно SHA-256 PASS. Новых исходников/настроек
ради PASS не меняли, никаких повторных UART captures или других images.

## Сборки, запись и возврат

Оба профиля собраны из чистого detached source, project_version **1bfc7e2**,
через tools/build_idf.sh PROFILE6.1.1local; version/config/header/layout/ELF/hash
checks PASS. Здесь validation.json и build.sha256 обоих профилей (пути хешей
относительны соответствующему build directory, не папке evidence).
Bench app1578176bytes, SHA256c2495d85cd3d71e67cbc02be02ef7a0c1a97d89c2fc26c2197ba50cebab7fad0.
Ordinary app1573568bytes, SHA2565e8f69807a54ffee2021d100f92c71cc234cc18df77566b592b4628bbd7c6fbc.
Flash16MB DIO80MHz, factory2MiB (не A/B), PSRAM octal80MHz8MB.

Preflight известной платы: flags0, Secure Boot/Flash Encryption Disabled,
SPI_BOOT_CRYPT_CNT0. Esptool5.6.0 записал bench bootloader/partition/app:
exit0/3 hashes verified. После capture таким же способом вернули **ordinary
rev12 того же1bfc7e2**, exit0/3 hashes verified. NVS отдельно не стирали,
eFuse/security settings не меняли. Не запускали OTA body/installer/boot selection,
CAN/STM32/приводы. Возврат ordinary не является factory backup restore trial.
Поля device_access=false в validation.json относятся только к offline build check,
а не ко всему этому отдельно согласованному аппаратному испытанию.

## Одно наблюдаемое окно

Один UART open, согласованный RTS reset, baud115200, deadline90s, timeout<=0.25s.
Измерено90.007s с завершением чтения/cleanup,21896bytes. В capture один
rst:0x1/POWERON, app1bfc7e2/SDK6.1.1/PSRAM8MB/RGB/UI ready подтверждены startup.
17 diagnostics, uptime7415..87679ms строго растёт. Panic/Guru Meditation не
наблюдались в этом окне. Причина UART-related reset отдельно не изолирована;
open/close могут reset. После close и после возврата ordinary UART не открывали,
поэтому нет утверждения о непрерывной работе вне окна или UART-confirmed boot
ordinary. После возврата владелец подтвердил главный экран, касания и Wi-Fi.
Это qualitative проверка, не замер физической latency/scanout/FPS.

Raw security/flash/UART logs остаются локально вне Git. Здесь только allowlisted
startup/bench lines, numeric/fixed-schema diagnostics и offline build metadata;
IP/SSID/MAC/паролей/ключей/dump/header values нет.

## Сетевая проба

| Этап | Наблюдение |
|---|---|
| Настройки | allocator=spiram,tls_rx16384,tls_tx4096,cert_dates1 |
| SNTP | result0,time_result0,2973ms,cleanup1,source2 (SNTP_UNAUTHENTICATED) |
| TLS/HEAD | result0,status200,2831ms,cleanup1,header_bytes5590 |
| Errors | native_error0,tls_error0,cert_flags0 |
| Monitors | Оба monitor_stop0,historical_min_restored1 |
| Final | finished pass=1 stage=tls |

Один HEAD публичного https://github.com/avlyubimov/TV4-CNC, без auth/cookies/body/
redirects. Штатные CA bundle/hostname/X.509 notBefore/notAfter включены;
успешный результат относится к этому endpoint/сертификату/моменту и config.
Negative bad-CA/expired/future/clock-jump/redirect/timeout cases на устройстве
не испытывались. Флаги0 здесь сопровождаются успешным TLS/HTTP200, в отличие
от OPEN FAIL предыдущей пробы. Actual SNTP принят private2026-only policy;
source остаётся unauthenticated, это не production UTC trust.

[Предыдущий b085abb](../2026-11-19-b085abb-utc-tls/README.md) завершался OOM
до TLS handshake. Этот изолированный failure преодолён при PSRAM-only SDK
allocator и включённых X.509 dates. Не выводить из этого общий memory budget,
работу GitHub assets/CDN/manifest/image download или разрешение install.
Профиль делает одну пробу на boot; возможные загрузки вне capture не измерены
и не объединяются с этим наблюдаемым окном.

## Память и стек

Все значения bytes; local minima включают параллельные HMI/Wi-Fi/SDK/monitor,
это не attribution одного TLS allocation. Largest — sample, не continuous minimum.

| Snapshot | Internal free | Min (window/history) | Largest | PSRAM free | PSRAM min | Worker stack watermark |
|---|---:|---:|---:|---:|---:|---:|
| SNTP window start |22795|22795|7680|7614424|7614424|3760|
| SNTP window end |22367|20331|7680|7614424|7614424|3312|
| TLS window start |22367|22367|7680|7614424|7614424|3312|
| TLS window end |21515|3123|7680|7614424|7581240|912|
| Worker end, historical tracking |21515|2408|7680|7614448|7581240|912|

**Internal headroom и stack остаются тесными:** TLS window min3123bytes,
после stop historical min2408bytes, watermark912bytes из6144. History после
stop — другая совокупность SDK minima, её нельзя смешивать с window как
одновременный снимок. Рост стека без review может дополнительно забрать internal
heap. Не утверждаем достаточный safety margin или отсутствие regression.

После self-delete, последний diag: internalfree28307,min2408,largest9216,
PSRAMfree7614448,min7581240,total8388608. PSRAM free вернулась к sampled before
значению, но concurrent snapshots не доказывают leak/no-leak. Остальные
allocations HTTP/TCP/SDK могут оставаться internal при PSRAM-only mbedTLS.

Во всех17records display_errors/touch_errors/queue_drops/export_drops/
wifi_queue_drops/watchdog_boot/wifi_disconnects=0; последний wifi_ip_events1.
CPU/physical touch-to-display/RGB scanout=null; idle render_samples не FPS0.
SDK I/O/deadline не hard preemption. Не измерены пиковая stack потребность на
всех cert/error paths, Wi-Fi scan/reconnect/keyboard pressure или длительный soak.

## Следующий gate и текущее устройство

Нужен отдельный review оставшихся internal allocations/stack worst cases и
запаса HMI/Wi-Fi прежде runtime OTA session/body или installer. Production UTC,
CA/negative/redirect/CDN/failure/soak/performance и cache-off/PSRAM/security
политики также открыты. PSRAM этого bench не зашифрована; session material
мог находиться в ней, production конфиденциальность не заявляется.

Текущее устройство: **1bfc7e2 ordinary rev12, diagnostic probes off**, запись
проверена3/3, HMI/touch/Wi-Fi подтверждены владельцем. Ordinary image сохраняет
internal allocator и исходные параметры; TLS/UTC worker в нём отсутствует.
Согласованные операции исполнены; новые device/flash/reset/serial/network
тесты требуют отдельного разрешения на конкретный image и процедуру.

Исторический отчёт1bfc7e2. Позднее выполнена отдельно согласованная
[headroom проба8f0d899 и возврат ordinary](../2026-11-19-8f0d899-headroom/README.md).
Старые measured minima и ограничение production budget сохраняются.
