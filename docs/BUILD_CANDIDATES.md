# PR-05a — кандидаты сборок для review

Существующая CI-матрица ESP-IDF6.1.1/5.3.2 × headless/rev12/memory-bench/TLS-bench
теперь готовит **metadata-only review artifact** для каждого варианта. Это первая
реализованная часть PR-05: упаковка, проверка целостности/совместимости, provenance
для review и закреплённые Actions. Подпись, загрузка app binaries в GitHub Releases,
production SemVer/trust provisioning и установка остаются отдельными этапами.

## Что проверяется до упаковки

`tools/build_candidate.py create` работает offline с clean Git checkout и готовым
build tree. Требуется полный ожидаемый source SHA; проверяются HEAD/tree, отсутствие
изменённых/неотслеживаемых файлов и повторный clean check перед завершением.
Генерируемые build*/ директории игнорируются Git; новые source-файлы — нет.

Повторно выполняется существующий config/header/partition inspector, без перезаписи
его отчёта; сохранённый validation.json обязан совпасть. Для кандидата принимается
только действующая NVS/phy/factory2MiB layout, не неактивный A/B CSV.
Полный app image дополнительно проверяет `tools/esp_app_image.py`:

- ESP32-S3, DIO80MHz/16MB, 1–16 segments, bounded/aligned lengths, descriptor;
- project tv4_hmi, точная SDK version и display version из clean Git;
- текущий secure_version0, XOR checksum, zero padding, appended SHA-256 и конец
  файла без дополнительных/обрезанных bytes;
- descriptor ELF SHA-256 совпадает с полным локальным ELF; app digest/size — с
  повторно проверенным validation.json. ELF/app/metadata читаются с лимитами.

Build script передаёт реальную Git display version в SDK container, где .git
linked worktree может находиться вне mount. Больше не получается fallback `1`
при обычном вызове Docker из Git worktree. PROJECT_VER также обновляется при
повторной local сборке. Release SemVer не придумывается; неизвестный source или
грязная/старая версия не получает review bundle. Необычные/слишком длинные Git
tags требуют отдельного решения по версии, а не молчаливого усечения.

Это build-output inspection, не проверка firmware signature, загрузка на железе,
ESP-IDF bootloader verification/rollback или proof of reproducible build.
Связь ELF↔app подтверждается хешем, но не доказывает происхождение произвольной
внешней пары ELF/app. CI связывает сборку с checkout конкретного SHA. Поле
reference_sdk_image указывает закреплённый image для этой SDK; локальный SDK
не выдаётся за реально запущенный container. Криптографической CI attestation нет.

## Содержимое и повторная проверка

Каждый `review-candidate-SDK-PROFILE` содержит ровно пять файлов:

| Файл | Содержание |
|---|---|
| candidate.json | Schema/kind, exact commit/tree, SDK/reference image digest, profile, app descriptor/hash/size, ELF/config/partition hashes, честная factory layout, закрытые release gates |
| validation.json | Компактный config/size report без абсолютных путей/сырого sdkconfig |
| dependencies.lock | Точный per-SDK dependency lock из clean source |
| LICENSE-NOTICE.md | Текущее решение владельца о лицензии (ещё не выбрана) |
| checksums.sha256 | Детерминированный SHA-256 inventory четырёх предыдущих файлов |

В artifact нет app/ELF/bootloader/partition binaries, raw sdkconfig/
project_description, Flash/NVS dumps, ключей или сетевых credentials. CI upload
использует явный allowlist. Test/headless/bench profiles остаются самостоятельными
кандидатами с собственными флагами, не выдаются за ordinary rev12.

`verify` независимо проверяет inventory, strict/canonical JSON, scope/schema,
ожидаемые SHA/SDK/profile, factory layout, внутренние ссылки на digest/size.
С `--image` также заново проверяет весь локальный app image и его совпадение с
review. Без `--image` результат явно image_checked=false; это не проверка binary.
Checksums обнаруживают повреждение, **не устанавливают подлинность**: атакующий
может заменить одновременно JSON/inventory. Ожидаемый source SHA берётся отдельно
из review; подписанный manifest и локальный trusted key нужны для будущего OTA.

