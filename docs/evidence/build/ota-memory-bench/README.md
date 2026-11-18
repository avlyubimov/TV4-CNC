# PR-04g — проверки 18 ноября 2026

Root CTest 14/14 PASS, ASan/UBSan/LeakSanitizer 14/14 PASS вне ptrace sandbox
без suppressions. Native bench worker не исполнялся. Host проверяет только
profile artifact gates и pure buffer helper/guards, не реальную PSRAM.

build_validation: headless/rev12 отказывают при bench flag on; bench profile
требует flag on + Rev1.2/PSRAM. memory tests: guard active/hash/null, buffer
clear и adjacent field canaries; прежние allocator/fault/cleanup/retry tests.
C volatile helper гарантирует CPU writes/reads, не uncached/whole-memory test.

| SDK | Profile | App bytes | Probe flag / linked worker | Build/validation |
|---|---|---:|---|---|
| 6.1.1 | headless | 1052624 | off | PASS |
| 6.1.1 | rev12 | 1567344 | off | PASS |
| 6.1.1 | rev12-memory-bench | 1569008 | on | PASS |
| 5.3.2 | headless | 1066992 | off | PASS |
| 5.3.2 | rev12 | 1522976 | off | PASS |
| 5.3.2 | rev12-memory-bench | 1524672 | on | PASS |

Target sizeof=34000 bytes на всех шести ELF. Native linked bench start/helper
есть только в двух bench images; в четырёх ordinary images start отсутствует.
Final completed/pass summary string проверена в обоих bench binaries.
Size JSON читается check_ota_memory.py; validation JSON: profile gate, hashes,
16MB Flash, PSRAM, factory bounds. Active factory остаётся 2 МиБ, не A/B.

6.1.1 — локальный установленный SDK; 5.3.2 — pinned SDK Docker из build_idf.sh.
Команды: ./tools/build_idf.sh PROFILE SDK local (6.1.1) / docker (5.3.2).
PROFILE=headless/rev12/rev12-memory-bench. Скрипт compile-only, без устройств.
Host GCC15.2.0/CMake4.2.3/Ninja1.13.2/OpenSSL3.5.5/LVGL9.2.2.
-fanalyzer pure helper/host stub, workflow YAML, changed-doc links, diff PASS.
Main e804cb9 Actions — все четыре workflow success перед публикацией ветки.

SDK native task/headroom/stack/PSRAM cache behavior, allocation/pattern/free
latency, UI responsiveness во время пробы и completed=3 pass=1 на плате UNKNOWN.
Ни новых serial/reset/flash, ни HTTPS/session_run/installer/motion/keys/releases
не выполняли. На плате остаётся b93cf3b. Probe numbers не production TLS policy.
Existing sdkconfig defaults/rev12, partition layout, board/UI/Wi-Fi/STM32/CAN/
vendor/locks/timings сохранены; main добавляет только compile-gated scheduling.
Следующая запись bench image/reset/UART требует отдельного разрешения после
review/CI и проверки backup. До этого аппаратного PASS нет.
Логи рабочего дерева до commit; точный SHA CI — на ветке/PR.
