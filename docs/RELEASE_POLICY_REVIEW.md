# PR-05b — проверка release metadata перед будущим OTA

20 ноября 2026. База main `455ccc6`, PR #1–33 слиты, пять main workflows PASS.
Это review и программные проверки существующего HMI contract, не новый релиз,
provisioning, runtime OTA worker или STM32 production manifest.
[План](plans/PR05B_RELEASE_POLICY_REVIEW.md),
[результаты](evidence/build/release-policy-review/README.md).

## Исправленные расхождения

1. cJSON1.7.19 возвращает ноль из внутреннего hex decoder и для `0000`, и для
   невалидных hex digits. Старый local gate проверял лишь написание `\u0000`.
   Поэтому подписанное `"chip":"esp32s3\u00g0hidden"` могло пройти как
   `esp32s3`, а некорректная строка notes — незаметно обрезаться. Regression
   на старой реализации воспроизведён. Gate теперь до allocation проверяет все
   четыре hex digits, запрещает decoded NUL и неизвестные short escapes;
   корректность surrogate pairing по-прежнему проверяет cJSON. Upstream source
   и его SHA не изменены. Это не обход обязательной подписи: проблема касается
   интерпретации уже подписанных malformed bytes.
2. Python `base64.b64decode(validate=True)` допускает ненулевые неиспользуемые
   pad bits. Native envelope их уже отклонял. Offline checker теперь требует
   bounds12..96/multiple4, decoded8..72 и точное canonical re-encode equality.
   Канонические DER signatures принимаются по прежним правилам.
3. Required envelope nodes/strings проверяются явно до dereference, чтобы
   actual-source GCC analyzer не полагался на непрозрачный cJSON_IsString API.
   Нет suppressions или ослабления проверок ради PASS.

Native filter остаётся линейным по bounded input; новых heap allocations,
buffers, worker/UI вызовов и зависимостей нет. Fixture peak2323bytes/71 allocations
прежний, **не worst case и не измерение производительности ESP32**.

## Проверенная политика и её пределы

| Область | Существующий contract, проверенный программно | Что ещё нужно для production |
|---|---|---|
| Подпись | Exact raw bytes; local P-256 SPKI/key ID; envelope не создаёт доверие | Owner-reviewed key provisioning/rotation и signer authority |
| SemVer | Core uint32, ASCII prerelease, numeric/lexical precedence; до64bytes; без build metadata; HMI app до31bytes | Настоящие HMI/bootloader/protocol/peer версии и provenance из local state |
| Каналы | Local stable/prerelease должен совпасть с signed channel и release prerelease marker; stable не принимает prerelease app | Отдельное owner approval promotion/publication |
| Компоненты | Ровно один HMI rev12, явные hardware/revision/chip/profile/layout | Нельзя добавить фиктивный motion до target build/production schema review |
| Раздельные версии | Release и component версии независимы; prerelease bundle может содержать stable component | Не выводить стабильность машины из label канала или release tag |
| Replay/minima | New HMI строго выше installed; installed/bootloader minimum, protocol и optional peer range | Trusted installed context; reboot/recovery и persistent policy ещё не готовы |
| Unknown context | NULL/empty/non-SemVer versions, unknown key/layout/target отклоняются | Git-derived display version не подменяет reviewed SemVer |
| Источник/URL | Lowercase40hex source commit и exact allowlisted asset URL | Signer/pipeline должны связать source commit, build review candidate и подписанные bytes без пересборки |
| Candidate | Совпадают release/version/URL/notes/size/SHA; native rejection очищает весь output | Candidate не write/install/physical-ready token |

Правила precedence сверены с [SemVer2.0.0, §9–11](https://semver.org/spec/v2.0.0.html).
Ограничения uint32/64bytes/31bytes/no-build-metadata — существующий **subset
проекта**, а не полная SemVer grammar. Actual версии, keys или channel для
первого релиза этим review не назначаются.

## Проверки C и Python

Host-only `ota_policy_driver` держит новый ephemeral private P-256 key только
в памяти и подписывает каждый raw corpus input. Он вызывает настоящий
`tv4_ota_signed_manifest_read`, а Python — настоящий offline signature/policy
путь с теми же bytes/key/envelope/context. Это не тестовый signer в прошивке.
Сравнение проверяет accept/reject и все candidate fields, не только код успеха.

Corpus115:16 accept/99 reject; encoding/raw signature/tampering, channel,
independent versions, replay/downgrade, unknown local versions/key/layout,
protocol/peer bounds, component/domain separation, byte/UTF-8 limits, malformed
JSON/Unicode/duplicates/NUL, wrong keys/envelopes. SemVer2809 comparisons,
Base64 130 canonical/noncanonical cases, Unicode oracle65536 BMP escapes и
420 non-hex slot substitutions; surrogates/short escapes/truncation отдельно.

Offline `read_metadata` отделён от последующего image read для честного сравнения
одного слоя. Публичный CLI по-прежнему вызывает `verify`: полный image length/
SHA/header обязателен. Offline file check не знает physical board identity;
native board_rev12=false проверяется прежними native tests. Corpus использует
общий representable context с board_rev12=true; C API принимает C strings,
не Python strings со скрытым suffix после embedded NUL. Нулевые байты в raw
network metadata отдельно отклоняются обоими verifier.

Host/LVGL29 suites и ASan/UBSan/LeakSanitizer29 PASS, actual-source analyzer3
native policy units PASS. CI сохраняет все STM32 gates и8 SDK profiles,
добавляет corpus/Unicode tests в normal и sanitized jobs и text analyzer artifact.

## Следующий STM32 этап

Независимый F407 verifier остаётся на экспериментальной
[`tv4-motion-admission-draft-1`](STM32_IMAGE_ADMISSION.md) с counters/test IDs.
Он не проверяет release SemVer/channel/source commit общего HMI manifest.
Нельзя переводить SemVer в draft counter или передавать HMI success/receipt
как замену независимой подписанной production metadata проверки загрузчиком.

До production integration нужны review component/domain/schema/version policy,
binding release↔component↔target/profile↔image bytes, local key/version/boot
context и persistent rollback/recovery. Новые поля нельзя молча добавлять в
строгий draft parser. Raw CAN codec/IDs/bitrate, storage lease backend и
fresh session negotiation остаются отдельным этапом. После operator wait
полная проверка saved bytes выполняется заново; historical outcome не install
или physical quiescent authority.

## Аппаратное состояние и откат

Owner SWD/core/Flash1024KiB/3,252V/backup1048576bytes/vector structure PASS
сохранены; в новом ответе владелец повторил только VGT6. Полная маркировка,
HSE/RAM/CCM/vector contract/erase/protection/GPIO/CAN/restore не подтверждены.
Инженерные synthetic profiles не становятся реальной F407 map. Flash addresses
не резервируются. На ESP32 прежний ordinary8f0d899; новый image не записывался.

Revert PR возвращает прежние проверки. Нет device/serial/SWD/reset/Flash,
secrets/keys/release, NVS/layout/GPIO или drive changes. Новое доверие/установка
не включены, PR не сливается агентом.
