# Следующий этап: завершение проверки и STM32 board contract

20 ноября 2026. База main `e0bd84e`, PR #1–32 слиты, пять workflow PASS.
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

## Текущая ветка PR-07e

[Completion/outcome](STM32_UPDATE_OUTCOME.md) выполняет существующие проверки,
закрывает lease и возвращает historical result через bounded report/ACK.
Потери/duplicates/conflict/clock/reset и cleanup/quarantine проверены в RAM;
ожидание оператора не держит lease. Peer report не authentication/install token.
Host27/sanitizers27/standalone7 и ARM/static/source/image/output —
[evidence](evidence/build/stm32-update-outcome/README.md). Actual board/Flash gates прежние.

## Что делать дальше программно

1. Review production release metadata/SemVer/channel/component binding и
   локального version/key context. Draft/counters/test IDs не переносить в
   реальный release manifest; provisioning/signing authority — отдельный owner gate.
2. Review production negotiation/boot identity/replay и raw CAN fragmentation/
   codec/IDs/bitrate. Logical terminal outcome уже есть; он не authenticated и
   не переносит candidate через ожидание оператора. Для следующей операции
   проверять saved bytes полностью и подтверждать новый session/context.
   Текущий DATA RX deadline не продлевать молча.
3. Разделить software candidate и будущую установку: fresh session-bound
   отсутствие движения/шпинделя/программы, operator exact-image approval,
   reviewed recovery/vector/erase gate. Не вводить permissive boolean stub,
   который открывает Flash writer или изображает physical inhibit.

## Что блокирует настоящий board image

1. Проверить физическую маркировку/package/board revision, RAM/CCM bounds и
   полный device IRQ/vector contract по согласованным reference sources.
   Device ID/suffix и1MiB capacity не заменяют эту проверку.
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
