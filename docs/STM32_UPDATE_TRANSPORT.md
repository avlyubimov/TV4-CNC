# PR-07a — сеанс передачи образа без установки

Исторический контракт PR-07a от main `74a7af9`; PR #29 теперь слит.
В main `28fe7bc` есть независимая portable C11 библиотека [update_session](../protocol/can/update_session.h).
Это программная часть будущего CAN bootloader: отправитель, приёмник и
ограниченный stop-and-wait. Новый [read-only handoff](STM32_UPDATE_HANDOFF.md)
вызывает RX poll в software tests, не меняя transport API/deadlines. Ни ESP32
runtime, ни STM32 startup/supervisor эти библиотеки не запускают.

## Что подтверждает результат

Передача закончилась, когда потребитель принял ровно `image_bytes`, а
отправитель получил соответствующий ACK. Статус называется
`COMPLETE_UNVERIFIED`: образ **не проверен и не разрешён к запуску**.
Поле `image_sha256` — непрозрачная идентичность от caller, а не результат
проверки SHA. `begin` — локальное открытие транспорта, не разрешение установки.

Нет Flash addresses/sectors, storage driver, SHA/signature verifier на MCU,
quiescent detector, COMMIT, valid flag, vector handoff, reset или jump. Нет
CAN driver, fragmentation, wire encoding, version negotiation, arbitration IDs,
bitrate или GPIO. Объекты C нельзя передавать как packed CAN messages:
padding/enum/endianness ABI не являются wire specification. Старый mock
heartbeat 4 bytes/v1/500 ms сохранён без изменений.

## Идентичность и границы

- `boot` и `transfer` — два ненулевых логических идентификатора. Локальный
  `boot` передаётся при init, BEGIN обязан ему соответствовать. Новый BEGIN
  отклоняется, пока объект не переинициализирован; terminal fault/abort не
  снимается ACK/reconnect. После init нет automatic resume.
- Уникальность/неповторяемость boot+transfer после reset и свежая negotiation
  обеспечиваются будущим внешним слоем. Это не случайное число, MAC или
  защита от целевого replay/authentication. Перезапуск с прежней identity
  не защищён этой библиотекой и запрещён интеграционным контрактом.
- Offset начинается с нуля и означает позицию **в потоке**, не адрес MCU.
  Sequence начинается с нуля. Принимается только следующий блок длиной
  `min(block_bytes, image_bytes - accepted_bytes)`. Пропуски, перекрытия,
  короткий средний блок, oversized/zero block и неизвестные offsets отклоняются.
  Проверка длины предшествует доступу к payload. Сумма cursor+length не выходит
  за заявленную длину; максимум `UINT32_MAX` — граница типа, не ёмкость платы.
- `max_image_bytes`, block 1..256, retry limit и timeouts задаются явно.
  Нет production defaults. 256 — только текущий software ceiling; в Classic
  CAN такой блок потребует отдельного ограниченного reassembly. Flash 1 МиБ
  не используется как допустимый размер образа или обоснование A/B.

## Приём, ACK и повторы

```text
RX: IDLE -> RECEIVING -> BLOCK_PENDING -> RECEIVING / COMPLETE_UNVERIFIED
TX: IDLE -> NEED_DATA -> TX_READY -> WAIT_ACK -> NEED_DATA / COMPLETE_UNVERIFIED
active -> FAILED / ABORTED
```

RX сначала копирует блок в собственный фиксированный буфер и возвращает
PENDING. Owner читает pending, выполняет ограниченную локальную работу и
вызывает consume с identity/sequence и результатом. Только успешный consume
продвигает cursor и создаёт ACK. Отказ consumer — sticky failure, без ACK.
Повторный consume уже принятого блока не разрешён.

Точный повтор pending блока оставляет его pending, без ACK. Точный повтор
последнего принятого блока повторяет ACK без нового pending/consume. Сверяются
все поля и payload, а не только CRC; host-тест содержит настоящую CRC collision.
Более старые/будущие блоки и изменённый duplicate отклоняются. У RX только один
retained block: после следующего принятого блока старый не переигрывается.

Pending getter можно повторить до consume; owner обязан не выполнять локальный
consumer дважды. Это не exactly-once durable storage после power loss. Будущий
writer обязан самостоятельно решать идемпотентность, recovery и проверку
сохранённых байтов. Нынешние тесты используют только RAM.

