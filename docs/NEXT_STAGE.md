# Следующий этап: release context и STM32 board contract

21 ноября2026. База main `deace1c`, PR #1–41 слиты, main CI5/5 PASS.
ESP32 HMI/Wi-Fi/OTA primitives/candidates и STM32 transport/admission существуют;
не создавать их заново. Данный план не разрешает доступ к устройствам или merge.

## Подтверждённые характеристики

[Owner report](evidence/hardware/stm32f407-owner-swd/README.md): ST-Link V2/SWD,
Cortex-M4r0p1, raw ID0x101f6413, Flash1024KiB, напряжение3,252V,
backup1048576bytes, structural vector check PASS. Агент их не перечитывал.
Это не physical package/clock/RAM/sector/GPIO/CAN/restore validation.

## Слитая программная база

- PR #28: [startup/sections/no-I/O](STM32_STARTUP.md), portable data/BSS init,
  Cortex-M4 reset object, synthetic ARM link tests. Нет real board MEMORY/map,
  полного IRQ table или clock configuration. BOARD_IMAGE=ON fail; no-I/O
  inhibit=false и watchdog unavailable не объявляют физическую readiness.
- PR #29: [logical sender/receiver](STM32_UPDATE_TRANSPORT.md), bounded blocks,
  consumer/ACK, retry/deadlines и RAM fault tests. COMPLETE_UNVERIFIED не
  является crypto/physical safe state; нет CAN wire driver/IDs/bitrate.
- PR #30: [independent admission](STM32_IMAGE_ADMISSION.md), pinned Mbed TLS3.6.7,
  P-256 signature/draft metadata/compatibility/counters/SHA/length/EOF. Нет
  install token, persistent anti-rollback, writer, vector validation или jump.

## Слитая часть PR-07c, PR #31

[Read-only handoff](STM32_UPDATE_HANDOFF.md) связывает completed RX, immutable
source lease и verifier. Boot/transfer/policy/source lifetime и deadline guards
отзывают candidate при изменении контекста. Lease удерживается после verify;
release failures не теряют handle, retries ограничены, quarantine нельзя reset
через clear/reopen. Нет production backend или physical quiescent gate.

Историческая приёмка PR #31: host/LVGL25 suites, sanitizer25, standalone5; 54 signed end-to-end RAM
transfers и10 fault/lifecycle groups; ARM/source/import/frame/static checks,
прежние board/output gates. [Evidence](evidence/build/stm32-update-handoff/README.md).
CI сохраняет все8 ESP SDK profiles. Аппаратных операций или самостоятельного
merge нет. Source generation не является блокировкой; это обязанность backend.

## Слитая часть PR-07d, PR #32

[Image preflight](STM32_IMAGE_PREFLIGHT.md): bounded read-only model checker,
явные alignment/vector count/reserved IRQ/RAM/load bounds, проверка того же
held image после signature/SHA. Unknown profile отклоняется; PROFILE_MODEL_MATCH
не подтверждает hardware/bootability. Actual F407 memory/erase map не выбирается.
Исторические Host26/sanitizers26/standalone6 и ARM/static/source/image/output gates —
[evidence](evidence/build/stm32-image-preflight/README.md). Hardware gates ниже прежние.

## Слитая часть PR-07e, PR #33

[Completion/outcome](STM32_UPDATE_OUTCOME.md) выполняет существующие проверки,
закрывает lease и возвращает historical result через bounded report/ACK.
Потери/duplicates/conflict/clock/reset и cleanup/quarantine проверены в RAM;
ожидание оператора не держит lease. Peer report не authentication/install token.
Host27/sanitizers27/standalone7 и ARM/static/source/image/output —
[evidence](evidence/build/stm32-update-outcome/README.md). Actual board/Flash gates прежние.

## Слитая часть PR-05b, PR #34

[Release policy review](RELEASE_POLICY_REVIEW.md): исправлены malformed Unicode
escapes в native gate и noncanonical Base64 offline; общий signed corpus115,
SemVer2809 comparisons/Unicode65536. Host29/sanitizers29, actual analyzer3units,
local ESP-IDF6.1.1 headless/rev12 PASS; прежние STM32 gates и SDK matrix8 сохранены.
Это проверка HMI contract, не production Motion schema, key/version provisioning
или разрешение установки. [Evidence](evidence/build/release-policy-review/README.md).

## Слитая часть PR-08a, PR #35

