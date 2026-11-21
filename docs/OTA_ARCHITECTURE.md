# OTA TV4-CNC — проект архитектуры

PR-07u от main `bc35a37`: [подтверждённая пауза приёма](STM32_INGRESS_OWNER.md) связывает actual
ingress и channel RX. Смена фазы ждёт exact driver ACK и actual pending/staged/
ring/RX drain; resume выдаёт новую epoch без сброса clocks/counters/IDs.
Только lower fragment timeout восстанавливается после ACK+cleanup; sticky faults
и original upper deadlines сохраняются. Software barrier не peripheral/IRQ/DMA/
remote drain. Target port/identity/resources/runtime/install/hardware gates открыты.

Слитый PR #52, PR-07t от main `25e635c`: [очередь входящих кадров](STM32_FRAME_INGRESS.md) добавляет bounded owned
SPSC capture/stage/fresh-check, сохранение original capture time при full/retry
и sticky clock/expiry/overrun/exhaustion. Host71/sanitizers71/standalone45/TSan6
PASS. ARM1060text/data0/bss0/frame48 отдельно, прежние budgets/SDK pins сохранены.
FRAME не verified; empty не producer-stop/phase-cutover. Реальные target port/
clock/backend/identity/runtime/installer и hardware gates открыты. Устройств не касались.

Слитый PR #51, PR-07s от main `db50840`: [owned driver dispatch](STM32_DRIVER_DISPATCH.md) связывает actual
claimed frame, bounded publication/take/fresh check и correlated return. Untaken/
taken jobs и первый pending report сохраняются при cancel/timeout/full. Host68/
sanitizers68/standalone42/TSan4 PASS. Software revocation не physical inhibit;
target ISR/RTOS/cache/DMA/clock/backend и аппаратные gates открыты. Устройств не касались.

Слитый PR #50, PR-07r от main `2c6fd1c`: [SPSC completion mailbox](STM32_COMPLETION_MAILBOX.md) добавляет bounded
атомарную передачу owned reports от одного producer к одному consumer и actual
bound owner. Host65/sanitizers65/standalone39, ThreadSanitizer2/2 PASS. Это C11
software contract; target ISR/RTOS/cache/DMA/driver truth и аппаратные gates ещё
не подтверждены. Аппаратных операций не выполняли.

Слитый PR #49, PR-07q от main `efa2727`: [очередь событий завершения](STM32_DRIVER_EVENTS.md) связывает actual
egress и owned event FIFO. Full означает backpressure/retry; current-time service
обрабатывает максимум1 event, не продлевает deadlines. Host61/sanitizers61/
standalone35 PASS. Это serialized software API; реальные ISR/RTOS/driver/reset,
identity/runtime/install и аппаратные gates остаются открытыми. Аппаратных операций не выполняли.

Слитый PR #48, PR-07p от main `555007d`: [bound TX egress/driver tickets](STM32_UPDATE_EGRESS.md)
добавляет bounded frame ledger и owned channel TX. Phase advance ждёт actual
correlated local driver returns; timeout/uncertain сохраняют held claim. Host59/
sanitizers59/standalone33 и independent signed chains проходят. Это local
ownership contract, не real driver/remote ACK/auth/physical/install proof.
Hardware facts/blocks прежние; устройств нет, Flash/pins не назначены.

PR-07o от main `389de77`: [shared update channel](STM32_UPDATE_CHANNEL.md)
объединяет TU/MS в одном bounded software link с retained counters, owned frame
queue и local phase/role filters. Host56/sanitizers56/standalone30 и independent
signed chains проходят; original image windows/verifier/lease/model/close прежние.
Local queue empty не remote delivery/flush/auth/install proof. Runtime/driver/
identity/resources и все hardware blockers остаются открытыми; устройств нет.

PR-07n от main `d18cf93`: [metadata setup/shared namespace](STM32_METADATA_SETUP.md)
связывает original CLOSED image negotiation, live empty image endpoints и
metadata DATA. Paired ID выделяется из общего volatile pool, raw MS exchange
проверен с потерями всех фаз, complete metadata endpoints закрывают pool.
Host54/sanitizers54/standalone28, independent verifier/lease/model/close прежние.
Это software local contract; authentication/persistent identity/runtime/installer
и все physical F407 blockers открыты. Устройств не касались.

