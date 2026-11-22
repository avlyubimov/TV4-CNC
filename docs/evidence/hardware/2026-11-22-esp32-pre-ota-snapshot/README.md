# ESP32: резервный снимок перед OTA, 22 ноября 2026

По отдельному разрешению владельца выполнены чтение/проверка Flash и reset
текущей Waveshare ESP32-S3-Touch-LCD-7 Rev1.2. Владелец подтвердил изоляцию:
только питание/USB, без CAN и приводов. **Новая прошивка не записывалась.**

[Машиночитаемые результаты](summary.json). Приватные raw dumps, UART logs,
идентификатор платы, NVS/пароли и полные локальные пути не публикуются.

## Наблюдённые результаты

- ESP32-S3/chip ID9 и ожидаемая плата совпали; физическая Flash16МиБ,
  security flags0, Secure Boot/Flash Encryption отключены, crypt count0.
  Это чтение состояния; eFuse не изменялись.
- Один полный снимок **16777216bytes**, read time384,3s по esptool5.6.0,
  460800baud. Штатный RAM stub сверил MD5 при передаче; отдельный device MD5
  всей Flash совпал с полученными bytes до запуска приложения. Второго полного
  чтения не было; MD5 здесь проверяет соответствие данных, не подлинность firmware.
- SHA-256 snapshot: `47c4f9c4e6d7fb1fcfce2b0065c2f69af0ea01664d1407b3504ee6e4a6a13107`.
  Обе локальные копии заново хешированы; разные filesystems и физические NVMe
  подтверждены через findmnt/lsblk. Raw files0600, новые folders0700, существующие
  backups не перезаписаны. После reset выполнялись только проверки локальных файлов.
- Partition table MD5, app checksum/appended SHA, project/chip/SDK/границы образа
  проверены offline. Действующая карта: NVS0x9000/0x6000, PHY0xf000/0x1000,
  factory0x10000/0x200000, flags0. Она соответствует legacy precondition миграции.
- App descriptor: `tv4_hmi`, `8f0d899`, ESP-IDFv6.1.1, DIO80MHz/16MB;
  BIN1573568bytes, SHA-256 `b2680a2abe0aa44b22875ff6445154b7c7331b91a2b65de4715792082a5365da`.
  Descriptor — метаданные для отображения, не signature/provenance attestation.
- Штатный reset выполнен; владелец ответил: «Да, всё работает как прежде»
  на вопрос о главном экране, касаниях и Wi-Fi.

## Ход сеанса и пределы

Разрешённое окно началось08:19:10UTC (13:19:10Asia/Almaty), результат уже получен
к08:28:21UTC (13:28:21), раньше12-минутного предела. Первая попытка остановилась
до полного чтения: host script пропустил штатный ROM `attach_flash` перед JEDEC ID.
Сработал отказ по неизвестному размеру и reset текущего ПО. Инициализация SPI
исправлена по установленным исходникам esptool; fake-SDK проверка теперь требует
attach до ID. Повторное подключение и чтение остались в исходном окне/объёме
разрешения. Сырые журналы обеих попыток сохранены приватно.

Это не hardware failure Flash и не изменение критерия16МиБ. Изменялись только
volatile ROM/stub состояния; Flash program/erase, eFuse, STM32, CAN и motion
команд не было. Открытие/закрытие UART/RTS могло вызвать дополнительные reset;
точное число физических импульсов не измерялось.

## Следующий этап

Подготовлен exact clean-main `fecc05d676a0b1e3376db5317fa241274e8e310a`,
`rev12-ota`/ESP-IDF6.1.1, unprovisioned; build/independent bundle verification PASS.
App1597984bytes, SHA-256 `de55953fc9e3c4080cb277d734ca7d1f02c74880db38dac0f4b53830f587d021`.
Package plan SHA-256 `897da89ab7a8fd3384ef0a26735c166de9a839e3c4d2cefd6cb0d467bd5bdd2b`.
Эти значения относятся к **будущему** образу, не к считанной/работающей firmware.

Перед USB migration нужно отдельное exact-bundle разрешение владельца. Будущая
смена bootloader/table/factory/otadata не защищается app rollback: при отказе
нужен BOOT/RESET USB recovery и проверенный полный snapshot. Restore этого
snapshot на ESP32 пока не испытывался; factory backup сохранён отдельно.
PSRAM XiP boot, HMI/GT911/Wi-Fi под новым profile, configured signed OTA,
rollback/power loss и runtime RAM/stack/FPS остаются неподтверждёнными.
Отсутствующий owner key/context сохраняет сетевой installer закрытым.

[Процедура](../../../ESP32_OTA_INSTALLATION.md), [резервные копии](../../../BACKUP_STATUS.md).