[Local firmware info UI](FIRMWARE_INFO_UI.md): «Параметры» → Wi-Fi / ПО,
build/SDK/profile из local descriptor, unknown STM32/CAN, отсутствие настроенного
updater. Panels создаются по требованию и освобождаются при выходе. Host30/
sanitizers30, две render formats/64 navigation cycles и local SDK6.1.1 headless/
rev12 PASS; [evidence](evidence/build/firmware-info/README.md). Не создаёт
runtime HTTPS/verifier/session/install caller или новое устройство доверия.

## Слитая часть PR-07f, PR #36

[Fresh logical negotiation](STM32_UPDATE_NEGOTIATION.md): boot-pair pool,
REQUEST/OFFER/ACCEPT/CONFIRM, monotonic counters/high-water, fixed deadline,
bounded replies и однократный take для DATA.1024 sequential sessions и6 новых
signed full chains/3 negative bindings, host31/sanitizers31/standalone8,
ARM/static/source/image/output PASS — [evidence](evidence/build/stm32-update-negotiation/README.md).
Не создаёт runtime caller, production wire/capabilities/entropy provider,
authentication или install/physical authority. DATA deadlines прежние.

## Слитая часть PR-07g, PR #37

[Independent release binding](STM32_RELEASE_BINDING.md): отдельный signed15-field
Motion draft, release/component SemVer/channel/source selection и прежние
target/counter/full image checks через один existing crypto engine. Host33/
sanitizers33/standalone10,2745 SemVer comparisons, ARM/static/source/image/output
PASS — [evidence](evidence/build/stm32-release-binding/README.md). Workspace17664
прежний, candidate224/policy288 на host. Legacy strict11-field API сохраняется.
Это исторический standalone этап PR #37. Production bundle/CAN ABI и
доверенный installed context/installer остаются открытыми.

## Слитая часть PR-07h, PR #38

[Release handoff](STM32_RELEASE_HANDOFF.md) соединяет signed15-field API с
существующими live lease/owner/source/RX/deadline/vector guards и completion/
outcome. Raw metadata SHA связан с negotiation; новый локальный режим явный,
legacy strict11 path сохранён. Host34/sanitizers34/standalone11,6 новых full
chains, ARM/static7/source/board/output PASS — [evidence](evidence/build/stm32-release-handoff/README.md).
Нет runtime caller/production context или install/physical authority.

## Слитая часть PR-07i, PR #39

[RAM staging source](STM32_STAGING_SOURCE.md) добавляет portable bounded backend
для pending DATA и read lease. Init не reset, discard сохраняет high-water и
monotonic counters; writes/discard запрещены while leased. Host36/sanitizers36/
standalone13,45 variants/1536 cycles/8 signed actual backend chains, ARM/static8/
source/board/output PASS — [evidence](evidence/build/stm32-staging-source/README.md).
Это volatile RAM, без default capacity/адресов или runtime caller. Full image
помещается только в явно заданный buffer; production RAM/staging medium/durable
recovery ещё не утверждены.

## Слитая часть PR-05c, PR #40

[Offline cohort review](RELEASE_COHORT_REVIEW.md) проверяет существующие signed
HMI и optional Motion документы/полные образы как один expected release/source/
channel. App descriptor и prospective HMI→Motion peer range проверены; HMI-only
не создаёт Motion entry. Host38/sanitizers38/standalone13, CLI3/cohort9 methods,
analyzer8 embedded+1 host и ARM/source/board/output/scope gates PASS —
[evidence](evidence/build/release-cohort-review/README.md). Report metadata-only;
production manifest/context/protocol mapping/runtime/install gates не закрыты.

## Слитая часть PR-05d, PR #41

[Build binding audit](RELEASE_BUILD_BINDING.md) соединяет external-pinned unsigned
candidate/full HMI bytes/ELF digest с existing signed cohort. Factory/A-B layout
и headless/bench/ordinary profile mismatch явно блокируют, Motion build evidence
не создаётся. Captured metadata не перечитывается; новый audit всегда blocked,
exit3 и все authority gates false. Host39/sanitizers39/standalone13, новая suite
13methods, прежние ARM gates и CI SDK matrix8 с candidate integrity pin checks.
Unsigned metadata pin не authentication/CI attestation; actual SemVer/keys/layout/
provenance/runtime/installer не настроены. Hardware действий/новых измерений нет.

## Текущая ветка PR-07j

