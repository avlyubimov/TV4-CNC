# Резервные копии ESP32 — заводская и перед миграцией OTA

## Текущий checkpoint после reset/rollback, 24 ноября 2026

[Стенд](evidence/hardware/2026-11-24-esp32-reset-rollback/README.md) PASS:
сброс до подтверждения → ABORTED → OTA0; обе записи отклонены → factory;
затем восстановлен **bench.4, OTA1 VALID**. Три записи только метаданных.
Приложения, factory, загрузчик и таблица не переписывались.

Итоговые 16 777 216 байт действительно считаны с устройства. MD5 на устройстве,
сравнение всех байтов вне рабочего NVS, SHA-256 и две копии на разных физических
NVMe с правами 0600/0700 и fsync PASS. Предварительный снимок был целиком
проверен по Flash со свежим NVS; это не новое полное чтение перед стендом.
Прежние снимки сохранены.

Метаданные снова seq1 VALID2 OTA0 и seq2 VALID2 OTA1. Использовать актуальный
checkpoint и повторную проверку NVS/всего устройства; не запускать закрытые
одноразовые скрипты. Хеши приватных снимков, дампы, NVS и UART не публикуются.
Владелец подтвердил конечный интерфейс. Ниже история предыдущих checkpoints.


## Текущий checkpoint: bench.4 OTA1 VALID, 24 ноября2026

[Аппаратный стенд](evidence/hardware/2026-11-24-esp32-display-bench4/README.md) закрыт
PASS; на ESP32 оставлен новый bench.4. Перед записью предыдущий полный snapshot
16MiB обновлён свежим NVS24KiB и **целиком** повторно сверён с текущим device MD5,
затем сохранён в двух новых fsynced копиях на разных физических NVMe. Это сборка/
полная сверка снимка, не новое чтение16MiB по UART.

После подписанной сетевой установки действительно считаны все16777216bytes
текущей Flash; device MD5, SHA-256 обеих одинаковых новых копий, native metadata,
apps/padding и protected regions PASS. Files0600/folders0700, private hashes/dumps/
NVS не публикуются. Factory fecc05d recovery и прежние vendor/pre-test snapshots
сохранены; успешный новый release не возвращался. Владелец подтвердил конечные
экран/касания/сохранённый Wi-Fi. Первый metadata-check отказ отдельно завершён
проверенным восстановлением, full16MiB recovery не потребовался ни в одном из
двух сеансов.

**Нынешняя boot selection не erased:** OTA0 receiving `.3` VALID и OTA1 release
`.4` VALID, seq1/seq2. Следующие finite пакеты должны учитывать этот checkpoint;
старый validator «factory sole/оба slots FF» и старые one-use процедуры не
переиспользовать. Обычного rollback к fecc05d после PASS больше нет. Ниже история
соответствующих дат; заводской16MiB backup с ранее проверенной SHA-256 сохранён.

## Fresh fecc05d checkpoint перед3927063,22 ноября,23:25–23:37

Новое полное чтение16777216bytes/device MD5/SHA-256 и две новые приватные копии
на разных NVMe PASS (0600/0700/fsync); старые snapshots/заводской образ сохранены.
SHA снова `ce6af4e88d755254f65ca2cf952d17ad9892a8ec7f487b0ed4391eccc3355e5a`.
Обе host-копии повторно сверены после стенда. Весь factory2MiB fecc05d возвращён
из нового снимка, SHA `081d99e707f578824cc9058d1d18c4f87700b2e60a09afa907e98a9c33bffd1a`,
verify/MD5 вне factory до boot/UART/owner HMI-touch-saved-Wi-Fi-SETUP PASS.
Полный16MiB restore не понадобился; NVS не декодировалась, нового полного
readback после возврата нет. [Отчёт](evidence/hardware/2026-11-22-esp32-worker-tls-3927063/README.md).
Сеанс743,183s закрыт после HEAD/AES allocation FAIL, разрешение использовано.
Ниже сохранена история предыдущих checkpoints.

