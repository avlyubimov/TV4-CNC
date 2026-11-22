# Аппаратный статус TV4-CNC

Программное продолжение от main973a9ba переносит optional export/Wi-Fi start
после первого полного HMI frame/UI_READY. Это ещё не аппаратная проверка или
подтверждённое улучшение памяти; установленный fecc05d и закрытый scope прежнего
сеанса сохраняются. [План и границы проверки](plans/P1_STARTUP_MEMORY_ORDER.md).

## 22 ноября, 16:44: фазный BOOT стенд, fecc05d возвращён

Отдельный разрешённый fc90200/rev12-ota/SDK6.1.1/diagnostics1 завершён за
608.700s. Fresh16MiB snapshot MD5/SHA-256 и две private NVMe-копии PASS.
SHA-256: d84fe13900806d9646e1e2edb8df0c4fe897aa6de85f14c28bfd0624f101ff46.
App-only write/verify/MD5 вне factory, capture90.103s, возврат всего factory2MiB
fecc05d/verify/capture30.051s PASS; bootloader/table/otadata сохранены.
Owner подтвердил экран/касания/saved Wi-Fi/OTA SETUP. [Полный отчёт](evidence/hardware/2026-11-22-esp32-ota-memory/README.md).

7startup points и BOOT window: APP_ENTRY history202308, local minimum1887
к UI_READY, после stop history112. Конкретная transient allocation не найдена;
SDK per-heap сумма не simultaneous free/OOM или разрешённый floor.
16samples: internal free22279..22343/largest7680, PSRAM6090124,
OTA-worker остаток1488bytes из3072; configured8192/TLS/install не испытывались.
Factory self-test marker PASS; полный16MiB restore/slot confirmation/rollback/
power-loss/FPS/latency/soak не испытаны. Factory-partition restore проверен.
На плате fecc05d/factory/прежняя A/B-карта; F407 ee7d179 не открывался.
Сеанс закрыт, новых устройств/reset/Flash после него нет. История ниже относится
к более ранним этапам, включая прежнее отсутствие post-migration snapshot.

## 22 ноября, 14:05: ESP32 переведён на factory + A/B

Отдельно разрешённая USB migration fecc05d/rev12-ota/ESP-IDF6.1.1 завершена за
117,810s. Четыре записи/MD5/readback PASS; NVS/PHY до app boot побайтно сохранены.
Действующая карта теперь tv4-16m-ab-v1: factory2MiB + два OTA slots3MiB.
Загрузился factory fecc05d; PSRAM8MiB, instructions/rodata XiP, SDK memory test,
RGB/GT911/LVGL ready подтверждены UART. Один capture89,114s, без panic/reboot loop.
Владелец подтвердил экран/касания/saved Wi-Fi и OTA SETUP с недоступными
проверкой/установкой. Ключ/context отсутствуют. [Подробный отчёт](evidence/hardware/2026-11-22-esp32-ota-migration/README.md).

16 periodic samples: internal free22851–22907/largest7680/history112bytes;
PSRAM free6090732bytes. History112 требует разбора по фазам; это SDK сумма
per-heap minima, не simultaneous peak/OOM или production floor. Render/event
samples0 не позволяют судить о FPS/latency; OTA-worker stack не измерен.
Configured install/slot confirm/rollback/network/power loss/recovery/soak ещё
не проверены. Условный restore не выполнялся; обе private16MiB NVMe копии
снимка8f0d899 и original factory backup сохранены. Новый полный snapshot не делался.

На ESP32 оставлен fecc05d factory. F407 ee7d179 не открывался; eFuse/STM32/CAN/
motion не затронуты. Сеанс завершён, после него новых UART/reset/Flash операций нет.
История ниже описывает состояние до миграции, не текущую установленную firmware.

## 22 ноября, 13:28: резервный снимок ESP32 перед OTA

По отдельному разрешению выполнены read-only preflight, полный snapshot16МиБ,
проверка соответствия всей Flash по MD5, SHA-256 и приватные копии на двух
физически разных NVMe. Security flags0, Secure Boot/Flash Encryption отключены;
eFuse/Flash program/erase не выполнялись. [Результаты и пределы](evidence/hardware/2026-11-22-esp32-pre-ota-snapshot/README.md).
Действующая карта NVS/PHY/factory2MiB и app descriptor8f0d899/ESP-IDF6.1.1
проверены по локальному снимку. После reset владелец подтвердил прежний HMI,
касания и Wi-Fi. Разрешённый12-минутный сеанс завершён, новые операции им не разрешены.

