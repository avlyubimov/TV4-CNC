# GitHub Releases и CI/CD

Статус PR-05a: [проверяемые metadata-only кандидаты](BUILD_CANDIDATES.md)
реализованы в существующей ESP-IDF матрице. Есть host/sanitizers и ARM core
compile-only; полноценного F407 image нет. Подпись, binary Releases, защищённое
окружение и OTA install ещё не реализованы. Push в main не создаёт релиз и
не является разрешением владельца на его выпуск. Лицензия приложения ещё не
выбрана (LICENSE-NOTICE.md); public CI review artifact не содержит binary.

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
2. После PR-02 добавить ARM GCC + CMake + Ninja, точные CMSIS/HAL/LL/лицензии,
   startup/vector/linker checks, Flash/RAM budgets, host state/fault tests и static
   analysis. Target F407 job блокируется до подтверждения MCU/memory/clock;
   placeholder .bin не генерировать и не публиковать.
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
