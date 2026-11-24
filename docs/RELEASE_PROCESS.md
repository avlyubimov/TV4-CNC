# GitHub Releases и CI/CD

## Актуальный экспериментальный выпуск bench.4

[v0.1.0-bench.4](https://github.com/avlyubimov/TV4-CNC/releases/tag/v0.1.0-bench.4)
опубликован после source30/main15 PASS и отдельного exact approval владельца;
tag15 также PASS до hardware. Точный source6c7b1d5,11 assets, independent reverse
downloads/SHA/signature/image PASS. [Build/publication evidence](evidence/build/esp32-ota-display-bench4/README.md).
[Signed INSTALL/readback/VALID](evidence/hardware/2026-11-24-esp32-display-bench4/README.md)
PASS; новый release оставлен на плате, factory recovery сохранён. Owner подтвердил
стабильность экрана при записи. Bench.2/bench.3/bench.4 immutable, private keys/
contexts/backups/raw UART не публикуются. Это prerelease, без latest/stable.

Это не production acceptance: network negative rollback/Wi-Fi/interruption,
production policy/version discovery/budget и runtime5.3.2 ещё открыты. Contexts
ограничены25ноября2026 08:00 Asia/Almaty. Compile-time date пока включён; нужны
отдельные reproducible config и две clean builds обоих SDK. Новые release/tag
требуют отдельного exact approval; push/merge не разрешают публикацию.

Ниже сохранён предыдущий процесс/история; production gates остаются обязательными.


[Prerelease v0.1.0-bench.2](https://github.com/avlyubimov/TV4-CNC/releases/tag/v0.1.0-bench.2)
уже опубликован по отдельному разрешению владельца на source c013ddf:11 точных
assets, downloads/SHA/signature/full BIN offline PASS. Это read-only CHECK
payload, без INSTALL acceptance; tag/assets не менять. Текущие GitHub digests
совпали с согласованными файлами24ноября. [Протокол публикации](evidence/build/ota-signed-check/publication.md).
Есть pinned CI обоих ESP-IDF/host/sanitizers/F407. Full signed CHECK a29c9ed/
3b09bba и native USB confirmation/refusal/rollback a74bfcd PASS; factory fecc05d
возвращён. Сетевой INSTALL, Flash-write resource acceptance и production release
остаются открытыми. Приватный ключ вне Git/CI; лицензия собственного кода MIT выбрана
владельцем, атрибуции зависимостей сохранены. Новые публикации требуют отдельного
утверждения точного commit/tag/assets после CI; push/merge релизы не выпускает.

Подготовка следующего installable bench payload описана в
[инструкции authoring](OTA_SIGNED_CHECK_BUNDLE.md) и [плане](plans/P1_INSTALLABLE_BUNDLE.md).
Отдельный signed-install-bench не публикует release, не использует CI secrets и
не разрешает INSTALL автоматически. Published bench.2 остаётся read-only;
реальные новые source/tag/версии/assets утверждает владелец после полного CI.

PR-05b: [review существующего release contract](RELEASE_POLICY_REVIEW.md),
общие signed C/Python scenarios и исправления Unicode/Base64 decoding. Это
не назначает actual versions/keys, production Motion schema или release approval.

## Версии и публикация

- Версионировать отдельно HMI, Motion, CAN protocol и manifest schema/layout.
  Release tag связывает точный commit и согласованный набор компонентов.
- prerelease/stable — отдельные каналы. Prerelease не становится stable
  автоматически; promotion требует нового явного approval владельца.
- Сборка metadata-only review-кандидата автоматическая в текущей CI; publication
  запускается вручную только после зелёного CI, review и разрешения владельца
  на точные commit/tag/канал/список артефактов и их digest.
- Merge PR, workflow_dispatch неуказанного пользователя или успешный build
  сами по себе не заменяют это разрешение. Release job должен иметь ручной
  owner gate/protected environment, насколько это поддерживает настройка
  репозитория; если gate недоступен, публикация остаётся ручной владельцем.
- Не создавать новый tag/release, не загружать ключи/секреты и не менять
  permissions/protection в PR-01. Разрешение на документацию не разрешает релиз.

## План CI в PR-05

1. Сохранить ESP-IDF 6.1.1/5.3.2 × headless/rev12, exact lock files и image
   digests, host logic/render tests (XRGB8888/RGB565), ASan/UBSan/LSan.
   Проверять target/chip, Flash 16 МБ, PSRAM profile, headers, partition bounds,
   slot/image sizes и неизменность dependencies. Build не выполняет flash/monitor.
2. Сохранять ARM GCC/CMake/Ninja, exact source pins, startup/vector/linker и
   Flash/RAM checks настоящего F407 HSI no-motion image. Basic bench ee7d179
   подтверждён; CAN/motion/bootloader OTA ещё не приняты. F407 asset не включать
   в HMI CHECK bundle, placeholder .bin не генерировать и не публиковать.
3. PR-05a: все GitHub Actions закреплены полными reviewable commit SHA,
   SDK images/toolchains/dependency locks остаются точными прежними версиями.
4. PR-05a: собирать/проверять неподписанный metadata-only candidate без production secrets на PR.
   Fork/PR code не получает signing credentials или contents:write.
   App/ELF/config/partition hashes и source identity доступны в review artifact;
   публикация app binary ждёт решения о лицензии и отдельных release gates.
5. В доверенном контексте одобренного commit/tag сформировать manifest,
   hashes, release notes и подпись; проверить подпись локальным доверенным
   публичным ключом. Не пересобирать бинарники после подписания digest.
6. Owner просматривает evidence, size/config, известные ограничения,
   hardware/rollback результаты и точные hashes. Публикация только после gate;
   contents:write ограничить release job, обычные jobs — read-only.
7. Скачать опубликованные assets и повторно проверить raw SHA, manifest
   signature/URL/совместимость. Ошибка → отозвать доступность кандидата,
   не заменять незаметно бинарник под тем же version/tag.

## Артефакты

- tv4-hmi-esp32s3-vX.Y.Z.bin — **app-only rev12 OTA image**, не полный Flash dump.
- headless — отдельно маркированный test artifact; manifest не предлагает его
  Waveshare как аппаратную прошивку. Bootloader/partition bundle для первой
  USB-миграции выпускается отдельно и явно не помечается обычным OTA image.
- tv4-motion-stm32f407-vX.Y.Z.bin — только после реального target build и
  подтверждённой memory map. Пока его нет, нет и записи motion в manifest.
- manifest.json, manifest.sig, checksums.sha256, release-notes.md;
  дополнительно build/config/license/test evidence без паролей/приватных дампов.

Архивы factory/Blue Pill backup, NVS дампы, private signing keys, реальные
пароли и локальные окружения исключены из Git, releases и upload-artifact.
Публичный verification key не секрет, но его fingerprint/включение в trust store
подлежат review. Production private key — offline signer или отдельно
согласованный защищённый signing service; не файл в репозитории и не echo в CI.
Отладочные/фиктивные ключи не принимаются production firmware.

## Схема манифеста — пример, не действующий релиз

Ниже невалидные placeholders намеренно не являются опубликованными hashes/URL/
подписью. HMI schema и signature envelope проверяются test vectors PR-04b/04d;
production key provisioning и release approval ещё не выполнены.
Один релиз может содержать только HMI, без фиктивной STM32 записи.

```json
{
  "schema_version": 1,
  "release_version": "<reviewed-semver>",
  "channel": "<prerelease-or-stable>",
  "source_commit": "<full-reviewed-git-sha>",
  "signing": {"key_id": "<approved-key-id>", "algorithm": "ECDSA-P256-SHA256"},
  "protocol": {"min": "<agreed-version>", "max": "<agreed-version>"},
  "components": [{
    "name": "hmi",
    "version": "<hmi-semver>",
    "hardware_id": "waveshare-esp32s3-touch-lcd-7",
    "hardware_revision": "1.2",
    "chip": "esp32s3",
    "profile": "rev12",
    "layout_id": "tv4-16m-ab-v1",
    "minimum_installed_version": "<agreed-minimum>",
    "minimum_bootloader_version": "<agreed-minimum>",
    "compatible_peer_versions": {"motion_min": "<agreed-minimum>", "motion_max": "<agreed-maximum>"},
    "size_bytes": 0,
    "sha256": "<64-lowercase-hex-digits>",
    "url": "https://github.com/avlyubimov/TV4-CNC/releases/download/<tag>/<hmi-asset>.bin"
  }],
  "release_notes": "<reviewed-changelog>"
}
```

Условия min/max peer/protocol не выводить из текущего тестового heartbeat v1.
В target-specific STM32 component добавить подтверждённые board/MCU/memory map
ID и bootloader minimum; текущий generic F407 не является достаточной целью.
Raw manifest bytes подписываются; envelope содержит algorithm/key_id/signature,
которые сверяются с локальным trusted allowlist, а не добавляют новый ключ.

## Приёмка выпуска и откат

До первого prerelease: подтверждённый ESP32 migration/rollback/recovery и
negative signature/compatibility tests; F407 release дополнительно требует
собственных recovery/power-loss/безопасного состояния испытаний. Документировать
неизмеренные показатели и ограничения канала; stable не означает «станок
commissioned» без отдельного отчёта испытаний приводов/аппаратной защиты.

Ошибочный релиз пометить отозванным в discovery policy и сообщить ограничения;
установленные устройства не получают молчаливую команду downgrade. Предыдущая
подтверждённая app/recovery остаётся доступной в рамках OTA policy. Изменённый
бинарник получает новую версию и подпись. Публикация исправления снова требует
CI и отдельного owner approval. Отмена/update history не удаляет доказательства.

Основа: [GitHub Releases](https://docs.github.com/en/repositories/releasing-projects-on-github/about-releases).
Подробная модель доверия и миграции: [OTA_ARCHITECTURE.md](OTA_ARCHITECTURE.md).

## Подготовка кандидата PR-04a

[OTA_VERIFICATION.md](OTA_VERIFICATION.md) описывает локальный read-only checker
подписи/compatibility/hash. Он не создаёт release, не подписывает и не включает
установку. Поддерживается только один HMI component; motion остаётся будущим.

PR-04d: manifest.sig — JSON envelope до 2048 байт с ровно key_id, algorithm
(ECDSA-P256-SHA256), signature (canonical Base64 DER).
См. [native HTTPS/envelope contract](OTA_HTTPS.md); production release не создан.

## PR-07g — отдельная проверка Motion document до production release

[Независимый signed release draft](STM32_RELEASE_BINDING.md) связывает release/
component/target/profile/source/channel с полным образом на стороне STM32
library. Явный selected release/source и installed/minimum SemVer context
обязателен, numeric counters независимы. Это не общий HMI manifest или готовый
production bundle ABI; HMI по-прежнему принимает только собственный component.

Новый format/API не создаёт Motion artifact, keys/version defaults, signer или
публикацию. Source commit в подписи не доказывает происхождение build; связь
review candidate→signer→exact bytes остаётся отдельным pipeline/owner review.
PR-07h [release handoff](STM32_RELEASE_HANDOFF.md) закрывает software live
lease/coordinator соединение; production bundle/context/runtime и
installation/recovery/hardware gates открыты.

## PR-05c — offline-проверка состава перед production bundle

[Release cohort review](RELEASE_COHORT_REVIEW.md) связывает существующие signed
HMI/Motion документы с explicit expected release/source/channel и полными bytes.
HMI app descriptor/SDK и prospective HMI→Motion peer range проверяются;
HMI-only не требует фиктивного Motion image. Native Motion frontend вызывает
прежний independent verifier и не компилируется для ARM.

Неподписанный review report не OTA manifest, signer/build provenance, trust
provisioning или release approval. Все authority gates false; actual SemVer/
keys/context/protocol mapping/layout/лицензия/публикация остаются owner review.
PROJECT_VER/CI candidates остаются прежними; новый инструмент не назначает
release versions и не публикует binaries/tags/releases. Production format и
связь с approved CI artifacts/signing custody — следующий отдельный этап.

## PR-05d — связывание exact bytes без release authority

[Build binding audit](RELEASE_BUILD_BINDING.md) добавляет explicit external
candidate.json SHA pin, повторные unsigned candidate/full HMI/ELF digest checks
и existing signed cohort verification. Bindings используют captured snapshots.
Factory/A-B и headless/bench/ordinary mismatches явно остаются blockers;
Motion build provenance ещё отсутствует. Новый JSON всегда blocked/exit3,
не manifest/approval/installer input. CI pin данного job не authentication;
source/build/signer custody и production bundle/context/keys/layout/license/
runtime/recovery/exact owner approval по-прежнему требуют отдельного review.