PR-07m [metadata transport foundation](STM32_METADATA_TRANSPORT.md) добавляет
bounded raw document/signature/key ID container и separate-ID DATA receiver.
TAKEN_UNVERIFIED — owned bytes, не signature/hash/installer permission. Actual
independent verifier/live image/lease/physical/operator/recovery gates прежние.
Authenticated descriptor setup/identity/routing/runtime worker ещё не реализованы;
действующая разметка/bootloader reserve/устройства не меняются.

PR-07l [one-clock timing tests](STM32_UPDATE_TIMING.md) проверяет композицию
existing fragments/negotiation/DATA/outcome/admission с queue delays/loss/retries.
Только HOST fixture, не runtime scheduling/CAN driver или measured time budget.
Production metadata transport/identity/routing/worker/crypto CPU budgets и
physical/operator/recovery/installer остаются отдельными gates.

PR-07k [bounded fragment/reassembly draft](STM32_UPDATE_FRAGMENTS.md) добавляет
opt-in envelope вокруг codec, без production driver/IDs/bitrate/runtime. Local
route/generation не serialized authentication/reboot replay isolation. Future
CAN bootloader/ESP32 OTA ещё требуют signed metadata transport, identity/durable
source и независимых physical/operator/recovery/install gates. Flash addresses/
сектора по предположению не резервируются; действующая разметка не меняется.

PR-07j [complete-message byte draft](STM32_UPDATE_WIRE.md) кодирует existing
logical negotiation/DATA/outcome для host/ARM review. Codec не authentication,
CAN frame/fragmentation/production version или runtime caller. Existing
independent signature/lease/model/deadline/physical/recovery/install gates прежние;
metadata transport, production framing/identity/driver ещё требуют review.

PR-05d: [offline build binding audit](RELEASE_BUILD_BINDING.md) связывает
existing unsigned candidate/full HMI/ELF digest с signed cohort. External pin
фиксирует bytes, не CI/source authentication. Factory/A-B/profile mismatch и
missing Motion build evidence не снимаются; report всегда blocked/exit3.
Production manifest/context/keys/runtime/installer/layout gates остаются открыты.

Статус: целевая архитектура. PR-04a добавляет [verification foundation](OTA_VERIFICATION.md),
но downloader/installer/A-B/rollback ещё не реализованы. PR-01 не меняет partitions.csv,
sdkconfig, bootloader или исполняемый код. PR-04/05/07/08 реализуют и проверяют
этот проект отдельными review. Release source — GitHub Releases, не произвольный
URL из UI. Производственные доверенные ключи ещё не созданы/не установлены.

## Два независимых пути

```text
GitHub Releases -> HTTPS discovery -> signed manifest verification
                                     -> HMI image -> inactive ESP32 OTA slot
                                     -> Motion image -> ESP32 cache/stream
                                                       -> CAN -> F407 bootloader
```

ESP32 OTA и F407 update имеют собственные version/status/history и восстановление.
Ошибка motion update не превращает HMI в генератор STEP. Автопроверка разрешена;
загрузка/установка отражаются явно, установка конкретного образа требует
подтверждения оператора. Нет автоматической установки при обнаружении версии.
Оборудование для движения не включается в результате любого обновления/rollback.

## Доверие, манифест и ограничения

Предлагаемый профиль подписи: ECDSA P-256 / SHA-256 стандартной библиотекой,
без своей криптографии. Окончательный encoding/test vectors проверяются PR-04
на обоих SDK и будущей STM32 библиотеке с известной лицензией. Подписываются
**точные байты UTF-8 manifest.json**, а не JSON после повторной сериализации.
Манифест связывает аппаратную цель, layout/protocol версии, размер и SHA-256
каждого образа. SHA без доверенной подписи не подтверждает автора.