## Fresh fecc05d checkpoint перед a5f8b7e,22 ноября,22:10–22:20

Снова считаны16777216bytes, device MD5/SHA-256/two independent NVMe copies/
0600/0700/fsync PASS. SHA-256 снова
`ce6af4e88d755254f65ca2cf952d17ad9892a8ec7f487b0ed4391eccc3355e5a`;
это новое чтение/новые файлы, старые копии не перезаписывались. После failed
control touch_fresh весь factory2MiB fecc05d возвращён из этого fresh snapshot,
verify/MD5 вне factory до boot и owner HMI/touch/saved Wi-Fi/SETUP PASS.
TLS image и full16MiB restore не записывались. NVS не декодировалась и не
сравнивалась между всеми boot; snapshot pre-test, не full readback после возврата.
[Отчёт](evidence/hardware/2026-11-22-esp32-worker-tls-control/README.md).
Сеанс608.667s и разрешение закрыты; далее исторические checkpoints.

## Новый fecc05d checkpoint перед worker8192,22 ноября,20:22–20:37

Свежие16777216bytes снова считаны с устройства, device MD5/SHA-256 и две
новые независимые NVMe-копии0600/папки0700/fsync PASS. SHA-256 снова
`ce6af4e88d755254f65ca2cf952d17ad9892a8ec7f487b0ed4391eccc3355e5a`:
совпадение с прошлым checkpoint не означает повторное использование старого файла.
Factory2MiB возвращён из этого нового snapshot, verify/MD5 вне factory до boot
и owner HMI/касания/saved Wi-Fi/SETUP PASS; полный16MiB restore не потребовался.
Factory SHA-256 `081d99e707f578824cc9058d1d18c4f87700b2e60a09afa907e98a9c33bffd1a`.
NVS не декодировалась/после всех boot побайтно не сравнивалась. Это pre-test
checkpoint, не новый полный readback после возврата. Старые копии сохранены.
[Отчёт](evidence/hardware/2026-11-22-esp32-worker-stack/README.md). Сеанс928.998s
и разрешение закрыты; ниже история прежних snapshots.

## Fresh fecc05d checkpoint перед b5f4b38, 22 ноября, 18:11–18:21

16777216bytes, device MD5 и SHA-256 двух одинаковых private копий на разных
физических NVMe PASS; files0600/folders0700/fsync. Snapshot SHA-256:
`ce6af4e88d755254f65ca2cf952d17ad9892a8ec7f487b0ed4391eccc3355e5a`.
Старые d84fe1…/legacy8f0d899/vendor08ede6… сохранены, не перезаписаны.
Из свежего снимка возвращены все factory2MiB fecc05d с verify/MD5 preservation,
reset/30.048s UART PASS. Factory SHA-256 `081d99e707f578824cc9058d1d18c4f87700b2e60a09afa907e98a9c33bffd1a`.
Владелец подтвердил экран/касания/Wi-Fi/OTA SETUP на b5f4b38 и после возврата fecc05d.
Full16MiB/vendor recovery не выполнялись; NVS после boot побайтно не сравнивалась.
Checkpoint до диагностики, не full read после возврата. [Отчёт](evidence/hardware/2026-11-22-esp32-startup-order/README.md).
Сеанс609.074s закрыт; новых device permissions нет. Разделы ниже — история.

## Снимок fecc05d перед фазным стендом, 22 ноября 2026

Полный16777216bytes snapshot текущего fecc05d/tv4-16m-ab-v1 считан по отдельному
разрешению: device MD5, SHA-256 обеих одинаковых private копий и разные
физические NVMe PASS; files0600/folders0700, fsync. [Стенд](evidence/hardware/2026-11-22-esp32-ota-memory/README.md).

SHA-256: d84fe13900806d9646e1e2edb8df0c4fe897aa6de85f14c28bfd0624f101ff46.

