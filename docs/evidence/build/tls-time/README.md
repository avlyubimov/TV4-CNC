# PR-04h — проверки 18 ноября 2026

Root host CTest15/15 PASS, ASan/UBSan/LeakSanitizer15/15 PASS вне ptrace sandbox,
без suppressions. Новый test ota_time: pure policy/provenance/range/expiry/skew,
forward/backward UTC step, backward/large64-bit monotonic, 2040 epoch, TTL и
skew boundaries, revoke/no revival, unreviewed/invalid policy, UNKNOWN/SNTP source.
Host SDK sampler UNAVAILABLE очищает output; реальный clock не изменяется.

Portable guarded transport: true hint без anchor, clock reader failure, jump
в open/read/consumer/close, expiry in open. Ошибка после полного body всё равно
не возвращает success; open/close сбалансированы, anchor отозвана. Mock I/O,
не реальные сертификаты/TLS/UTC source. Прежние tests сохранены.

| SDK | Profile | App bytes | Workspace | Time/guard/HTTPS linked |
|---|---|---:|---:|---|
| 6.1.1 | headless | 1054224 | 34000 | PASS |
| 6.1.1 | rev12 | 1568928 | 34000 | PASS |
| 6.1.1 | rev12-memory-bench | 1570576 | 34000 | PASS |
| 5.3.2 | headless | 1068624 | 34000 | PASS |
| 5.3.2 | rev12 | 1524624 | 34000 | PASS |
| 5.3.2 | rev12-memory-bench | 1526320 | 34000 | PASS |

6.1.1 — установленный SDK; 5.3.2 — pinned SDK Docker tools/build_idf.sh.
Команды: ./tools/build_idf.sh PROFILE SDK local (6.1.1) / docker (5.3.2).
Readelf: time adopt/check/clock_read, timed fetch и native HTTPS non-UND.
Bench start по-прежнему только в bench profile. Target sizeof34000 на всех6ELF,
size/validation JSON содержат hashes/Flash16MB/PSRAM/factory/profile flags.
Native SDK sampling/TLS adapter compiled/linked, на ESP32 не исполнялся.

Host GCC15.2.0/CMake4.2.3/Ninja1.13.2/OpenSSL3.5.5/LVGL9.2.2.
-fanalyzer time_gate/timed_download/host HTTPS stub, YAML/links/diff PASS.
Main441d6dd Actions: все4 workflow success перед публикацией новой ветки.

Новых network/time-setting tasks, SNTP/DNS/settimeofday/TZ, serial/reset/Flash
или production keys/release/merge не выполнялось. На плате остаётся разрешённая
memory-bench1c38d1e. Defaults/overlays/layout/locks/main/board/UI/Wi-Fi/STM32/CAN/
vendor сохранены. Public internal HTTPS C API получил обязательный anchor
argument; в репозитории единственный прямой host call обновлён.

Не измерены SDK sample-pair jitter/false rejects, real UTC provenance, network
clock attacks/CA/TLS failure, TLS peak/internal headroom и физические latency/
soak targets. Policy defaults и UTC source отсутствуют; production TLS не включён.
SNTP_UNAUTHENTICATED label не является authentication/NTS. Это gate/guard,
не готовый time synchronization service. Результаты рабочего дерева до commit;
точный SHA CI — на ветке/PR.
