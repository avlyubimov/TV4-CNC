# PR-04j — проверки UTC/TLS bench без устройства

Host CTest17/17 PASS, ASan/UBSan/LeakSanitizer17/17 PASS вне ptrace sandbox
без suppressions. New HEAD lifecycle mocks: time/deadline/open/header/status/
cleanup/final clock jump/cancel, no redirects и host starter stub off.
Profile/CA bundle/mutual exclusion negative tests PASS; прежние tests сохранены.
Ни реальных SNTP/TLS/CA, ни clocks/network settings в host tests нет.

| SDK | Profile | App bytes | UTC/TLS worker | Build/validation |
|---|---|---:|---|---|
| 6.1.1 | headless | 1055488 | off | PASS |
| 6.1.1 | rev12 | 1573568 | off | PASS |
| 6.1.1 | rev12-memory-bench | 1575216 | off | PASS |
| 6.1.1 | rev12-tls-bench | 1577024 | on | PASS |
| 5.3.2 | headless | 1069920 | off | PASS |
| 5.3.2 | rev12 | 1529280 | off | PASS |
| 5.3.2 | rev12-memory-bench | 1531040 | off | PASS |
| 5.3.2 | rev12-tls-bench | 1532848 | on | PASS |

Target workspace34000 на всех8ELF. Worker и уникальный HEAD user-agent только
в двух TLS images; memory worker только в двух memory images; ordinary off.
Probe/SNTP/time/native HTTPS symbols linked/non-UND. Validation JSON: hashes,
Flash16MB/PSRAM/factory/profile flags; size JSON из ELF constant.

6.1.1 установленный SDK, 5.3.2 pinned Docker tools/build_idf.sh. Commands:
./tools/build_idf.sh PROFILE SDK local (6.1.1) / docker (5.3.2).
PROFILE=headless/rev12/rev12-memory-bench/rev12-tls-bench. Compile-only.
Both SDKs provide heap local minimum monitor APIs. On6.1.1 TLS details use
client getter, on5.3.2 official example DISCONNECTED error-handle pattern;
-1 means unavailable details on5.3.2, not certificate success.

Host GCC15.2.0/CMake4.2.3/Ninja1.13.2/OpenSSL3.5.5/LVGL9.2.2.
-fanalyzer portable probe/host native stub, YAML/links/diff PASS.
Main3c113de Actions все4 workflow success до публикации этой ветки.

Native networking/UTC update/certificate checks/heap windows/stack/latency на
плате не исполнялись. Live probe pass и production memory budget UNKNOWN.
Local monitor includes concurrent HMI/Wi-Fi/bookkeeping, not exact TLS attribution;
existing resources minimum temporarily windowed, historical tracking restored
после stop. SDK calls not hard-preemptible; timeouts reject late result.

Новых serial/reset/flash/network probes не выполняли, на плате last confirmed
memory-bench1c38d1e. Existing defaults/rev12/memory overlay/layout/locks/board/
UI/Wi-Fi/STM32/CAN/vendor/timings сохранены; main adds compile-gated scheduling.
Нет downloaded body/manifest/image, installer/boot selection/eFuse/keys/release/
auto-merge. Private2026 time policy только explicit bench, not production.
Следующий board/network test требует review/CI и отдельного разрешения.
Результаты рабочего дерева до commit; точный SHA новой CI — на ветке/PR.