Новый rev12-ota не записан; PSRAM XiP boot, USB migration/recovery, signed OTA,
rollback/power-loss и runtime margins остаются открытыми. На F407 по-прежнему
ee7d179; к нему, CAN и приводам в этом сеансе не обращались. Подготовленный
clean-main fecc05d bundle — будущий образ, не installed firmware.


## 22 ноября, 11:33: успешный базовый стенд F407

22 ноября 2026, база main `07fb406e37ebd1f1e019dbee0b54e8bb5571be1a`, PR #1–57 слиты.
После отдельного разрешения один стенд ee7d179 завершён за16,196s, exit0:
program/verify/exact identity, RUNNING, data/BSS/один CCM word/guards, progress,
watchdog reset и UsageFault capture/reset **PASS**. На F407 оставлен работающим
clean `ee7d179435f3f9e7598c7646b85ff64f4d5e9a32`, BIN2540bytes. Условный restore
не понадобился. Предыдущий c09 FAIL8 и проверенный полный snapshot restore — история.

Плата изолирована, только питание/SWD; option bytes/OTP/CAN/motion/ESP32 не менялись.
Этот разрешённый сеанс завершён. Полный RAM/CCM stress, stack high-water,
точные HSI/LSI/HSE частоты, physical GPIO/CAN/inhibit и power-loss остаются открытыми.
P0 выполнен на базовом стенде; следующий приоритет — P1 ESP32 OTA с existing
HTTPS/crypto и review16MiB factory+двух OTA slots/USB migration. Новых hardware
операций или будущих CAN loader addresses это не разрешает.

[Успешный стенд ee7d179](evidence/hardware/2026-11-22-f407-ee7d179-bench/README.md), [пять snapshots](evidence/hardware/2026-11-22-f407-ee7d179-bench/snapshots.md).
Ticks1499→2997 и heartbeat/feed14→29 при boot1; watchdog request дал boot2,
UsageFault request — boot3/CFSR.UNDEFINSTR/валидный frame/PC0x0800044c в точном
ELF. После fault ticks3005→4501, heartbeat/feed30→45, boot3 неизменен. Пять
warm-history CRC проверены локально. Это минимальный init/probe, не full RAM test.
Probe sample3.255666V не является calibrated/load measurement; nominal HSI16MHz
не измеряет частоту. NRST не подключён; новый manual RESET не понадобился.
Backup2×1MiB и independent NVMe copy сохранены, новые full-Flash reads не делались.
ESP32 ordinary8f0d899 прежний. Остальные аппаратные blockers ниже сохраняются.

## История до 11:33: c09 отказал, snapshot восстановлен

22 ноября 2026, main `4fe84f10a70228b9f440f41c6ccc50869919042a`, PR #1–56 слиты.
Первый реальный c09a20e был записан и verify прошёл, но boot bench завершился
**FAULT/error8 (IWDG_TIMEOUT)**. После восстановления SWD с DBG_SLEEP полный
проверенный снимок1MiB записан обратно, verify/reset-run успешны.
**На завершение прежнего сеанса был восстановлен snapshot, не c09a20e.** Все пять аппаратных сеансов завершены.
Надпись STM32F407VGT6 сверена владельцем; Flash1024KiB/driver12sectors/RDP0/
nWRP0xFFF прочитаны. Current backup2×1MiB и SHA-проверенная копия на другом
физическом NVMe сохранены приватно. NRST к ST-Link не подключён; RESET отпущен.

Исправлены IWDG start-before-sync и DBG_SLEEP в OpenOCD, обновлены
регрессионные тесты и runbook. Новый ELF/BIN программно проверен; его работу
на плате ещё не подтверждали. После CI/review потребуется **новое разрешение
на точный образ и стендовые действия**. Watchdog/UsageFault/CCM/guards/progress
пока не прошли; нет GPIO/CAN/motion, option-byte/OTP или изменений ESP32.

