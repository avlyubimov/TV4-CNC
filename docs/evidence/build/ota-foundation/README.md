# PR-04a проверки, 18 ноября 2026

- Host CTest: 9/9 PASS, включая весь HMI/STM32 набор, crypto и candidate.
- ASan/UBSan/LeakSanitizer: 9/9 PASS, без suppressions/отключения LSan.
- Python candidate suite: 9 методов с дополнительными subtests — PASS.
- GCC -fanalyzer verification.c, git diff --check, Markdown links/YAML — PASS.
- Candidate A/B CSV: оба штатных SDK parser + roundtrip — PASS.

| SDK | Профиль | App bytes | Flash | Crypto symbol linked |
|---|---|---:|---|---|
| 6.1.1 | headless | 740848 | 16 МиБ | yes |
| 6.1.1 | rev12 | 1355312 | 16 МиБ | yes |
| 5.3.2 | headless | 760240 | 16 МиБ | yes |
| 5.3.2 | rev12 | 1302384 | 16 МиБ | yes |

6.1.1: локальный Ubuntu SDK; 5.3.2: pinned Docker image tools/build_idf.sh.
Команды: ./tools/build_idf.sh PROFILE SDK local (6.1.1) / docker (5.3.2).
readelf -Ws ELF подтверждает retained tv4_ota_verify_manifest, не UND.
Header/chip/16MB/PSRAM/действующий factory 2 МиБ проверены существующим
check_idf_build.py. SHA-256 собранных app/bootloader — в JSON рядом.

Candidate: python3 tools/check_ota_layout.py --sdk SDK_PATH; 5.3.2 выполнялся
в immutable SDK Docker с read-only project mount. Результат только в tmp,
активный partitions.csv не менялся. Новые SDK dependencies/locks не добавлялись.

Host: GCC 15.2.0, CMake 4.2.3, Ninja 1.13.2, OpenSSL library/CLI 3.5.5.
SDK 6.1.1: mbedTLS 4.1.1/TF-PSA-Crypto, 5.3.2: SDK mbedTLS.
Crypto API ESP32 проверен сборкой/линковкой, на устройстве не выполнялся.
Host crypto/backend и ephemeral keys не являются production trust provisioning.

Все результаты — для рабочего дерева этого PR до финального commit; новые
GitHub checks относятся к точному SHA ветки. Firmware version/build metadata
может отличаться после commit; опубликованных release binaries здесь нет.
Нет HTTP/Flash/serial/rollback/power-loss/hardware/OTA install испытаний.
Candidate с 512-byte test header — fixture, а не deployable ESP32 app.