TX копирует одну порцию source в собственный буфер, а повторно передаёт этот
же блок после ack timeout. ACK принимается лишь в WAIT_ACK с точными identity,
next sequence и next offset. Stale ACK не продвигает ни cursor, ни deadline;
ACK до первой отправки, с другим session или будущим cursor не завершает блок.
ACK означает только транспортный cursor; подлинность ACK здесь не проверяется.

CRC-32C/Castagnoli: reflected polynomial `0x82f63b78`, init/xorout `0xffffffff`.
Для логического block checksum обрабатываются boot/transfer/sequence/offset
как LE32, length как LE16 и ровно length payload bytes. Padding, crc field и
неиспользованный tail не входят. Это правило checksum, **не CAN frame layout**.
CRC обнаруживает случайное повреждение; злоумышленник может пересчитать её.
Отдельный тест меняет payload, пересчитывает CRC, завершает транспорт и получает
SHA mismatch от стандартного host OpenSSL. MCU crypto gate пока отсутствует.

## Время, ресурсы и ошибки

Время — monotonic uint32 ms modulo 2^32, вызовы через интервалы <2^31 ms.
Обнаруживаемый обратный ход/прыжок >=2^31 — sticky CLOCK failure.
Ограничения: `0 < ack_timeout < progress_timeout <= total_timeout <= INT32_MAX`.
Deadline срабатывает при elapsed **>=** limit, до обработки позднего DATA/ACK/
consume; пакет не скрывает уже пропущенный срок. Owner обязан poll без пакетов.

Только новый успешный consume/ACK сбрасывает progress timer и retry counter.
Duplicate, PENDING, wrong-session и malformed traffic не продлевают сроки.
Общий срок не продлевается никогда. TX допускает `retry_limit` повторных
отправок после первой; RX — столько же duplicates/rejections между progress.
Wrong-session не расходует RX retries, но проверка времени всё равно выполняется.
Счётчик не переполняется даже при limit 255. RX хранит последний ACK до прежних
progress/total deadlines либо исчерпания retries; после этого сеанс FAILED.
TX COMPLETE_UNVERIFIED остаётся terminal, пока owner явно не abort/reinit.

Один owner, без потокобезопасности/reentrancy/ISR. Нет heap, callbacks, I/O,
ожиданий, mutable globals или зависимости от motion/HAL/RTOS/crypto.
Все циклы ограничены block ceiling; CRC bitwise. RX/TX занимают по 384 байта
в локальной host-сборке, compile-time ceiling 512 на объект. ARM object budget
8192 bytes text, data/bss 0; индивидуальный compiler frame <=128 bytes.
Это регрессионные software limits, **не полный stack/RAM/Flash budget target**.
CPU/throughput/physical CAN timeouts пока не измерены.

## Проверки и дальнейшая интеграция

[Evidence](evidence/build/stm32-update-transport/README.md): 17 unit groups,
2049 deterministic memory transfers с loss/corruption/reorder/duplicate/ACK,
10 volatile-reset cuts, truncated/partial consumer и стандартный SHA oracle.
Оба теста входят в root host/LVGL/sanitizer matrix. ARM GCC 14.2.1 компилирует
библиотеку отдельно; CI проверяет imports, code/global/frame budgets и сохраняет
только текст. Это не устройство, raw CAN fuzz или power-loss Flash испытание.

Следующий software этап — контракт независимой bootloader-side проверки
подписанных metadata/length/SHA/hardware/protocol и recovery, до writer/commit.
Production identity/negotiation и bounded raw CAN codec требуют отдельного review;
никакие реальные IDs/bitrate/pins здесь не назначаются.
До hardware остаются [package/RAM/vectors/clock](STM32_STARTUP.md), verified
erase/protection map, backup SHA/independent copy/restore, physical quiescent
feedback/interlocks и [OTA state policy](OTA_ARCHITECTURE.md).

Software init/abort/reconnect/COMPLETE_UNVERIFIED не является physical inhibit,
подтверждением останова, ARM/START или разрешением установки. Независимый
hardware E-stop сохраняется. Откат — revert PR, данные устройств не менялись.
