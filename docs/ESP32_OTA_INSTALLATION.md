# ESP32 OTA: интеграция установки и USB-пакет

## 22 ноября: worker8192 reset-стенд завершён

База main `dfa9f28`, PR #1–67 слиты. [Стенд3072/8192](evidence/hardware/2026-11-22-esp32-worker-stack/README.md)
завершён22ноября за928.998s по отдельному разрешению: fresh16MiB/device MD5/
SHA-256/two-NVMe, контроль3072 и три reset-запуска8192, три app/factory writes,
пять UART captures и возврат всего factory2MiB fecc05d/verify PASS.
Владелец подтвердил HMI/касания/Wi-Fi/OTA SETUP временного8192 и возвращённого
fecc05d. На ESP32 fecc05d/factory/tv4-16m-ab-v1; F407 ee7d179 не открывался.
Полный16MiB restore не понадобился; сеанс и разрешение закрыты.

Worker8192 watermark6608–6784bytes, BOOT local15767–15863; periodic internal
free14291–16863/largest7680 против control21795–21799/largest12288 после IP event.
Первый8192 включал UI-проверку: нагрузка различалась. Это unprovisioned reset
boot evidence, не configured TLS/install/production floor или cold-power/soak.
Ключ/context не заданы, CHECK/INSTALL закрыты. FPS/latency/slot confirm/rollback/
power-loss/full recovery не приняты. Новых hardware разрешений нет.

Ниже сохранены результаты предыдущих завершённых сеансов.

## 22 ноября: BOOT memory стенд и возврат текущего factory

[fc90200](evidence/hardware/2026-11-22-esp32-ota-memory/README.md) испытан app-only на действующей A/B карте, затем весь factory2MiB
fecc05d возвращён из свежего проверенного16MiB snapshot; две NVMe-копии/verify/
owner HMI/touch/saved Wi-Fi/SETUP PASS. Bootloader/table/otadata не мигрировались.
Factory-partition restore проверен; full16MiB restore не выполнялся.
APP_ENTRY history202308, BOOT local1887, после stop history112; worker1488из3072.
Конкретный startup transient и configured8192/TLS/install budget не определены,
production floors не назначены. Factory self-test явно PASS, OTA-slot
confirmation/rollback не испытывались. Ключ/context отсутствуют, установка
по-прежнему закрыта. Сеанс608.700s завершён, новых hardware разрешений нет.
Остальные датированные статусы ниже — история до этого измерения.

## Аппаратный статус после первой миграции

22 ноября2026 отдельно разрешённый пакет fecc05d/rev12-ota/SDK6.1.1 записан и
проверен: bootloader/table/factory/otadata, NVS/PHY до boot сохранены. Действующая
карта на плате — tv4-16m-ab-v1, загрузился factory fecc05d. PSRAM8MiB/XiP/RGB/
GT911/UI подтверждены UART, владелец подтвердил экран/касания/saved Wi-Fi и
SETUP с недоступной установкой. [Результаты, память и пределы](evidence/hardware/2026-11-22-esp32-ota-migration/README.md).
Условный restore не выполнялся. Сеанс закрыт; следующие операции требуют нового
разрешения. Обычный rev12 flash не является безопасным возвратом к старой карте.

Configured install/slot confirmation/rollback/Wi-Fi loss/power loss/recovery
не проверены. Ключ/context отсутствуют по решению владельца. 16 periodic samples
дали SDK internal historical minimum112bytes, current free22851..22907 и
largest7680: нужен разбор фазного бюджета, не готовые floors. Historical сумма
per-heap minima не является одновременным замером остатка/OOM. OTA-worker stack
не измерен; render/event samples0 не подтверждают FPS/latency/soak.

## Исходный программный результат PR #59

