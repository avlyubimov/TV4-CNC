# Открытые вопросы и блокирующие решения

## Актуально: b01ff48 контроль PASS, CHECK остановлен на нулевой длине HTTP302

[Один автономный CHECK](evidence/hardware/2026-11-24-esp32-signed-check-b01ff48/README.md)
закрыт: control PASS; auto attempted1/pass0, HTTP302/header bytes5704/bad_headers1.
Resource guard0/checks2/stack6896 — ещё не full session budget. Весь factory
fecc05d возвращён/verify/UART30 PASS, full restore0; owner UI ночью не наблюдался.
[SWD readback F407](evidence/hardware/2026-11-24-f407-readback/README.md) PASS:
current1MiB/ee7d179 exact bytes, self-tests/guards/progress, resume; без прошивки/
reset/RAM test requests. Две приватные current copies на разных NVMe проверены.

[Следующий exact пакет](plans/P1_AUTONOMOUS_SIGNED_CHECK.md) исправляет
семантику ESP-IDF5/6: fetch_headers выставляет is_chunked для Content-Length0,
даже без Transfer-Encoding. Narrow adaptation только для explicit zero length
без TE; empty200, unknown framing, CL+TE, duplicates/compression и прочие
contradictions отвергаются. Сначала237 host/sanitizers/оба SDK/CI, затем новый
fresh16MiB/two-NVMe → control flag0 PASS → один full CHECK flag1 → весь fecc05d.
Стенд b01ff48 закрыт, не повторять. Подпись/full stream/install/rollback ещё не
приняты. Пороги/CA/hostname/date/timeouts и обычные профили сохраняются.

Действует разрешение владельца на автономные обратимые проверки изолированных
плат до24ноября08:00 Asia/Almaty: finite≤1800s/recovery reserve, без eFuse/
option bytes/CAN/motion/merge/новых релизов. Published bench.2 остаётся read-only.

## После PR #77: опубликованный payload и следующий configured CHECK

