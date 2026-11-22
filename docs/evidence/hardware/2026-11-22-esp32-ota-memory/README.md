# ESP32: фазные замеры BOOT и возврат fecc05d, 22 ноября 2026

База — main fc9020087e426c5c06129654f8a242612e184079, PR #1–62 слиты.
По отдельному разрешению на изолированной Waveshare ESP32-S3 Rev1.2 выполнен
один стенд rev12-ota / ESP-IDF6.1.1 / TV4_OTA_MEMORY_DIAGNOSTICS=1.
Диагностический app1600528bytes, SHA-256:
11476e581bd84bb3b6cdd8622416f8857ae52032fadcf4e9fd5946548a30e398.
Этот отчёт меняет только документацию; новые firmware исходники не добавлены.

[Машиночитаемый итог](summary.json), [числовые samples](diagnostics.json),
[проверка actual ELF/objects](build-inspection.json), [26 программных tests процедуры](procedure-tests.log).
[Проверка согласованности, ссылок и scope](checks.json).
Raw UART/SDK logs, идентификатор устройства, NVS и все дампы остаются приватными.

## Подтверждённые аппаратные действия

1. Сверены ожидаемые identity/ESP32-S3/chip ID9/16MiB/нулевые security flags,
   Secure Boot и Flash Encryption отключены. Штатный esptool5.6.0 RAM stub,
   460800baud. eFuse/OTP не программировались.
2. Считаны все16777216bytes текущего fecc05d; device MD5 и SHA-256 локальных
   копий проверены. Две одинаковые приватные копии на разных физических NVMe,
   files0600/folders0700; исходные backups не перезаписаны.
   Snapshot SHA-256: d84fe13900806d9646e1e2edb8df0c4fe897aa6de85f14c28bfd0624f101ff46.
   Это checkpoint fecc05d с tv4-16m-ab-v1 до диагностического запуска,
   не legacy8f0d899 и не vendor factory image.
3. До записи совпали exact bootloader/table/fecc05d prefix и FF otadata.
   Записан только diagnostic app по0x10000. Затем SDK verify, контроль MD5
   всех областей вне factory0x10000..0x20ffff, bootloader/table verify и
   чтение FF otadata — PASS. Никакой повторной миграции разметки не было.
4. Reset и capture90.103s/23176bytes.
   Одна app-загрузка fc90200, PSRAM8MiB/SDK memory test/instructions+rodata XiP,
   RGB800×480/GT911/UI, явный boot_self_test checks_pass=1 sdk_finish=1 — PASS.
   Все7 startup slots, mask127 и одно BOOT window start0/stop0/complete1.
   Нет panic/assert/watchdog/reset-loop/touch-stale ошибок.
5. По0x10000 возвращены все2097152bytes factory из свежего снимка.
   App prefix SHA-256: de55953fc9e3c4080cb277d734ca7d1f02c74880db38dac0f4b53830f587d021.
   Полный factory payload SHA-256: 081d99e707f578824cc9058d1d18c4f87700b2e60a09afa907e98a9c33bffd1a.
   SDK verify и контроль всех областей вне factory/bootloader/table/otadata — PASS.
   После reset capture30.051s/10218bytes подтвердил fecc05d/SDK6.1.1/
   PSRAM8MiB/XiP/RGB/GT911/UI, одну загрузку и отсутствие fatal/error counters.
6. Владелец после возврата подтвердил экран, касания, saved Wi-Fi и OTA SETUP
   с недоступной установкой. На плате оставлен fecc05d/factory, та же A/B-карта.
   Factory-partition restore испытан; условный полный16MiB restore не понадобился
   и не выполнялся. Заводской vendor restore этим не испытывался.

Сохранность вне factory проверялась MD5 до/после каждой записи, до app boot.
Побайтного сравнения NVS не было; Wi-Fi может законно менять NVS после запуска.
Нельзя выдавать эти проверки за неизменность NVS между всеми последующими boot.
Всего один сеанс608.700s при разрешённых1200s, два capture, app-write1/
factory-return1/full-recovery0. Начало11:34:48.028UTC, конец11:44:56.728UTC
(16:34–16:44 Asia/Almaty). Сеанс закрыт; новых device operations после него нет.
STM32 ee7d179, CAN/приводы и eFuse/защитные настройки не затрагивались.

## Измерения памяти и стека

Все размеры ниже в байтах. sample_ms — ESP timer, не полная задержка
от включения питания. Startup sample_stack относится к main, а не OTA-worker.
Minimum внутри startup1..6 кумулятивен от начала BOOT window; это не
изолированный пик каждой строки и не simultaneous global free.

