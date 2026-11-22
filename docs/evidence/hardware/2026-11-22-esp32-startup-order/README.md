# ESP32: аппаратный замер нового порядка старта, 22 ноября2026

База main `b5f4b381fd392877498b5df210b3bb85c551985b`, PR #1–64 слиты. Один отдельно
разрешённый стенд Waveshare ESP32-S3 Rev1.2, только питание/USB, без CAN/приводов.
Профиль rev12-ota / ESP-IDF6.1.1 / diagnostics1, owner key/context отсутствуют.
Этот PR содержит только документацию и числовые результаты; firmware не меняется.

[Итог](summary.json), [samples](diagnostics.json), [сравнение с fc90200](comparison.json),
[ELF/config checks](build-inspection.json), [32 offline tests процедуры](procedure-tests.log).
Raw UART/SDK logs, dump/NVS/credentials и идентификаторы устройства приватны.

## Подтверждённые действия

1. Identity/chip9/ESP32-S3/Flash16MiB/security0/отключённые Secure Boot и Flash
   Encryption проверены, esptool5.6.0 official RAM stub/460800baud. eFuse не писались.
2. Fresh16777216bytes fecc05d snapshot: device MD5 и SHA-256 двух одинаковых
   private копий на независимых NVMe PASS, files0600/folders0700/fsync.
   SHA-256 `ce6af4e88d755254f65ca2cf952d17ad9892a8ec7f487b0ed4391eccc3355e5a`. Старые checkpoints не перезаписаны.
3. Exact fecc05d prefix, bootloader/table/erased otadata совпали до записи.
   App1600576bytes SHA-256 `e4a2ded8c559c0fb28c939e69c8332e022ed5733954d3026649519368787427f` записан только по0x10000.
   SDK verify/MD5 всех областей вне factory/bootloader/table/otadata PASS.
   ELF SHA-256 `582c3af94a18ec91398d096d43230af4441241b71f59c22d0baab15e0ac1d5fc`.
4. Reset, capture90.173s: одна загрузка b5f4b38,
   PSRAM8MiB/SDK test/instructions+rodata XiP/RGB800×480/GT911/LVGL ready,
   явный factory self-test checks_pass1/sdk_finish1. Все7 slots/mask127 и
   BOOT start0/stop0/complete1;16 periodic samples, valid OTA stack, Wi-Fi IP
   event, zero error/drop/stale-touch/panic/reset-loop checks PASS.
5. Из fresh snapshot возвращены все2097152bytes factory fecc05d, verify/
   MD5 сохранности вне factory/bootloader/table/otadata/reset PASS.
   Factory SHA-256 `081d99e707f578824cc9058d1d18c4f87700b2e60a09afa907e98a9c33bffd1a`;
   app prefix `de55953fc9e3c4080cb277d734ca7d1f02c74880db38dac0f4b53830f587d021`.
   Capture30.048s подтвердил одну загрузку fecc05d,
   PSRAM/XiP/RGB/GT911/UI и4 periodic samples без ошибок, Wi-Fi IP event.
6. Владелец подтвердил экран/касания/Wi-Fi/OTA SETUP на b5f4b38 и после возврата fecc05d.
   На ESP32 оставлен fecc05d/factory/tv4-16m-ab-v1. Full16MiB recovery не
   понадобился и не испытывался; factory-partition return проверен.

Сеанс609.074s из1200: 2026-11-22T13:11:01.516033+00:00 → 2026-11-22T13:21:10.590533+00:00
(18:11–18:21 Asia/Almaty), diagnostic-write1/factory-return1/full-recovery0,
два UART capture. Разрешение использовано, новых device/reset/UART/SWD действий
после закрытия нет. F407 остаётся ee7d179; CAN/motion/option bytes/OTP не трогали.
MD5 вне factory проверялся до/после каждой записи, до app boot. NVS после boot
может обновляться; побайтного сравнения NVS между всеми загрузками не было.

## Измеренные значения

Байты, sample_ms — ESP timer, не полное время от включения питания. Samples
печатаются по ID0..6; таблица ниже отсортирована по новой хронологии
0,1,2,3,5,6,4. UI_READY теперь предшествует optional services; WIFI_STARTED —
возврат service_start, не готовность DHCP. Scope1 — local BOOT cumulative minima.

| ID/граница | ms | Internal free | Minimum | Largest | PSRAM free | Main stack watermark |
|---|---:|---:|---:|---:|---:|---:|
| 0 APP_ENTRY | 51 | 267039 | 202308 | 172032 | 6860396 | 14980 |
| 1 RGB_READY | 376 | 229907 | 229907 | 155648 | 6091556 | 14084 |
| 2 DRAW_READY | 381 | 191371 | 191371 | 118784 | 6091556 | 14084 |
| 3 TOUCH_READY | 700 | 178887 | 166119 | 104448 | 6091356 | 12196 |
| 5 DASHBOARD_READY | 1384 | 78371 | 78371 | 63488 | 6091356 | 12196 |
| 6 UI_READY | 2305 | 86363 | 60843 | 63488 | 6091356 | 10516 |
| 4 WIFI_STARTED | 2316 | 52515 | 52515 | 43008 | 6090892 | 10516 |