[Update byte codec draft](STM32_UPDATE_WIRE.md): отдельная opt-in C11 library,
8 existing kinds/exact bounded LE bytes/explicit enum codebook, no padding/tail.
Shared shape validator не state/auth gate. Host42/sanitizers42/standalone16,
golden12/65536 outcome pairs/200000 mutations и8 signed RAM chains через codec;
analyzer12+1/budget guard6/ARM/source/board/output/scope PASS. ARM codec1219text,
data/bss0/frame32 отдельно от existing4 archives43362text (+18). Полный stack/
RAM/CPU budget не подтверждён. Нет production CAN version/IDs/bitrate/driver/
fragmentation/metadata transfer/runtime/identity/install или hardware действий.

## Что делать дальше программно

1. HMI policy проверена PR-05b; PR-07g проверяет independent release/component/
   target/profile/image draft; PR-05c добавляет offline signed cohort link,
   PR-05d проверяет его связь с explicit-pinned unsigned build claims/bytes и
   показывает blockers. Дальше review production bundle↔component contract и
   schema, доверенного local context и production immutable backend. PR-07h
   закрывает software live coordinator adapter; PR-07i добавляет RAM backend.
   Production staging medium/RAM budget/durable provider и runtime ещё требуют
   review; full image нельзя считать помещающимся в F407 RAM по Flash capacity.
   Не принимать standalone historical candidate вместо lease/vector/deadline
   checks, не менять RX lifetime. Draft counters не переводить в SemVer;
   actual local versions/key provisioning/signing authority — owner gate.
   Candidate digest брать из independently reviewed run, не из самого unknown
   artifact; self-generated CI integrity pin не доказывает происхождение. Для
   production нужны approved source/build/signer custody и actual target layout,
   не подавление factory/A-B blocker или замена current PROJECT_VER.
2. PR-07f проверяет volatile logical negotiation, но production boot epoch
   provider/persistence/collision/authentication, capabilities/version и raw CAN
   fragmentation/IDs/bitrate требуют review. PR-07j закрывает только experimental
   complete-message codec, не production wire adoption. Дальше bounded
   fragment/reassembly с routing/session identity, cancellation/deadline и
   backpressure, передача signed metadata/signature и production codebook review.
   Decode OK не обновляет context или timeouts; owner poll остаётся обязательным.
   Logical terminal outcome уже есть; он не authenticated и
   не переносит candidate через ожидание оператора. Для следующей операции
   проверять saved bytes полностью и подтверждать новый session/context.
   Текущий DATA RX deadline не продлевать молча.
3. Разделить software candidate и будущую установку: fresh session-bound
   отсутствие движения/шпинделя/программы, operator exact-image approval,
   reviewed recovery/vector/erase gate. Не вводить permissive boolean stub,
   который открывает Flash writer или изображает physical inhibit.
4. Для активной проверки обновлений из нового UI нужны approved local key/
   version/layout/time context, runtime resource/load/fault/soak budget и
   отдельный bounded worker/snapshot contract. Не превращать local build
   descriptor в SemVer/release trust и не показывать фиктивную историю/progress.

## Что блокирует настоящий board image

1. Проверить физическую маркировку/package/board revision, RAM/CCM bounds и
   полный device IRQ/vector contract по согласованным reference sources.
   Device ID/suffix и1MiB capacity не заменяют эту проверку.
   Новый ответ владельца20ноября повторяет только VGT6; HSE/полной маркировки нет.
2. Утвердить clock/reset policy. Возможен отдельный минимальный HSI-only путь;
   неизвестный HSE блокирует HSE/PLL, фактические частоты/пригодность CAN ещё
   нужно измерить. Текущий код clock не устанавливает.
3. После memory/vector/stack review подготовить standalone no-I/O board profile
   без пинов приводов/bootloader reserve. Synthetic fixture addresses/stack/
   core vectors никогда не использовать как реальную карту F407.
4. До любой записи отдельно уточнить backup SHA/независимую копию/restore,
   затем согласовать exact image и SWD/write/reset/capture. Нынешнее задание
   этих операций не разрешает. GPIO/CAN/уровни ENABLE — отдельное commissioning.

## Flash и будущий OTA

[Архитектура](OTA_ARCHITECTURE.md): signed GitHub component → ESP32 bounded
cache/transport → независимый F407 verifier/recovery. До reviewed erase sectors,
protection/RAM/vector/commit/power-loss решения нельзя выбирать A/B/staging,
app offset или metadata address. **Flash1024KiB не резервирует ни одного адреса.**

Сохранённый valid flag/receipt не заменяет signature/hash/vector check перед
commit/jump. Reboot/reconnect/update не разрешают движение. Fresh physical
quiescent/operator/recovery checks обязательны, SIMULATION/no-I/O/ACK их не
подменяют. Аппаратная NC E-stop/STO цепь независима от CAN и обоих MCU.
