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

## PR-07f — negotiated transport fixture

Читать docs/STM32_UPDATE_NEGOTIATION.md. Production admission/handoff/completion
не меняются; host tests начинают свежий DATA из one-time negotiated params,
затем выполняют настоящие independent P-256/SHA/model/cleanup/outcome проверки.
Opaque request/tag/counter не подменяет их. Нельзя зашивать fixture epochs или
принимать HMI handshake success как signature/install/physical permission.

## PR-07g — independent signed release document

Читать docs/STM32_RELEASE_BINDING.md. Только opt-in release-draft-1 API через
тот же single-owner bounded Mbed TLS engine. Legacy draft strict11 fields/API
сохраняются; новый15-field draft не утверждает production HMI/CAN/release ABI.
Полные raw signature/SHA/EOF обязательны; selected release/source/channel и
installed/minimum component SemVer — local inputs, numeric counters отдельно.
Нет defaults/provisioning/перевода git display версии в trusted context.
Source commit signer claim не является build provenance proof. Новый standalone
candidate не live receipt/install token; PR-07h ниже добавляет explicit coordinator.
Не менять workspace/dependency/heap/crypto owner или старые lifetime/vector/
deadline gates. Нет actual board profile/map/writer/keys/runtime/device actions.

## PR-07h — release-aware source/completion adapter

Читать docs/STM32_RELEASE_HANDOFF.md. Explicit local API/mode, новый caller-owned
adapter поверх того же core. Legacy positive APIs/clear не обходят release mode;
legacy poll/abort/close только conservative guards/cleanup. Shared helpers private,
нет caller verifier callback/success boolean и новых crypto/arena/heap instances.
Полные release signature/selection/SemVer/image checks + live source/RX/owner/
deadline/vector gates, raw metadata digest из negotiation, clean close до outcome.
Adapter/core/RX/context один owner/disjoint/alive до clear. Пока completion active,
не выполнять независимые adapter/core operations, отменять completion_abort.
Не терять held handle при retries/quarantine, не продлевать DATA lifetime.
Production bundle/local context/backend/runtime/hardware/installer gates открыты.
