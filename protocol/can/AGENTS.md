# CAN — правила протокола

Наследовать корневой AGENTS.md. Прочитать README.md, docs/ARCHITECTURE.md,
docs/OTA_ARCHITECTURE.md и docs/OPEN_QUESTIONS.md.

- CAN не safety-rated канал и не замена аппаратному E-stop/блокировкам.
- Текущий 4-байтный heartbeat v1 и 500 мс — инертный host/bench envelope;
  это не согласованная версия production protocol и не разрешение движения.
- Реальные GPIO, arbitration ID, bitrate, termination, transceiver и USB/CAN
  mux не выбирать без отдельного документированного решения владельца.
- Будущий протокол описывает capabilities/version negotiation, heartbeat,
  boot/session identity, machine state, coordinate frame/units, RPM/feed,
  fault codes, intent/ACK/reject, sequence, bounded timeouts и FW transport.
- Приём валидирует длины, диапазоны, версии, reserved поля и порядок.
  Повторный пакет не исполняет действие дважды; пропуски/stale/duplicates/
  wraparound/disconnect/reboot проверяются host/fault-injection тестами.
- ACK доставки не равен готовности/разрешению движения. Reconnect не ARM/START.
- Update-сеанс отделён от motion; chunks имеют sequence/offset/length/integrity,
  ограниченные ACK/retry/reassembly и полный SHA/подпись на стороне загрузчика.
  При reboot/потере сеанса требуется новое согласование безопасного состояния.
- Никаких CAN ожиданий в UI callbacks и никакого STEP из CAN ISR.
  До появления трансиверов проверять всё с memory/mock transport.
- PR-01 описывает требования, но не выделяет IDs и не меняет wire format.

## PR-07a: программная граница

Читать docs/STM32_UPDATE_TRANSPORT.md. update_session — logical structs, не
CAN frames или утверждённая production version. Fixed block ceiling256 не
задаёт Classic CAN fragmentation. Локальный begin/consumer ACK и opaque hash
не заменяют независимые signature/SHA/compatibility/quiescent/recovery gates.
У COMPLETE_UNVERIFIED нет install/commit/jump. Новых runtime callers, writer,
пинов/IDs/bitrate или physical inhibit в этом этапе нет. Unique boot/transfer
negotiation в PR-07a внешнее; PR-07f добавляет logical pool/handshake ниже.
Boot epoch uniqueness после reset остаётся внешним обязательством; identity не переиспользовать.
После reset требуется новый сеанс; volatile mock не доказывает durable recovery.
Host tests используют только RAM/OpenSSL oracle; embedded crypto dependency
не добавлять под видом CRC или вызова ESP32 verifier из F407 bootloader.

## PR-07e — terminal outcome channel

Читать docs/STM32_UPDATE_OUTCOME.md. Outcome/ACK — bounded historical peer
reports, НЕ authentication/install/local verified/physical ready proof. Не
назначать wire/CAN IDs/bitrate/codec. Bind image и raw metadata SHA; borrowed
completed DATA TX до terminal state, exact duplicate без redelivery/deadline
extension, mismatch/conflict/clock/retry/reset fail closed. Ни одно сообщение
не запускает/возобновляет motion. DATA deadlines не менять. После operator wait
нужны fresh negotiation и независимые полные checks до любого будущего install.

## PR-07f — volatile negotiation model

Читать docs/STM32_UPDATE_NEGOTIATION.md. Только logical typed C REQUEST/OFFER/
ACCEPT/CONFIRM, fixed boot pair/compatibility tag/generation и monotonic pool
counters. No wire IDs/codec/version/bitrate, runtime caller или identity provider.
Single-use take не является auth/install token; copied descriptor не оживляет
сеанс. Не clear/reinit live state или reset counters ради retries. Boot epochs/
persistence/collision policy/authentication ещё не реализованы. Полностью matching
forged reply может пройти; не объявлять защитой от attacker или physical safe state.
DATA lifetime и independent crypto/model/lease gates сохраняются.

## PR-07j — opt-in byte codec draft

Читать docs/STM32_UPDATE_WIRE.md. Отдельная tv4_update_wire library, не runtime
CAN driver или production wire version. Existing typed APIs/core policies прежние;
shared tv4_neg_message_valid проверяет только shape. Explicit header/tokens/LE,
exact bounded message, no C layout/padding/tail, decode failure zero output.
Input/output spans valid/disjoint/exclusive, no in-place/ISR/threads. CRC не
вычислять повторно в codec: unchanged DATA RX проверяет его до progress/consume.
Opaque tags/hashes/outcomes/codec OK не authentication/install/physical state.
Malformed traffic не отменяет обязанность owner poll clocks; deadlines прежние.
Future fragmentation/reassembly/metadata-signature transfer/identity/authentication
и production adoption ещё открыты. Не назначать IDs/bitrate/pins/driver или
подключать в MCU app/CAN/HMI. Byte fixtures не настоящие CAN frames/boot epochs.
Host/ARM budgets и negative cases обязательны; только text evidence, no binaries.