- manifest.json ограничен 16 КиБ; manifest.sig — 2 КиБ (предлагаемые лимиты).
  Оболочка подписи PR-04a содержит key_id/algorithm/signature (base64 DER);
  schema_version=1 находится в подписанном manifest.
  key_id только выбирает из локального allowlist публичных ключей; неизвестный
  key_id/алгоритм отклоняется. Ключ из загруженного манифеста/URL не доверенный.
- Подпись проверяется до применения содержимого manifest. Затем строгая схема:
  duplicate keys, неподдерживаемые обязательные поля/schema, длины/URL,
  неизвестные hardware/layout/protocol ranges → отказ. Никаких default «подходит».
- HTTPS с проверкой цепочки/имени/срока сертификата. Ошибка времени или доверия
  не включает insecure fallback. GitHub API metadata используется только для
  discovery; подлинность задаёт подпись, а не badge/tag/имя файла.
- URL берутся из подписанного manifest и проверяются против allowlist GitHub
  download hosts, утверждённого/проверенного в PR-04 на реальных redirects.
  Только HTTPS, ограничение redirects/timeouts/размера, без HTTP downgrade,
  без слепой пересылки Authorization через redirect на другой host.
- Stable/prerelease выбираются явно. Версии сравниваются как версии, не строки.
  Повтор/понижение по сети запрещается политикой подтверждённых релизов;
  контролируемый rollback на предыдущий одобренный образ остаётся возможным.
  Без Secure Boot/Flash Encryption это защита канала обновления, а не гарантия
  от физической модификации Flash/ключей. eFuse anti-rollback не включается.
- Смена доверенных ключей — отдельный review и переход с действующей подписью;
  нельзя заменить trust anchor неподписанными сетевыми настройками.
- Приватные ключи не в устройстве/репозитории/логах. До provisioning утверждённого
  публичного ключа установка заблокирована. Пример manifest — в RELEASE_PROCESS.

## ESP32: текущая и проектируемая разметка

Сейчас [partitions.csv](../firmware/esp32-hmi/partitions.csv) содержит NVS
0x9000/0x6000, phy_init 0xf000/0x1000 и factory 0x10000/0x200000; **OTA слотов нет**.
Предлагаемая карта 16 МиБ ниже — только таблица в Markdown, не конфигурация
сборки и не инструкция немедленной записи. `layout_id` условно `tv4-16m-ab-v1`.

| Область | Предлагаемые type/subtype | Offset | Size | End exclusive |
|---|---|---:|---:|---:|
| Bootloader / partition table | Существующие служебные области | 0x000000 | до 0x009000 | 0x009000 |
| nvs | data/nvs | 0x009000 | 0x006000 | 0x00f000 |
| phy_init | data/phy | 0x00f000 | 0x001000 | 0x010000 |
| factory recovery HMI | app/factory | 0x010000 | 0x200000 | 0x210000 |
| otadata | data/ota | 0x210000 | 0x002000 | 0x212000 |
| Резерв выравнивания | Не раздел | 0x212000 | 0x00e000 | 0x220000 |
| ota_0 | app/ota_0 | 0x220000 | 0x300000 | 0x520000 |
| ota_1 | app/ota_1 | 0x520000 | 0x300000 | 0x820000 |
| resources (резерв) | data, subtype определить в PR-04 | 0x820000 | 0x300000 | 0xb20000 |
| filesystem (резерв) | data, subtype/FS определить отдельно | 0xb20000 | 0x4e0000 | 0x1000000 |

Factory recovery должен помещаться в 2 МиБ; оба OTA образа — каждый в 3 МиБ.
PR-04 проверит app alignment, erase boundaries, bootloader size, subtype choices,
размеры образов, реальную Flash и парсером обоих SDK подтвердит карту до миграции.
NVS/phy и начальный адрес factory сохраняются, но это не доказательство
безопасной миграции уже записанного устройства. Factory не переписывается
обычным OTA. Это recovery HMI, а не заводской vendor demo.