Из этого снимка после диагностики возвращён весь factory2MiB с exact app
fecc05d prefix de55953fc9e3c4080cb277d734ca7d1f02c74880db38dac0f4b53830f587d021.
SDK verify/MD5 сохранности вне factory до boot и owner HMI/touch/saved Wi-Fi/
OTA SETUP PASS. Проверено восстановление factory-раздела HMI; полный16MiB
restore и восстановление vendor firmware этим не испытывались.
NVS после app boot побайтно не сравнивалась. Это checkpoint перед диагностикой,
не новый полный read после возврата. Сеанс закрыт; новые device operations
требуют разрешения. Raw dumps/NVS/private UART не публикуются.
Legacy8f0d899 и original factory backups ниже сохранены и не перезаписывались.
Следующий раздел — история до этого fresh snapshot.

## После миграции, 22 ноября 2026

На плате теперь fecc05d/rev12-ota factory, карта tv4-16m-ab-v1. [Миграция и первый запуск](evidence/hardware/2026-11-22-esp32-ota-migration/README.md)
прошли по отдельному разрешению; NVS/PHY до app boot сохранены. Условный restore
не потребовался и не выполнялся. Две private NVMe-копии прежнего8f0d899 ниже
снова проверены до записи и сохранены как recovery checkpoint; original factory
backup сохранён отдельно. Новый полный snapshot после миграции не считывался.
Restore ESP32 остаётся не испытанным; завершённое разрешение не разрешает
новую запись/чтение/reset. Дампы и NVS не публикуются.

## Снимок 8f0d899 перед миграцией, 22 ноября 2026

После отдельного разрешения считана вся Flash ESP32: **16777216bytes**.
Один полный read, штатный transfer MD5 и отдельный device MD5 whole Flash,
затем SHA-256 обеих локальных копий — PASS. Копии находятся на двух физически
разных NVMe, files0600/folders0700; старые backup files не перезаписаны.

```text
47c4f9c4e6d7fb1fcfce2b0065c2f69af0ea01664d1407b3504ee6e4a6a13107
```

[Отчёт чтения](evidence/hardware/2026-11-22-esp32-pre-ota-snapshot/README.md) относится
к прежнему8f0d899 и карте factory2MiB **до** последующей миграции fecc05d.
Partition MD5 и app checksum/appended SHA проверены; эта legacy карта совпала
с precondition миграции. NVS не декодировалась. После read-only reset владелец
подтвердил экран/касания/Wi-Fi. В том сеансе Flash не записывалась; позднейшая
запись отражена выше. Дампы/сырые UART логи приватны и не входят в Git/CI/PR.

## Заводская копия — сохранена


Пользователь подтвердил успешное считывание всей Flash платы: **16 МиБ**.
На диске найден локальный `backups/waveshare_factory_16mb.bin` размером
**16 777 216 байт**. SHA-256 заново вычислен по файлу и совпал с сохранённым
`waveshare_factory_16mb.bin.sha256`:

```text
08ede6fb7755971377fac7a3438ef82fa0ffd54c08e7efd58a599aa53a728882
```

Это подтверждение размера и целостности сохранённого образа. В рамках проверки
SDK 6.1.1 устройство не подключалось и повторно не считывалось. По сообщению
пользователя исходное считывание и проверка SHA-256 уже успешно выполнены.
Восстановление из бэкапа не испытывалось. Позднее владелец предоставил
журнал записи HMI и фото физического изображения/списка Wi-Fi; это не тест
восстановления, полного покрытия сенсора или производительности.
Актуальный статус: [HARDWARE_STATUS.md](HARDWARE_STATUS.md).

Образ и checksum-файл остаются локально в игнорируемой папке backups/.
Ни образ, ни его содержимое не включены в Git/PR или CI-артефакты.
Конфигурация новых сборок — Flash 16 МБ; это не меняет образ бэкапа и не
выполняет операций записи на устройстве.