[Prerelease v0.1.0-bench.2](https://github.com/avlyubimov/TV4-CNC/releases/tag/v0.1.0-bench.2)
опубликован по отдельному разрешению на c013ddf:11 downloaded assets/SHA,
подпись и полный BIN Python+C PASS; CI source30/main15/tag15 PASS.
[Протокол публикации](evidence/build/ota-signed-check/publication.md).
[Первый configured стенд](evidence/hardware/2026-11-23-esp32-signed-check-c013ddf/README.md)
закрыт: boot/touch PASS, два ранних owner CHECK во время контроля дали
SESSION_RESOURCE/stack3024 ниже floor3072. Planned CHECK420s не запускался;
factory fecc05d возвращён/verify/UART/owner PASS, full restore0.

Следующий результат — [configured worker headroom](plans/P1_CONFIGURED_WORKER_HEADROOM.md):
12288 вместо8192 только при provisioned context; ordinary3072 и bench8192
прежние. Floors/CA/time/touch не снижать. Новый exact BASE после двух SDK/CI:
чистый контроль, один CHECK по команде, возврат. Время проверки явно продлено
владельцем; finite1800s и recovery reserve сохраняются. SESSION4/production
budget/install/confirmation/rollback ещё не приняты; F407/CAN/motion не менялись.


## 23 ноября: LVGL PSRAM и HTTPS — PASS, далее signed CHECK

PR #75 слит владельцем, main1682da0 CI15/15 PASS. [Стенд38f70b9](evidence/hardware/2026-11-23-esp32-lvgl-38f70b9/README.md)
завершён за1063,692s: fresh16MiB/device MD5/SHA/two-NVMe, baseline46cf400 →
новый control → три TLS reset boots → весь factory2MiB fecc05d/verify/UART30 PASS.
Четыре writes/шесть captures; full restore не понадобился. На плате fecc05d,
F407 ee7d179 не открывался. Владелец заранее разрешил необходимые P1 стенды
23ноября2026 Asia/Almaty без повторных вопросов; этот exact пакет уже закрыт.

Свободная internal RAM control16771–16831 →124703–124755bytes, largest7680→63488.
Три verified HEAD HTTP200, cleanup1, полные DMA traces без allocation failures;
TLS local minimum108563–108867, remaining stack3760–3904/8192, touch2–19ms/errors0.
Возврат fecc05d touch1–11ms PASS. Это UART evidence, не physical touch/FPS:
render/event samples0, визуального owner подтверждения сегодня нет.

[Software evidence](evidence/build/esp32-lvgl-psram/README.md):16 clean ELF/BIN
двух SDK,183 host/183 sanitizer tests, actual allocator analyzer/ELF/config PASS.
Global malloc policy/stack/DMA64KiB/TLS16384/4096/RGB/pins/clocks/touch/CA/time
не ослаблены. [Следующий этап](plans/P1_SIGNED_SESSION_RESOURCE.md) — настоящая
signed CHECK/session4 с полным manifest/image stream в существующем worker8192,
затем install/confirm/rollback и manual release acceptance. Локальный ключ уже создан; стендовые SemVer/метки согласованы.
Начальная time/resource policy для CHECK23ноября одобрена владельцем;
exact candidate теперь опубликован отдельно, см. актуальный раздел выше;
SETUP/CHECK/INSTALL gates сохраняются, test keys/floors из HEAD не назначать.

## История до стенда38f70b9

## 23 ноября: DMA headroom и LVGL allocator

[Стенд46cf400](evidence/hardware/2026-11-23-esp32-dma-46cf400/README.md)
подтвердил9 failed DMA|INTERNAL|8BIT allocations при TLS handshake, сохранил4
other-task requests190/190/190/136bytes. Потеря5 явно помечена; точный участок
SDK/AES request не подтверждён. Не подменять DMA headroom generic internal
largest. HTTPS HEAD, configured crypto/install/rollback и production floors
пока не приняты. Dashboard потратил100516bytes internal без расхода PSRAM:
следующий отдельный PR — официальный LVGL CUSTOM PSRAM backend только OTA.

Full fresh16MiB USB restore/verify/reset теперь реально проверен, после него
UART30s PASS. Это восстановление текущего fecc05d snapshot, не OTA rollback
или power-loss test. В старом fecc05d один touch sample дал UINT32_MAX; причина
соответствует известному clock-before-snapshot race. Сегодняшнего owner UI/
physical touch/FPS/latency подтверждения нет. Ключ/context по-прежнему не заданы.

## История до 23 ноября

## 22 ноября,23:25–23:37: touch PASS, AES allocation в HTTPS

[Новый стенд3927063](evidence/hardware/2026-11-22-esp32-worker-tls-3927063/README.md) завершён за743,183s:
fresh16MiB/MD5/SHA/two-NVMe PASS; control touch_fresh16/16, age2–19ms/errors0.
SNTP PASS; первый HEAD FAIL после `esp-aes: Failed to allocate memory`, TLS132/
status-1/cleanup1. Остальные TLS boots остановлены. Три writes/три captures,
весь factory2MiB fecc05d возвращён/verify/UART/owner UI-touch-Wi-Fi-SETUP PASS.
Владелец подтвердил и временный3927063. F407 ee7d179 не открывался; full restore
не понадобился, сеанс/разрешение закрыты. [Следующая работа](plans/P1_AES_DMA_MEMORY.md)
— измерить request/caps/DMA fragmentation и устранить дефицит без отключения
AES/CA/time или уменьшения TLS/DMA/display параметров. Signed OTA ещё не принят.

## История прежних стендов

## 22 ноября: контроль a5f8b7e остановлен, fecc05d возвращён

[Стенд](evidence/hardware/2026-11-22-esp32-worker-tls-control/README.md) завершён
за608.667s: fresh16MiB/device MD5/SHA-256/two-NVMe PASS, control8192 app-only
write/verify/UART90.101s. Одно touch_age_ms=4294967295 на uptime26335ms из16
samples при touch_errors0; остальные0–19ms. touch_fresh FAIL сохранён,
TLS image не записывался, SNTP/HEAD не запускались. Весь factory2MiB fecc05d
возвращён из свежего снимка/verify/UART30.046s/owner HMI-touch-Wi-Fi-SETUP PASS.
Две writes/два captures, полный16MiB restore не понадобился. На ESP32 fecc05d,
F407 ee7d179 не открывался; разрешение использовано, новых аппаратных действий нет.

В actual app_main найдены два clock-before-snapshot paths. Host injections
воспроизводят underflow/false-health, исправление копирует snapshot прежде clock.
Пороги100/500ms, polling20ms и строгий hardware parser сохраняются. Исправление
ещё не испытывалось на плате; TLS/crypto/install/floors/rollback остаются открыты.

## 22 ноября: worker8192 reset-стенд подтверждён, TLS/install gates открыты

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

Исторические разделы ниже относятся к завершённым прежним сеансам.

## 22 ноября: уточнение RAM/stack после реального BOOT стенда

[fc90200](evidence/hardware/2026-11-22-esp32-ota-memory/README.md): APP_ENTRY history202308 → local BOOT minimum1887 к UI_READY
→ history112 после stop; OTA-worker остаток1488из3072. Нужна локализация
startup transient allocations между dashboard и UI ready; конкретный allocator
не измерен. Per-heap сумма не simultaneous OOM/floor. Configured8192/TLS/
installer budget и production thresholds остаются открытыми.
Fresh16MiB fecc05d/two-NVMe и factory2MiB return/verify/owner HMI PASS; full16MiB
recovery не испытывался. На ESP32 fecc05d/A-B, F407 ee7d179 не открывался.
Key/context отсутствуют, CHECK/INSTALL закрыты. Сеанс завершён; ниже история
до фазного стенда, включая прежнее отсутствие OTA-worker watermark.

## История до фазного стенда

22 ноября2026: [первая USB migration](evidence/hardware/2026-11-22-esp32-ota-migration/README.md)
закрыла базовый переход на tv4-16m-ab-v1/factory fecc05d, verify/NVS preservation,
PSRAM8MiB/XiP/GT911/UI и owner saved Wi-Fi/SETUP. Два backup NVMe сохранены,
restore не выполнялся. OTA-ESP-01 открыт для configured install/confirm/rollback/
Wi-Fi/power loss/recovery; ключ/context по решению владельца отсутствуют.

Новые измерения:16 periodic samples, internal free22851..22907/largest7680,
SDK history112bytes; OTA-worker stack watermark отсутствует. History112 требует
разбора по фазам, не доказывает simultaneous OOM и не задаёт допустимый floor.
Render/event samples0 не закрывают FPS/latency/soak. Production RAM/stack budget
installer не утверждён. Сеанс закрыт, новый UART/reset/Flash требует разрешения.

## История подготовки до миграции


Текущий P1 от main d1317a8: [installer/self-test/rollback/USB package](ESP32_OTA_INSTALLATION.md)
подключены программно в explicit rev12-ota. Владелец выбрал настройку без ключа;
OTA-KEY/CONTEXT и production resource/time floors остаются открытыми, установка
fail-closed. OTA-ESP-01 теперь требует отдельно разрешённой USB migration/recovery
и configured install/rollback/network/power-loss испытаний, не ещё одной модели.
Реальная peak/low-water RAM/PSRAM/stack нагрузка installer не измерена; HEAD bench
не заменяет её. Устройства/текущие образы не менялись; сведения ниже исторические.


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

[Успешный стенд ee7d179](evidence/hardware/2026-11-22-f407-ee7d179-bench/README.md); [HARDWARE_STATUS.md](HARDWARE_STATUS.md).
Ни одна оставшаяся неизвестная характеристика не блокирует software работу P1.
Новый аппаратный сеанс/образ не покрываются завершённым разрешением ee7d179.

PR-05b [release policy review](RELEASE_POLICY_REVIEW.md) проверяет существующий
HMI contract и закрывает два encoding расхождения; actual local SemVer/key context,
production Motion metadata, CAN и hardware gates ниже остаются открытыми.

PR-08a [local info UI](FIRMWARE_INFO_UI.md) показывает build/SDK/profile; это
не закрывает OTA-CONTEXT-01, доверие/installer/resource gates или физический
version handshake F407. STM32/CAN версии намеренно остаются unknown.

PR-07f [negotiation model](STM32_UPDATE_NEGOTIATION.md) закрывает RAM lifecycle/
fresh counter gap внутри сохранённого boot pair. Boot epoch provenance/persistence/
collision policy, authentication/capabilities и raw wire остаются открытыми.
Полностью matching forged reply может пройти; это не защита от attacker.

PR-07g [signed release binding](STM32_RELEASE_BINDING.md) проверяет отдельный
experimental Motion document, явный local selection/SemVer/channel/target и
полные signature/SHA/EOF. Это не final production bundle/schema, установленный
context или реальный GitHub artifact. Source claim не доказывает build provenance;
PR-07h [release handoff](STM32_RELEASE_HANDOFF.md) теперь проверяет новый API
в live lease/coordinator/completion цепочке. Production runtime/backend/installer
ещё нет; аппаратные вопросы ниже не закрыты.

PR-07i [staging source](STM32_STAGING_SOURCE.md) реализует bounded volatile RAM
consumer/lease через тот же transport и verifier. Source immutability действует
для API одного owner; это не DMA/MPU protection, persistent recovery/identity
или подтверждение размещения всего образа в RAM F407.

PR-05c [cohort review](RELEASE_COHORT_REVIEW.md) закрывает offline сравнение
signed release/source/channel/full artifacts и prospective HMI peer range.
Это не production bundle/двусторонняя совместимость, protocol mapping,
trusted installed-state/key provider, build attestation или install authority.

PR-07k [bounded fragments](STM32_UPDATE_FRAGMENTS.md) закрывает local buffer/
order/duplicate/deadline/backpressure software contract вокруг codec. Он не
передаёт local generation/route на wire, не различает old frames после reboot
и не аутентифицирует peer. Production identity/routing/queue lifetime, metadata/
signature transport, measured bus/worker budget и old logical timeout composition
ещё требуют review. Известные1MiB Flash не определяют RAM или bootloader sectors.

PR-07l [shared-clock tests](STM32_UPDATE_TIMING.md) проверяет losses/queue/retry/
existing deadlines на одной шкале. Это не измеренный scheduling/bus/crypto budget
и не firmware owner. Old START принимается новым RX после local generation
change: production reboot replay isolation/routing/authentication ещё открыты.
Следующий программный contract — bounded raw metadata/signature transport,
immutable snapshots и digest/session binding; физические blockers не закрыты.

PR-07m [metadata container/RX](STM32_METADATA_TRANSPORT.md) передаёт raw document/
signature/key ID в отдельном DATA stream. Transport output UNVERIFIED, raw SHA/
signature всё ещё проверяет independent engine. Setup descriptor пока explicit
local input; discovery/authentication/global cross-role ID namespace/persistence/
routing/phase ownership и runtime resource budgets ещё открыты. Current pair
checks не заменяют provider и не дают reboot replay isolation/physical authority.

PR-07n [metadata setup](STM32_METADATA_SETUP.md) закрывает bounded paired role
ID allocation из общего fixed volatile pool и setup/metadata phase ownership.
Это не production discovery/authentication/identity persistence/routing/runtime.
MS/raw mode local fixture не даёт reboot replay isolation; original image windows
продолжаются, independent verifier обязателен. Full MCU RAM/stack/worker/physical
CAN/Flash/installer gates ниже остаются открытыми.

PR-07o [shared channel](STM32_UPDATE_CHANNEL.md) закрывает bounded local TU/MS
multiplexing/outbox/phase filtering, не remote delivery/queue drain/driver или
upper authority. Phase/generation не сериализованы; reboot/authentication/
production epoch/routing/worker/medium/RAM/installer и physical gates ниже open.

PR-07p [bound egress](STM32_UPDATE_EGRESS.md) закрывает local frame custody/
ticket/phase guard gap в software API, не truth/reset/completion provenance
реального adapter. Unknown ownership не force-cleared, late return не upper
resume. Trusted driver/event/identity/routing/runtime/physical/install gates open.

PR-07q [owned event inbox](STM32_DRIVER_EVENTS.md) добавляет serialized FIFO,
bounded current-time service и no-drop backpressure. Это не concurrent/ISR-safe
очередь или реальный driver adapter; synchronization/provenance/reset/reconciliation,
persistent identity/runtime/resources и все hardware/install gates остаются open.

PR-07r [SPSC mailbox](STM32_COMPLETION_MAILBOX.md) проверяет C11 one-producer/
one-consumer ownership и host races. Это не target ISR/RTOS/cache/DMA validation,
не producer-stop/physical drain proof. Actual driver/provenance/reset/identity/
resources/runtime/installer и все hardware gates остаются open.

PR-07s [driver dispatch](STM32_DRIVER_DISPATCH.md) связывает actual frame claim/
publication/fresh check/correlated return; no external copy gap in software API.
Это не real backend/clock/ISR/RTOS/cache/DMA/cancel truth или physical inhibit.
Persistent identity/routing/resources/runtime/install и все hardware gates open.

PR-07t [frame ingress](STM32_FRAME_INGRESS.md) проверяет bounded SPSC RX copies,
original capture age (включая retained external BUSY input) и sticky loss/fault.
Empty не producer-stop/phase-cutover/backend drain; FRAME не upper authority.
Нужен actual stop/ack/barrier с правдивым backend reconciliation, common clock/
loss reporting и route lifetime до runtime adoption. Аппаратных наблюдений нет.

PR-07u [ingress owner](STM32_INGRESS_OWNER.md) закрывает software publication
barrier: exact pause ACK + actual pending/staged/ring/RX drain, epoch permits,
bounded lower-timeout recovery с прежним временем. Это не physical peripheral/
IRQ/DMA/remote stop или trusted route/identity/provider. Новый actual target
stop/reconciliation/clock/resource port и все hardware/install gates ещё открыты.

PR-07v [threaded signed HOST link](STM32_THREADED_LINK.md) проверяет full signed
composition через driver pthread и concurrent RX; TSan8+1 сохраняет прежние
races и добавляет эту цепочку. Это не target scheduler/clock/backend/physical
validation. External BUSY capture намеренно блокирует ACK до reconciliation;
stop/join не освобождает actual claim. Верхние image/crypto/lease/model/close
окна и все аппаратные blockers сохраняются. [Evidence](evidence/build/threaded-link/README.md).

| ID | Что нужно подтвердить/решить | Блокирует |
|---|---|---|
| HW-F407-01 | Надпись STM32F407VGT6 сверена владельцем; raw ID/core/Flash подтверждены. REV_ID0x101f означает5 или6; точная silicon revision/package pin routing ещё неизвестны | Полный board pinout/commissioning; не блокирует HSI/no-motion сборку |
| HW-F407-02 | HSI/divider registers и nominal tick progress подтверждены ee7d179; watchdog/UsageFault reset PASS. Точные частоты HSI/LSI/HSE, схема пинов неизвестны; manual RESET recovery ранее проверен | Дальнейшие timer/CAN frequency measurements; не блокирует P1 software |
| HW-F407-03 | Базовые data/BSS cookies, один CCM word и оба guards PASS; полные SRAM/CCM stress/high-water не измерены | Заявления о полной памяти/stack budget и долгой стабильности; базовый boot больше не блокер |
| HW-F407-04 | Driver12sectors/RDP0/nWRP0xFFF, c09/ee7d179 program/verify и прежний full1MiB snapshot restore/verify/reset подтверждены; power-loss/erase-boundary stress не выполнены | P4 loader layout/write/recovery остаётся отдельным решением, адреса сейчас не резервируются |
| HW-F407-05 | Физические GPIO/CAN, transceiver/pin routing, безопасные уровни/полярности ENABLE/STEP/DIR/шпинделя | Любой physical adapter/drive commissioning; no-I/O adapter не подтверждает physical inhibit |
| HW-USB-01 | Отдельно подтвердить, что устройство 0483:5720 принадлежит именно F407 стенду | Идентификация; VID/PID не доказывает MCU/Flash/прошивку |
| HW-DRIVE-01 | Получение первого NEMA23, точные двигатель/драйвер/питание, STEP/DIR/ENABLE polarity, encoder/alarm, второй привод | Любые испытания приводов/осей |
| HW-SAFETY-01 | Схема независимого NC E-stop, контактор/STO/earthing, mechanical/electrical manual/CNC interlocks с квалифицированной проверкой | Всё опасное движение и commissioning |
| HW-MACHINE-01 | Ходы X/Z, ШВП/передачи, направления/люфт/пределы, X radius/diameter, homing sequence/концевики | Реальные координаты/planner/soft limits; ±1000 мм сейчас только simulation |
| HW-SPINDLE-01 | Привод/торможение шпинделя, прямой энкодер/разрешение/направление/крепление | Feedback, threading/C-axis/индексация/фрезерная приставка |
| HW-HMI-01 | Полные PSRAM runtime/stress, GT911 углы/ориентация/ошибки, tearing и стабильность RGB/сети | Подтверждение работоспособности всего HMI, не блокирует host/docs работу |
| NET-01 | Базовые подключение/DHCP подтверждены на b93cf3b; owner HMI/Wi-Fi PASS после возврата ordinary8f0d899. Остаются неверный пароль, забывание/профили/reconnect/NVS power-cycle stress | Полная аппаратная проверка Wi-Fi lifecycle; новый local info UI её не заменяет |
| CAN-01 | Получение трансиверов, схема линка, уровни/termination/длины, USB/CAN mux и допустимый маршрут на Rev 1.2 | Physical CAN; до этого только host transport |
| CAN-02 | Документированное owner decision по IDs/bitrate/GPIO, production versions/capabilities, timeouts, safe-state/ACK semantics | Реальный driver и протокол; 4-byte v1/500 мс не согласованный wire protocol |
| CAN-WIRE-01 | Production codebook adoption, bounded fragmentation/reassembly/routing/session identity/backpressure/deadline, signed metadata/signature transfer | PR-07j даёт только opt-in complete-message byte draft; это не CAN frame/production version/driver/authentication/runtime |
| CAN-IDENTITY-01 | Reviewed unique boot epoch provider, counter continuity при peer reset, persistence/collision/authentication и compatibility-tag provenance | Runtime negotiation/replay claims; PR-07f volatile pools не доказывают uniqueness после reset или доверие CAN |
| SD-01 | Наличие/тип карты, FAT32, фактическая шина/CS/card detect и извлечение конкретной Rev 1.2; лимиты/имена/ошибки/политика файлов | PR-06; не назначать пины по аналогии |
| OTA-ESP-01 | Layout tv4-16m-ab-v1, fresh16MiB backup/two-NVMe, USB migration/factory fecc05d/XiP/owner HMI PASS. Остались configured install/slot confirm/rollback/Wi-Fi/power-loss и recovery trials | Без key/context установка закрыта; текущую карту не заменять ordinary flash, каждый новый стенд отдельно разрешать |
| OTA-KEY-01 | Owner-provisioned public key/fingerprint, signer custody, алгоритм/encoding/test vectors/key rotation, manifest schema/min versions | Любая доверенная установка; приватных ключей в Git/логах нет |
| OTA-CONTEXT-01 | Утвердить SemVer HMI release вместо текущего git-derived project version; источник версии bootloader/production protocol/реального layout и peer compatibility | PR-04b policy готовит проверку, но не подставляет тестовые версии; installer заблокирован неизвестным context |
| OTA-MOTION-01 | Утвердить production release/component schema и link общего signed bundle с Motion document, trusted installed context, numeric counter/boot/protocol policy и signer build provenance | PR-05c offline cohort review и PR-07g/h draft/coordinator не закрывают production contract/context/protocol mapping; legacy11/release15 раздельны |
| OTA-AES-DMA-01 | 46cf400 выявил DMA failures; PR #75 перенёс LVGL в PSRAM, на38f70b9 три verified HEAD200 с valid/no-failure DMA traces, internal free вырос примерно на105KiB | Наблюдавшийся blocker устранён для этих reset HEAD probes. Нужны full signed SESSION4/load/fault measurements и reviewed floors; HEAD не является production budget |
| OTA-TLS-01 | Правильное время для TLS, CA bundle/host allowlist/redirect bounds GitHub, отсутствие insecure fallback | Реальный HTTPS download; статичные часы HMI не источник доверенного времени |
| OTA-F407-01 | По подтверждённым секторам и RAM budget выбрать staging medium/A-B/recovery; metadata/vector/commit/power-loss план | PR-07i RAM backend не утверждает размер RAM/full-image fit и не даёт durable recovery; кэш ESP32 не заменяет решение |
| OTA-STATE-01 | Fresh подтверждение F407 без движения/оборотов/программы, session lifecycle и повтор после reboot; согласованный safe state | CAN install; симуляция/heartbeat/ACK доставки не подтверждают останов |
| RELEASE-01 | Версии/каналы/tag policy, signing authority, independent CI/source provenance/candidate digest selection и owner gate для exact SHA/digests, доступность environment protections | PR-05d связывает unsigned candidate и signed bytes, но не подтверждает CI identity/автора; factory/A-B/profile blockers не закрыты. Main push не разрешение |
| RELEASE-LICENSE-01 | Владелец выбирает лицензию приложения, review прав на распространяемые assets/библиотеки; LICENSE-NOTICE.md пока оставляет решение открытым | Внешнее распространение app binary/source release; PR-05a публикует только review metadata |
| PERF-01 | Fecc05d:16 samples internal free22851..22907/largest7680/history112, PSRAM free6090732; нужен фазный бюджет. OTA-worker stack/нагрузка/TLS/FPS/touch-to-display/soak/CAN не измерены | History112 не simultaneous OOM/floor; render/event samples0 не FPS/latency proof; заявления о <50 мс/30 FPS/20–30 Гц не подтверждены |
| RECOVERY-01 | Независимая копия ESP32 backup, испытание восстановления; данные/отчёт по восстановлению Blue Pill | Аппаратная migration/bootloader уверенность, не permission на запись |
| RECOVERY-F407-01 | Current snapshot2×1MiB byte/SHA/basic vectors PASS, independent copy на другом физическом NVMe SHA PASS; snapshot restore/verify/reset PASS. Original factory provenance не проверена | Новые записи требуют актуального snapshot/hash и разрешения; backup dumps/их SHA не публиковать |

## Уже установлено / не открывать заново

- ESP32 Rev 1.2, RGB/GT911/CH422G: есть официальный пример LVGL 9 commit
  bc51a84b30c0cea018fee6e0d7427ec10102060e и сверенный прямой адаптер.
  Не использовать legacy LVGL 8 demo как замену LVGL 9.
- Flash в проекте 16 МБ; полный ESP32 backup 16 МиБ с проверенным SHA-256.
  Раннее расхождение 8/16 MB в vendor family не основание вернуть профиль к 8 МБ.
- ESP-IDF 6.1.1 и 5.3.2, LVGL 9.2.2, touch 1.2.1, GT911 1.1.3 и per-SDK locks
  уже собираются. Обновлять API заново без ошибки/цели не нужно.
- Физическое изображение HMI/списка сетей наблюдалось на фото. Формального
  полного теста touch/performance это не закрывает.
- F407 DevEBox получена; владелец подтвердил SWD/ST-Link V2, Cortex-M4r0p1,
  raw ID0x101f6413, Flash1024KiB,3,252V, backup1048576bytes/vector structure PASS.
  Теперь полная надпись сверена владельцем; driver geometry/protection/restore
  проверены. Базовые ee7d179 boot/CCM/guards/watchdog/fault PASS; HSE/частоты,
  RAM stress и GPIO/CAN ещё не подтверждены.
  NEMA23 ещё нет, второго двигателя и трансиверов нет.
- Blue Pill по отчёту владельца доступна через SWD/64 КиБ, backup сохранён;
  это не production motion target. Не подменять ей недостающую F407 geometry.

Статусы закрывать только с источником/датой/версией и пределами результата.
Проведение проверки/прошивка требуют отдельного разрешения владельца;
этот список не разрешает подключаться к оборудованию.

Для F407 scope/split зависимостей и последовательность следующего этапа:
[NEXT_STAGE.md](NEXT_STAGE.md), [STM32_STARTUP.md](STM32_STARTUP.md).
Закрыт blocker SWD access/Flash capacity **по сообщению владельца**, а не все
условия deployment. Physical inhibit, production CAN/bootloader и motion запрещены.

[PR-07a transport](STM32_UPDATE_TRANSPORT.md) проверяет только bounded logical
sender/receiver в памяти. CAN-01/02, OTA-F407-01, OTA-STATE-01, keys/context и
recovery не закрыты. Unique boot/transfer, raw CAN framing/reassembly, независимая
подпись/SHA/compatibility и fresh quiescent proof требуют последующего review.
COMPLETE_UNVERIFIED/CRC/consumer ACK не являются доказательством этих условий.

## После baseline b93cf3b

[Аппаратный отчёт](evidence/hardware/2026-11-18-b93cf3b/README.md): startup,
PSRAM, basic touch/keyboard/password visibility и Wi-Fi/DHCP подтверждены.
Перед runtime HTTPS нужны memory allocation review (largest internal block
12288 bytes при host session 34024), TLS peak и проверенный time provider.
Отдельно разобраться с reset при UART open/close; две выборки не являются soak.

PR-04f добавляет [PSRAM-only allocation/lifetime](OTA_MEMORY.md) и target sizeof
из ELF; это не runtime allocation test и не подтверждение достаточного TLS
reserve. Memory blocker остаётся до аппаратного allocation/TLS peak review.

PSRAM allocation test на 1c38d1e [прошёл три цикла](evidence/hardware/2026-11-18-1c38d1e-psram/README.md).
Runtime allocation UNKNOWN закрыт для этой isolated пробы; production TLS
peak/headroom и time provider, cache-off/full RAM stress и UART cause не закрыты.

## После UTC/TLS b085abb — 19 ноября 2026

[SNTP PASS, TLS allocation FAIL](evidence/hardware/2026-11-19-b085abb-utc-tls/README.md).
Изолированная private2026 UTC policy приняла actual SNTP; production provenance
и time policy остаются открытыми. TLS context не создан (-141 insufficient
memory): certificate/HTTP200/production peak не подтверждены. Следующий gate —
review allocation capabilities/PSRAM/buffers без ослабления CA/time/DMA reserve,
затем новое отдельно разрешённое испытание. На плате ordinary rev12 b085abb.

PR-04k [готовит исправленный TLS diagnostic profile](TLS_MEMORY_REVIEW.md)
(PSRAM-only allocator + SDK certificate dates). Старый local time anchor сам
не включал X.509 expiry check. Runtime OOM/verified HEAD/production memory и
security/performance gates не закрывать по compile/ELF/source review.

## После TLS/PSRAM 1bfc7e2 — 19 ноября 2026

[Verified HEAD HTTP200 прошёл](evidence/hardware/2026-11-19-1bfc7e2-tls-psram/README.md)
в isolated image. Blocker «неизвестно, создастся ли TLS context после PSRAM fix»
закрыт для одной пробы. Production TLS/OTA memory budget остаётся открытым:
internal min3123(window)/2408(history),stack watermark912/6144bytes. Нужен
review оставшихся internal allocations/stack/error paths до body/session.
SNTP не authenticated; production UTC, negative certificate/redirect/CDN/soak/
performance остаются отдельными gates. На плате ordinary1bfc7e2,owner HMI PASS.

PR-04l [сокращает собственный кадр TLS worker и HTTP RX](TLS_HEADROOM.md),
добавляет phase snapshots для следующего review. Реальные headroom minima,
max stack/fragmented-header/failure/soak/CPU остаются открыты до нового отдельно
разрешённого exact-image теста; compile или request size не являются budget.

## После headroom8f0d899 — 19 ноября 2026

[Повторная проба](evidence/hardware/2026-11-19-8f0d899-headroom/README.md):
HEAD200 PASS,internal window minimum5307/history4204,worker watermark1840bytes.
Наблюдаемый запас вырос относительно1bfc7e2, но production budget не утверждён.
Остались worst-case handshake/error/stack/load/soak и controlled fragmented/
oversized headers; total5587>RX2048 не доказывает fragmentation одного поля.
Самый низкий window minimum уже после TLS open; review allocations/admission
нужен до runtime session/body. На плате ordinary8f0d899,owner HMI PASS.

PR-04m [добавляет контракт resource admission](OTA_RESOURCE_GUARD.md), но не
назначает floors из8f0d899 minima. Native guarded body/session budget/stack worst
case/inside-handshake peak и UI/load/failure/soak остаются открыты. Snapshot
не reservation и не защита от transient OOM/overflow до возврата SDK.

PR-05a [автоматизирует review-кандидаты](BUILD_CANDIDATES.md), фиксирует clean
source/build identity, hashes и exact Actions. Это не release/signing authority,
утверждение SemVer/protocol/layout или лицензии и не закрытие hardware gates.

## PR-07b — программная проверка не закрывает аппаратные gates

[Admission](STM32_IMAGE_ADMISSION.md) проверяет draft metadata и поток образа
через отдельную pinned Mbed TLS. Production format/key/context и связь с общим
GitHub manifest ещё требуют review. Счётчики не persistent anti-rollback;
VERIFIED_CANDIDATE не заменяет fresh quiescent/operator/recovery/vector checks.
HW-F407-01–05, OTA-KEY/CONTEXT/F407/STATE и RECOVERY-F407 остаются открытыми.

## PR-07c — lifetime кандидата проверен только программно

[Handoff](STM32_UPDATE_HANDOFF.md) закрывает software binding completed RX →
immutable lease → admission и cleanup/revoke tests. Не закрыты production
metadata/SemVer/key context, boot/transfer uniqueness, backend immutability/
durable recovery и протокол ожидания оператора за пределами RX deadline.
HW-F407-01–05, OTA-F407/STATE/KEY/CONTEXT и RECOVERY-F407 остаются открытыми;
новых разрешений на devices/Flash/motion нет.

## PR-07d — model consistency не hardware contract

[Preflight](STM32_IMAGE_PREFLIGHT.md) проверяет explicit model и её связь с
signed image/session/source. Actual memory_profile/IRQ count/reserved vectors/
VTOR alignment должны пройти board/reference review; generic ARM minimum
не подставляется как настройка F407. HW-F407-01–05, OTA-F407/STATE/KEY/CONTEXT,
recovery и production profile/version/keys остаются открытыми.

## PR-07e — terminal report остаётся историей

[Logical completion/report/ACK](STM32_UPDATE_OUTCOME.md) закрывает software
cleanup/delivery gap, не доверие к CAN. Нужны production boot/session uniqueness/
replay/codec/fragmentation/IDs/bitrate, reviewed actual profile/metadata/keys и
fresh physical/operator/recovery gate. После ожидания нельзя устанавливать по
старому report: saved bytes проверяются заново. Hardware/Flash blockers прежние.