На первом этапе assets, как сейчас, остаются связанными с app. Резерв ресурсов
не обновляется in-place: будущий формат обязан сохранять данные, совместимые
с предыдущей app, либо иметь versioned/двухпоколенный commit. То же для NVS:
деструктивная schema migration до health confirmation запрещена. FS/microSD
не обязательны для boot HMI или первоначального app OTA.

### Первая миграция — только отдельный USB этап

1. Получить разрешение владельца на конкретную миграцию; остановленный
   изолированный стенд, проверенный полный backup и независимая копия.
2. Считать/проверить действующие Flash/security/layout и сохранить текущие
   данные/NVS отдельно от Git (они могут содержать пароли).
3. Подготовить согласованный комплект bootloader + новая таблица + recovery
   HMI с OTA и доверенным публичным ключом. Сверить hashes/offsets/объём.
4. Отдельной проверенной процедурой записать по USB, проверить readback,
   загрузку recovery и отработку первичной OTA data. Прерывание во время
   смены таблицы может потребовать USB восстановления; это не A/B app OTA.
5. Испытать оба слота, rollback и восстановление backup прежде чем считать
   migration завершённой. Не размещать этот процесс в обычной OTA кнопке.

### Загрузка и активация ESP32

Статусы: IDLE → CHECKING → AVAILABLE → DOWNLOADING → VERIFIED →
WAIT_OPERATOR → INSTALLING/BOOT_PENDING → SELF_TEST → VALID либо ROLLED_BACK/ERROR.
Это проект состояния updater, не существующая motion state machine.

Планируется advanced esp_https_ota begin/perform/finish/abort и esp_ota_ops.
Точное число полученных raw .bin bytes и потоковый SHA-256 сверяются с подписанным
manifest **до вызова finish/переключения boot partition**. Проверяется app header,
chip/revision, длина, project identity, layout, версии protocol/minimum firmware.
Не использовать простой one-shot API, который активирует образ до внешней
проверки SHA. В локальном SDK 6.1.1 проверено, что успешный esp_https_ota_finish
вызывает esp_ota_end и выбирает boot partition; PR-04 должен учесть эту границу.
Контроль SHA реализовать проверенным streaming hook либо собственным ограниченным
HTTP read + esp_ota_begin/write/end; он не должен считать redirects/error bodies
частью образа или активировать слот до финальной валидации. Интеграция обоих API
и отрицательные тесты — приёмка PR-04, не выполненная реализация PR-01.

Записывается только неактивный слот. Ошибка сети/подписи/хеша/длины вызывает abort,
не переключает boot. Прерванная загрузка не делает частичный образ bootable.
Resume допускается только после проверки сохранённой идентичности образа и
повторного полного SHA; иначе безопасно начать заново. Отмена доступна до commit;
во время изменения boot metadata UI не предлагает небезопасную отмену.

Rollback support включается в будущем bootloader/app конфиге PR-04, не в PR-01.
Новый OTA образ при pending verify подтверждается esp_ota_mark_app_valid_cancel_rollback
после ограниченного self-test: PSRAM/runtime allocation, RGB/GT911, LVGL owner,
обслуживание watchdog и версия данных. Аппаратные ошибки → invalid/rollback;
сбой до подтверждения обрабатывается bootloader. Недоступные Wi-Fi/AP/CAN/microSD
не считаются обязательной неисправностью HMI. Watchdog/self-test deadline должны
привести к reset/rollback при зависании, а не бесконечному ожиданию в новом слоте.
Новый confirmed version сохраняется после health success; миграции/политика
версий не должны запрещать предусмотренный rollback предыдущей app.
Factory fallback — отдельный recovery, не утверждение, что factory имеет A/B rollback.

## STM32F407: CAN bootloader

После mainb19a40b [владелец подтвердил](evidence/hardware/stm32f407-owner-swd/README.md)
SWD/ST-Link V2, Cortex-M4r0p1, raw ID0x101f6413, Flash1024KiB,3,252V и backup
1048576bytes/vector structure PASS. Размер Flash больше не UNKNOWN; package,
RAM/CCM, HSE, erase geometry/protection, physical GPIO/CAN и restore неизвестны.
Этот результат не выбирает bootloader sectors, metadata address, app base или A/B.
[Synthetic startup/link probe](STM32_STARTUP.md) не является прошивкой, Flash
layout или проверкой jump/commit; адреса fixtures никогда не переносить в installer.

