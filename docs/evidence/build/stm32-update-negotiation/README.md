# PR-07f — software negotiation evidence

21 ноября2026, Asia/Almaty. База main `f0bd86606b158591648cd0aabc2846b8cce2a6f0`,
PR #1–35 слиты, пять main workflows PASS. [Контракт](../../../STM32_UPDATE_NEGOTIATION.md),
[план](../../../plans/PR07F_UPDATE_NEGOTIATION.md). Проверен modified worktree;
[source hashes](source-hashes.json), [точные версии](versions.json).

| Проверка | Результат | Evidence |
|---|---|---|
| Root host/LVGL | **31/31 PASS**,2.87s | [CTest](host-ctest.txt), [full log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **31/31 PASS**,6.67s, без suppressions | [CTest](sanitizer-ctest.txt), [full log](sanitizer-tests.log) |
| Standalone bootloader/transport | **8/8 PASS**,0.64s | [standalone log](standalone-tests.log) |
| Negotiation | **7 groups /1024 sequential sessions**; loss/duplicates/stale/clock/context/ownership/counter faults | [groups](case-groups.txt) |
| New signed full chain | **6 success chains +3 negative**; negotiation→DATA→actual P-256/SHA→model→close→outcome/ACK | case-groups.txt |
| Handoff/completion total | **19 groups**, прежние checks сохранены | case-groups.txt |
| New ARM unit | **1930text,data/bss0bytes** | [transport-size](transport-size.txt) |
| All transport objects | **5442text,data/bss0**, прежний8KiB ceiling PASS | transport-size.txt |
| Individual compiler frames | negotiation max40bytes; all transport max64<=128 | [frames](transport-frames.txt) |
| ARM4 archives total | **38437text,data8,bss27**,<=64KiB regression ceiling | [arm-budget](arm-budget.json) |
| Actual-source GCC15.2.0 analyzer | **3 protocol units PASS**; warnings as errors | [static-analysis](static-analysis.txt) |
| Imports | protocol: libc memory helpers only; combined crypto imports allowlisted | [imports](transport-imports.txt), arm-budget.json |
| Pinned independent Mbed TLS3.6.7 | **178 files PASS** | [source-check](source-check.txt) |
| Deployable image/output gates | Expected rejection **PASS** | [board gate](board-gate.txt), [output gate](output-gate.txt) |

31 =30 main suites +update_negotiation; standalone8 =7 main suites +negotiation.
New integration case count6 относится к изолированным RAM fixture worlds с
ephemeral OpenSSL private keys только в памяти, не к настоящим boot epochs.
Independent Mbed TLS проверяет подписи; valid re-signed metadata с другим raw SHA
не проходит старый negotiated binding. Меняющийся DATA CRC не заменяет SHA.
Повторный take не выдаёт descriptor снова; старые REQUEST после clear
не резервируют новые IDs из сохранённого pool. Output values — transport only.

Явный unit test принимает fully matching forged OFFER/CONFIRM без peer: handshake
не является аутентификацией. Pool high-water volatile, его утрата/повтор epochs
после reset не обнаруживаются этим механизмом. Reviewed identity/entropy/
persistence/collision/authentication и runtime owner ещё нужны.

Host storage: coordinator368,pool32,message128bytes. Это caller-owned state,
не full RAM/CCM/stack/CPU/ISR budget. Object totals/individual frames не являются
собранным F407 image или измерением на плате. Настоящая memory/erase map не выбрана.
DATA/outcome source/APIs/deadlines и admission/model/completion production code
не менялись; дополнены только bootloader integration tests.

## Воспроизведение

TV4_LVGL_SOURCE: exact9.2.2 commit7f07a129e8d77f4984fff8e623fd5be18ff42e74.
TV4_MBEDTLS_SOURCE_ARCHIVE: official3.6.7 archive с SHA из origin.json; optional
cache override всё равно проверяется. CI default скачивает тот же pinned source.

```bash
set -euo pipefail
cmake -S . -B build-host -G Ninja -DLVGL_DIR="$TV4_LVGL_SOURCE" \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE"
cmake --build build-host -j4
ctest --test-dir build-host --output-on-failure

cmake -S . -B build-sanitized -G Ninja -DLVGL_DIR="$TV4_LVGL_SOURCE" \
  -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE" \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'
cmake --build build-sanitized -j4
ctest --test-dir build-sanitized --output-on-failure

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

Exact protocol analyzer/frame/import commands: `.github/workflows/motion.yml`.
Actual `-fanalyzer -c` для всех трёх protocol units; не syntax-only. Прежние
STM32 boot/motion/synthetic startup/image/source/output gates сохраняются в CI.
Все8 SDK profiles сохранены. В этом protocol-only этапе local ESP builds не
повторялись: HMI source/config/locks прежние; результаты PR CI смотреть отдельно
в Checks. Нельзя выводить SDK/hardware PASS из ARM compile или host tests.

Sanitizers вне ptrace, default environment без suppressions; logs проверены на
ASan/LSan/UBSan diagnostics. Host GCC15.2.0, local Ubuntu ARM GCC14.2.1/Binutils
отличаются от official CI14.2.Rel1 archive; обе provenance явно в versions.json.
LVGL9.2.2/cJSON1.7.19/Mbed TLS3.6.7/SDK locks и лицензии прежние.

Owner SWD/ST-Link V2/Cortex-M4r0p1/rawID0x101f6413/Flash1024KiB/3,252V/
backup1048576bytes/vector structure PASS сохранены без новых чтений. Full marking/
HSE/RAM/CCM/erase/protection/GPIO/CAN/restore unknown. На ESP32 ordinary8f0d899.
Нет device/SWD/UART/reset/flash/backup reads, raw wire/CAN IDs/bitrate/codec,
actual profile/Flash address/writer/commit/jump, production keys/release/tag или
самостоятельного merge. Message kind/size и compatibility tag не задают CAN ABI.
