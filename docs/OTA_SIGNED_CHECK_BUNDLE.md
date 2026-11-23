# Подписанный пакет для стендового OTA CHECK

`tools/prepare_ota_release.py` готовит локальный payload полного CHECK, без
доступа к сети/плате, установки и публикации. Он использует прежние
check_idf_build, inspect_app, verify_ota_candidate и OpenSSL. Приватный ключ
остаётся в выбранном владельцем каталоге вне Git; публичный ключ не берётся
из bundle. [Scope и ограничения](plans/P1_SIGNED_CHECK_BUNDLE.md).

Локальный JSON-план принимает ровно поля:

| Поле | Значение/проверка |
|---|---|
| reviewed | true только после согласования владельцем |
| purpose / channel | signed-check-bench / prerelease |
| source_commit / sdk | полный reviewed clean SHA; 5.3.2 или6.1.1 |
| release_version / installed_version | явные SemVer≤31; candidate новее local installed |
| bootloader_version / protocol_version | утверждённые локальные метки совместимости |
| key_id / public_key_sha256 | локальный ID и SHA-256 canonical DER SPKI91bytes |
| release_notes | непустой UTF-8≤4096bytes; явно стендовый scope |

Версия app descriptor фактического ELF/BIN должна совпадать с release_version.
Использовать `TV4_BUILD_PROJECT_VERSION` только с явно утверждённым значением.
Configured build получает версию из своего отдельного TV4_OTA_CONTEXT.
План не содержит private key, time/floors или downloaded trust. Не переносить
его в firmware как OTA context — это разные, отдельно проверяемые входы.

```bash
python3 tools/prepare_ota_release.py \
  --build /absolute/path/to/validated-rev12-ota-build \
  --plan /absolute/path/to/reviewed-local-release-plan.json \
  --public-key /absolute/path/to/owner/public.der \
  --private-key /absolute/path/to/owner/private.pem \
  --output /absolute/path/outside-checkout/new-signed-check-bundle
```

Output должен быть новым. Private key0600/parent0700, без symlinks и вне
source/build/output; ключ читается OpenSSL, его материал/путь в отчёт не входит.
Штатная команда поддерживает существующий локальный PKCS#8 без пароля;
пустой passin не допускает интерактивного ожидания encrypted key. Пароли в CLI
или логах не передавать. Другой способ custody требует отдельного решения.

Независимая повторная проверка принимает только внешний local plan/public key,
без private key, build или подключения:

```bash
python3 tools/prepare_ota_release.py \
  --verify /absolute/path/to/signed-check-bundle \
  --plan /absolute/path/to/reviewed-local-release-plan.json \
  --public-key /absolute/path/to/owner/public.der
```

PASS: подписанные manifest raw bytes, image length/SHA, полный ESP32 app
checksum/digest/descriptor, policy/source/version binding и inventory проверены.
Build-evidence — локальные claims; их digest не аутентифицирует CI/toolchain.
Публичная подпись manifest не подтверждает источник самого build-evidence.
В результате явно false: install_enabled, release_published, device_access,
production_resource_qualified. Не использовать его как разрешение публикации
или доказательство прошедшей аппаратной signed session.
