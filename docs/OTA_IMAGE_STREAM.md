# PR-04c — read-only потоковая проверка ESP32 образа

База main b867967 после PR #10. Слой принимает bytes из будущего download worker,
но **не содержит HTTP клиента, sink, Flash writer, installer или boot selection**.
Ни активная разметка, ни app_main/драйверы не меняются. Это не готовый OTA.

## План и приёмка

- Stateful SHA-256 без хранения всего образа в RAM, chunk 1..4096 bytes.
- Проверка signed size/SHA, ESP header/segments/descriptor/checksum/simple hash.
- Явные receive/verified/failed/cancelled состояния, cleanup на каждом отказе.
- Idle/total deadlines с wraparound, отмена/обрыв, запрет implicit resume.
- Host/sanitizers/fault tests, 4 SDK сборки и format smoke реальных app binaries.
- Только отдельный PR, без подключения/записи устройства, ключей и релизов.

## Контракт и память

ota/image_stream.h: zero-initialized single-owner struct; start получает candidate
**только из authenticated manifest policy** и локальный chip revision context.
Сам stream не проверяет подпись и не устанавливает доверие к произвольному
candidate. Expected hash/size/version копируются при start; изменение caller
candidate затем не меняет текущий сеанс. Нет default реальной ревизии MCU:
chip_revision_known=false — отказ.

feed принимает последовательные immutable borrowed bytes, 1..4096 за вызов;
указатель не сохраняется. Hash contexts через библиотечный backend: OpenSSL
на host, SDK mbedTLS на ESP32 (PSA init для 6.1.1). Два контекста: SHA всего
файла относительно signed manifest и SHA ESP body до appended simple hash.
Наш слой не создаёт image-sized/per-chunk buffers. В struct только headers,
256-byte app descriptor, digests, версия и counters. Host sizeof=504 bytes;
это не измерение размера/heap на ESP32. Backend allocations создаются при open,
освобождаются при finish/failure/abort/close; backend heap overhead ещё не измерен.

Состояния: IDLE → RECEIVING → VERIFIED либо FAILED/CANCELLED. Старт/повторный
finish в неподходящем состоянии отклоняется; для нового сеанса нужен close,
который очищает RAM state. Никакого resume с неполным hash state. verified здесь
означает **stream integrity/format**, не готовность к boot/установке/движению.
Нет commit callback, доступа к разделам или автоматического restart устройства.

## Проверяемый формат

Поддержан текущий профиль ESP32-S3: ESP image magic 0xe9, 1..16 сегментов,
DIO/80 МГц/16 МБ header, chip_id=9, hash_appended=1, допустимая full chip revision
(major*100+minor; legacy major fallback для S3). Неизвестная локальная ревизия
не подменяется типовым значением. GPIO/flash mode конфигурация не меняется.

Segment lengths положительные, кратны 4, не выходят за declared total; первый
сегмент содержит esp_app_desc 256 bytes. Проверяются magic 0xabcd5432,
secure_version=0 для текущей политики без eFuse, project_name=tv4_hmi,
NUL-terminated app version и точное совпадение с signed candidate. HMI version
ограничена 31 байтом в manifest policy/offline checker: SDK descriptor имеет
32-byte поле с NUL. Release tag SemVer может оставаться до 64 bytes.

XOR checksum data segments (initial 0xef), zero padding до checksum на конце
16-byte блока, embedded SHA-256 и полный expected SHA/размер сверяются до
VERIFIED. Extra/truncated bytes, сегмент сверх bounds, неподходящая версия/
проект/revision, corruption → терминальный отказ и освобождение contexts.
Secure Boot signature trailers, encryption/compression, hash_appended=0 и
другие режимы не поддержаны; их нельзя молча пропускать.

Это не замена штатной ESP-IDF image validation: load/entry address, memory/MMU
mapping, eFuse block/revision детали, bootability и policy всей системы проверяет
будущий native installer/bootloader. Прочитанный файл не исполняется и не пишется.

## Прерывания и сроки

Caller задаёт idle_timeout_ms и total_timeout_ms (1..INT32_MAX, total>=idle),
monotonic uint32 ms; poll cadence <2^31 ms. Deadline проверяется **до** обработки
нового chunk: поздние bytes не скрывают timeout. Total deadline абсолютный,
поэтому медленные одиночные bytes не продлевают загрузку бесконечно. Feed нулевой
длины не считается keepalive. finish требует exact length и завершённый формат.

abort(cancelled=true) очищает crypto и оставляет CANCELLED; network/transport
abort — FAILED/IO. После терминального состояния feed/finish не возобновляют
сеанс. Здесь нет асинхронного timer/task: worker обязан вызывать poll, задавать
HTTP timeouts и закрывать соединение при ошибке/отмене. Эти network операции
пока не реализованы. Аналогично этот слой не запускает реальный watchdog.

## Проверки и источники

Формат сверялся с установленным SDK 6.1.1 esp_app_format.h/esp_app_desc.h и
реальными app images обеих SDK, не с физической платой. Stable поля header,
segment и первых descriptor полей используются по offsets, без cast packed
struct/unaligned reads. В SDK CI retained stream entry points должны реально
линковаться, а не удаляться как unused code.

[evidence/build/ota-stream](evidence/build/ota-stream/) содержит root host и
sanitizers, SDK validation и actual-sdk-stream.log. Тесты: chunks от 1 byte,
разрывы внутри header/descriptor/tail, все 480 truncation points fixture,
cancel/IO, late timeout/total timeout/wrap, bad format/revision/version/project/
secure version/checksum/hash, oversize/extra/invalid arguments и resource cleanup.

Дополнительный host helper читает реальные app binaries, формирует ожидаемый
hash из этого же локального файла и проверяет streamed format/digest. Это
**не authenticity test и не install approval**: он не использует production
ключ/подписанный релиз, а текущие SDK app versions git-derived. Для этого
smoke helper временно загружает файл в host RAM; firmware verifier этого не делает.
Никаких test binaries не публикуется как firmware release.

## Следующие части и откат

Нужны approved production trust store, реальные версии/context, bounded
signature envelope/HTTPS worker с CA/time/redirect policy, storage adapter для
неактивного слота, SDK image verification до boot selection, operator confirmation,
health self-test/rollback и отдельно разрешённая USB migration на backup.
Жёсткий запрет обновления F407 во время движения этим слоем не заменяется.

Read-only hash checker не доказывает отзывчивость HMI или безопасный power-loss
commit. Native crypto/image code собран для ESP32, но на устройстве не исполнялся.
CPU/performance/soak PR-03 и hardware recovery остаются открытыми.
Откат — revert PR-04c. Flash/NVS/layout устройства не менялись; release/eFuse/
прошивка не выполнялись. После PR остановиться на review, не merge самостоятельно.
