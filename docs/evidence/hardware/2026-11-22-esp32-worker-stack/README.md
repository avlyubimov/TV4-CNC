# ESP32: контроль3072 и три запуска OTA-worker8192

22 ноября2026,15:22:15.225176–15:37:44.223029UTC (20:22–20:37UTC+5).
Источник — чистый main `dfa9f281e9fe686d31e956a9bd05c8354ba81363`,
ESP-IDF6.1.1/rev12-ota, memory diagnostics1, unprovisioned.
[Опциональный профиль и команды](../../../ESP32_OTA_WORKER_STACK_BENCH.md).

## Подтверждённый аппаратный результат

Один отдельно разрешённый сеанс завершён с exit0 за928.997833s при лимите1800s.
Чип/защита/Flash16MiB проверены; новый полный snapshot16777216bytes проверен
через device MD5 и SHA-256, сохранён0600/fsync в двух новых папках0700 на
разных физических NVMe. [Сводка](summary.json), [проверки](checks.json).

Выполнены три записи только в factory: контрольное приложение, worker8192,
возврат всего factory2MiB из свежего snapshot. Каждая запись прошла verify,
MD5 сохранности вне factory до boot и проверку bootloader/table/otadata.
Три измерительных запуска8192 использовали один записанный образ, затем только
reset/verify/capture. Захваты90.166/90.142/90.099/90.146/30.043s; ни один не повторялся.
Полный16MiB restore не понадобился. UART закрыт, разрешение израсходовано.

PSRAM8MiB/XiP/SDK memory test, RGB800×480, GT911, UI ready/self-test,
startup7points/BOOTwindow и periodic observations PASS у всех четырёх trace
запусков. Владелец один раз подтвердил экран/касания/Wi-Fi/OTA SETUP на временном
8192 образе и отдельно всё это с сохранённым паролем после возврата fecc05d.
Это не три независимые пользовательские проверки интерфейса.

**Остался fecc05d/factory/tv4-16m-ab-v1**, F407 ee7d179 не открывался.
Factory2MiB SHA-256 `081d99e707f578824cc9058d1d18c4f87700b2e60a09afa907e98a9c33bffd1a`;
app prefix SHA-256 `de55953fc9e3c4080cb277d734ca7d1f02c74880db38dac0f4b53830f587d021`.
Это verify возвращённых bytes до boot; полного readback после последнего boot нет.

## Реальные измерения, bytes

Для сравнения periodic выбраны только `heap_min_scope=0` и
`wifi_ip_events>0`:15 из16 samples каждого trace запуска. Счётчик IP накопительный:
это событие получения адреса, не доказательство непрерывной связи или равной
радионагрузки. BOOT window этим сетевым критерием не квалифицирован.

| Запуск | Requested stack | Worker watermark, free | BOOT local minimum | BOOT end largest | Periodic internal free, IP-event subset | Periodic largest |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Контроль3072 |3072|1568|20815|14336|21795–21799|12288|
|8192 №1|8192|6608|15863|9728|14291–16835|7680|
|8192 №2|8192|6752|15847|9728|16827–16863|7680|
|8192 №3|8192|6784|15767|9728|16795–16839|7680|

[Полные numeric samples](observations.json), [диапазоны](measurements.json),
[критерий сравнения](comparison.json). Свободная PSRAM в8192 запусках6090060bytes,
в контроле6090108. После SDK stop BOOT history7828 у всех8192 против7796 контроля;
в первом8192 periodic history позже7796. После возврата fecc05d history снова112,
internal free22711–22759/largest7680 в трёх IP-event samples. Это суммы per-heap history, не
одновременный глобальный остаток/OOM или production floor.

В первом8192 владелец проверял страницу ПО; нагрузка UI была иной. Между
uptime46418 и51434ms internal free снизился16835→14291, watermark main10516→10068.
Совпадение с UI-проверкой не изолирует конкретный allocator. Конфигурации
отличались ровно worker-stack flag, но runtime нагрузка не была одинаковой.
Три пробы ограниченно показывают повторяемость reset-старта; контроль только один.

## Артефакты и программные проверки

[Точные BIN/ELF/source hashes](artifacts.json), [реальные build inspections](build-inspection.json).
Оба target образа пересобраны локально на этом чистом commit; их app headers/
checksums/appended SHA/ELF digest/Flash16MiB/PSRAM/partition/locks проверены.
Normal/trace-control3072 и configured8192 defaults не менялись.
Контроль app SHA-256 `a1bbb2cd8ef13243ff27cd0d360b9a652214015b3dff6cf51faf05902cdd1aa3`;
8192 app SHA-256 `53763fac6aed56b3280defed4303ee66e808e6734b2fda2df675f2e9d551877a`.

[30 host fault/parser methods](procedure-tests.log) выполнены до устройства:
identity/security/geometry, snapshot/second disk/time bounds, failed writes/
return/recovery/no-repeat и marker/default/authorization guards. FakeSDK/RAM
не измеряют устройство. [4 offline export methods](summary-tests.log) проверяют
strict numeric allowlist, запрет raw/private text и отказ активного/неполного
сеанса. Итоговый отчёт сформирован после закрытия UART. Новые firmware tests
ради docs-only отчёта не объявляются; source main CI15/15 PASS, в том числе
[14 SDK variants](https://github.com/avlyubimov/TV4-CNC/actions/runs/45035114740).
[Предыдущая software приёмка PR #67](../../../evidence/build/esp32-ota-worker-stack-bench/README.md)
с108 host/sanitizer suites относится к своему зафиксированному source.

## Бэкап и границы

Свежий snapshot SHA-256 `ce6af4e88d755254f65ca2cf952d17ad9892a8ec7f487b0ed4391eccc3355e5a`.
Он совпал с прежним checkpoint, но здесь считан заново в новые файлы и проверен
с устройством. Старые backup files не перезаписывались. [Factory hash из обеих
новых копий](factory-source-sha256.json). NVS не декодировалась и не сравнивалась
побайтно после всех boot: штатный Wi-Fi может её обновлять. Raw UART, snapshots,
SSID/IP/MAC/пароли и authorization receipts остались приватными.

Ключ/context отсутствуют: CHECK/INSTALL закрыты, TLS/crypto/session/install здесь
не выполнялись. Стек8192 действительно создаётся на этом аппаратном профиле,
но full call-stack peak, configured runtime RAM, fragmentation/CPU/FPS/latency/
tearing/soak/cold power и production floors не квалифицированы. USB app writes
в загрузчике не проверяют Flash-запись installer при работающем RGB. Factory
self-test не подтверждает OTA-slot confirmation/rollback. Wi-Fi/power-loss/full
recovery не испытывались. eFuse/STM32/option bytes/CAN/motion не изменялись.

[Hardware status](../../../HARDWARE_STATUS.md), [backup status](../../../BACKUP_STATUS.md),
[следующий этап](../../../NEXT_STAGE.md). Любой новый hardware сеанс требует
нового конкретного разрешения; этот пакет нельзя запускать повторно.
