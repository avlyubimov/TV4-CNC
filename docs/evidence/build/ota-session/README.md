# PR-04e — проверки 18 ноября 2026

Root host CTest 13/13 PASS, ASan/UBSan/LeakSanitizer 13/13 PASS
(вне ptrace sandbox, без suppressions). Прежние HMI/STM32/OTA tests сохранены.
Session tests: ephemeral RAM-only P256 key, native envelope/policy, настоящий
HTTP policy engine с mock I/O, synthetic ESP image 480 bytes. Не release firmware.

Проверены metadata-only и signed image success; failure/cancel всех трёх
fetch stages; cancel/deadline после authentication до image; bad signature/
envelope; corrupt digest/header; 29 truncations (step 17, offsets 0..476);
bounded metadata consumer; общий бюджет между стадиями; image idle timeout;
wrap-safe clock, неизвестные time/revision, URL/tag mismatch и terminal/reset.
Hash contexts закрыты, HTTP open/close сбалансированы, candidate/metadata
очищены при отказе. session-tests.log: sizeof workspace 34024 bytes на host.

| SDK | Профиль | App bytes | Session/HTTPS/policy/stream symbols linked |
|---|---|---:|---|
| 6.1.1 | headless | 1052496 | PASS |
| 6.1.1 | rev12 | 1566624 | PASS |
| 5.3.2 | headless | 1066848 | PASS |
| 5.3.2 | rev12 | 1522240 | PASS |

6.1.1 установлен локально; 5.3.2 — pinned Docker из tools/build_idf.sh.
Команды: ./tools/build_idf.sh PROFILE SDK local (6.1.1) / docker (5.3.2).
Validation JSON: hashes, Flash 16MB, PSRAM, factory layout bounds.
Readelf -Ws: entry points linked/non-UND в каждом ELF. SDK код не исполнялся.
Host: GCC 15.2.0, CMake 4.2.3, Ninja 1.13.2, OpenSSL 3.5.5, LVGL 9.2.2.
-fanalyzer session.c, changed-doc links, workflow YAML, git diff --check PASS.
Main e6e8cbd: все четыре workflow success до публикации этой ветки.

Нет реальных TLS/DNS/CA/redirect tests или ESP32 heap/stack/CPU/latency/soak.
Session не выделяется на stack в firmware и не вызывается app_main/UI.
Новых runtime tasks/сетевых requests/Flash writes/boot selection не добавлено.
Active partitions/config/locks/main/board/STM32/CAN/vendor сохранены.
Нет обращения к устройству, production keys, релиза или merge.
[Первый board gate](../../../HMI_BENCH_GATE.md).
Логи рабочего дерева до commit, SHA CI — на ветке/PR.
