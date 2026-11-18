# PR-04a — проверка OTA-кандидатов, без установки

База: main 36c0530 после PR #8. Это первая часть PR-04, а не готовый OTA.
Существующие factory 2 МиБ, sdkconfig, bootloader и GPIO не меняются.
Нет сетевого downloader/installer, переключения слота или записи устройства.

## План и приёмка

1. Добавить bounded P-256/SHA-256 verification primitives на ESP32 и host.
2. Проверять локальный подписанный кандидат, strict schema и совместимость;
   отклонять повреждённые/неподходящие/повторные образы.
3. Проверить отдельный неактивный A/B CSV обоими SDK.
4. Host/sanitizers + четыре SDK/profile сборки, evidence и отдельный PR.
5. Остановиться на review, не merge/flash/release/provisioning.

## Криптографический слой

firmware/esp32-hmi/ota/verification.c использует библиотечные ECDSA P-256 и
SHA-256: OpenSSL EVP на host, SDK mbedTLS на ESP32. IDF 6.1.1 использует
Mbed TLS 4.1.1/TF-PSA-Crypto, поэтому перед crypto вызывается psa_crypto_init;
на 5.3.2 сохраняется SDK mbedTLS API. Собственной криптографии нет.
Manifest ограничен 16 КиБ, ASN.1 DER подпись 8..72 байта. Подписываются точные
байты, включая пробелы/переводы строк. Поддерживается только canonical DER SPKI
91 байт: named prime256v1, uncompressed public point. Префикс проверяет формат
и curve OID; валидность ключа/подпись проверяет библиотека, не этот префикс.

API получает **уже доверенный локальный публичный ключ**. Curve check сам
по себе не устанавливает доверие. В firmware нет trust anchor/списка ключей,
вызова verifier из UI/сети или installer. Entry point сохраняется linker option,
чтобы матрица действительно разрешала crypto symbols, а не выкидывала весь код.
Входы не вызываются при загрузке: нет новой радио/Flash операции или проверки
фиктивного production key. Host crypto tests создают ephemeral key только в RAM.

## Локальная проверка релиза

Новый tools/verify_ota_candidate.py читает только локальные файлы. Он не
скачивает, не подписывает, не генерирует ключи и не прошивает. Требуются Python
3.10+ и OpenSSL CLI; host CMake требует OpenSSL development library >=3.0.
Firmware использует уже закреплённый SDK mbedTLS, managed dependencies не меняются.

Подпись — JSON envelope до 2 КиБ с ровно key_id/algorithm/signature:

```json
{"key_id":"<approved-local-key-id>","algorithm":"ECDSA-P256-SHA256","signature":"<base64-DER>"}
```

key_id должен совпадать с локально переданным trusted-key-id; публичный ключ
не читается из manifest/URL/envelope. Для нескольких ключей нужен будущий
reviewed trust store. До provisioning ни один ключ не считается доверенным
самим устройством. Пример команды — с реальными локальными данными владельца,
не с тестовыми ключами:

```bash
python3 tools/verify_ota_candidate.py \
  --manifest /local/candidate/manifest.json \
  --signature /local/candidate/manifest.sig \
  --public-key /local/trust/approved-public.der \
  --trusted-key-id APPROVED_ID \
  --image /local/candidate/hmi.bin \
  --installed-version X.Y.Z --bootloader-version X.Y.Z \
  --protocol-version X.Y.Z --layout-id tv4-16m-ab-v1 --channel stable
```

Это шаблон, а не утверждение об установленных версиях. Пока устройство на
factory layout, команда с его фактическим layout обязана отклонить app OTA.
Не вводить вымышленные версии ради успешной проверки. Тестовая protocol 1.0.0
не назначает production CAN protocol.

