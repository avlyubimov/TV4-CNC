# Pending OTA: локальный trust до native confirmation — software PASS

Exact clean source `a74bfcd62ffbc290f6ab5d692a9a40bbed69778b`, отдельный PR #81,
base `c46bd608d67257929a3a627ff5deefa586ec3454`/PR #80. Regression pending без
context падает на прежнем service.c (`regression-before.txt`). После исправления
265/265 host и265/265 ASan/UBSan/LSan, detect_leaks=1, PASS. Actual service
variants0–8 и прежний native installer suite проверяют orchestration и SDK
boundary отдельно, без настоящей Flash/сети. Ошибки ключа/descriptor/policies,
missing partition, factory/VALID recovery и boot без сети покрыты.

Четыре реальные ELF/BIN6.1.1/5.3.2 × configured/unprovisioned собраны с pinned
SDK/dependencies, прошли Flash16MiB/PSRAM/XiP/layout/resource/config guards,
checksum/appended SHA-256 и binding полного ELF SHA из app descriptor.
Source CI30/30 и baseline main15/15 PASS до нового USB-сеанса (`ci.json`).
Actual-source GCC -fanalyzer PASS для трёх configured policy вариантов;
CI повторяет эти проверки без suppressions.

Configured app version `0.1.0-bench.1`, worker12288; unprovisioned worker3072,
SDK6 descriptor `a74bfcd`, SDK5 descriptor `1`. Эти unprovisioned бинарники не
release payload. Private key не использован; local context содержит только
согласованный public key/settings и остаётся вне Git/CI. Ни один из четырёх BIN
не является новым опубликованным релизом или разрешением сетевой установки.

| Configured build | App bytes | Изменение относительно a29c9ed control | Own worker frame |
|---|---:|---:|---:|
|ESP-IDF6.1.1|1601632|+48bytes|80bytes|
|ESP-IDF5.3.2|1554272|+64bytes|80bytes|

Own operation frame480bytes и writable/frame ceilings сохранены. Это compiler
frames, не полный runtime stack. Существующая configured() проверка перемещена
до подтверждения и её результат переиспользован: повторный разбор ключа не
добавляется. Нет новых задач, буферов, Flash APIs, криптографии или eFuse.

[План native slot bench](../../../plans/P1_PENDING_TRUST.md) требует отдельного
hardware evidence. Этот software report не подтверждает OTA INSTALL/rollback,
physical UI, производственный RAM/stack budget или устойчивость к потере питания.
`results.json` содержит точные BIN/ELF hashes, SDK commits/config и frames.
