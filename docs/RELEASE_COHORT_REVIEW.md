# PR-05c — offline-проверка состава релиза

21 ноября2026. База main `2c06cdc`, PR #1–39 слиты, main CI5/5 PASS.
[План](plans/PR05C_RELEASE_COHORT.md), [evidence](evidence/build/release-cohort-review/README.md).

## Результат и граница

[review_release_cohort.py](../tools/review_release_cohort.py) проверяет набор
локальных подписанных артефактов HMI и, опционально, Motion. Отдельные verifier'ы
сохраняются: HMI — существующий offline OpenSSL policy/stream checker,
Motion — существующий independent Mbed TLS release verifier через новый
[host frontend](../firmware/stm32-bootloader/tools/motion_review.c).

Проверяются подписи точных raw metadata bytes, весь image SHA/length, local
context и общий expected release/source/channel. Для HMI дополнительно работает
прежний полный ESP app inspector: checksum/appended SHA, project/SDK/descriptor,
DIO80/16MB и signed component version. Для пары проверяются согласованность
локальной текущей Motion version и signed HMI peer range для новой Motion version.
Release version и версии компонентов могут различаться, stable/prerelease
остаются явным выбором. HMI-only не создаёт фиктивного Motion component.

Это **review-only tooling**, не новый production bundle/OTA manifest/CAN ABI.
HMI manifest по-прежнему содержит ровно один HMI component; Motion остаётся
отдельным экспериментальным15-field документом. Итоговый неподписанный JSON
`tv4-release-cohort-review-draft-1` — отчёт для человека, не installer input.
Нет firmware/runtime caller, network/download/signing/publish/install actions.

## Порядок проверки и владение bytes

1. Operator явно задаёт expected release SemVer, full nonzero lowercase40hex
   source SHA, channel и SDK. Expected source брать из отдельного review/CI,
   не копировать из непроверенного manifest. Это всё ещё не build provenance.
2. Local context читается строго: schema/keys/types/bounds, без duplicates/NUL/
   invalid UTF-8/nonfinite numbers, unknown/zero/default версий и counters.
   Public keys задаются локальными файлами; downloaded document не регистрирует
   ключ, executable, command или путь к локальному файлу.
3. Все bounded regular artifact files копируются в owned private snapshots до
   crypto. Symlink/FIFO/nonregular/empty/oversized inputs отклоняются. Нужна
   quiescent owned filesystem; hostile local process/filesystem не sandboxed.
4. HMI `verify_with_metadata` возвращает signed fields из того же raw snapshot,
   который прошёл signature policy и image stream. Legacy `verify`/CLI report
   сохранены. Повторного открытия manifest для binding fields нет.
5. Motion frontend вызывает `tv4_boot_admit_release` с explicit local policy,
   streaming256-byte FILE reader, monotonic host clock и прежним bounded
   workspace. На failure candidate не публикуется; workspace cleanup проверен.
   Это standalone historical candidate: нет live source/vector/bootability gate.
6. Сверяются release/source/channel, app descriptor и prospective HMI→Motion
   peer range. Только после всех проверок печатается отчёт. При ошибке stdout
   не содержит частичного успешного cohort report.

Trusted local host executable и OpenSSL входят в доверенную toolchain. Путь к
Motion frontend задаёт operator, не JSON. Его SHA в отчёте — traceability,
не code attestation. Не запускать downloaded/unreviewed program как verifier.
Public key fingerprints фиксируют выбранные local inputs, не provisioning.
Новые tests используют **два разных ephemeral P-256 ключа**, private keys только
в памяти signer processes; новых private key files или signing service нет.

## Local context и команды

Строгий draft context имеет top-level `schema` =
`tv4-cohort-local-context-draft-1`, обязательный `hmi` и `motion` только при
явном выборе Motion inputs. Ни одного production context/default файла нет.

| Объект | Обязательные поля |
|---|---|
| hmi | key_id, installed_version, bootloader_version, protocol_version, motion_version (string/null), layout_id |
| motion | key_id, hardware_id, hardware_revision, memory_profile, installed_component_version, minimum_component_version, installed_image_counter, minimum_image_counter, bootloader_counter, protocol, max_image_bytes, timeout_ms |

