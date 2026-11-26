# Waveshare Rev1.2: signed INSTALL и traced operator restart — PASS

26 ноября2026, принимающий source `8b3db2f62584267e9e00f372d65b95c8029cdfe1`,
ESP-IDF6.1.1. [Код PR #95](https://github.com/avlyubimov/TV4-CNC/pull/95),
[сборки/host/CI](../../build/esp32-ota-restart-trace/README.md),
[числовой отчёт](measurements.json). Новый конечный сеанс закрыт за1333.173 с
при лимите3300 с. UART закрыт; **bench.5 оставлен в OTA1 VALID**.
После закрытия владелец подтвердил главный экран, касания и Wi-Fi.

Это одна успешная диагностическая post-INSTALL попытка. Причина
[прежнего RTCWDT bench.5](../2026-11-25-esp32-bench5/README.md) не установлена;
его FAIL сохраняется. [Первый control-отказ этого этапа](attempt1.md) также
сохранён: malformed startup marker source `2cae6c4`, остановка до сети,
одна полная свежая recovery к bench.4 с verify/owner health PASS.

## Реальные образы и последовательность

Временный receiver: версия0.1.0-bench.4, source `8b3db2f`,
1603280 байт/SHA-256 `c5f689c6e259694c669f06e6372e39718fb86e9ee2b7d955d4e84d16be37de9c`.
Это диагностический образ с новым startup logger, не published bench.4.
Контроль90 с дал37/37 базовых guards, intact serialized boot marker
`reset=1 core=0 operator=1` и `armed ready=1` PASS; автоматического CHECK/reboot нет.

Кандидат — прежний неизменяемый published `v0.1.0-bench.5`,
source `0dbc1e919bc671554a005dac1896562c6554fe26`,1600720 байт,
SHA-256 `d6a59a78595541d0fa45fc327dd2ba0a6e0fb5006303c0f5921c4669cd49347a`.
Нового release/tag/ключа нет; подписанный manifest, локальный public key,
TLS certificate/hostname/date, SHA/размер/совместимость и policy сохранены.

Владелец выполнил штатные CHECK → INSTALL → Подтвердить → Перезапуск.
SNTP/time/cleanup:(0,0,1) дважды. CHECK state6/error0/9resource checks,
INSTALL state10/error0/798 checks; terminal stages5/8,error0.
Непрерывный UART охватил request core 1 → shutdown_hook core 1 → noos core 1,
затем ROM `RTC_SW_CPU_RST` и полный90.160 с passive capture без host reset.
Исходный SDK reset не заменялся; watchdog, драйверы, часы и DMA прежние.
21/21 candidate guards PASS;17 samples, последний uptime86462 мс,
во всех reset_reason3/watchdog_boot0. Native self-test/confirmation наблюдались.
Расположение на ядре — измерение одного сеанса, не установленная affinity.

## Ресурсы и сенсор

| Наблюдение | Результат |
|---|---:|
| SESSION local minimum internal free | 96739 байт |
| SESSION local minimum PSRAM free | 5903896 байт |
| Наименьший boundary largest internal | 63488 байт |
| Минимальный operation stack remainder | 7072из12288 байт |
| Максимальный touch sample age во время операций | 20 мс |
| Максимальный touch sample age passive candidate | 13 мс |

Прежние floors internal64/32 KiB, PSRAM1 MiB/128 KiB, stack3072 сохранены.
Все serialized heap windows/cleanup, ошибки touch/RGB/очередей/export и
монотонность samples прошли строгую проверку. PSRAM8 MiB/memory test/XiP,
RGB800×480/LVGL9.2.2/GT911 initialization и сохранённый Wi-Fi наблюдались.
Это resource minima и sample freshness данного сеанса, не полный transient
stack/heap worst case, physical touch-to-display latency или FPS.

## Flash и оставленная прошивка

Свежие16МиБ реально считаны, device verify/SHA-256/две физические NVMe-копии
проверены до записи. Нормальные host writes: весь OTA0[0x220000,0x520000),
первые4 KiB otadata по0x210000 и возврат исходного полного OTA0 только после
строгого candidate PASS. Все3writes прошли verify и сравнение Flash вне span.
Сам INSTALL записал неактивный OTA1 через прежний native SDK backend.
Bootloader/table/factory/PHY/resources/store обычными host writes не затрагивались.

После полного native reboot/self-test независимо прочитаны metadata:
seq3VALID/OTA0 и seq4VALID/OTA1; выбранOTA1. Полный candidate+FF padding проверен.
После возврата inactive slot все16МиБ заново реально считаны и проверены.
Точный published BIN, оба VALID metadata sectors, весь исходный inactive OTA0
и все остальные байты вне NVS[0x9000,0xf000) совпали с ожидаемой картой.
Две финальные NVMe-копии и повторное независимое сравнение сохранённых файлов PASS.
Второй host reset выполнялся после passive90s для независимого USB readback;
последний UART30 с health21/21PASS, Wi-Fi подключён. Full recovery0;
рутинного возврата к bench.4 нет. Временная диагностика на плате не оставлена.

## Вывод и оставшиеся блокеры

Startup marker больше не перемешался с SDK log в наблюдавшемся control boot.
Реальный signed INSTALL → normal restart → строгий watchdog/self-test →
independent full readback прошёл на SDK 6.1.1. Сравнение с первоначальным отказом
не устанавливает причину: instrumentation/timing, core placement, предшествующие
TLS/Flash и состояние периферии ещё не разделены. Следующий измеримый P1 —
сопоставимый конечный запуск без restart instrumentation с теми же guards;
пороги/watchdog не менять и закрытые пакеты не повторять.

Стендовая policy .4→.5 действует только до27 ноября2026 08:00 Asia/Almaty;
автоматического продления нет. Production policy/budget/discovery, независимые
RF/AP/power loss, runtime 5.3.2 и physical FPS/latency открыты. Успешная
диагностическая попытка не является production approval или завершением P1.
F407ee7d179, CAN, приводы, eFuse и option bytes не открывались/не изменялись;
питание не отключалось, независимая аппаратная E-stop цепь сохраняется.
Дампы/NVS, SSID/IP/MAC, ключи/context и сырой UART приватны.