| BOOT point | Scope | Internal free | Minimum | Largest | Sampler stack |
|---|---:|---:|---:|---:|---:|
| before | 0 | 267039 | 202308 | 172032 | 14772 |
| start | 1 | 267039 | 267039 | 172032 | 14772 |
| end | 1 | 22787 | 20715 | 14336 | 1968 |
| after stop | 0 | 22787 | 7796 | 14336 | 1968 |

Startup sample_stack — main; BOOT end/after — worker. Late worker_stack_at_emit
нельзя выдавать за main stack на старой границе.16 valid periodic OTA samples
дали1664bytes из3072; main10516, touch2504, Wi-Fi1112, export min1184bytes.
Неизмеренный будущий worker8192 не выводится арифметическим вычитанием из heap.

## Сравнение и инженерные выводы

| Метрика | fc90200 | b5f4b38 |
|---|---:|---:|
| APP_ENTRY history | 202308 | 202308 |
| BOOT local minimum | 1887 | 20715 |
| History после stop | 112 | 7796 |
| Largest в BOOT end | 7680 | 14336 |
| Periodic largest | 7680 | 12288 |
| Periodic internal free | 22279..22343 | 21575..21751 |
| Periodic PSRAM free | 6090124 | 6090108 |
| OTA-worker watermark | 1488/3072 | 1664/3072 |
| UI_READY sample_ms | 2307 | 2305 |
| WIFI_STARTED sample_ms | 763 | 2316 |

Подтверждены новый порядок и числовой результат этого запуска: local minimum
и largest block больше, steady free немного меньше. Это совместимо с меньшим
перекрытием cold render/service allocations и иным порядком фрагментации;
каждый allocator/heap не трассировался, причинный вклад по функциям не доказан.
Обе local SDK configs и table побайтно прежние, clocks/DMA64KiB/TLS16384/4096/
framebuffer/dependency pins не менялись. Host cold frame до/после был одинаковым,
но host builtin pool не равен target CLIB/internal/PSRAM allocator.

По одному instrumented boot на версию нельзя установить worst-case floor,
устранение всех OOM или performance. Тот же возвращённый fecc05d в этой пробе
дал history6956 вместо прежнего112 при exact same factory bytes: allocation
history зависит от runtime/сети/расписания. SDK суммирует per-heap minima, а не
измеряет simultaneous global free. Нельзя объявлять20715 резервом под TLS.
UI_READY почти совпал в двух запусках; Wi-Fi стартует позже по замыслу PR #64.
Это не измерение полной boot/network latency, FPS, tearing или touch-to-display.

BOOT window заканчивается по HMI health/PSRAM checks, не ждёт Wi-Fi/DHCP/IP.
И в baseline, и в новом run первый periodic sample имел wifi_ip_events0;
следующие —1. Факт выдачи IP известен из периодических counters, не точной
привязки каждого transient allocation к DHCP.
Поэтому BOOT minima не сравнивают одинаковую завершённую сетевую нагрузку.
В connected-only subset: baseline15samples против новых15samples,
новые uptime11389..81465ms, internal free
21575..21739, largest12288,
history7796, OTA stack1664. Largest выше baseline7680 и после наблюдаемого IP,
но transient peak/производительность TLS/install этим не измерены.

## Программные проверки и оставшиеся аппаратные gates

Перед стендом clean main b5f4b38 дал normal1598560bytes и trace1600576bytes,
image/ELF/Flash16MiB/PSRAM/A-B/config/provenance PASS; trace data432≤512bytes,
own compiler frame≤112≤192bytes. Это не полный runtime stack. Normal b5f4b38
в этом стенде не записывался. Точный источник/план/procedure/analyzer hashes
в summary.json.32 actual finite-procedure/FakeSDK tests проверяют backup/write/
return/recovery faults, новый temporal order, privacy и отсутствие авторизации.
Это offline tests, не реальные отказы питания/Flash или security bypass.
Main CI15/15 до стенда зелёный; предыдущий [software evidence](../../build/esp32-startup-memory-order/README.md)
сохраняет host97/sanitizers97 и SDK6.1.1+5.3.2 matrix12. Docs-only отчёт не
объявляет новую локальную firmware сборку после закрытия аппаратного сеанса.

Следующее: повторяемость/soak нового старта и бюджет worker8192/настоящего TLS/
install после reviewed local key/SemVer/time/resource context. По решению
владельца ключа пока нет; CHECK/INSTALL в SETUP закрыты. Не назначать floors из
этой таблицы. Configured install, inactive slot confirmation/rollback,
Wi-Fi/power-loss/full16MiB recovery, normal b5f4b38 runtime и FPS/latency/soak
не приняты. Новые device operations требуют отдельного exact-image разрешения;
повторная migration/ordinary rev12 flash не входят в следующий шаг автоматически.

[Следующий этап](../../../NEXT_STAGE.md), [аппаратный статус](../../../HARDWARE_STATUS.md),
[резервные копии](../../../BACKUP_STATUS.md). Исторический fc90200 report сохранён.