[аппаратные факты и логи](evidence/hardware/2026-11-22-f407-swd-preflight/README.md). В recovery прочитаны state5/error8, watchdog_started0,
ticks/heartbeat/feed0, history previous_reason8. Эти нули означают, что boot
self-tests не достигнуты; они не доказывают дефект SRAM/CCM. Debug CR0→1 и
доступ после ручного RESET зафиксированы. Полное восстановление1MiB имеет
Verified OK и marker VERIFIED_SNAPSHOT_RESTORED, общий bench exit1 сохранён.
Probe samples3.257257/3.260437V — не calibrated/load test. REV_ID0x101f означает
silicon5 или6 по ST, exact revision ещё неизвестен; core r0p1 — другое поле.
Snapshot проверен как текущее ПО до стенда, его original factory identity не доказана.
HSE/точность HSI-LSI, полные RAM/CCM/stack stress, pinout/GPIO/CAN/inhibit остаются
неподтверждёнными. Ни один сеанс не менял option bytes/OTP/CAN/motion.

## Исторический программный snapshot P0 (до разрешённого SWD preflight)

22 ноября 2026, от main610f174 (PR #55 слит). Подготовлен настоящий
[STM32F407VG HSI bench](STM32_FIRST_IMAGE.md): ELF/BIN/map, официальный
Flash1MiB/SRAM112+16KiB/CCM64KiB/12sectors/98vectors contract, watchdog и
RAM/SWD diagnostics. Это программная подготовка, **не исполнение на плате**.
SWD/serial/USB/read/reset/Flash/backup действия не выполнялись.
Суффикс VGT6 и owner facts ниже прежние; full physical marking/package,
реальные HSI/LSI/HSE/RAM/CCM/reset/watchdog/protection/erase/restore/GPIO/CAN
не подтверждены. Официальные адреса теперь рассмотрены и не являются unknown
для software bench profile; это не подтверждает экземпляр или CAN loader layout.
Перед первой записью — [отдельное разрешение и runbook](STM32_STLINK_RUNBOOK.md),
original F407 backup SHA/независимая копия/protection preflight. Установленный
ESP32 ordinary8f0d899 не менялся. Всё ниже — факты владельца и история этапов.


PR-07v от main `5e76626`: [threaded signed HOST tests](STM32_THREADED_LINK.md)
добавляют программную проверку отдельного driver actor, не аппаратное испытание.
Owner SWD/core/Flash1024KiB/3,252V/backup/vector facts ниже прежние; новых
подключений/SWD/UART/reset/Flash/backup reads не было. ESP32 ordinary8f0d899.
Full marking/package/RAM/CCM/HSE/IRQ/erase/protection/GPIO/CAN/restore остаются
неподтверждёнными; software ACK и join не доказывают physical drain/inhibit.

Слитый PR #53, PR-07u от main `bc35a37`: [подтверждённая пауза приёма](STM32_INGRESS_OWNER.md) связывает actual
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

PR-07m [metadata transport](STM32_METADATA_TRANSPORT.md) — software host/ARM
foundation, не новый аппаратный результат. RX1728/snapshot1212 host bytes и
отдельный archive1568text не подтверждают F407 RAM/clock/CAN/bootloader reserve.
Owner facts/ordinary8f0d899 прежние; никаких device/SWD/UART/reset/Flash/backup reads.

PR-07l [one-clock host composition](STM32_UPDATE_TIMING.md) не добавляет
аппаратных фактов. Synthetic delays/2551ms не measured CAN/CPU/latency или
подтверждённый timing/RAM reserve. Owner facts/ordinary8f0d899 и blockers ниже
сохранены; никаких новых устройств/SWD/UART/reset/Flash/backup rereads.

PR-07k [fragment/reassembly](STM32_UPDATE_FRAGMENTS.md) — только host/ARM compile,
без новых аппаратных фактов. Local route/generation не actual CAN mapping,
identity/authentication или физический safe state. Owner SWD/core/Flash/backup
facts ниже сохранены; ordinary8f0d899 прежний. Устройства/backup не читались.

PR-07j [byte codec](STM32_UPDATE_WIRE.md) проверен на host/ARM compile и signed
RAM chains; это не физический CAN/link/identity/память или новый boot платы.
Никаких SWD/UART/reset/Flash/backup rereads. Owner facts и ordinary8f0d899 прежние.

PR-05d [build binding audit](RELEASE_BUILD_BINDING.md) — только host tooling,
не новые измерения. Unsigned build pin/full bytes/signature consistency не
подтверждают actual layout, F407 RAM/HSE/Flash geometry/GPIO/CAN или recovery.
Factory/A-B/profile mismatch остаются blockers; устройства/backup не читались.

Актуализация20ноября2026: владелец повторил только суффикс VGT6, новых аппаратных
измерений нет. Остальной снимок на19ноября2026. Источники: сообщения/фотографии/терминальные журналы
владельца и проверенные файлы репозитория. В PR-01 агент не подключался
к устройствам и не повторял аппаратные проверки. «Сообщено владельцем» не
равно независимому измерению агентом. Новые результаты добавлять с версией
ПО, способом проверки, датой и ограничениями; исходные дампы не публиковать.

## Наличие и подтверждения

| Узел | Подтверждено / источник | Что не подтверждено |
|---|---|---|
| ESP32 HMI | Waveshare ESP32-S3-Touch-LCD-7 PCB Rev 1.2, 800×480; владелец и фото | Длительный stress/thermal test, стабильность всех подсистем |
| Flash ESP32 | Приняты 16 МБ; полный локальный образ 16 777 216 байт, SHA-256 проверен; [backup](BACKUP_STATUS.md) | Успешное восстановление заводского образа, отдельная копия вне основного диска |
| PSRAM ESP32 | ROM/esptool сообщает Embedded PSRAM 8MB AP_3v3; rev12 требует octal 80 МГц | На b93cf3b агент подтвердил startup PSRAM 8MB/pool 8192K и свободную память; полный runtime memtest/stress и пропускная способность не проверены |
| Запись HMI | 19 ноября после headroom test возвращён ordinary rev12 8f0d899, 3/3 hashes verified; владелец подтвердил HMI/touch/Wi-Fi; отчёт ниже | Boot ordinary не перечитывали по UART; физические FPS/latency/soak не измерены |
| RGB LCD | На фото владельца HMI и список Wi-Fi выводятся на физический экран | Измерения tearing, FPS/scanout, ориентация/качество всех областей |
| GT911 | На b93cf3b владелец подтвердил касания, удобную клавиатуру и показ/скрытие пароля | Покрытие углов, multitouch/ошибки, latency, воспроизводимый полный тест сенсора |
| Wi-Fi | Физический список SSID/RSSI виден на фото; владелец сообщил ошибку 263 | На b93cf3b владелец подтвердил подключение/IP, агент видел wifi_ip_events=1 в обоих запусках. Неверный пароль, полный reconnect/NVS power-cycle stress ещё не проверены |
| Защита ESP32 | По переданному get-security-info Secure Boot и Flash Encryption Disabled | Это не разрешение менять eFuse; не утверждается защита от физического чтения паролей |
| F407 | STM32F407VGT6 marking owner; core/ID/Flash1024KiB/driver12sectors/RDP0/nWRP0xFFF прочитаны; ee7d179 program/verify, базовый boot/CCM/guards/progress/watchdog reset/UsageFault PASS22ноября | Полные RAM/CCM stress/high-water, HSE/точные частоты, pinout/GPIO/CAN/inhibit и power-loss recovery неизвестны |
| Backup F407 | Current snapshot2×1048576bytes byte/SHA/basic vectors PASS, independent NVMe copy SHA PASS; в прежнем failed-c09 сеансе snapshot restore/verify/reset PASS. В успешном ee7d179 restore не понадобился | Factory provenance snapshot не доказана; power-loss recovery не испытан; dumps/их SHA/private paths не публикуются |
| USB F407 | Владелец сообщил 0483:5720 Mass Storage Device | Принадлежность именно F407 плате нужна отдельная проверка; VID/PID не определяет part number/память/bootloader |
| ST-Link V2 | По отчёту владельца работает с Blue Pill и с конкретной F407 по SWD | Версии probe/debugger для нового F407 сеанса и испытание restore не сообщены; доступ не разрешает новые device operations |
| Blue Pill | STM32F103C8T6; OpenOCD 0.12.0 видит Cortex-M3, Flash 64 КиБ, SWD работает; отчёт владельца | CAN, таймеры, bootloader, подключение к станку не испытаны |
| Backup Blue Pill | Сохранён локально, таблица векторов структурно проверена; отчёт владельца | Путь/хеш в публичном Git не фиксируются без отдельной безопасной записи отчёта; восстановление не проверено |
| Ось X/Z | Один closed-loop NEMA23 заказан, ещё не получен; второй не приобретён | Точная модель двигателя/драйвера, ток, питание, encoder/alarm/ENABLE, стендовые результаты |
| CAN | Внешние трансиверы отсутствуют, по актуальному сообщению владельца | Нельзя считать наличие физического линка подтверждённым по Wiki семейства Waveshare |
| microSD | Поддержка в проекте не реализована | Наличие/тип карты, mount/FAT32/извлечение, разводка/режим шины конкретного стенда |
| OTA / motion | Read-only verification/session PR #9–13 слиты; installer/реальный motion отсутствуют | ESP32 A/B, STM32 bootloader, CAN update, реальные оси/шпиндель не испытывались |

Актуальное сообщение о заказанном, но не полученном двигателе заменяет старое
неоднозначное «purchased/test drive». Старое имя STM32F407VGT6 не считать
аппаратным измерением. В новом сообщении для PR-02 владелец подтвердил
суффикс VGT6; источник — сообщение, не SWD чтение агента. Новый owner SWD report
ниже подтверждает доступ/core/Flash, но не физическую маркировку корпуса.
Ответ20ноября «vgt6» повторяет этот суффикс; полное обозначение и маркировка
HSE не переданы. Новые SWD/clock/GPIO/CAN или backup операции не выполнялись.

PR-08a [локальные сведения о ПО](FIRMWARE_INFO_UI.md) проверены только host/SDK
builds; на устройство этот UI не записывался. «Локальные данные HMI» означает
app descriptor, не новую аппаратную проверку. На стенде прежний ordinary8f0d899;
версии STM32/CAN остаются неизвестными, hardware gates не закрыты.

PR-07f [logical negotiation](STM32_UPDATE_NEGOTIATION.md) — только RAM/ARM compile
и signed integration. Test boot epochs/pool counters не являются измеренными
device identity или persistent recovery. Новых подключений/прошивки/измерений нет.

PR-07g [independent release binding](STM32_RELEASE_BINDING.md) — host/ARM software
проверка signed document/образа. Local policy и RAM budgets не подтверждают MCU
context, memory/erase map или readiness. Новых hardware действий нет; ordinary8f0d899
и owner facts ниже сохраняются.

PR-07h [release handoff](STM32_RELEASE_HANDOFF.md) — software integration нового
signed document с live lease/RX/context/vector guards и historical completion.
Host/sanitizer/ARM checks не подтверждают реальную память/Flash/GPIO/CAN или
physical quiescent state. Новых измерений/подключений нет, аппаратные факты прежние.

PR-07i [RAM staging source](STM32_STAGING_SOURCE.md) — host/ARM software
consumer/lease backend. Explicit caller buffer не резервирует RAM/Flash и не
доказывает, что image1024KiB помещается в F407 RAM. Production medium/budget и
durable recovery не выбраны; новых hardware действий или подтверждений нет.

PR-05c [offline cohort review](RELEASE_COHORT_REVIEW.md) добавляет host tooling
для signed artifacts/identity/peer checks. Synthetic fixture reports не
подтверждают installed versions, Flash layout, RAM, GPIO/CAN, bootability или
physical state. Новых устройств/измерений нет, ordinary8f0d899 прежний.

## STM32F407 — подтверждение владельца после main b19a40b

Приняты перечисленные в таблице результаты нового задания владельца:
SWD/ST-Link V2, Cortex-M4 r0p1, device ID0x101f6413, Flash1024KiB,
напряжение3,252V, backup1048576bytes и structural vector check PASS.
Данные записаны 19 ноября2026; точная дата/команды измерения не переданы.
Агент не подключался к STM32, не повторял чтение и не открывал raw dump.

Размер1MiB больше не UNKNOWN. Он не определяет sector boundaries/erase policy,
RAM/CCM, package pins, HSE или возможность двух OTA slots. Raw device ID сохранён
как сообщён; core r0p1 не выдаётся за chip silicon/package revision. Напряжение —
один сообщённый результат, не проверка питания под нагрузкой. Подробные пределы
доказательств — [owner report](evidence/hardware/stm32f407-owner-swd/README.md).

Исторический следующий шаг этого owner report — [startup/sections/no-I/O foundation](STM32_STARTUP.md).
Текущий P0 образ и актуальные аппаратные gates описаны в начале документа.
Synthetic link fixture не использует эту1MiB как production layout и не резервирует
bootloader/app addresses. Hardware startup/clock/GPIO/CAN остаются открытыми.

[PR-07a logical transport](STM32_UPDATE_TRANSPORT.md) проверяется только
на ПК/RAM и не добавляет аппаратных результатов. Нет CAN/Flash operations,
crypto/install/commit или physical safe-state proof. Owner facts выше сохранены;
новое чтение MCU или бэкапа агентом не выполнялось.

## Границы доказательств

- Компиляция обоих SDK/профилей и host render — программные результаты.
- Фото подтверждает наличие изображения в момент съёмки, не FPS/latency.
- Структурно корректный backup не доказывает работоспособность восстановления.
- Ни эта инвентаризация, ни симуляция не разрешают включать привод.
- Следующая аппаратная работа — только отдельное разрешённое испытание;
  список неизвестных и критерии: [OPEN_QUESTIONS.md](OPEN_QUESTIONS.md).

## Базовые данные ПО

Актуальный main после согласования PR-01: `0c4168e864d4c867ba0f49a86eba72c0d59205a1`.
[PR #5](https://github.com/avlyubimov/TV4-CNC/pull/5), head
`b92c559c304f318e4a76c37f438717af2fac4124`, уже слит: координаты и исправление
Wi-Fi/ввода включены в main. Это не подтверждает их испытание на устройстве.
Версия на плате не выводится из имени main. PR-01 сохраняет код main и не
выполняет повторную прошивку.

## Первый baseline b93cf3b — 18 ноября 2026

[Sanitized evidence и ограничения](evidence/hardware/2026-11-18-b93cf3b/README.md).
Internal free около 25–27 KB, min 7796 bytes, largest block 12288 bytes:
runtime HTTPS пока блокирован memory budget/allocation review и TLS peak test.
Вторая serial выборка содержит новый boot; влияние UART open/close не изолировано,
не считать это непрерывным soak или watchdog failure. Новых serial/reset операций
агент не выполняет без согласования следующего испытания.

## PSRAM bench 1c38d1e — 18 ноября 2026

По отдельному разрешению записан rev12-memory-bench; [три цикла PASS и evidence](evidence/hardware/2026-11-18-1c38d1e-psram/README.md).
Буфер34000bytes реально выделен в PSRAM, pattern/release PASS, PSRAMfree
возвращалась к исходному значению. Bench watermark минимум2684bytes.
Владелец подтвердил сохранение работы HMI/touch/Wi-Fi. Один60s UART capture,
в нём один boot и нет ошибок; после close порт не открывали. Internal largest
всё ещё12288: TLS peak/time/budget review остаётся отдельным блокером.
На завершение теста 18 ноября оставалась 1c38d1e **memory-bench**; текущий статус ниже.

## UTC/TLS bench b085abb — 19 ноября 2026

[Разрешённая проба и numeric evidence](evidence/hardware/2026-11-19-b085abb-utc-tls/README.md).
SNTP PASS/source unauthenticated, но TLS OPEN FAIL: mbedtls_ssl_setup -0x008D,
PSA_ERROR_INSUFFICIENT_MEMORY. HTTP/сертификат не проверены. Internal window
min8795/largest sample7680bytes, monitors cleanup PASS. Это реальный blocker
runtime HTTPS; production memory budget не подтверждён. Один90s capture,
17 diagnostics/один boot; ошибки display/touch/queues/watchdog не наблюдались.
После пробы b085abb **вернули ordinary rev12 b085abb**, SDK6.1.1, 3/3 hashes verified;
UART после возврата не открывали. Владелец подтвердил главный экран/касания/Wi-Fi.
Проверка записи не равна factory restore. Старое разрешение исполнено.

Software PR-04k [исправляет allocator и SDK certificate dates](TLS_MEMORY_REVIEW.md)
только в TLS diagnostic profile. Его программные проверки сами по себе
не подтверждают hardware; последующее испытание и текущая запись — ниже.

## TLS/PSRAM bench 1bfc7e2 — 19 ноября 2026

[Отчёт и sanitized evidence](evidence/hardware/2026-11-19-1bfc7e2-tls-psram/README.md).
По отдельному разрешению испытан merged PR #22: SNTP PASS, TLS/HEAD HTTP200
PASS, cleanup/monitors PASS. Один90s capture,17diagnostics/один boot, ошибки
касаний/дисплея/очередей/watchdog не наблюдались. Прежний isolated TLS OPEN OOM
не повторился, но production gate не закрыт: internal window min3123bytes,
historical min2408,worker stack watermark912/6144bytes. Нужен memory/stack review.
После пробы возвращён **ordinary rev12 1bfc7e2**,SDK6.1.1,3/3 hashes verified,
UART больше не открывали. Владелец подтвердил HMI/touch/Wi-Fi. Это состояние на конец пробы1bfc7e2;
новых аппаратных операций/OTA install тот отчёт не разрешал. Текущий статус ниже.

PR-04l [готовит TLS headroom fix](TLS_HEADROOM.md) без новых аппаратных операций.
На момент software PR запись оставалась ordinary1bfc7e2. Последующая проба
ниже не закрывает production/CPU/worst-case budget.

## TLS headroom8f0d899 — 19 ноября 2026

[Отчёт и сравнение](evidence/hardware/2026-11-19-8f0d899-headroom/README.md).
По отдельному разрешению испытан merged PR #24: SNTP/verified HEAD200/cleanup/
monitors PASS. Один90s capture,17diagnostics/один boot, display/touch/queue/
watchdog errors не наблюдались. Internal window min3123→5307,history2408→4204,
worker stack watermark912→1840bytes; это два разных boots, не causal/production
budget. Lowest window sample уже после TLS open; header phase не снизила minimum.
После теста возвращён **ordinary rev12 8f0d899**,SDK6.1.1,3/3 hashes verified;
UART больше не открывали, владелец подтвердил HMI/touch/Wi-Fi. Это текущая запись.
Worst-case/failure/load/soak/CPU/OTA memory budget остаются открытыми.

PR-04m [добавляет программный resource guard](OTA_RESOURCE_GUARD.md), native
sampler только compiled/linked. Новых device/serial/reset/SNTP/HEAD операций
нет, текущая запись ordinary8f0d899 и её измерения не меняются.

## Software PR-07b — 20 ноября2026

[Read-only image admission](STM32_IMAGE_ADMISSION.md) добавляет только PC tests
и ARM compile. Новых hardware результатов/доступа к платам/backup reads нет.
RAM arena и code/frame metrics не подтверждают карту памяти или готовый
bootloader. Все package/HSE/erase/GPIO/CAN/recovery unknowns сохранены.

## Software PR-07c — 20 ноября2026

[Session/source handoff](STM32_UPDATE_HANDOFF.md) проверен в RAM и ARM compile,
без device/SWD/serial/reset/Flash operations. Owner факты SWD/core/rawID/Flash
1024KiB/3,252V/backup1048576bytes/vector structure остаются принятыми. Lease/
generation/receipt не подтверждают physical inhibit, RAM/CCM/erase/protection,
HSE/package/pinout/GPIO/CAN/restore. На ESP32 ordinary8f0d899 без новых записей.

## Software PR-07d — 20 ноября2026

[Vector/model preflight](STM32_IMAGE_PREFLIGHT.md) проверен только в synthetic
RAM/host/ARM compile. Не читали MCU/raw dump и не повторяли SWD/Flash/reset.
Owner SWD/core/ID/Flash1024KiB/3,252V/backup1048576bytes/vector structure facts
сохранены; RAM/CCM/package/HSE/full IRQ/pinout/erase/protection/GPIO/CAN/restore
не подтверждены. Actual alignment/map неизвестны; reference docs не заменяют
проверку платы. На ESP32 ordinary8f0d899, без новых записей.

## Software PR-07e — 20 ноября2026

[Completion/outcome](STM32_UPDATE_OUTCOME.md) проверен только в host RAM/ARM
compile. Новых device/SWD/UART/Flash/reset/backup reads нет. Owner SWD/core/ID/
Flash1024KiB/3,252V/backup1048576bytes/vector structure facts сохранены.
Package/RAM/HSE/IRQ/erase/protection/GPIO/CAN/restore и physical quiescent gate
не подтверждены; peer report/ACK не закрывает их. На ESP32 ordinary8f0d899.