## PR-07k — opt-in bounded fragments

Читать docs/STM32_UPDATE_FRAGMENTS.md. Только local envelope v1/route/generation,
no serialized identity/authentication/reboot anti-replay claim. Fixed286byte
buffers, no live reinit/copy/move/counter reset; explicit config/deadlines/quota.
Peek не progress, commit только exact locally queued frame, не remote ACK.
RX no silent restart; duplicates/rejects/BUSY не продлевают deadlines; READY
unchecked/one codec take не dispatch/CRC/crypto/install/physical authority.
Poll clocks без traffic, sticky context/clock/exhaustion нельзя clear. Все
existing logical/independent verifier/lease/model/deadline gates прежние. Не
подключать в app/ISR/driver, не выбирать production CAN IDs/bitrate/pins или
Flash map. Host/ARM/archive/import/frame/negative guards и text evidence обязательны.

## PR-07l — host scheduling fixture

Читать docs/STM32_UPDATE_TIMING.md. tests/fragment_test_link только HOST: bounded
single frame slot per direction/explicit delays/shared caller clock, без sleep,
heap или driver. Не переносить target в ARM/runtime. Upper APIs/limits/deadlines
не менять, recovery не reinit/reset counters/time. New local generation не
wire authentication/reboot replay isolation; негативный test сохраняет этот
known blocker. CPU/bitrate/physical scheduling не измерены; synthetic timings
не defaults. Source hashes/text evidence/host+sanitizers/scope gates обязательны.

## PR-07m — metadata container/receiver

Читать docs/STM32_METADATA_TRANSPORT.md. Отдельная opt-in library, no runtime.
Distinct globally fresh metadata DATA ID и local image binding, no same-ID role
mixing. High-water проверяет только собственный lifetime; shared namespace/
persistence/authenticated descriptor setup/routing ещё external. Never lend its
stream as image RX. ACK after copy, snapshot TAKEN_UNVERIFIED one time/owned;
codec/descriptor SHA fields не integrity/authentication. Original independent
raw digest/signature/image/lease/deadline gates обязательны. Image RX не reset
после metadata. Discard не сбрасывает ID/time; context/clock sticky. Input/output
valid/disjoint/exclusive, no in-place/threads/DMA aliases. Limits1024/72/31 согласованы
с Motion verifier, не HMI16KiB. Separate ARM guard/import/frame/no globals и
host+sanitizers/role/fault tests обязательны; crypto/IDs/Flash/pins не добавлять.

## PR-07n — bounded metadata setup

Читать docs/STM32_METADATA_SETUP.md. Paired metadata image+1 ID из original
CLOSED image pool, single allocation/no wrap/no rollback; retain CLOSED fields
до clear. Setup owns pool до actual completed metadata TX / TAKEN RX close.
Borrow empty original image endpoints; poll their unchanged time/descriptor/
limits/context, не reinit или продлевать окна. MS172 separate codec; explicit
raw fragment mode не отключает legacy typed TU decoder и не даёт dispatch/auth.
Host test helper applies MS decode before upper receive. One take UNVERIFIED,
real independent signature/raw digest/image/lease/model/close остаются нужны.
Responder bounded CONFIRM retry после take; duplicates не reset clocks/sends.
No new crypto owner/heap/runtime/IDs/bitrate/Flash/pins/devices. Host-only fixtures
не ARM. Scope/import/frame/global/negative guards, text evidence обязательны.
Production authenticated identity/routing/persistence/staging/resources/installer
и actual board hardware gates остаются открытыми. No self-merge.

## PR-07o — shared channel

Читать docs/STM32_UPDATE_CHANNEL.md. Opt-in local queue/phase filter, no runtime.
One owned frame outbox/retained counters; last queued frame deadline even lower
TX IDLE. RX single strict TU/MS codec/no fallback/owned UNVERIFIED take. Wrong
phase/role ID no dispatch. Advance only locally idle; not upper protocol success
or external driver queue drain/auth/physical/install proof. Original upper
coordinators/CRC/independent crypto/lease/model/deadline/clean close обязательны.
Host helper explicit phases after real states + fixture queue drain; no tests
in ARM. No counter/time/reinit reset for recovery. No new bytes/IDs/driver/heap/
crypto owner/pins/Flash/device actions. Existing budgets unchanged; separate ARM
imports/exports/frame/no globals guard + negatives, text evidence обязательны.