До реального Flash driver нужны verified part/RAM/erase geometry/protection,
vector placement, clock/reset/recovery и решение по памяти. HSE не обязателен
для reviewed HSI-only пути, но частоты и пригодность CAN нужно подтвердить.
**В PR-01 адресов/размеров F407 не выбирали**; owner1024KiB выше — новый факт,
не готовая sector/slot map.
A/B выбирать только если оба приложения и metadata помещаются в подтверждённые
сектора. Иначе использовать staged образ и power-loss-safe recovery/commit;
bootloader и его transport/crypto не зависят от motion app или наличия сети ESP32.
Образ можно кэшировать на ESP32/microSD, но загрузчик должен самостоятельно
оставаться в recovery и принимать повторную передачу после отказа ESP32/CAN.

Перед update motion отвечает fresh session-bound подтверждением: нет движения,
оборотов шпинделя и выполняемой программы, выходы в утверждённом безопасном
состоянии, аппаратные interlocks соблюдены. HMI не выводит это из SIMULATION,
старого heartbeat или одного ACK. До согласованного подтверждения install запрещён.
Дальнейшая потеря связи/сброс не возобновляют движение; новое подтверждение
сеанса необходимо после reboot. Аппаратная E-stop/STO цепь независима от updater.

Логические сообщения (не распределение CAN ID/битов): capabilities/version,
BEGIN(image identity/hardware/protocol/length/hash/signature), DATA(session,
sequence, offset, length, integrity), ACK/NACK, VERIFY, COMMIT, STATUS и ABORT
там, где он безопасен. Classic CAN fragmentation, размер блоков, bitrate/ID,
CRC и retry windows выбираются отдельно. Reassembly/timeouts/retry ограничены;
дублирование блока идемпотентно, неизвестные offsets/sequences отклоняются.

Сам bootloader проверяет доверенную подпись, hardware/protocol compatibility,
полную длину и SHA-256 перед commit. После сбоя нельзя доверять одному valid flag:
полнота, metadata, hash и signature проверяются снова перед jump. SP/PC/Thumb и
диапазоны vector table проверяются относительно **подтверждённой** карты MCU.
При отказе/частичном образе — recovery, без запуска приложения/приводов.
Первичная установка — SWD/ST-Link с отдельным разрешением и бэкапом.

## Единый UI и проверки

Software PR-07b: [независимый verifier кандидата STM32](STM32_IMAGE_ADMISSION.md)
с pinned Mbed TLS3.6.7, локальным allowlist, raw-byte signature и bounded
image SHA/EOF. Он использует явно экспериментальную metadata schema;
это не изменение действующего HMI release manifest или CAN wire protocol.
Нет production context/keys, writer/commit/vector jump или runtime caller.
Receipt относится к проверенному snapshot и не является install permission;
сохранённый образ перед commit/jump потребуется проверять снова.

PR-07c [source/session handoff](STM32_UPDATE_HANDOFF.md) удерживает immutable
lease после verify, сверяет descriptor с signed length/SHA и повторно проверяет
boot/transfer/policy/source при выдаче receipt. RX/local deadline остаются
конечными; release failure сохраняет ресурс до bounded retry/quarantine. Нет
производственного storage/Flash writer, install permission или новых адресов.

PR-07f [logical negotiation](STM32_UPDATE_NEGOTIATION.md) даёт bounded memory-model
REQUEST/OFFER/ACCEPT/CONFIRM и one-time transport setup. Pool counters не заменяют
reviewed boot identity/persistence/authentication provider. Independent signature/
SHA/model/physical/operator/recovery gates сохраняются; production wire нет.

PR-08a [local firmware info UI](FIRMWARE_INFO_UI.md) показывает только текущие
build/SDK/profile и unknown STM32/CAN. Runtime discovery/install ещё не настроены;
SDK descriptor не подменяет trusted version/key context. Целевой полный UI ниже:

