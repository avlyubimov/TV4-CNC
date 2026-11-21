# Открытые вопросы и блокирующие решения

Обновлено от main `555007d`,21ноября2026. Единственный свод аппаратных фактов —
[HARDWARE_STATUS.md](HARDWARE_STATUS.md). Не заполнять неизвестные поля
типовыми данными семейства, USB VID/PID, старым заказом или host mock.

Ответ владельца20ноября повторил только VGT6; full physical marking/HSE неизвестны.
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

| ID | Что нужно подтвердить/решить | Блокирует |
|---|---|---|
| HW-F407-01 | Owner SWD/core/device ID/Flash1024KiB подтверждены; остаются физическая маркировка/package и согласование reference documentation. Device ID не заменяет package check | Выбор полного device vector/CMSIS/board profile; raw device ID0x101f6413 не даёт адресов bootloader |
| HW-F407-02 | HSE/PLL/реальная частота, schematic/pinout/boot-reset/recovery. SWD работает;3,252V — один owner sample | Physical clock/timer/DMA/CAN. HSE не обязателен для будущего reviewed HSI-only startup, но HSI/частоты пока не измерены |
| HW-F407-03 | Фактические RAM/CCM bounds/availability, stack/heap budget и утверждённый полный vector/memory contract | Настоящий linker/board image; synthetic link probe не подтверждает размер/работу RAM |
| HW-F407-04 | Sector/erase geometry и protection/option byte state до отдельного Flash writer/bootloader решения | PR-07 layout/write/recovery; Flash1024KiB не доказывает sector map или пригодность A/B |
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
| OTA-ESP-01 | Review candidate 16 МиБ layout, recovery size, оба SDK partition parser, backup/USB migration/recovery trials | PR-04; текущий factory 2 МиБ не менять в PR-01 |
| OTA-KEY-01 | Owner-provisioned public key/fingerprint, signer custody, алгоритм/encoding/test vectors/key rotation, manifest schema/min versions | Любая доверенная установка; приватных ключей в Git/логах нет |
| OTA-CONTEXT-01 | Утвердить SemVer HMI release вместо текущего git-derived project version; источник версии bootloader/production protocol/реального layout и peer compatibility | PR-04b policy готовит проверку, но не подставляет тестовые версии; installer заблокирован неизвестным context |
| OTA-MOTION-01 | Утвердить production release/component schema и link общего signed bundle с Motion document, trusted installed context, numeric counter/boot/protocol policy и signer build provenance | PR-05c offline cohort review и PR-07g/h draft/coordinator не закрывают production contract/context/protocol mapping; legacy11/release15 раздельны |
| OTA-TLS-01 | Правильное время для TLS, CA bundle/host allowlist/redirect bounds GitHub, отсутствие insecure fallback | Реальный HTTPS download; статичные часы HMI не источник доверенного времени |
| OTA-F407-01 | По подтверждённым секторам и RAM budget выбрать staging medium/A-B/recovery; metadata/vector/commit/power-loss план | PR-07i RAM backend не утверждает размер RAM/full-image fit и не даёт durable recovery; кэш ESP32 не заменяет решение |
| OTA-STATE-01 | Fresh подтверждение F407 без движения/оборотов/программы, session lifecycle и повтор после reboot; согласованный safe state | CAN install; симуляция/heartbeat/ACK доставки не подтверждают останов |
| RELEASE-01 | Версии/каналы/tag policy, signing authority, independent CI/source provenance/candidate digest selection и owner gate для exact SHA/digests, доступность environment protections | PR-05d связывает unsigned candidate и signed bytes, но не подтверждает CI identity/автора; factory/A-B/profile blockers не закрыты. Main push не разрешение |
| RELEASE-LICENSE-01 | Владелец выбирает лицензию приложения, review прав на распространяемые assets/библиотеки; LICENSE-NOTICE.md пока оставляет решение открытым | Внешнее распространение app binary/source release; PR-05a публикует только review metadata |
| PERF-01 | Реальные LVGL/CPU/heap/PSRAM/stacks/reset/watchdog/queues/reconnect/CAN timeout measurements, touch-to-display и soak methodology | PR-03 acceptance, заявления о <50 мс/30 FPS/20–30 Гц |
| RECOVERY-01 | Независимая копия ESP32 backup, испытание восстановления; данные/отчёт по восстановлению Blue Pill | Аппаратная migration/bootloader уверенность, не permission на запись |
| RECOVERY-F407-01 | Owner backup1048576bytes/vector structure PASS; SHA-256, повторное чтение/независимая копия и restore не сообщены | Recovery confidence перед новой записью; dump не включать в Git/CI |

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
  Суффикс VGT6 сообщён ранее; маркировка/RAM/geometry/HSE/GPIO/CAN не закрыты.
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
