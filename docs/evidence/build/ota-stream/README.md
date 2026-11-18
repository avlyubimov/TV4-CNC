# PR-04c проверки, 18 ноября 2026

- Root host CTest: 11/11 PASS, включая все прежние HMI/STM32/OTA tests.
- ASan/UBSan/LeakSanitizer: 11/11 PASS, вне ptrace sandbox без suppressions.
- Fixture 480 bytes: все 480 truncation points, chunks 1/7/23/24/31/255/256/4096,
  cancel/IO/deadlines/wrap, header/descriptor/checksum/hash/size errors — PASS.
- Host verifier struct sizeof=504 bytes, без image-sized buffer в firmware.
- Readelf: stream feed/finish linked, не UND, на обоих SDK.
- -fanalyzer image_stream.c/hash.c, diff --check, links и workflow YAML — PASS.

| SDK | Профиль | App bytes | Stream actual SDK app (host) |
|---|---|---:|---|
| 6.1.1 | headless | 750928 | PASS |
| 6.1.1 | rev12 | 1365648 | PASS |
| 5.3.2 | headless | 774736 | PASS |
| 5.3.2 | rev12 | 1317088 | PASS |

6.1.1: установленный SDK; 5.3.2: pinned SDK Docker из tools/build_idf.sh.
Команды: ./tools/build_idf.sh PROFILE SDK local (6.1.1) / docker (5.3.2).
Validation JSON содержит app/bootloader SHA-256, 16MB/header/PSRAM/factory bounds.
Native entry points проверены readelf -Ws ELF. Код crypto/image на ESP32 не исполнялся.

actual-sdk-stream.log: host helper по файлам из этих сборок. Он сам вычисляет
expected hash файла и читает его descriptor version. Это format/digest smoke,
не подпись/manifest authorization, не проверка rev12 vs headless и не approval.
Helper использует host malloc полного файла; firmware stream checker этого не делает.
Current app versions git-derived, для production native manifest нужны реальные SemVer.

Host: GCC 15.2.0, CMake 4.2.3, Ninja 1.13.2, OpenSSL 3.5.5, LVGL 9.2.2.
Backend heap на ESP32, CPU/HMI latency, HTTP disconnect/redirect/TLS, native
rollback/power-loss/USB migration и hardware recovery не испытывались.
Нет sink/Flash/boot activation/HTTP/serial calls. Active layout/config/app_main/
board/locks/STM32/CAN/vendor sources сохранены. Production keys не создавались.
Результаты рабочего дерева до commit; точный SHA CI — на ветке/PR.
