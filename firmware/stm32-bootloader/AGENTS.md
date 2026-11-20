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

## PR-07c — local transport/source handoff

Читать docs/STM32_UPDATE_HANDOFF.md. Только read lease, borrow live receiver,
boot/transfer/policy/source generations и существующая независимая crypto.
Immutable/exclusive snapshot гарантирует reviewed backend, не generation число.
Не терять handle при release failure; bounded retries/quarantine без force reset.
Receipt временный, не install/ARM/START token; copied receipt нельзя считать
действительным без проверки живого coordinator. RX deadline не продлевать.
Нет storage writer/Flash layout или physical quiescent proof. Memory mock не
закрывает persistent recovery/physical inhibit/операторское подтверждение.

## PR-07d — explicit memory/vector model

Читать docs/STM32_IMAGE_PREFLIGHT.md. PROFILE_MODEL_MATCH не bootability и не
board/Flash/install permission. Только read-only prefix inspection после crypto
из того же lease, signed memory_profile/local policy generation binding.
Нет actual F407 profile/default addresses/vector count/alignment. Generic ARM
minimum не подставлять вместо device-specific VTOR review. Test maps не
переносить в firmware/Flash layout. Errors отзывают candidate, cleanup соблюдает
прежний held-resource/retry/quarantine contract. Hardware gates не закрывать.

## PR-07e — completion history

Читать docs/STM32_UPDATE_OUTCOME.md. Completion выполняет настоящие crypto/model
checks и закрывает lease, не принимает old receipt/success boolean. Отчёт —
история; pending release не positive, quarantine держит ownership и запрещает
clear/restart. Borrowed context живёт до конца wrapper start/poll (включая post-close
guard). Peer outcome/ACK не аутентифицированы и не install/quiescent/ARM authority.
Никаких runtime/Flash/wire/real MCU profile actions; будущий install проверяет
saved bytes и свежие physical/operator/recovery условия заново.
