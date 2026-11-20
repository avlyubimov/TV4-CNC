# PR-05b — release policy evidence

20 ноября2026, Asia/Almaty. База `455ccc624baeef502c8c05aeedf00b02a2bf0856`,
PR #1–33 слиты, пять main workflows PASS. Проверен modified worktree;
источники связаны [SHA-256](source-hashes.json), версии — [versions.json](versions.json).
[План](../../../plans/PR05B_RELEASE_POLICY_REVIEW.md),
[review/границы](../../../RELEASE_POLICY_REVIEW.md).

| Проверка | Реальный результат | Evidence |
|---|---|---|
| Root host/LVGL/logic | **29/29 PASS**,2.52s | [CTest](host-ctest.txt), [полный log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **29/29 PASS**,5.35s, без suppressions | [CTest](sanitizer-ctest.txt), [полный log](sanitizer-tests.log) |
| Общий native/offline signed corpus | **115 cases,16 accept/99 reject**; candidate fields совпадают; rejected native output zero | [conformance.txt](conformance.txt) |
| SemVer / Base64 | **2809 comparisons /130 cases PASS** | conformance.txt |
| Unicode | **65536 BMP escapes**,63487 accepted,420 non-hex substitutions и surrogate/truncation tests PASS | [json-escape.txt](json-escape.txt) |
| GCC15.2.0 actual-source -fanalyzer -c | **3 units PASS**,warnings as errors | [static-analysis.txt](static-analysis.txt) |
| ESP-IDF6.1.1 headless | **Build/config PASS**,app1056240bytes,PSRAM disabled | [validation](idf-6.1.1-headless.json), [build](idf-6.1.1-headless.log) |
| ESP-IDF6.1.1 rev12 | **Build/config PASS**,app1574384bytes,octal PSRAM80MHz | [validation](idf-6.1.1-rev12.json), [build](idf-6.1.1-rev12.log) |
| Old main regressions | **Expected failures reproduced**; old JSON gate accepts malformed signed chip; old offline accepts pad-bit mutation | [baseline-details](baseline-details.txt), [initial failing test](baseline-regression.txt) |

29 =27 main suites +Unicode oracle +signed conformance. Existing STM32 motion/
startup/DATA/outcome/admission/source/handoff/profile suites включены в root runs.
Новый standalone/ARM build STM32 локально не выполнялся: STM32/protocol source
не менялся, отдельные ARM/source/image/output gates остаются в CI. Все8 SDK
profiles сохранены, включая5.3.2; их PR CI результаты смотреть отдельно, не
выводить из двух local6.1.1 builds. Остальные workflow тоже не ослаблены.

Оба local ESP builds относятся к modified455ccc6 / PROJECT_VER455ccc6-dirty,
не к опубликованному релизу или согласованному образу для устройства. Полные
логи содержат стандартную подсказку IDF о flash, но исполнены только build/
config validation. Factory2MiB/Flash16MiB, framebuffer1/PCLK16MHz, DMA reserve64KiB,
main stack16384, TLS worker6144, USB_SEL0 и dependency locks прежние. Код
`json_guard` одинаков на host/SDK; фактическое исполнение новых проверок на MCU,
latency/CPU/heap/soak не измерялись. Allocation fixture peak2323/71 прежний,
не worst-case budget.

Новый C driver — только host tests, key создаётся в его памяти. Python видит
public DER/signatures, временно сохраняет только public metadata/image fixtures,
вызывает реальный OpenSSL offline verifier. Private key не сериализуется.
Эта гарантия относится к новому corpus; существующие host unit tests не заменены.
Baseline replay использует original json_guard из main455ccc6 и original offline
tool; helper/source/binaries остаются ignored local test artifacts.

## Воспроизведение

`TV4_LVGL_SOURCE`: exact9.2.2 commit7f07a129e8d77f4984fff8e623fd5be18ff42e74.
`TV4_MBEDTLS_SOURCE_ARCHIVE`: official3.6.7 archive SHA из origin.json. Optional
cache override всё равно проверяет SHA; CI default скачивает тот же archive.

```bash
set -euo pipefail
cmake -S . -B build-host -G Ninja \
  -DLVGL_DIR="$TV4_LVGL_SOURCE" \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE"
cmake --build build-host -j4
ctest --test-dir build-host --output-on-failure

cmake -S . -B build-sanitized -G Ninja \
  -DLVGL_DIR="$TV4_LVGL_SOURCE" \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE" \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'
cmake --build build-sanitized -j4
ctest --test-dir build-sanitized --output-on-failure

TV4_IDF_PATH="$HOME/.espressif/v6.1.1/esp-idf" ./tools/build_idf.sh headless 6.1.1 local
TV4_IDF_PATH="$HOME/.espressif/v6.1.1/esp-idf" ./tools/build_idf.sh rev12 6.1.1 local
```

Exact analyzer commands: `.github/workflows/host.yml`, actual `-c` for
json_guard/envelope/manifest, без diagnostic suppressions. Sanitizers запускались
вне ptrace; ASAN_OPTIONS/LSAN_OPTIONS/UBSAN_OPTIONS не установлены, logs без
ASan/LSan/UBSan diagnostics. Shell Python3.14.4 и CTest3.14.6 записаны отдельно.

Dependencies/licenses прежние: cJSON1.7.19 MIT; LVGL9.2.2 MIT;
independent STM32 Mbed TLS3.6.7 standard sources / Apache-2.0 selected profile;
SDK6.1.1 и backup5.3.2 images/locks неизменны. Application license/release gates
открыты. Build/test logs не содержат private keys/passwords/Flash dumps.

## Аппаратные границы

Owner SWD/ST-Link V2/Cortex-M4r0p1/rawID0x101f6413/Flash1024KiB/3,252V/
backup1048576bytes/vector structure PASS — прежние owner facts, не новые чтения.
20ноября повторён только VGT6; full marking/HSE/RAM/CCM/IRQ/vector/erase/
protection/pinout/GPIO/CAN/restore всё ещё unknown. Нет actual F407 image/profile,
Flash reservation, CAN codec/IDs/bitrate, installer/commit/jump или drive outputs.
На ESP32 ordinary8f0d899. Нет device/SWD/UART/reset/flash/option bytes/network
bench/backup reads, production keys/tag/release или самостоятельного merge.
