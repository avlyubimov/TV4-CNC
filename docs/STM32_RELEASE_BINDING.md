# PR-07g — независимая проверка сведений релиза STM32

21 ноября 2026. База main `3381236`, PR #1–36 слиты. [План](plans/PR07G_RELEASE_BINDING.md).
Это следующий software review после [HMI policy](RELEASE_POLICY_REVIEW.md)
и [logical negotiation](STM32_UPDATE_NEGOTIATION.md). Подписанный экспериментальный
Motion document связывает release/component/target/profile/image одним raw-byte
сообщением. Это **не утверждённый production manifest, CAN ABI или installer**.

## Два явных API и domain separation

- `tv4_boot_admit` по-прежнему принимает только прежние11 полей и schema
  `tv4-motion-admission-draft-1`. Legacy handoff/completion вызывают его.
- Новый [tv4_boot_admit_release](../firmware/stm32-bootloader/include/release_admission.h)
  принимает только15 полей и schema `tv4-motion-release-draft-1`. Нет fallback
  между схемами, автоматического определения API по скачанному JSON или
  преобразования HMI success/receipt в доверие загрузчика.
- В обоих случаях один independent Mbed TLS3.6.7 engine проверяет P-256 signature
  **до parser**, затем policy, полный image SHA/length/EOF и cleanup. Новых
  crypto/dependency/allocator instances, heap или скрытого второго image pass нет.

В PR #37 это standalone candidate API. Последующий PR-07h
[release handoff](STM32_RELEASE_HANDOFF.md) добавляет explicit live lease/
negotiation/completion path через те же lifetime/generation/deadline/vector
правила. Runtime caller и production backend пока отсутствуют. Historical candidate не переносится
через operator wait, reboot, смену policy/source или последующую установку.

## Строгий подписанный документ

Как в прежнем [draft](STM32_IMAGE_ADMISSION.md), raw bytes ограничены1024,
порядок полей произвольный, JSON whitespace допускается. Ровно15 scalar fields:

| Поля | Проверка |
|---|---|
| schema | Только `tv4-motion-release-draft-1` |
| component | Только `motion` |
| hardware_id / hardware_revision / memory_profile |1..31 ASCII `[A-Za-z0-9._-]`, exact local target/model |
| image_counter / minimum_bootloader_counter / protocol_min / protocol_max / image_size / sha256 | Все прежние counter/range/size/lowercase SHA правила сохранены |
| release_version | Bounded SemVer, точное совпадение с local selected release |
| component_version | Bounded SemVer, строго выше local installed и не ниже local minimum |
| channel | Только stable/prerelease, точное совпадение с local channel |
| source_commit |40 lowercase hex, не all-zero; точное совпадение с local selected source |

Missing/unknown/duplicate/nested fields, escapes/Unicode/NUL, sign/leading-zero/
fraction/exponent/uint32 overflow, trailing/truncated bytes отклоняются после
signature check. Подписывается точный полученный документ, не повторная JSON
сериализация. Existing allowlist до4 local keys/canonical DER SPKI/P-256 signature
и её8..72-byte bounds прежние; downloaded document не добавляет доверенный ключ.

`metadata_sha256` в новом candidate относится **к этому новому raw документу**,
не к старому draft, GitHub API response или общему HMI manifest. Одинаковые image
bytes в разных документах не делают документы взаимозаменяемыми.

Source commit — аутентифицированное заявление доверенного signer. Это не
reproducible-build attestation и не доказательство происхождения image из Git.
Build review candidate→signer→exact bytes и production bundle linkage ещё требуют
отдельного pipeline/owner review. Новый формат нельзя выдавать за уже поддержанный
общий GitHub manifest или добавлять фиктивный Motion artifact в HMI/release.

## Явный локальный контекст и версии

Caller передаёт весь прежний target/key/counter/protocol/size/timeout context,
а также installed/minimum component SemVer, выбранные release/source/channel.
Empty, unterminated, malformed или неизвестный context отклоняется. Нет actual
versions, default keys, test-to-production conversion, provisioning или context
discovery. SDK display/git version HMI не является trusted installed version.

Release и component version независимы. Component должен быть новым относительно
**installed component**, а не bundle version. Local minimum — floor для нового
component. Stable требует release и component без prerelease; prerelease channel
требует prerelease release, но может содержать stable component. Channel mismatch
не включает fallback. Selected release/source должны совпасть **точно**.

SemVer precedence основан на [SemVer2.0.0, §9–11](https://semver.org/spec/v2.0.0.html).
Subset проекта:1..31 ASCII bytes, core uint32, без build metadata. Numeric
prerelease сравнивается по количеству цифр/цифрам, без uint32 truncation;
numeric identifier не имеет leading zero. Это не вся SemVer grammar и не
назначение первой production версии. Numeric image/boot/protocol counters
остаются отдельными проверками; SemVer никогда не переводится в них.

## Память, ownership и результаты

Один caller-owned workspace **17664bytes**, arena16384/metadata1024/chunk256 —
прежние. Новый historical candidate224bytes, local policy288bytes на host.
Дополнительных global state/heap/dependency или нового рабочего буфера нет.
Конкретные compiler object/frame результаты — [evidence](evidence/build/stm32-release-binding/README.md),
не board RAM/CCM/stack/CPU/latency измерения. Shared engine frame984bytes прежний;
новый wrapper592bytes, legacy dispatch shim16bytes. Их нельзя принимать за полный
call-stack budget или доказательство прежнего total stack/CPU; измерения ещё нужны.

Новый wrapper snapshot всей local release policy, существующий engine snapshot
request/raw document/signature/selected public key **до callbacks**. Один owner
общего upstream allocator; legacy↔new и new↔new reentry получают BUSY, не трогая
active storage/output. Это не thread/ISR lock. Те же bounded synchronous callbacks,
clock/cancel/final guard/arena detach и zeroize; errors очищают весь новый output,
illegal aliases/BUSY оставляют caller storage untouched. Capacity/context C
preconditions и отсутствие adversarial concurrent writers обязательны.

Candidate публикуется только после signature/policy/full image/hash/exact EOF/
cleanup. Signature без корректного образа, новая SemVer со старым counter или
новый counter со старой SemVer не проходят. Коды прежнего enum: POLICY для invalid
local context, METADATA для malformed signed document, COMPATIBILITY для selection/
channel/target mismatch, ROLLBACK для version/counter floor, остальные crypto/
source/length/hash/clock/cancel/resource результаты прежние.

## Граница следующего этапа

Нужны owner-reviewed production schema, версия bootloader/protocol/counters,
provisioned keys/signer authority, доверенный installed context, link общего
signed release с component document, durable anti-rollback/recovery и live
coordinator adapter. Нет raw CAN codec/IDs/bitrate, physical safe-state detector,
Flash map/writer/COMMIT/jump или ARM/START. Before install повторно проверять
saved bytes, vectors и fresh physical/operator/recovery условия.

Owner SWD/ST-Link V2/Cortex-M4r0p1/rawID0x101f6413/Flash1024KiB/3,252V/
backup1048576bytes/vector structure PASS сохранены без новых чтений. Full marking,
HSE/RAM/CCM/IRQ/erase/protection/GPIO/CAN/restore неизвестны. На ESP32 ordinary
8f0d899. Нет device/SWD/UART/reset/Flash, ключей/релизов или самостоятельного merge.
Откат — revert PR; migration и действия с устройствами не нужны.
