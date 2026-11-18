# PR-04f — проверки 18 ноября 2026

Host CTest 14/14 PASS; ASan/UBSan/LeakSanitizer 14/14 PASS вне ptrace sandbox,
без suppressions. Прежние UI/Wi-Fi/STM32/crypto/policy/HTTP/session tests сохранены.
memory-tests.log: admission/lifetime/fault/overflow/region/retry cleanup PASS.
Host workspace 34024 bytes, target workspace **34000 bytes**.

Проверены synthetic local floors (не production values), unreviewed/invalid
policy, before/after snapshot failure, insufficient total/contiguous internal/
PSRAM memory, allocator failure, wrong region/start/end, post-allocation reserve
change, BUSY active stage release, idle/terminal hash cleanup и 100 retry cycles.
Нет настоящего PSRAM backend в host; SDK adapter не исполнялся на плате.

| SDK | Profile | App bytes | Target sizeof | Memory/session/HTTPS linkage |
|---|---|---:|---:|---|
| 6.1.1 | headless | 1052624 | 34000 | PASS |
| 6.1.1 | rev12 | 1567344 | 34000 | PASS |
| 5.3.2 | headless | 1066992 | 34000 | PASS |
| 5.3.2 | rev12 | 1522976 | 34000 | PASS |

6.1.1 — установленный ~/.espressif/v6.1.1/esp-idf; 5.3.2 — pinned SDK Docker
из tools/build_idf.sh, без device passthrough. Команды:
./tools/build_idf.sh PROFILE SDK local (6.1.1) / docker (5.3.2).
validation JSON: hashes/Flash 16MB/PSRAM/factory bounds. size JSON извлечён
check_ota_memory.py из linked const uint32 object в ELF32 Xtensa file-backed
section, без выполнения firmware или дополнительного BSS allocation.
Static_assert/CI limit ≤36KiB; точный размер не является runtime heap peak.

Host: GCC 15.2.0, CMake 4.2.3, Ninja 1.13.2, OpenSSL 3.5.5, LVGL 9.2.2.
-fanalyzer memory.c, workflow YAML, changed Markdown links, diff --check PASS.
Production dependency/config/locks/layout/main/UI/Wi-Fi/board/STM32/CAN/vendor
сохранены. Новые worker/network calls/Flash/boot selection не добавлены.

На плате остаётся b93cf3b; новых serial/reset/прошивок не выполняли. Baseline
largest internal block 12288/min 7796 не меняется этим неактивным кодом.
Нет approved production TLS budget, runtime allocation/TLS peak/latency/cache/
soak measurements. Никаких claims об улучшении FPS/heap. Нет production keys,
релиза, auto-merge. Следующая аппаратная проверка требует отдельного разрешения.
Результаты рабочего дерева до commit; SHA новой CI — на ветке/PR.
