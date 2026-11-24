# Bench.4: точный display-fix release и программные проверки

[План исправления](../../../plans/P1_OTA_DISPLAY_PROGRESS.md), [source PR #85](https://github.com/avlyubimov/TV4-CNC/pull/85),
[выпуск v0.1.0-bench.4](https://github.com/avlyubimov/TV4-CNC/releases/tag/v0.1.0-bench.4).
Исходники `6c7b1d56769fe8a084c4360e6c2df4bb594f58fa`; владелец слил PR в
main `cf8ffb043d2ef1266cbc5277efa130c7e2ef6465`. Тег указывает на точный source commit,
а не на автоматически меняющуюся ветку. Публикация и новые bench contexts отдельно
утверждены владельцем; latest/stable не назначены, bench.2/bench.3 неизменны.

[Результаты](results.json), [все реальные CI jobs](ci.json), [публикация и 11 SHA](publication.json).
Полные274 host/274 ASan/UBSan/LSan с leak detection и actual LVGL invalidation/geometry
проверены в [runtime-fix evidence](../esp32-ota-display-progress/README.md).
После окончательного source commit повторены четыре настоящих ELF/BIN обоих
pinned ESP-IDF6.1.1/5.3.2: receiving `.3` с trace и normal candidate `.4` без trace.
Config/image/memory gates PASS; LCD ISR/bounce fill/DMA EOF в `.iram0.text`.
Source30/main15/tag15 jobs PASS до аппаратной записи. Native C/Python verifier и
stream, в том числе ASan/UBSan/LSan на обоих подписанных BIN, PASS.

SDK6.1.1 app:1600464bytes, SHA-256
`1e3de8c629ceb1800c5ba1f947858a5a54e5cce1587810eaa45f7febf5e27945`.
Inventory SHA-256:
`73d13c76342fefc274da6164919e6f625b8d65db9ba0b1b9e59b146301c17b41`.
Все11 assets повторно скачаны по HTTPS с обычной CA/hostname/date проверкой,
точными длинами/SHA-256; raw manifest signature и реальный app проверены локальным
доверенным публичным ключом. В ZIP161entries,157 исходных vendor notices сверены
побайтно с закреплёнными SDK/toolchain/managed sources;4 metadata entries. Ключи,
contexts, backups, NVS и raw UART не публикуются.

Процедура повтора прошла15 [fault tests](procedure-tests.log),16 [loader/tamper/expiry
tests](loader-tests.log),8 [actual passive UART collector tests](passive-uart-tests.log),
плюс8 проверок native metadata с настоящим pinned SDK method на file-only fake.
Включены KEEP-on-PASS, один conditional full recovery, запреты записи до backup/CI/
approval, byte/time limits, native CRC/state, strict resource/touch guards, partial
UART lines, обычный reboot и полный self-test до ROM, а также отказ короткого boot.
Это host-проверки процедуры; аппаратные результаты описываются отдельно.

Только экспериментальные `.3→.4` contexts до25ноября2026 08:00 Asia/Almaty.
Receiving `.3` — локальный fixture того же display-fix source, не замена published
bench.3. Штатные TLS/подпись/floors/timeouts/worker12288 сохранены. Compile-time date
пока включён; совпадение одного incremental build не доказывает bit reproducibility.
Production budget, runtime5.3.2, cold-power/fault qualification и motion/CAN
этим release не утверждаются.
