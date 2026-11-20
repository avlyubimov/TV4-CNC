# PR-07a — ограниченный сеанс передачи образа

База: main `74a7af9` после PR #28. Все четыре workflow main PASS.
Продолжение владельца разрешает программную работу и отдельный PR;
подключение к устройствам, прошивка, release и merge не выполняются.

## Результат

- Независимая portable C11 библиотека в `protocol/can`: приёмник и отправитель
  stop-and-wait, один блок в полёте, фиксированная память без heap/I/O.
- Логические boot/transfer identity, относительные offset/sequence/length,
  CRC-32C, exact duplicate handling, bounded retries и два deadline.
- Двухфазный приём: ACK только после явного завершения локального consumer;
  повтор не отдаёт блок consumer второй раз. Все terminal errors sticky.
- Конечный статус только COMPLETE_UNVERIFIED. Нет Flash addresses, writer,
  COMMIT/jump, production CAN framing/IDs/bitrate или разрешения motion.
- Unit, memory loopback, loss/reorder/corruption/reboot/timeout tests,
  sanitizer, ARM compile/static/object budgets; текстовые evidence и CI.

## Критерии

Побайтовое соответствие принятого потока, единственная доставка consumer,
ограниченные повторы без продления deadline дубликатами, отказ старых session/
ACK и неполных передач. Отдельный тест показывает: изменённый образ с заново
вычисленной CRC может закончить транспорт, но не становится проверенным.
SHA-256 используется стандартным OpenSSL только в host test oracle.

## Следующие зависимости

Подписанный manifest/bootloader-local crypto, независимый fresh quiescent gate,
reviewed memory/sector/protection/recovery contract и wire specification
нужны до любого installer. Эта библиотека не заменяет ни один из этих gates.