Порядок проверки: bounded bytes → строгий envelope/локальный ключ → подпись
exact manifest bytes → strict UTF-8 JSON без duplicate keys/NaN/Infinity →
schema/versions/hardware/layout/protocol/URL → streaming image length/SHA.
При чтении образа учитывается signed size, максимум 3 МиБ; буфер 4096 байт.
Проверяются также базовый ESP32-S3/16MB header. Это **не полный esptool/SDK image
validation** сегментов/app descriptor/project/chip revision; он обязателен в
будущем installer до выбора boot partition. Успех выдаёт verified-offline-only,
install_enabled=false и device_access=false, никогда «можно прошивать».

Schema следует примеру RELEASE_PROCESS, но foundation принимает ровно один
HMI component и строгий набор полей. Motion component пока отклоняется, а не
подменяется фиктивным образом. Compatible_peer_versions допускается только
при известной motion-version. SemVer без build metadata, без leading zeros;
новая версия app строго выше текущей, minimum versions соблюдаются.
Контролируемый recovery/downgrade — отдельная будущая процедура, не bypass flag.
Stable канал не принимает prerelease app. Signed URL — ровно GitHub TV4-CNC
release path с утверждённым именованием; download redirect/TLS политика здесь
не реализована, поскольку сетевого I/O нет. Неподдерживаемое поле — отказ.

## Неактивная A/B-карта

ota/partitions.ab.candidate.csv переносит предложенную карту из архитектуры
в проверяемый файл, **не referenced by sdkconfig/build/flash**. Factory recovery
2 МиБ, otadata 8 КиБ, два слота по 3 МиБ, резервы resources/store до 16 МиБ.
Последние используют opaque custom data subtypes 0x40/0x41, не заявляются
работающими filesystem и не монтируются. NVS/phy/factory offsets сохранены.

tools/check_ota_layout.py проверяет точную карту, границы/erase/app alignment,
затем вызывает parser выбранного SDK и round-trip через временный binary.
Ничего не пишет в активную таблицу или build directory. CI повторяет на 6.1.1
и 5.3.2, проверяя также наличие linked crypto entry point в ELF. Partition binary
остаётся временным, не выпускается для устройства.

## Оставшиеся части PR-04

- Reviewed production trust store/provisioning/key rotation/schema test vectors.
- Firmware manifest parser/compatibility context с реальными версиями.
- Bounded HTTPS worker/CA/time/redirect allowlist; no insecure fallback.
- Streaming image hash + SDK validation до finish/set_boot_partition.
- Native A/B/otadata rollback config, health self-test/deadline/watchdog и
  управление предыдущей confirmed version; совместимость NVS/resources.
- Отдельно разрешённая USB migration/recovery с verified backup/readback;
  power-loss/bad-image/rollback испытания на изолированном стенде.
- Unified Updates UI — PR-08; STM32 CAN bootloader — PR-07.

Устройство пока не имеет A/B/rollback. В этом PR нет esp_ota_begin/write/end,
esp_https_ota_finish/set_boot_partition, release/signing pipeline или auto-install.
Приватные production ключи не создавались; test keys временные и уничтожаются,
не добавляются в Git/логи/артефакты. Никаких eFuse/Secure Boot/Flash Encryption.

## Проверки, риски и откат

[evidence/build/ota-foundation](evidence/build/ota-foundation/) содержит реальные
логи сборок/host/sanitizers и обоих SDK partition parsers. Проверяются mutation
exact bytes, неверный key/curve/signature, strict JSON, несовместимость версий/
hardware/layout/protocol/channel/URL, size/hash/truncation и limits.
Native ESP32 crypto API скомпилирован/слинкован на обоих SDK, но на устройстве
не исполнялся; host использует другой crypto backend. Это явная граница проверки.
Тестовый 512-byte header fixture не является рабочей ESP32 прошивкой.

Рост размера firmware из-за retained crypto измеряется build evidence, не
объявляется нулевым; новых crypto вызовов при загрузке/работе UI нет. Аппаратное влияние размера
образа на производительность пока не измерено.
Откат — revert PR. Разметка/данные устройства не менялись, восстанавливать
Flash не требуется. Применение candidate layout не входит в этот PR.
