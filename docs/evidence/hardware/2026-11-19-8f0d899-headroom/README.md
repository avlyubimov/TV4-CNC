# TLS headroom 8f0d899 — 19 ноября 2026

**SNTP/verified HEAD HTTP200 PASS; наблюдаемый запас RAM/стека вырос.**
После одного capture возвращён ordinary rev12 того же SHA, владелец подтвердил
HMI/touch/Wi-Fi. Это одна проба, не production OTA memory/performance approval.

Source8f0d8991a8f76762cc6b213c6936bb2aa0a66c68, main после review/merge PR #24.
Waveshare ESP32-S3-Touch-LCD-7 Rev1.2, ESP-IDF6.1.1, LVGL9.2.2.
Владелец отдельно разрешил обе записи этого SHA/security preflight/reset/UTC/
network и один UART capture около90s; подтвердил только USB/питание без CAN/
исполнителей. Main CI4workflows success. Factory backup16777216bytes повторно
SHA-256 PASS. Никаких исходников/проверок/резервов ради успешного теста не меняли.

## Сборки и запись

Чистый detached source, оба профиля tools/build_idf.sh PROFILE6.1.1local,
project_version8f0d899. Version/config/header/layout/ELF/SHA PASS, compact getter/
phase markers и X.509 date funcs в bench ELF присутствуют. В evidence — оба
validation.json и build.sha256; пути хешей относительны build directories.
Bench app1578432bytes,SHA256b5a4e50e2a420171c69e33e9e625160ea989510cc204f50dadbc5c3a12ae45bd.
Ordinary app1573568bytes,SHA256b2680a2abe0aa44b22875ff6445154b7c7331b91a2b65de4715792082a5365da.
Flash16MB DIO80MHz,factory2MiB (не A/B),PSRAM8MB octal80MHz.

Preflight известной платы: flags0, Secure Boot/Flash Encryption Disabled,
SPI_BOOT_CRYPT_CNT0. Esptool5.6.0: bench bootloader/partition/app exit0/3 hashes
verified; после capture ordinary rev12 того же8f0d899 exit0/3 hashes verified.
NVS отдельно не стирали, eFuse/security не меняли. Не запускали body/installer/
OTA boot selection/CAN/STM32/приводы. Возврат ordinary — не factory restore test.
validation.json device_access=false относится к offline build inspector,
не ко всему этому отдельно согласованному аппаратному испытанию.

## Одно окно наблюдения

Один UART open, согласованный RTS reset,115200baud,deadline90s/timeout<=0.25s.
Измерено90.008s с окончанием чтения/cleanup,22775bytes. Capture содержит один
rst:0x1/POWERON, app8f0d899/SDK6.1.1/PSRAM8MB/RGB/UI ready. 17diagnostics,
uptime7376..87642ms строго растёт. Panic/Guru Meditation в окне не наблюдались.
Причина UART-related reset не изолирована; open/close могут reset. После close
и после возврата ordinary UART не открывали. Boots/пробы вне capture неизвестны
и не объединяются с этим окном; профиль допускает одну пробу на каждый boot.

Raw UART/security/flash logs только локально вне Git. Опубликованы allowlisted
startup/bench lines, numeric diagnostics/capture/comparison и offline artifacts,
без IP/SSID/MAC/паролей/ключей/header values/dump. После ordinary возврата
владелец подтвердил главный экран, касания и Wi-Fi — qualitative, не UART boot
verification, не latency/FPS/нагрузочный тест во время TLS.

## Результаты

| Этап | Наблюдение |
|---|---|
| Allocator | spiram,TLS RX16384/TX4096,cert_dates1 |
| HTTP | RX2048/TX512,HEAD без auth/body/redirect |
| SNTP | result0,time_result0,374ms,cleanup1,source2 (SNTP_UNAUTHENTICATED) |
| TLS | result0,status200,2871ms,cleanup1,header_bytes5587 |
| Errors | native_error0,tls_error0,cert_flags0 |
| Monitors | Оба monitor_stop0,historical_min_restored1 |
| Final | pass1,stage=tls |

Endpoint https://github.com/avlyubimov/TV4-CNC, штатные CA bundle/hostname/
X.509 notBefore/notAfter и fresh anchor. Успех относится к этому endpoint/config/
сертификату/моменту, не negative expired/CA/hostname/clock cases или production
UTC trust. Actual SNTP принят private2026-only policy, source unauthenticated.

