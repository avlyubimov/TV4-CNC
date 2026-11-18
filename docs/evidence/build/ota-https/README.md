# PR-04d — проверки 18 ноября 2026

Host CTest 12/12 PASS, включая прежние UI/Wi-Fi/STM32/crypto/policy/stream.
ASan/UBSan/LeakSanitizer 12/12 PASS вне ptrace sandbox, без suppressions.
http-tests.log: mock-only allowlist/redirect/status/framing/length/consumer/
cancel/idle/total/wrap/cleanup; workspace sizeof=8212 bytes на host.
Envelope: real ephemeral P256 signatures, tampered manifest, bad algorithm/key,
Base64 padding/length, duplicate/extra fields и oversize. Production keys нет.

| SDK | Профиль | App bytes | Envelope/HTTPS/stream symbols linked |
|---|---|---:|---|
| 6.1.1 | headless | 1051024 | PASS |
| 6.1.1 | rev12 | 1565120 | PASS |
| 5.3.2 | headless | 1065360 | PASS |
| 5.3.2 | rev12 | 1520784 | PASS |

6.1.1 — локальный ~/.espressif/v6.1.1/esp-idf; 5.3.2 — pinned SDK Docker
из tools/build_idf.sh, без device passthrough. Команды:
./tools/build_idf.sh PROFILE SDK local (6.1.1) / docker (5.3.2).
Validation JSON содержит app/bootloader SHA-256, Flash 16MB, PSRAM и factory bounds.
readelf -Ws подтверждает linked/non-UND entrypoints в каждом ELF.

Host: GCC 15.2.0, CMake 4.2.3, Ninja 1.13.2, OpenSSL 3.5.5, LVGL 9.2.2.
-fanalyzer (download/envelope/json_guard/host HTTPS stub), diff --check,
workflow YAML и локальные ссылки изменённых docs — PASS.
Матрица main 07007a1: все четыре workflow success перед публикацией ветки.

HTTPS SDK adapter только compiled/linked, не исполнялся на ESP32. Транспортные
тесты используют mock I/O, не реальные TLS/DNS/CA/redirect endpoints. SDK CA
проверка сертификата/hostname включена в коде; real clock provisioning ещё нет.
SDK heap/stack/CPU/HMI latency, сетевой soak и аппаратный recovery не измерены.

Active partitions/config/locks/app_main/board/STM32/CAN/vendor не изменялись.
Ни сетевые requests из app_main, ни Flash writes/boot activation не добавлены.
Нет обращения к плате, production keys, firmware release или auto-merge.
Логи относятся к рабочему дереву перед commit; SHA CI — на ветке/PR.