| Граница | sample_ms | Internal free | Minimum | Largest | PSRAM free | Main stack watermark |
|---|---:|---:|---:|---:|---:|---:|
| APP_ENTRY | 51 | 267039 | 202308 | 172032 | 6860412 | 14980 |
| RGB_READY | 376 | 229907 | 229907 | 155648 | 6091572 | 14036 |
| DRAW_READY | 381 | 191371 | 191371 | 118784 | 6091572 | 14036 |
| TOUCH_READY | 698 | 178887 | 166119 | 104448 | 6091372 | 12148 |
| WIFI_STARTED | 763 | 149431 | 149431 | 75776 | 6090908 | 12148 |
| DASHBOARD_READY | 1516 | 19767 | 19511 | 11264 | 6090804 | 12148 |
| UI_READY | 2307 | 23055 | 1887 | 7680 | 6090804 | 10580 |

| BOOT point | Scope | Internal free | Minimum | Largest | PSRAM free |
|---|---:|---:|---:|---:|---:|
| before | 0 | 267039 | 202308 | 172032 | 6860412 |
| start | 1 | 267039 | 267039 | 172032 | 6860384 |
| end | 1 | 22311 | 1887 | 7680 | 6090096 |
| after stop | 0 | 22311 | 112 | 7680 | 6090124 |

16 periodic samples: uptime6322..81405ms, internal free22279..22343,
largest7680, historical minimum112, PSRAM free6090124/min6075952/total6865080.
Все16 OTA-stack samples valid: остаток1488bytes при выделенных3072bytes.
Main watermark10580; touch минимум2504, Wi-Fi1104, export1220bytes.
Четыре samples после возврата fecc05d: internal free22743..22807/largest7680/
history112, PSRAM free6090732/min6076588, main10708/touch2504/Wi-Fi1104/
export минимум1164. В обоих capture display/touch/queue/watchdog counters0;
есть Wi-Fi IP event, disconnect0. Render/event samples0 не измеряют FPS/latency.

## Инженерная интерпретация и ограничения

- В этой диагностической сборке history202308 на APP_ENTRY; 112 там не было.
  Гипотеза о появлении малого112 ещё до HMI данным запуском не подтверждена.
  Нельзя переносить этот APP_ENTRY на прежний образ fecc05d без его измерения.
- Локальный BOOT minimum1887 наблюдён к UI_READY после DASHBOARD_READY
  (1516..2307ms). В этом интервале source выполняет export-worker start,
  ready splash refresh, удаление splash и первый dashboard refresh.
  Параллельно исполняются Wi-Fi/RTOS задачи; граница main не приписывает
  расход одному компоненту. Конкретный allocator/call chain и simultaneous
  minimum ещё не локализованы.
- SDK local stop восстанавливает minimum как min(saved, local) для каждого heap.
  Последующая сумма может быть меньше обеих сумм до/внутри окна.
  Поэтому 202308 → local1887 → history112 согласуется с per-heap семантикой;
  это не измеренный simultaneous остаток112 или доказательство OOM.
  Проверены локальные components/heap/heap_caps.c и multi_heap.c:478..485
  SDK6.1.1 commit051e4c4eb20346e043cdd47bc388fde75ff8af63;
  [официальный API](https://docs.espressif.com/projects/esp-idf/en/v6.1.1/esp32s3/api-reference/system/mem_alloc.html).
- Инструментированный образ имеет другой footprint (module432bytes и SDK
  bookkeeping), поэтому разность free с fecc05d не изолирует стоимость одного
  компонента. 112bytes compiler frame модуля не равны полному runtime stack.
  Late worker_stack_at_emit отделён от старого sample_stack в diagnostics.json.
- Наблюдённый малый BOOT minimum1887 требует локализации/исправления transient
  allocations перед configured OTA. Настроенный worker8192bytes и TLS/install
  нагрузка не испытывались: измерялся только unprovisioned worker3072bytes.
  Вычитать5120 из minimum1887 и объявлять OOM также некорректно: это разные
  per-heap/allocator состояния. Production floors/performance не утверждены.

Следующий результат P1 — локализовать startup allocations и проверить реальный
configured budget без уменьшения DMA reserve64KiB/TLS records16384/4096/
framebuffer и без выдуманного ключа/порогов. Owner key/SemVer/time/resource
context отсутствует, CHECK/INSTALL закрыты. Signed install в неактивный слот,
PENDING_VERIFY/confirm/rollback, Wi-Fi/power loss, full recovery, CPU/FPS/
touch-to-display/tearing/soak остаются отдельными испытаниями. Factory self-test
с явным marker не является OTA-slot confirmation. Новые UART/SWD/reset/Flash
требуют нового применимого разрешения; merge/release не выполнялись агентом.