Сумма headers5587 больше HTTP RX2048: поток успешно разобран при чтении по
ограниченным частям. Это не доказательство размера/разбиения конкретного поля:
длины отдельных headers не логируются, controlled fragmented/oversized value
cases ещё открыты. Прежние post-parser128/8192/16384 bounds не pre-allocation cap.

## Сопоставление с1bfc7e2

[Baseline](../2026-11-19-1bfc7e2-tls-psram/README.md), те же SDK/плата/worker
stack6144/TLS records16384/4096/DMA reserve64KiB. В новом image HTTP RX2048 и
status-only Wi-Fi getter, добавлены phase logs. Два разных boots/сетевых запроса,
нагрузка/ответ/время различаются; это наблюдаемое сравнение, не controlled attribution.

| Значение |1bfc7e2|8f0d899| Наблюдаемая разница |
|---|---:|---:|---:|
| TLS window internal minimum, bytes |3123|5307|+2184|
| Historical minimum после window stop, bytes |2408|4204|+1796|
| Worker stack watermark, bytes |912|1840|+928|
| TLS-end largest sample, bytes |7680|7680|0|
| Последний diag largest sample, bytes |9216|13824|+4608|
| HEAD lifecycle elapsed, ms |2831|2871|+40|

Window/history — разные SDK aggregates capability heaps; не смешивать их как
одновременный snapshot или continuous minimum largest block. Stack watermark —
наблюдение этого boot/path, не worst-case/approved margin. Разница928 согласуется
со static frame reduction PR #24, но не доказывает запас на всех error/cert paths.
Разницу heap не приписывать только RX2048: concurrent/fragmentation/logging также
участвуют. +40ms в одном сетевом запросе не измеряет CPU/regression/статистику.

## Фазы нового TLS window (bytes)

| Фаза | Internal free | Window min | Largest sample | PSRAM free | Worker stack watermark |
|---|---:|---:|---:|---:|---:|
| before HTTP init |22471|22471|7680|7614424|4240|
| after HTTP init |18683|18667|7680|7614424|4240|
| after TLS open |12947|5307|7680|7592704|1840|
| after headers |14823|5307|7680|7592704|1840|
| after cleanup |21435|5307|7680|7614424|1840|
| TLS window end |21631|5307|7680|7614424|1840|

Понижение window min до5307 уже наблюдается после open, чтение headers не дало
меньшего recorded minimum. Это сужает область дальнейшего review, но все окна
включают **все concurrent HMI/Wi-Fi/SDK/monitor/logging tasks**, не только TLS.
HTTP init/free разницы и PSRAM возврат не являются доказанным leak/no-leak.
После stop worker_end: internalfree21631,historymin4204,PSRAMfree7614448,
PSRAMmin7581240,stack1840. После self-delete последний diag: internalfree28407,
min4204,largest13824,PSRAMfree7614448,total8388608. PSRAM sampled free до/после
совпадает, continuous total-system stability/full RAM stress не проверялись.

Во всех17records display_errors/touch_errors/queue_drops/export_drops/
wifi_queue_drops/watchdog_boot/wifi_disconnects0; последний wifi_ip_events1.
CPU/physical touch-to-display/RGB scanout=null; idle render_samples не FPS0.
SDK calls не hard-preemptible. Нет quantitative UI load/keyboard/scan/reconnect/
thermal/soak/negative certificate/full OTA workload результатов.

## Текущее состояние и следующий gate

На плате **ordinary rev12 8f0d899, diagnostic probes off**, запись3/3 проверена,
владелец подтвердил HMI/touch/Wi-Fi. Ordinary использует internal mbedTLS allocator,
параметры UI/DMA/stack прежние. Разрешённые операции исполнены, новых device/
flash/reset/serial/SNTP/HEAD tests без отдельного согласования exact image нет.

HEAD PASS и улучшение headroom не являются production OTA budget approval:
internal minimum5307/stack1840 всё ещё ограничены, worst-case не измерен.
Далее нужны review handshake/internal allocations/call stack и bounded admission/
отказы/нагрузка перед runtime session/body. Production UTC/CA/redirect/CDN/
security/PSRAM/cache-off/performance/factory restore/install/rollback gates открыты.
PSRAM не шифруется; session material мог быть в ней. Новых ключей/eFuse/релиза,
установки OTA или управления станком этот отчёт не разрешает.
