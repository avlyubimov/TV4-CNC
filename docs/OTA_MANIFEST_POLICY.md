# PR-04b — native manifest parser и policy

Актуальное продолжение PR-05b: [release policy review](RELEASE_POLICY_REVIEW.md).
Unicode escapes проверяются до cJSON allocation; malformed hex и decoded NUL
отклоняются. Offline envelope требует canonical Base64 как native. Дополнительные
shared signed scenarios/Unicode oracle не включают runtime OTA или installer.

Продолжение PR-04c: [read-only stream verifier](OTA_IMAGE_STREAM.md).
HMI app version теперь ограничена 31 байтом по размеру ESP app descriptor;
остальные SemVer/release/key limits ниже сохраняются.

База main 6d63410 после слияния PR #9. Продолжение PR-04a: проверка совместимости
теперь реализована в C-коде, который собирается для ESP32 и host.
**Installer, HTTPS worker и USB migration не включаются.** Разметка/ключи
устройства/boot partition не меняются. Это ещё не готовое OTA обновление.

## План и приёмка

- Один native signature → JSON → policy entry point, bounded inputs и output.
- Строгая schema одного HMI component, реальные локальные context значения,
  минимальные версии, replay/downgrade/channel/hardware/layout/protocol/peer/URL.
- Ошибка всегда очищает candidate; неподписанные bytes не достигают parser.
- Тесты UTF-8/escaped NUL/duplicate keys/limits/OOM и signed malformed sweep;
  полные host/sanitizers и 4 SDK сборки с retained entry point.
- Нет изменений в active partitions/config/GPIO/installer или обращения к плате.

## API и доверие

ota/manifest.h предоставляет tv4_ota_manifest_read: raw manifest bytes, DER
signature, доверенный локальный context и выходной candidate. Envelope decoding
key_id/algorithm/base64 остаётся будущему bounded transport. Публичный ключ
должен выбираться только из local reviewed allowlist. Параметр key_id затем
сверяется с подписанным manifest, а не создаёт доверие к downloaded key.

Context содержит подтверждённые installed/bootloader/protocol versions,
board_rev12, layout_id и optional motion version. Это значения устройства,
не копия network manifest. Текущий project version выводится из Git и не
считается согласованным SemVer release; версию bootloader и production protocol
нужно определить/проверить отдельно. Значения 0.1.0/1.0.0 в тестах — фикстуры.
NULL/неизвестная версия, отсутствующий ключ,
неподходящая плата или factory layout не разрешают OTA. Production trust store
по-прежнему отсутствует, никакие test keys в firmware не встроены.

После exact-byte подписи вызывается bounded JSON gate, затем parser и policy.
Успех выдаёт metadata/размер/expected SHA/URL/changelog, **не разрешение записи**.
Нет install API/boot activation. Вся интеграция предназначена только worker,
не LVGL callback. Сейчас entry point ни из UI, ни из сети не вызывается.

## JSON dependency и ограничения

cJSON v1.7.19, commit c859b25da02955fef659d658b8f324b5cde87be3, MIT:
third_party/cjson содержит неизменённые upstream C/header/license и provenance.
CMake проверяет SHA-256 всех трёх файлов на host и SDK. Local namespace header
переименовывает 78 public symbols в tv4_ota_json_*: SDK/другой cJSON и его
allocator hooks не затрагиваются. Managed dependencies и IDF locks не меняются.

До allocations проверяется максимум 16 КиБ, UTF-8, отсутствие embedded/escaped
NUL и не более 128 structural separators. cJSON_NESTING_LIMIT=8. Полная JSON
грамматика проверяется библиотекой; lexical gate не заменяет parser. Только
unsigned integer tokens: fractional/exponent/negative/leading-zero numbers
отклоняются. В этой schema числами бывают только schema_version и size_bytes.
Whitespace после root допускается, trailing documents/garbage — нет.

Decoded object keys проверяются на дубли, включая альтернативное Unicode
escape написание. Неизвестное/отсутствующее поле — отказ. String lengths заданы
в байтах: SemVer/key_id до 64, URL до 512, release_notes до 4096. Выходной struct
около 5 КиБ принадлежит caller; не размещать без stack budget в будущей задаче.
Парсер использует динамические allocations, ограниченные input/node/depth
лимитами; это не статическая arena. При нехватке памяти удаляется весь tree,
возвращается BAD_JSON и пустой output. Фикстура host показала peak 2323 bytes
и 71 allocation; это **не worst case и не замер heap ESP32**. Firmware не
устанавливает allocator hooks; test hooks существуют только в namespaced host
тесте для последовательного отказа каждой allocation.

SemVer core numbers ограничены uint32; prerelease identifiers сравниваются
без overflow по SemVer правилам, build metadata не поддерживается. Stable
не принимает prerelease app. Новая app version строго выше текущей, minimum
installed/bootloader соблюдаются. Peer constraint без известной motion версии
отклоняется. Один rev12 HMI component, protocol диапазон, candidate A/B layout,
точный GitHub TV4-CNC release asset URL, size 1..3 МиБ и lowercase SHA-256.

Python offline checker приведён к тем же bounds core version/UTF-8 changelog/
NUL. Он остаётся независимым preflight tool, не источником доверенного runtime
context. Native module не читает image: streaming SHA/header/project/revision
и SDK validation остаются будущему download/install worker.

## Проверки

[evidence/build/ota-manifest](evidence/build/ota-manifest/) содержит host,
sanitizers, OOM/memory fixture и SDK build/validation logs. Включены bad signature
до parser allocations, escaped duplicate key/NUL, invalid UTF-8/surrogate,
excessive nesting/tokens, schema/version/target/layout/protocol/peer/URL/size,
SemVer границы, очистка output и signed byte-mutation sweep. Каждая из 71
allocation валидной fixture отдельно принудительно завершается отказом; tree
не протекает, прежний candidate не остаётся доступным.

CI сохраняет существующую SDK/profile matrix и требует linked native entry
points verification + manifest policy. Host использует OpenSSL, ESP32 — SDK
mbedTLS. Native parser source одинаков; исполнение crypto/parser на ESP32
ещё не проверено на устройстве. Программные тесты не заменяют power-loss,
recovery/rollback, TLS/redirect и hardware commissioning.

## Следующий этап и откат

Ещё нужны approved trust provisioning, bounded envelope/HTTPS worker,
реальная local version/partition context, streaming image validation, native
rollback/self-test deadline и отдельная USB migration на проверенном backup.
Нельзя обойти отсутствие ключа/контекста фиктивными default версиями ради demo.
Обновление STM32/motion и unified UI — отдельные PR-07/08.

Откат — revert PR-04b. Нет новых NVS records, сетевых/Flash/serial операций,
production keys, eFuse или релиза. Активный factory 2 МиБ сохранён.
После создания PR остановиться на owner review, не merge самостоятельно.