Параметры → Обновления: installed HMI/Motion/protocol, доступный релиз/канал,
changelog, hardware compatibility, размер, стадии download/install/verification,
результат/ошибки и история. Неизвестная версия отображается как «нет данных»,
не как фиктивная v0.1.0. «Проверить обновления», «Скачать», «Установить»,
«Отмена» отражают разрешённые состояния; UI не пишет Flash и не ждёт CAN/HTTP.

Обязательные будущие тесты: неверная подпись/key/schema, replay/downgrade,
wrong hardware/layout/protocol, oversized/truncated/corrupt image, TLS/redirect
failure, отсутствие SD/сети, power cut на каждом commit, ESP32 boot failure,
CAN loss/reboot/duplicate/reorder, F407 partial metadata/vector/hash и восстановление.
Подтвердить измерениями отзывчивость UI, heap/PSRAM/stacks и независимое safe state.

Источники: [ESP-IDF OTA 5.3.2](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/api-reference/system/ota.html),
[ESP HTTPS OTA](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-reference/system/esp_https_ota.html),
[Partition tables](https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/api-guides/partition-tables.html).
Для 6.1.1 дополнительно просмотрен установленный components/esp_https_ota/src/esp_https_ota.c;
web stable на момент проверки отображал 6.0.2, поэтому он не выдаётся за проверку 6.1.1.

Native signed manifest parsing/policy: [OTA_MANIFEST_POLICY.md](OTA_MANIFEST_POLICY.md).
Это проверка кандидата, не активация A/B или завершённый OTA Manager.

PR-04c: [потоковая проверка образа](OTA_IMAGE_STREAM.md), без storage/installer
или изменения boot partition. SDK validation/health rollback ещё нужны.

## PR-04d — signature envelope / HTTPS

Добавлены bounded конверт подписи и read-only HTTPS transport primitives.
Не подключены к app_main/UI, нет discovery/OTA Manager/installer/Flash writes.
[Контракт, проверки и ограничения](OTA_HTTPS.md).

## PR-04e — read-only session

Единая worker-only операция связывает metadata/envelope/policy/image checks,
общий timeout/cancel, cleanup и terminal result. Нет runtime task/UI/install.
[Контракт, evidence и board gate](OTA_SESSION.md).

## PR-04f — PSRAM-only session workspace

Добавлены bounded allocation/lifetime, explicit reviewed headroom policy и
ELF target sizeof check. Нет internal fallback, runtime calls или installer.
[Контракт, evidence и оставшиеся ограничения](OTA_MEMORY.md).

## PR-04g — explicit memory bench profile

Добавлен rev12-memory-bench: три bounded allocation/pattern/release cycles в
worker после 15 секунд работы UI. Обычные headless/rev12 отключены от probe.
Нет HTTPS/session_run/Flash/boot operations. [Контракт и board procedure](OTA_MEMORY_BENCH.md).

## PR-04h — time gate для native HTTPS

Свежая local provenance/age/skew anchor проверяется вокруг backend I/O и final
cleanup; boolean hint не даёт native transport разрешения без anchor. Нет
SNTP/clock updates/runtime UI. [Контракт и evidence](TLS_TIME_GATE.md).

## PR-04i — bounded SNTP source

Worker-only lifecycle с deadline/cancel/cleanup и SDK TCP/IP ownership.
Mark SNTP_UNAUTHENTICATED после stop/sample/policy; нет startup/UI/TLS trigger.
[Контракт, evidence и ограничения](SNTP_SOURCE.md).

## PR-04j — отдельный UTC/TLS bench

rev12-tls-bench выполняет one-shot SNTP/verified HEAD и heap-window snapshots
в worker. Ordinary profiles off, без response body/OTA install.
[Policy, scope, evidence и hardware gate](UTC_TLS_BENCH.md).

## PR-07a — host foundation для CAN update transport