Inputs ограничены по размеру; extra/missing files, symlinks/nonregular files,
duplicates/NaN/неподдерживаемые schema/scope отклоняются. Никакой распаковки
архивов или принятия путей из inventory нет. Output создаётся после проверки,
существующая директория не перезаписывается. Работа рассчитана на quiescent
owned checkout/build/output parent, а не против concurrent filesystem attacker.

## Локальные команды

Из clean корня текущей ветки после commit и успешной сборки:

```bash
./tools/build_idf.sh rev12 6.1.1 local
TV4_SOURCE_SHA="$(git rev-parse HEAD)"
python3 tools/build_candidate.py create --repo . \
  --build build-idf-6.1.1-rev12 --output build-review-6.1.1-rev12 \
  --source-commit "$TV4_SOURCE_SHA" --sdk 6.1.1 --profile rev12
python3 tools/build_candidate.py verify --candidate build-review-6.1.1-rev12 \
  --image build-idf-6.1.1-rev12/tv4_hmi.bin \
  --source-commit "$TV4_SOURCE_SHA" --sdk 6.1.1 --profile rev12
```

Скачанный и распакованный metadata artifact проверяют той же `verify` командой;
его ожидаемый SHA нужно сверить с конкретным Actions run/PR, не копировать из
непроверенного candidate.json. Повторная упаковка требует нового output path.

## CI, проверки и граница этапа

Все workflow используют checkout v4 `11d5960a326750d5838078e36cf38b85af677262`
и upload-artifact v4.6.2 `ea165f8d65b6e75b540449e92b4886f43607fa02`.
Refs сверены через GitHub connector 19 ноября 2026. Checkout pin сохраняет
текущие v4 backport fixes (в том числе ref/fork-checkout guard), а не старый v4.2.2.
SDK/toolchains/dependencies не обновляются. Contents permission — read, нет
pull_request_target, signing secrets, release/tag creation или deploy job.
SDK jobs собирают и повторно проверяют candidate до upload; retention14days.
В root SDK container до build регистрируется только точный GITHUB_WORKSPACE в
Git safe.directory и сверяется HEAD с github.sha. Временный HOME checkout action
не переносит эту настройку в следующие steps. Wildcard trust не используется.
Конкретные результаты — [evidence](evidence/build/ci-candidates/README.md).

LICENSE-NOTICE.md требует выбора лицензии владельцем до внешнего распространения
приложения; публичные артефакты этого этапа содержат только metadata. Release gates:
license/version/context, trusted public key/signer, migration/recovery, production
resource/fault tests и отдельное согласование exact release. Зелёный candidate
не снимает их и не становится OTA manifest.

На плате по-прежнему ordinary8f0d899. Firmware C/UI, board pinout, RGB/GT911,
PSRAM/TLS/stack settings, locks и partitions.csv не меняются. Новых device calls,
прошивок, UTC/network probes, CAN/motion/installer нет. Откат — revert PR.

## Последующий PR-05c

[Offline signed cohort review](RELEASE_COHORT_REVIEW.md) — отдельная проверка
подписанных HMI/Motion артефактов и metadata consistency, не замена данного
CI candidate/provenance workflow. Она не назначает PROJECT_VER/SemVer, не
публикует images и не делает current factory candidate пригодным к OTA.
Source claim в подписи/report не является доказательством build origin;
approved candidate→signer→exact bytes linkage ещё требует production review.

## PR-05d — дополнительный pin и blocked binding audit

`verify --candidate-sha256` проверяет explicit expected candidate.json SHA-256,
старый JSON report прежний. Все8 CI jobs сохраняют candidate-pin.sha256 и
проверяют этот pin. Сгенерированный тем же job pin — только integrity check,
не внешнее approval/CI/source authentication.

[Build binding audit](RELEASE_BUILD_BINDING.md) повторно проверяет current
candidate/full HMI image/ELF и signed cohort из private snapshots. Несовпадение
factory/A-B или test/ordinary profile явно остаётся blocker. Report всегда
blocked/exit3; production версии/разметка/signing/provenance не назначаются.
