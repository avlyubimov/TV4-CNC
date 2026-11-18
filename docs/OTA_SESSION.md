# PR-04e — read-only OTA session

После PR #12 переносимые части объединены в одну синхронную single-owner
операцию `tv4_ota_session_run`. Это не фоновый OTA Manager, не installer и
не код, вызываемый app_main/UI. Новых сетевых запросов при запуске HMI нет.

## Контракт и критерии приёмки

1. Передать immutable request, локальный trusted context и sequential transport.
   Все указатели/строки/context должны жить вне workspace весь вызов.
2. Скачать manifest.json ≤16384 bytes и manifest.sig ≤2048 bytes с того же tag.
   Endpoint envelope выводится из валидированного URL, не из downloaded fields.
3. Проверить envelope/подпись/native policy; release_version должен совпадать
   с выбранным tag. Неподписанный/несовместимый кандидат не скачивает image.
4. Metadata-only заканчивается CANDIDATE_READY. По verify_image выполняется
   read-only stream; IMAGE_VERIFIED означает digest/format, не bootability,
   одобрение оператора или готовую установку. Полный image в RAM не хранится.
5. Общий monotonic deadline покрывает все downloads и verification; каждой
   HTTP операции передаётся оставшийся бюджет с уменьшенным idle/I/O timeout.
   Есть cancel checks до/после операций и в consumers; ошибки закрывают hash
   contexts. Deadline не гарантирует жёсткую preemption blocking DNS/TLS SDK.
6. На отказе candidate и metadata очищены, result сохраняет failed_stage и
   конкретные HTTP/policy/image причины. Cancel имеет отдельный terminal state.
   При успехе остаётся authenticated candidate; metadata/HTTP workspace очищены.
   Повторный run запрещён до reset; reset — только владельцем вне run.

Session не thread-safe: ни UI, ни другая задача не читают/не сбрасывают её во
время run. Будущий worker обязан публиковать отдельные bounded snapshots,
владеть cancel token и проверенным time provider. fetch callback — доверенный
transport adapter, обязан соблюдать HTTPS contract и ту же cancellation source;
тестовый adapter использует настоящий tv4_http_fetch с mock HTTP I/O.
Нативный tv4_https_read подключается будущим worker, а не этим PR.

Workspace bounded, около 34 КиБ (точный host sizeof в evidence); не размещать
его на маленьком FreeRTOS stack. Перед runtime wiring измерить internal heap,
PSRAM и TLS peak, выбрать отдельное allocation/lifetime решение. Session не
аллочит workspace сама; backend crypto/TLS/cJSON используют bounded-by-input heap.

## Проверки

[Evidence](evidence/build/ota-session/README.md). End-to-end тест использует
RAM-only ephemeral signing key, подписанный fixture manifest, mock transport
с настоящим HTTP policy engine и 480-byte ESP image. Включены metadata-only,
все fetch stages fail/cancel, tampering, corrupt image, truncations, oversize
consumer, unknown revision/time, URL/tag mismatch, overall deadline, wrap,
terminal/reset/cleanup. Другие тесты не удаляются.

SDK 6.1.1/5.3.2 headless/rev12 compile/link, CI retains entry point. Подтверждения
TLS на реальной сети, heap/stack/latency/soak и bootability нет. Factory layout,
SDK configs/locks/main/board/STM32/CAN/vendor и зависимости сохранены.
Откат — revert PR; persistent state и устройство не менялись.

## Следующий аппаратный этап

[Bench gate](HMI_BENCH_GATE.md). До runtime network/UI integration нужны
проверенные Wi-Fi/DHCP/time и memory baseline; перед испытанием агент сообщает
владельцу конкретный SHA, команды и критерии, получает отдельное разрешение.
Нет production trust/version provisioning, discovery, release, A/B migration,
Flash writes/boot activation/self-test/rollback. Нынешний active factory layout
не выдаётся за совместимый OTA target; тестовый A/B context только fixture.
