# Независимая подготовка STM32 bootloader

Наследовать корневые правила и аппаратные blockers. Читать
docs/STM32_IMAGE_ADMISSION.md и docs/OTA_ARCHITECTURE.md.

- PR-07b — только read-only admission кандидата на ПК/ARM compile. Нет
  board image, MEMORY/Flash addresses, writer, commit, jump, CAN или GPIO.
- Криптография — отдельная Mbed TLS 3.6.7 с проверенным archive SHA-256.
  Не вызывать ESP32 OTA verifier и не использовать вывод HMI/CRC как доверие.
- Experiment metadata schema не является production release/CAN contract.
  Никаких actual keys/context/layout/version defaults. Fixture IDs — только tests.
- Все результаты fail-closed; VERIFIED_CANDIDATE не install permission и не
  physical quiescent/recovery proof. После reset/изменения storage проверять снова.
- Private test keys генерировать в памяти host tests, не сохранять/публиковать.
- Стандартный buffer allocator имеет единственного owner. Нет ISR/reentrancy,
  threads, libc heap fallback или hardware RNG. Arena — software test budget,
  не подтверждённый резерв RAM платы. CPU/stack/latency ещё измерять на стенде.
- Любые реальные device/Flash/reset/serial/option bytes и merge — отдельно
  разрешённые операции; данный этап их не выполняет.