Motion IDs1..31 ASCII, versions1..31 SemVer subset, counters positive uint32;
max image≤16MiB и timeout≤10000ms — **host review ceilings**, не actual F407
capacity/timing policy. HMI SDK только5.3.2/6.1.1, image≤2MiB — консервативный
предел существующего app inspector. Нет вывода RAM/Flash layout из этих ceilings.

Собрать из repo root с exact pinned crypto archive/source:

```bash
cmake -S . -B build-host -G Ninja -DLVGL_DIR="$TV4_LVGL_SOURCE"   -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE"
cmake --build build-host -j4
python3 tools/review_release_cohort.py   --context "$TV4_REVIEWED_LOCAL_CONTEXT"   --release-version "$TV4_REVIEWED_RELEASE" --source-commit "$TV4_REVIEWED_SOURCE_SHA"   --channel "$TV4_REVIEWED_CHANNEL" --hmi-sdk "$TV4_REVIEWED_SDK"   --hmi-manifest "$TV4_HMI_MANIFEST" --hmi-signature "$TV4_HMI_ENVELOPE"   --hmi-public-key "$TV4_HMI_PUBLIC_DER" --hmi-image "$TV4_HMI_IMAGE"
```

Для paired review добавить все5 options: `--motion-metadata`,
`--motion-signature` (raw DER), `--motion-public-key` (canonical91-byte P-256 SPKI),
`--motion-image`, `--motion-driver`. Последний — явно выбранный trusted
`build-host/firmware/stm32-bootloader/boot_motion_review`. Partial set отвергается;
для HMI-only `motion` context отсутствует. Вывод только stdout, файлов отчёта
инструмент не перезаписывает. Никаких image/keys/versions в примере не назначено.

Direct Motion frontend (host-only) принимает ровно19 positional arguments:
metadata, signature, public key, image, key ID, hardware ID/revision/profile,
installed/minimum image counter, bootloader counter, protocol counter,
max image bytes, timeout ms, installed/minimum component version,
selected release, selected source, channel. Все обязательны; no stdin/defaults.
Он отклоняет final symlinks/nonregular files, проверяет file identity/size/times
до/после чтения. В paired tool строгий regular/symlink-parent check и snapshots
выполняются дополнительно до его вызова.

## Что отчёт не подтверждает

`gates` всегда закрыты: publish/install/license/exact owner approval,
build provenance/actual installed context/layout/key provisioning/protocol
mapping/Motion vectors/live source/physical quiescent/recovery/resource budget.
Ни один flag не открывает runtime action. Исторический report нельзя применять
для install/COMMIT/jump или ожидания оператора вместо fresh checks.

HMI protocol SemVer и Motion protocol counter проверяются независимо по
explicit contexts; **между ними нет автоматического перевода или утверждённого
production mapping**. Peer range — только HMI→Motion declaration, не reverse
compatibility, ordering/atomicity совместного обновления или CAN authentication.
Source commit — signer claim/expected selection, не доказательство сборки image.

Current PROJECT_VER остаётся git-derived, Flash layout factory2MiB, production
SemVer/keys/context и USB migration не утверждены. Несовпадающий app descriptor
не получает положительный review от подписанного SemVer. Инструмент не меняет
PROJECT_VER/SDK/partitions/security и не разрешает миграцию. Actual F407 memory/
IRQ/HSE/Flash/driver/physical/recovery gates прежние, board image запрещён.

## Приёмка

Новые2 root suites (CLI3 +cohort9 test methods): реальные независимые signatures/
full bytes/descriptor, different role keys, HMI-only/stable/prerelease/разные
component versions, source/release/channel mismatches, old/prospective peer,
local unknown/type/duplicates/partial inputs, image/key/signature corruption,
nonregular/bounds, no partial stdout и mutation после metadata authentication.
Host/LVGL38, sanitizers38, standalone13, synthetic ARM link1 PASS; analyzer8
embedded+1 host frontend/source178 files/board/output/scope gates PASS.
ARM4 archives43344text/data8/bss27 и frames984/376/328/64 прежние; host frontend
не входит в ARM targets. Все8 ESP SDK CI profiles сохранены; local ESP builds
не повторялись на этом host-tooling этапе. Hardware actions/измерений не было.
