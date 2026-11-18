# PR-03: программные проверки, 18 ноября 2026

Host: GCC 15.2.0, CMake 4.2.3, Ninja 1.13.2, LVGL 9.2.2.

- host-tests.log: 7/7 PASS, XRGB8888/RGB565 layout и новая панель ресурсов.
- sanitized-tests.log: те же 7/7 PASS с ASan/UBSan/LeakSanitizer.
- GCC -fanalyzer для core/diagnostics.c, git diff --check и ссылки — PASS.

| SDK | Профиль | App bytes | Flash | PSRAM |
|---|---|---:|---|---|
| 6.1.1 | headless | 646528 | 16 МиБ | disabled |
| 6.1.1 | rev12 | 1354704 | 16 МиБ | octal 80MHz |
| 5.3.2 | headless | 696304 | 16 МиБ | disabled |
| 5.3.2 | rev12 | 1301936 | 16 МиБ | octal 80MHz |

6.1.1: установленный Ubuntu SDK; 5.3.2: закреплённый Docker image из tools/build_idf.sh.
Команды: ./tools/build_idf.sh PROFILE SDK local (6.1.1) либо docker (5.3.2).
Заголовки app/bootloader, ESP32-S3, 16 МБ, PSRAM и bounds factory 2 МиБ проверены
check_idf_build.py; SHA-256 бинарников в соседних validation JSON. Подтверждены
именно сборки рабочего дерева, не hardware results или release artifacts.

CPU per-core, touch-to-display, scanout, baseline/soak/reconnect/pressure на
плате не измерены. Данные на host кадре искусственные. К USB/плате не подключались,
flash/monitor/erase не выполнялись. Производительность на устройстве не заявляется.
Четыре профиля собраны после окончательных изменений схемы экспорта.
Sanitizers выполнялись вне ptrace sandbox без suppressions или отключения LSan.
