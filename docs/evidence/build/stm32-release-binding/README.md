# PR-07g — software release binding evidence

21 ноября2026, Asia/Almaty. База main `33812368cb70e782389c30dfb77b2d7f61b39e9e`,
PR #1–36 слиты, открытых PR перед работой нет; [main CI5/5](base-ci.json).
[Контракт](../../../STM32_RELEASE_BINDING.md), [план](../../../plans/PR07G_RELEASE_BINDING.md),
[source hashes](source-hashes.json), [точные версии/лицензии](versions.json).

| Проверка | Результат | Evidence |
|---|---|---|
| Root host/LVGL | **33/33 PASS**,3.30s | [CTest](host-ctest.txt), [full log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **33/33 PASS**,7.45s; default env, без suppressions | [CTest](sanitizer-ctest.txt), [full log](sanitizer-tests.log) |
| Standalone bootloader/protocol | **10/10 PASS**,0.80s | [full log](standalone-tests.log) |
| Новый signed release API | **8 groups PASS**: actual P-256/SHA, selection/version/channel/target, unknown context, schema, source/crypto faults, alias/snapshot/reentry/cleanup | [groups](case-groups.txt) |
| Independent bounded SemVer | **2745 comparisons PASS**, C policy vs Python oracle; official precedence chain, uint32/31-byte bounds, numeric prerelease | case-groups.txt |
| Старые signed full chains | **6 negotiation→DATA→P-256/SHA→vectors→close→outcome +3 negative**, handoff19 groups PASS | case-groups.txt |
| Caller storage | Workspace**17664** прежний; новый candidate224/policy288 host bytes;604-byte wide-field document и signed padding до1024 | case-groups.txt |
| ARM4 archives | **40230text/data8/bss27**,+1793text; прежний64KiB ceiling/imports PASS | [budget](arm-budget.json), [admission objects](admission-size.txt) |
| Individual compiler frames | Engine984, new wrapper592, legacy shim16; max admission984 прежний | [frames](admission-frames.txt) |
| Actual-source GCC analyzer | **6 units PASS**, warnings as errors, actual `-fanalyzer -c` | [static analysis](static-analysis.txt) |
| Independent Mbed TLS3.6.7 source | **178 files PASS**, pinned tree/archive | [source](source-check.txt) |
| Deployable board/output gates | Expected rejection **PASS**, synthetic marker removed | [board gate](board-gate.txt), [output gate](output-gate.txt) |
| Local documentation | Links validated | [check](docs-check.txt) |

33 =31 main suites +release admission/version conformance; standalone10 =8 main
suites +те же2. Новая проверка использует только ephemeral OpenSSL test private
keys в памяти; embedded verifier — отдельная pinned Mbed TLS. Нет сохранённых
ключей или обращения к ESP32 verifier. Все старые tests также выполнены.

Новый API получает raw document и image stream. Полностью signed, но неверный
release/source/channel/target, старая SemVer с новым counter и старый counter с
новой SemVer отвергаются. Changed bytes/signature/length/EOF, timeout/backward/
wrap/cancel/OOM и legacy↔new/new↔new BUSY проверены. Inputs меняются после snapshot
в callback, результат сохраняет оригинальные verified значения. Ошибки не
публикуют candidate; invalid alias/BUSY не меняют защищаемую caller storage.

Это **не** новый полный live coordinator chain: existing handoff/completion
продолжают принимать старый strict11-field draft. Новому standalone15-field API
ещё нужны reviewed lease/context/vector/deadline adapter и production bundle
linkage. Legacy/new formats проверяют domain separation без fallback.

Object/frame totals — static libraries, не board image или полный call-stack/
RAM/CCM/CPU/latency measurement. Добавленный shim16 и wrapper592 должны входить
в будущий full-stack review; прежний max984 не доказывает прежний total stack.
Новых mutable globals/heap imports/arena нет; общий single owner сохранён.

## Воспроизведение

LVGL source: exact9.2.2 commit7f07a129e8d77f4984fff8e623fd5be18ff42e74.
Mbed TLS source archive: official3.6.7/SHA из third_party/mbedtls/origin.json;
cache override проверяется тем же SHA/source tree checker. Commands из repo root:

```bash
set -euo pipefail
cmake -S . -B build-host -G Ninja -DLVGL_DIR="$TV4_LVGL_SOURCE" \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE"
cmake --build build-host -j4
ctest --test-dir build-host -V

cmake -S . -B build-sanitized -G Ninja -DLVGL_DIR="$TV4_LVGL_SOURCE" \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE" \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'
cmake --build build-sanitized -j4
ctest --test-dir build-sanitized -V

cmake -S firmware/stm32-bootloader -B build-boot-host -G Ninja \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE"
cmake --build build-boot-host -j4
ctest --test-dir build-boot-host --output-on-failure

cmake -S firmware/stm32-bootloader -B build-boot-arm -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/firmware/stm32-motion/cmake/arm-none-eabi.cmake" \
  -DCMAKE_BUILD_TYPE=MinSizeRel -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE"
cmake --build build-boot-arm -j4
python3 tools/check_boot_admission_build.py build-boot-arm
```

Actual-source analyzer/gates — `.github/workflows/bootloader.yml`; six units,
compiler version/PASS messages сохраняются в text artifact. Прежние motion/
transport/startup/invalid-layout/output gates остаются в CI. Все8 ESP-IDF
profiles сохранены. Local ESP builds в этом STM32-only этапе не повторялись:
HMI source/config/locks/partition/runtime прежние; точные PR CI результаты
смотреть отдельно в Checks. Host/ARM PASS не означает ESP/device PASS.

Trailing whitespace в сохранённых text logs нормализован; test/diagnostic content прежний.
Sanitizers выполнены вне ptrace, без options/suppressions; logs проверены на
ASan/LSan/UBSan diagnostics. Local Ubuntu GCC15.2.0/ARM14.2.1 и Binutils provenance
отличаются от official pinned CI ARM14.2.Rel1; обе явно записаны в versions.json.
LVGL9.2.2/cJSON1.7.19/Mbed TLS3.6.7/ESP SDK locks и лицензии прежние.

Owner SWD/ST-Link V2/core r0p1/rawID0x101f6413/Flash1024KiB/3,252V/
backup1048576bytes/vector structure PASS сохранены. HSE/full marking/RAM/CCM/
IRQ/erase/protection/GPIO/CAN/restore unknown. На ESP32 ordinary8f0d899.
Нет новых devices/SWD/UART/reset/Flash/backup reads, raw wire/actual MCU map/
writer/commit/jump, production versions/keys/defaults, релиза/тегов или merge.
