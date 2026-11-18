# PR-04b проверки, 18 ноября 2026

- Host CTest: 10/10 PASS, весь HMI/STM32/crypto/candidate/native parser набор.
- ASan/UBSan/LeakSanitizer: 10/10 PASS; вне ptrace sandbox, без suppressions.
- Offline Python policy: 10 методов + subtests — PASS.
- Native signature/UTF-8/duplicates/NUL/limits/policy/signed mutations — PASS.
- Каждая из 71 fixture allocation отдельно принудительно отказала: cleanup/output PASS.
- Фикстура parser: 2323 bytes peak; не worst-case, не замер ESP32 heap.
- -fanalyzer native manifest.c, diff --check, links, YAML/heredoc — PASS.
- SHA-256 upstream C/header/MIT license проверен CMake host и SDK.

| SDK | Профиль | App bytes | Entry points linked |
|---|---|---:|---|
| 6.1.1 | headless | 749008 | verification + native manifest |
| 6.1.1 | rev12 | 1363712 | verification + native manifest |
| 5.3.2 | headless | 772816 | verification + native manifest |
| 5.3.2 | rev12 | 1315152 | verification + native manifest |

6.1.1: установленный SDK; 5.3.2: pinned Docker из tools/build_idf.sh.
Команды: ./tools/build_idf.sh PROFILE SDK local (6.1.1) / docker (5.3.2).
readelf -Ws: оба entry point присутствуют, не UND; нет ненеймспейсированных
GLOBAL cJSON symbols. LOCAL static helpers cJSON_* допустимы, не экспортируются.
Headers/chip/16MB/PSRAM/factory 2 МиБ проверены check_idf_build.py; SHA в JSON.

Host: GCC 15.2.0, CMake 4.2.3, Ninja 1.13.2, OpenSSL 3.5.5, LVGL 9.2.2.
Vendor cJSON v1.7.19 MIT, exact SHA-256 check.cmake, namespace 78 public symbols.
cJSON upstream files не менялись. Managed dependency locks сохранены.

Native parser/crypto на ESP32 проверены compilation/linkage, не исполнением.
No HTTP/serial/Flash/installer/rollback/power-loss/migration/hardware tests.
Нет production/test public key в firmware. Private ephemeral keys тестов не
сохраняются в Git/логах/артефактах. Активный layout/config/board/main не менялись.
Сборки рабочего дерева до commit; GitHub checks проверяют точный SHA ветки.
