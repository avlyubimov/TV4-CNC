# PR-07g — независимая связь release/component/target/image

21 ноября 2026. База main `3381236`, PR #1–36 слиты, открытых PR нет.

1. Добавить отдельный opt-in API с подписанным плоским Motion release draft:
   release/component SemVer, channel/source commit и прежние target/profile/
   counter/protocol/size/SHA поля подписаны одним raw-byte документом.
   Прежний admission-draft-1 parser/API остаётся строгим и совместимым.
2. Требовать явный local installed/minimum component SemVer, выбранный release/
   source/channel и прежний trust/target/counter context. Не переводить numeric
   counters в SemVer; неизвестный context отклонять. Нет реальных версий/ключей.
3. Использовать один existing independent Mbed TLS engine/arena/clock/cancel/
   exact-length/SHA/EOF cleanup. Snapshot входов до callbacks, общий BUSY owner,
   новый candidate только после полной проверки. Размер workspace прежний.
4. Проверить реальные подписи/подмену release/component/source/target/image,
   policy/channel/replay, strict schema/domain, failure/timeout/cancel/reentry/
   alias/snapshot, fragment/EOF/OOM. Сверить bounded SemVer с Python oracle.
5. Host/LVGL/sanitizers/standalone, ARM/static/frame/import/source/image gates,
   прежняя ESP-IDF matrix8; text evidence, docs/status и отдельный PR без merge.

Это experimental software contract, не утверждение production bundle schema,
CAN ABI или доверенного installed context. HMI manifest/negotiation/DATA/outcome/
handoff lifetime и deadline не менять. Новый путь ещё не вызывается coordinator.
Нет runtime/device/SWD/UART/reset/Flash/wire/GPIO/motion/install/jump/release/key
provisioning. Source commit — подписанное заявление signer, не build provenance
proof. Hardware marking/HSE/RAM/erase/protection/GPIO/CAN/recovery gates прежние.

Во время проверки push CI найден старый flaky negative fixture PR-05a: замена
SHA tail на constant `x` иногда не меняла образ. Исправить только fixture на
XOR1, сохранить deterministic RAM reproducer и повторить host/sanitizers;
image verifier и правила приёмки не ослаблять.
