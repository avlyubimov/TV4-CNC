# PSRAM workspace bench — 18 ноября 2026

**Наблюдаемый результат: completed=3 pass=1.** Это allocation-only bench,
не TLS/HTTPS/OTA install или полный PSRAM stress test.

Firmware source 1c38d1e1e20d2d4d12128a3812767dfe01de044c, reviewed/merged PR #16.
Profile rev12-memory-bench, ESP-IDF 6.1.1, LVGL9.2.2, Waveshare Rev1.2.
Владелец отдельно разрешил запись/reset/один UART-сеанс 60 секунд и подтвердил
стенд только USB/питание без CAN/исполнителей. CI этого main все success;
локальная чистая сборка совпадает с app version 1c38d1e в boot log.

## Запись и capture

Заводской backup 16MiB повторно проверен SHA-256 PASS до записи. Esptool5.6.0
записал bootloader/partition/app; 3/3 Hash of data verified, exit0. App 1569008
bytes, SHA256 9153fe08f43ea97335a42b27fc20940e40d775325590b41d6f672e5323b30285.
Flash16MB DIO80MHz, factory2MiB не A/B. NVS отдельно не стиралась, eFuse не
изменялись; preflight Secure Boot/Flash Encryption disabled.

После записи — одно открытие UART, согласованный RTS reset и 60 секунд чтения.
В захвате один rst:0x1/POWERON, 11 diag records, uptime7535..57666ms строго растёт.
App version/PSRAM8MB/RGB/UI ready подтверждены startup-summary.log.
Panic/Guru Meditation, второй boot в этом окне не наблюдались. Состояние после
закрытия UART не наблюдалось: open/close ранее сопровождался reset, поэтому
нет утверждения об отсутствии reset после close или вне этой минуты.

Полные локальные flash/UART logs не добавлены. Здесь только allowlisted startup,
самодостаточные числовые bench lines и numeric/fixed-schema JSON без
USB/MAC/IP/SSID/паролей/ключей/dump.

## Три цикла, наблюдал агент

Target workspace34000 bytes. PSRAM-only acquire прошёл region/headroom checks.
Volatile A5/5A проверяет/очищает 22528 bytes metadata/HTTP chunk; control/hash
поля не изменяются. CPU/cache-visible probe, не uncached/DMA/все8MB.
Worker стартовал на19327ms (~15s после UI ready), завершил summary на20095ms.

| Цикл | Acquire us | Pattern us | Release us | PSRAM free до | Allocated | После release | Результат |
|---|---:|---:|---:|---:|---:|---:|---|
| 1 | 2281 | 4821 | 416 | 7616576 | 7581756 | 7616576 | PASS |
| 2 | 2330 | 5314 | 591 | 7616576 | 7581756 | 7616576 | PASS |
| 3 | 2369 | 4874 | 600 | 7616576 | 7581756 | 7616576 | PASS |

В каждом цикле pattern_ok=1 и release result=0. Free PSRAM возвращается
к7616576bytes после каждого release; роста удерживаемой PSRAM в этих трёх
snapshots не видно. Concurrent UI/Wi-Fi могут менять heap — это не общий
proof of no leaks и не production TLS policy. Частные probe floors остаются
только allocation-only параметрами этого reviewed image.

## RAM/ошибки

- Во время worker internal free22731..22767, largest12288bytes; internal min7796.
- После worker, последний diag: internal free26971, largest12288, PSRAMfree7616576,
  PSRAMmin7581756/total8388608. Internal headroom остаётся ограниченным.
- Bench stack minimum watermark2684bytes из4096; task завершилась self-delete.
- В diag: owner watermark10824, touch2548, Wi-Fi1116, export1280 (последний sample).
- Во всех11records display_errors/touch_errors/queue_drops/export_drops/
  wifi_queue_drops/watchdog_boot=0. wifi_ip_events=1/disconnects=0.
- cpu_percent/touch_to_display_ms/rgb_scanout_hz=null. us относятся к конкретным
  worker операциям, без printf; это не end-to-end UI latency/FPS/CPU measurement.

Владелец в ходе теста подтвердил: главный экран, касания, Wi-Fi работают как
прежде. Это qualitative подтверждение, не измерение физической задержки.

## Следующее

PSRAM allocation gate пройден в этой конфигурации. Production TLS budget ещё
НЕ подтверждён: нужны проверенный источник времени, TLS peak/CA/redirect/failure
profiling и запас internal heap. Никаких HTTPS/session_run/Flash OTA/boot selection,
CAN/STM32/приводов во время теста не запускали. Soak/power-loss/rollback/restore,
UART reset cause, полный PSRAM memtest и физические performance targets открыты.

На плате остаётся **1c38d1e, rev12-memory-bench**: каждый boot допускает одну
такую пробу, максимум три цикла. Возврат к ordinary rev12 или новый image/reset/
serial test требует отдельного разрешения; этот docs PR его не предоставляет.

Историческое состояние на18ноября. 19ноября выполнено отдельное согласованное
испытание и [возврат ordinary b085abb](../2026-11-19-b085abb-utc-tls/README.md).