P1 от main `d1317a89f6a29cf4ba950092a00ba00c78f80f1b` (PR #1–58 слиты).
Измеримый результат: настоящий `rev12-ota` ELF/BIN, installer на `esp_ota_ops`,
boot self-test/rollback, экран обновления и независимо проверяемый локальный
USB-пакет. Это программный результат. Устройства в этом этапе не открывались;
ESP32 по-прежнему ordinary8f0d899, F407 — ee7d179.

Владелец выбрал подготовку **без ключа**. В сборке нет проверочного ключа,
выдуманных production SemVer или порогов памяти. HMI загружается; экран
«Параметры → ПО и обновления» сообщает о необходимости настройки и отключает
проверку/установку. Host-тесты используют временные ключи только в своих каталогах.
Это ещё не подтверждённое на плате OTA и не публикация релиза P2.

## Профили и карта 16 МиБ

Обычные headless/rev12/memory-bench/tls-bench сохраняют `partitions.csv`:
factory 2 МиБ и прежние NVS/PHY, внутренний TLS allocator кроме tls-bench.
Только **rev12-ota** включает [отдельную карту](../firmware/esp32-hmi/ota/partitions.ab.csv)
и rollback. Исторический `partitions.ab.candidate.csv` не подключён к sdkconfig.

| Раздел | Адрес | Размер |
|---|---|---|
| nvs | 0x9000 | 0x6000 |
| phy_init | 0xf000 | 0x1000 |
| factory recovery | 0x10000 | 0x200000 |
| otadata | 0x210000 | 0x2000 |
| ota_0 | 0x220000 | 0x300000 |
| ota_1 | 0x520000 | 0x300000 |
| resources | 0x820000 | 0x300000 |
| store | 0xb20000 | 0x4e0000 |

Factory сохраняется отдельно и не обновляется app OTA. Пустая штатная otadata
загружает factory, первая установка выбирает ota_0, последующие чередуют слоты.
Проверяется точная карта, размер Flash 16 МиБ, отсутствие encrypted-разделов,
boot==running и состояние VALID текущего OTA-образа. Пока установленный образ
не перезапущен/не подтверждён, вторая установка запрещена. Ресурсы/store пока
не используются; их наличие не означает поддержку microSD или новой схемы NVS.

SDK bootloader может вернуться к предыдущему VALID слоту, а при отсутствии
такого — к валидному factory. Это проверено по локальным исходникам6.1.1 и [официальным исходникам5.3.2](https://github.com/espressif/esp-idf/blob/v5.3.2/components/app_update/esp_ota_ops.c#L633)
`esp_ota_check_rollback_is_possible` и требует отдельного
power-loss/rollback испытания на устройстве. Factory — восстановительный HMI,
не неизменяемый аппаратный root of trust; без Secure Boot физическая замена
Flash/ключа этим механизмом не предотвращается.

## Установка и первый запуск

Существующие HTTPS, signed manifest, P-256, SHA-256 и stream verifier сохранены.
Новой криптографии или HTTP-стека нет. Один worker владеет сетью/Flash;
LVGL callback отправляет неблокирующее сообщение в очередь длиной 1, snapshot
копируется под коротким critical section. Запросы и окна времени ограничены.

1. «Проверить» выполняет resource admission, явную SNTP-синхронизацию и HTTPS
   manifest/signature. Проверяются подпись локальным ключом, channel/SemVer,
   hardware/revision/profile/layout/protocol/minimum versions и точный URL релиза.
   Это проверка подписанных метаданных, не чтение полного app и не утверждение
   лицензионного/релизного provenance.
2. Экран показывает версию/размер. «Установить → Подтвердить» выбирает этот
   полный SHA-256; до worker commit доступна отмена. Worker повторно получает
   подписанные метаданные и сравнивает version/release/URL/размер/полный hash
   с локально сохранённым одобрением **до esp_ota_begin**.
3. Штатный SDK стирает/пишет только неактивный слот. Каждый chunk сначала
   проходит существующий verifier; полный SHA, app header/chip revision,
   project/version, checksum, appended digest, размер, HTTP EOF/cleanup,
   время/ресурсы/отмена проверяются до активации. `esp_ota_end` выполняет
   дополнительную SDK-проверку образа. Последняя операция — `set_boot_partition`.
   После успешного выбора boot нет проверки, способной объявить установку
   неуспешной; перезапуск требует отдельного нажатия оператора.
4. При ошибке/потере Wi-Fi/отмене handle закрывается, boot не выбирается,
   частичный слот остаётся неактивным. Resume не реализован: повтор начинается
   заново и снова проверяет подпись и полный hash. Отмена после commit не
   предлагается. При разрыве питания до выбора boot сохраняется прежний слот;
   надёжность записи otadata и rollback проверяются аппаратно отдельно.
5. Worker запускается до board init. За 30 секунд требуются 3 секунды непрерывных
   свежих health-сэмплов UI/GT911 (не старше 500 мс, без пропуска polling окна),
   отсутствие ошибки RGB flush, 256-байтный PSRAM write/read probe и точная карта.
   Проверка не требует AP/DHCP/CAN/microSD или настроенного OTA-ключа.
   Pending image подтверждается штатным SDK только при успехе; иначе SDK rollback
   и reset. Boot worker подписан на task watchdog; idle CPU0/CPU1 также наблюдаются,
   task watchdog panic/reset 5 секунд, interrupt watchdog включён. Это ещё не
   измеренный deadline на плате и не полный тест RAM/дисплея/сенсора.

Ни CAN, ни выходы движения в профиле не реализованы. Это профиль изолированного
HMI-стенда, не разрешение обновлять работающий станок. Fresh machine-quiescent
permission и запрет обновления при движении остаются обязательной интеграцией
перед commissioning/P3–P4. UI STOP не является физической защитой.

## Подготовка без доступа к устройству

Из корня чистой рабочей копии:

```bash
TV4_IDF_PATH="$HOME/.espressif/v6.1.1/esp-idf" ./tools/build_idf.sh rev12-ota 6.1.1 local
python3 tools/prepare_ota_migration.py --build build-idf-6.1.1-rev12-ota --sdk 6.1.1 --output build-migration-review
python3 tools/prepare_ota_migration.py --verify build-migration-review
```

После исправления DMA headroom explicit OTA использует официальный CUSTOM
allocator LVGL9 на SPIRAM|8BIT. Internal fallback отсутствует; глобальная malloc
policy, RGB buffers и обычные профили CLIB прежние. Старый OTA `sdkconfig`
мог сохранить CLIB choice: сначала сохраните этот генерируемый файл вне build
tree, затем удалите только исходный `sdkconfig` и повторите сборку из defaults.
Не удаляйте owner context/key или Flash backups. Несовместимый cached choice
намеренно останавливает компиляцию. Flash map при этом не меняется.

Build/prepare/verify не открывают serial/USB и не запускают esptool. Пакет
готовится только из clean commit, с проверкой версии app/project/source,
app checksum/встроенного SHA, ELF digest, точной карты, rollback/watchdog/security
config и unprovisioned context. Другой SDK, stale ELF/app, опасные адреса,
изменённый состав/размер/hash или повторный output отклоняются. SHA256SUMS
охватывает и plan/README; это целостность локального пакета, не цифровая подпись
или независимая аттестация произвольной внешней сборки.

## Отдельно разрешаемая USB-миграция и восстановление

**Первая миграция fecc05d выполнена**, см. аппаратный статус выше. Ниже сохранена
процедура для отдельно разрешённой миграции/восстановления; она не выполняется
автоматически повторно. Перед любым новым сеансом нужны exact commit/SDK/пакет/
hash, актуальная карта и verified backup, изолированная плата только с питанием/
USB и отдельное разрешение владельца.

1. Проверить security/Flash/действующую карту отдельным согласованным сеансом;
   сохранить свежий полный 16 МиБ snapshot с SHA-256 и вторую проверенную копию
   на другом физическом носителе. Заводской backup не заменяет snapshot текущих
   Wi-Fi/NVS/настроек. Приватные дампы/пароли не публиковать.
2. Выполнить независимый verify пакета. План содержит только четыре записи:
   bootloader 0x0, partition table 0x8000, factory app 0x10000, FF otadata 0x210000.
   Не использовать `erase_flash`, не писать NVS/PHY и не добавлять массовую
   запись OTA/data-слотов. Обычная команда `idf.py flash` не заменяет план:
   в пакете требуется явный сброс otadata. Команда записи здесь намеренно не
   автоматизирована; будет составлена под exact проверенный пакет/порт.
3. Начальная смена bootloader/table/factory **не является безопасным app OTA**:
   обрыв питания может потребовать USB bootloader recovery. Для восстановления
   сохранить штатный BOOT/RESET доступ; при отказе по отдельно согласованному
   сценарию вернуть полный проверенный snapshot, verify и reset. Сам rollback
   не восстанавливает partition table/NVS/потерянный factory.
4. Проверить factory boot, HMI/GT911/Wi-Fi/NVS, PSRAM/stack/heap и только затем
   отдельным разрешённым сценарием — подписанную установку, reboot/confirm,
   timeout/fault rollback, потерю Wi-Fi и питания на границах записи/otadata.
   Заводской backup остаётся приватным; eFuse/Secure Boot/Flash Encryption,
   STM32, CAN и приводы не изменять.

## Будущая локальная настройка доверия

Когда владелец подготовит публичный ключ и решения по версиям/ресурсам,
`TV4_OTA_CONTEXT=/absolute/path/.ota-context.local.json` разрешён только для
явного `rev12-ota local`. В Git/CI/обычные профили контекст не переносится.
Файл принимает ровно следующие поля; все значения требуют отдельного review,
поэтому готового production JSON с выдуманными значениями нет:

| Поле | Требование |
|---|---|
| reviewed | true после review владельца |
| key_id / public_key | ID и путь относительно JSON к PEM PUBLIC KEY P-256, не private key |
| installed_version / bootloader_version / protocol_version | Явный SemVer; installed_version становится app descriptor PROJECT_VER |
| manifest_url / prerelease | Точный HTTPS TV4 GitHub Releases manifest.json и явный выбор канала |
| resources | internal_free, internal_largest, psram_free, psram_largest, stack: положительные измеренные floors в байтах |
| time | allow_unauthenticated_sntp:true, earliest_epoch/latest_epoch, maximum_age_ms, maximum_skew_ms |

SNTP не authenticated: решение доверять этому источнику/окну явно локальное;
никакого insecure TLS fallback. Ключ проверяется как canonical P-256 SPKI,
unknown/duplicate поля, private key, неверная кривая/версия/URL/floors отклоняются.
При изменении локального JSON/PEM нужно снова выполнить `build_idf.sh`
(он явно запускает CMake), а не полагаться на incremental `idf.py build`.
Production signing/discovery/key rotation и выпуск релиза относятся к P2.
Текущий migration tool допускает только проверяемый **unprovisioned** пакет;
будущая provisioned migration/release требует отдельного review source/version
binding и не подменяется этим пакетом.

## Память и пределы программной проверки

Следующая [фазная диагностика](ESP32_OTA_MEMORY_PHASES.md) добавляет OTA-worker
watermark/boot marker и отдельный explicit TV4_OTA_MEMORY_DIAGNOSTICS=1 build.
Normal default off не сбрасывает global minima. Diagnostic app готовят для
отдельного app-only стенда на уже действующей карте, не через повторение USB
migration bundle: production migration inspector намеренно отвергает flag1.
Это не owner key/context, новый resource floor или runtime OTA acceptance.

Session workspace остаётся 34000 байт и размещается только в PSRAM; download
не хранит весь образ. TLS использует существующий SDK external allocator,
records 16384/4096, CA bundle и certificate-date checks. DMA reserve 64 КиБ,
RGB single framebuffer/bounce buffers и драйверы GT911/CH422G не меняются.
Только OTA profile включает SDK `SPIRAM_XIP_FROM_PSRAM` (оба FETCH_INSTRUCTIONS/
RODATA): инструкции/константы копируются в PSRAM, чтобы cache оставался доступен
RGB bounce ISR при Flash writes. Без этого текущий bounce-buffer режим не
совместим с OTA/NVS-записью: [официальное ограничение RGB LCD](https://docs.espressif.com/projects/esp-idf/en/v6.1.1/esp32s3/api-reference/peripherals/lcd/rgb_lcd.html#bounce-buffer-with-single-psram-frame-buffer).
Это требует дополнительной PSRAM под MMU pages кода/rodata и отдельного замера
bandwidth/FPS на плате; clock80MHz, DMA reserve и framebuffer не урезаются.
Ordinary profiles не получают XiP. Target section/page budget указан в evidence;
это не подтверждение отсутствия tearing или снижения latency. В unconfigured
профиле worker stack 3072 байта; настроенный installer требует 8192 байта.
Queue одна, status/approval ограничены, UI создаёт три кнопки только на странице ПО.

[Evidence](evidence/build/esp32-ota-installation/README.md) различает размеры
ELF/секций/индивидуальных compiler frames от реальных peak/low-water измерений.
Обычные профили также собираются и проверяются, но target FPS/tearing/touch
latency/heap/stack под download/write пока **не измерены**. Результат прежнего
HEAD TLS bench не задаёт installer floors. Настроенный installer не допускается
до утверждения этих параметров; неизвестный context fail-closed по решению владельца.

Источник поведения SDK: [ESP-IDF 6.1.1 OTA API](https://docs.espressif.com/projects/esp-idf/en/v6.1.1/esp32s3/api-reference/system/ota.html),
закреплённые SDK/pins в build script и lock-файлах. Это штатная подпись manifest
и SDK rollback, без eFuse anti-rollback. Новых зависимостей и pin updates нет.