[Контракт](STM32_UPDATE_TRANSPORT.md): независимые logical sender/receiver,
stop-and-wait, relative offset/sequence, CRC-32C, bounded retries/deadlines,
двухфазный local consumer/ACK и RAM fault tests. Модуль не подключён к runtime
ESP32/STM32 и не задаёт production wire protocol. Нет Flash map/writer/commit,
signature/SHA verification на MCU, physical safe-state detector или jump.

COMPLETE_UNVERIFIED означает только transport completion. PR #30 добавил независимые
bootloader-local trusted metadata/image checks; свежий quiescent gate нужен до
интеграции writer; ACK доставки и opaque hash не заменяют их. Boot/transfer
uniqueness после reset — будущая negotiation policy, не встроенная аутентификация.
Существующие geometry/recovery/key/context/owner-install gates сохранены;
адреса F407 Flash по-прежнему не резервируются.

## PR-07d — read-only vector/model preflight

[Checker](STM32_IMAGE_PREFLIGHT.md) связывает signed memory_profile с explicit
local model, проверяет stack/handler/vector bounds в том же held image и
отзывает candidate при ошибке/смене context. Это MODEL_MATCH, не проверка
физической памяти/Flash geometry/erase/protection или install/jump permission.
Никакого адреса F407/bootloader/OTA не резервируется.

## PR-07e — historical outcome после close

[Worker completion + bounded report/ACK](STM32_UPDATE_OUTCOME.md) возвращает
результат проверки только после clean lease close; release failures не создают
positive и сохраняют quarantine ownership. Peer message не authentication/
install/ARM authority. Operator wait не держит lease; новая операция требует
fresh negotiation/full saved-byte verification/physical/operator/recovery gates.
Нет actual MCU map/Flash writer/runtime или изменения DATA deadline.

## PR-07g — independent Motion release document

[Signed release binding](STM32_RELEASE_BINDING.md) добавляет отдельный opt-in
15-field Motion draft с release/component SemVer/channel/source и прежними
hardware/profile/counter/protocol/image checks. Один raw signature + полный
image SHA/EOF проверяются существующей независимой Mbed TLS; local unknowns
отклоняются. Legacy11-field draft/API и HMI manifest остаются прежними.

Первоначально PR #37 давал standalone software candidate. PR-07h
[release handoff](STM32_RELEASE_HANDOFF.md) связывает его с live source/RX/owner/
deadline/vector gates и прежним completion/outcome. Production bundle↔component
linkage, local versions/keys/context и
physical/operator/recovery/install gates ещё требуют review. Source commit —
signer claim, не build provenance proof. Никаких actual Flash addresses, wire
codec/IDs/bitrate, board image/keys/release или hardware actions.

## PR-07i — portable volatile RAM source

[Staging source](STM32_STAGING_SOURCE.md) копирует existing pending DATA в
caller buffer до ACK, затем отдаёт immutable lease через API одного owner.
Нет нового wire/crypto/state-machine protocol. Tokens/high-water сохраняются
при discard и не wrap; source нельзя заменить/стереть while leased.

SEALED не signature/install proof. RX lifetime проверяет прежний live coordinator;
RAM source не продлевает дедлайны. После release/discard outcome остаётся history.
Не выбраны real RAM capacity/production staging medium, persistent boot identity,
Flash/recovery/physical/operator gates или installer. Full image buffer — явное
требование, не вывод из Flash1MiB и не новый MEMORY address/default reserve.

## PR-05c — offline release cohort

[Review tool](RELEASE_COHORT_REVIEW.md) выполняет independent signature/policy/
full-image checks двух существующих форматов, затем сравнивает expected source/
release/channel и prospective HMI peer range. Поля берутся из authenticated
snapshots, не перечитываются из заменённого manifest. Motion frontend host-only;
нет нового embedded parser/crypto owner/runtime или изменённого workspace.

Report metadata-only: actual context/layout/provenance/protocol mapping/vector/
live source/physical/operator/recovery/install/publication gates закрыты.
Не добавлять две component записи в production HMI manifest по этому report;
его parser по-прежнему принимает только HMI. Real bundle/schema/keys/versions/
installer ещё требуют отдельного review, factory2MiB и на плате8f0d899 прежние.
