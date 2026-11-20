# PR-07b — read-only admission образа STM32

Исторический контракт PR-07b от main `74a7af9`, 20 ноября2026. При подготовке
PR #29 был открыт; теперь #29/30 слиты в main `28fe7bc`. Сам verifier остаётся
независимым. Новый [handoff coordinator](STM32_UPDATE_HANDOFF.md) использует его
снаружи, не меняя draft metadata, crypto config или pinned dependency.
[План](plans/PR07B_IMAGE_ADMISSION.md), [API](../firmware/stm32-bootloader/include/admission.h),
[результаты](evidence/build/stm32-image-admission/README.md).

В этой ветке независимая библиотека проверяет подпись metadata, локальную
совместимость и полный image stream. ESP32 verifier не вызывается. Единственный
публичный результат успеха — `VERIFIED_CANDIDATE`, временный результат проверки
байтов, **не разрешение install/commit/jump или движения**. Нет runtime caller,
Flash writer/map, board image, CAN driver, device access или production keys.

## Криптография и память

Отдельная [Mbed TLS 3.6.7](https://github.com/Mbed-TLS/mbedtls/releases/tag/mbedtls-3.6.7),
официальный release archive SHA-256
`a7e8bcbec0e6f761b4af24f25677626b35f762f68eef79c08677a363212d11f6`.
Источник/состав/лицензия — [origin.json](../third_party/mbedtls/origin.json),
[неизменённая лицензия](../third_party/mbedtls/LICENSE). Для этой зависимости
выбран предлагаемый upstream вариант Apache-2.0; лицензия приложения по-прежнему
не выбрана. Upstream 3.6 LTS заявлен поддерживаемым как минимум до марта2027;
версию и advisories нужно повторно проверить перед production release.

Собираются P-256/SHA-256/MPI/ASN.1 и стандартный buffer allocator, без TLS,
X.509, сети, файлов, hardware RNG и системного calloc/free. Нет собственной
криптографии. Стандартная библиотека содержит также общие signing primitives;
наш API их не предоставляет. Signing есть только в host tests через OpenSSL,
с ephemeral private keys в памяти; ключи не пишутся в файлы/Git/артефакты.

Подпись: только canonical DER ECDSA-P256-SHA256, 8..72 bytes. Public key —
canonical DER SPKI prime256v1/uncompressed point,91 bytes, из локального allowlist
до4 keys с уникальными ASCII key IDs. Prefix/DER проверяются, принадлежность
точки кривой и математическая подпись проверяются Mbed TLS. Загруженный key,
URL, CRC или заявленный SHA не создают доверие. JSON/Base64 signature envelope
и release discovery здесь не реализованы; будущий adapter обязан проверить
их schema/algorithm/key ID до передачи в этот фиксированный API.

В [конфигурации](../firmware/stm32-bootloader/config/tv4_boot_mbedtls_config.h)
уникальное имя header исключает случайное подключение штатного TLS config.
Compile guards проверяют версию/allocator/no-STD и отсутствие TLS/entropy.
Upstream P-256 window/fixed-point defaults сохранены. Буфер arena до16384bytes,
полный caller-owned workspace17664bytes. Это **software test ceiling**, не
подтверждённый резерв RAM/CCM платы. Снижение arena даёт отказ, без heap fallback.

Upstream allocator имеет process-global состояние. Допустим только один owner,
без threads/ISR/параллельных прямых вызовов этой Mbed TLS instance. Reentry из
callback возвращает BUSY, не меняя active storage/receipt. Это не thread lock.
При обнаруженной corruption allocator вызывает compiler trap; аппаратная
реакция fault/watchdog/reset ещё требует board review, physical inhibit не заявлен.
[Upstream allocator contract](https://mbed-tls.readthedocs.io/en/latest/kb/how-to/using-static-memory-instead-of-the-heap/).

## Экспериментальный формат metadata

Это отдельный **draft для software проверки**, не утверждённый GitHub release
manifest, CAN wire format или production bootloader ABI. Не добавлять его как
фиктивный motion component в существующий HMI manifest. Связь с общим подписанным
release manifest/SemVer/channel policy требует отдельного архитектурного review.

Подписываются точные raw bytes metadata (до1024bytes), не сериализованный заново
объект. Перед callbacks копируются metadata, signature, выбранный локальный key,
policy и request function pointers. Последующая подмена caller buffers не меняет
проверяемый snapshot. Parser вызывается только после успешной подписи.

Ровно11 обязательных scalar fields, порядок произвольный:

| Поле | Проверка |
|---|---|
| schema | Ровно `tv4-motion-admission-draft-1`, domain separator |
| component | Ровно `motion` |
| hardware_id / hardware_revision / memory_profile | Непустые локальные IDs, точное совпадение с reviewed policy |
| image_counter | UINT32, больше installed counter и не меньше local minimum |
| minimum_bootloader_counter | Ненулевой UINT32, не больше локального bootloader counter |
| protocol_min / protocol_max | Ненулевой упорядоченный диапазон, содержащий local protocol |
| image_size | Ненулевой UINT32, не больше local max_image_bytes |
| sha256 | Ровно64 lowercase hex digits |

Плоский parser не аллоцирует память. IDs имеют1..31 символ `[A-Za-z0-9._-]`;
нет string escapes/Unicode/NUL/человеческого текста/URL. Числа — минимальная
десятичная запись uint32 без sign, leading zero, fraction или exponent.
JSON whitespace разрешён; duplicate/missing/unknown fields, вложенные значения,
неверный schema/component, лишние и усечённые байты отклоняются.

Счётчики — только модель software admission. Они не назначают реальные версии,
не сохраняются во Flash/option bytes и не являются persistent anti-rollback.
Production counters/profile/protocol ещё не утверждены; zero/unknown context
отклоняется. Значения `test-f407`, `synthetic`, `test-no-addresses` и counters
встречаются только в tests и ничего не говорят о физической плате.

## Поток и завершение

После signature/schema/compatibility checks worker читает только source через
ограниченный callback: relative offset, capacity до256bytes, count и EOF.
Системных/аппаратных reads библиотека сама не выполняет. Exact signed length
хешируется стандартной Mbed TLS; затем выполняется1-byte probe EOF. Truncation,
extra byte, no-progress, count>capacity, reader failure и SHA mismatch — отказ.
В callback нельзя передавать raw сетевые pointers или нарушать capacity.

Обязательны monotonic uint32 ms clock и cancel callback. Интервал <2^31ms,
timeout1..INT32_MAX, elapsed>=timeout и обнаружимый обратный ход — отказ.
Guard срабатывает до/после reader, вокруг crypto/parser и после cleanup.
Это cooperative deadline на границах, а не прерывание зависшего callback или
ECDSA вычисления. Вызовы синхронные и bounded по контракту owner; не из LVGL
callbacks. Callback context для clock/cancel/source должен оставаться валидным
до final guard, вне очищаемого workspace. Hardware CPU/worst-case/total stack/
watchdog budgets ещё не измерены.

MPI/SHA contexts освобождаются, allocator отключается и весь workspace очищается
до final guard. Лишь после этого публикуется receipt: image bytes/counter/SHA,
raw metadata SHA и key ID. Ошибки очищают receipt; недопустимый alias и BUSY
сохраняют caller storage, чтобы не испортить активную операцию. Во всех случаях
caller обязан проверять return code, а receipt не использовать как разрешение.

Проверка относится к snapshot policy на входе; актуальность его generation
перед дальнейшим действием обязан подтвердить coordinator. Receipt не переживает
reboot, изменение policy/key/source или передачу другому
сеансу. Будущий installer обязан повторно проверить **сохранённые** bytes и
согласовать fresh session-bound quiescent/operator/recovery conditions. Signature
и hash произвольного потока не доказывают допустимые SP/PC/vector ranges или
bootability; это следующий gate по подтверждённой карте MCU.

## Проверки и следующие зависимости

Host: стандартный OpenSSL signer/oracle,14 test groups,5 dependency fault tests,
root LVGL/logic suites и sanitizers. ARM: обе библиотеки compile-only, строгие
imports, source/config/code/global/frame budgets и неизменная блокировка image.
Source pin проверяет178 headers/source/license files при каждой сборке, включая
извлечённый cache. CI сохраняет только текстовые evidence.

Нет исполнения ARM или аппаратных результатов. Owner SWD/core/Flash1024KiB/
3,252V/backup1048576bytes/vector structure facts остаются принятыми;
package/RAM/CCM/HSE/erase/protection/pinout/GPIO/CAN/restore неизвестны.
Адресов Flash/секторов/bootloader reserve нет. До writer/commit нужны reviewed
memory/vector/clock/erase/protection/recovery contract, реальные ключи и версии,
доказанный physical quiescent state и operator approval. E-stop независим.
Откат — revert PR; данные устройств не менялись.
