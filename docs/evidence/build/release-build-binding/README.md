# PR-05d — build-to-cohort binding evidence

21 ноября2026, Asia/Almaty. База main `aa068336635ab2e3b9c83d67a17ee71d8a821dbb`,
PR #1–40 слиты, открытых PR перед работой нет; [main CI5/5 PASS](base-ci.json).
[Контракт](../../../RELEASE_BUILD_BINDING.md), [план](../../../plans/PR05D_RELEASE_BUILD_BINDING.md),
[точные версии/лицензии](versions.json), [source SHA-256](source-hashes.json).

| Проверка | Фактический результат | Evidence |
|---|---|---|
| Root host/LVGL | **39/39 PASS**,4.13s | [summary](host-ctest.txt), [full log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **39/39 PASS**,8.67s, default env, no suppressions, вне ptrace | [summary](sanitizer-ctest.txt), [full log](sanitizer-tests.log) |
| Standalone bootloader/protocol | **13/13 PASS**,0.85s | [log](standalone-tests.log) |
| Новая root suite | **13 methods PASS**, actual independent crypto/full bytes | [case groups](case-groups.txt) |
| ARM4 libraries | **43344text/data8/bss27**, delta0 | [budget](arm-budget.json), [object sizes](admission-size.txt) |
| Individual frames | **984/376/328/64**, прежние, не full stack | [budget](arm-budget.json), [handoff frames](handoff-frames.txt) |
| Actual-source GCC analyzer | **8 embedded +1 host frontend PASS**, strict `-fanalyzer -c` | [analysis](static-analysis.txt) |
| Pinned Mbed TLS3.6.7 | **178 files PASS** | [source check](source-check.txt) |
| Synthetic ARM link contract | **1/1 PASS**,0.06s; no execution | [contract](motion-link-contract.txt), [inspection](motion-link-probe.json) |
| Scope/board/output gates | **PASS**; host reviewer absent from ARM; board builds reject; marker removed | [scope](host-review-scope.txt), [boot](board-gate.txt), [motion](motion-board-gates.txt), [output](output-gate.txt) |
| Actual audit reports | Correct **blocked** output, all authority false | [paired fixture](paired-blocked-fixture.json), [HMI-only fixture](hmi-only-blocked-fixture.json) |
| Documentation | Local file links checked, source hashes checked | [result](docs-check.txt) |

39 =38 base suites +`release_build_binding`. Standalone13 прежние. Новая suite
проверяет оба SDK×четыре профиля, два независимых RAM-only ephemeral P-256 role
keys, paired/HMI-only, wrong/missing/zero/noncanonical pin, rehashed substituted
source/SDK/schema/policy/layout/app claims, full HMI/ELF bytes, signatures/peer
range, nonregular/FIFO/symlink/oversize/empty/extra files, snapshot mutations,
different valid image между двумя verifier'ами и реальные CLI exit/report paths.

Legacy candidate/cohort/OTA CLI reports сохранены. Candidate pin проверяется
до candidate JSON parsing; build и cohort fields возвращаются из того же
checked snapshot, без reopening для binding. ELF проверен только как digest/
length; ELF format/execution/build origin не подтверждаются. Нет нового native
firmware parser или runtime crypto caller. Host I/O target не попадает в ARM.

## Результат модели и аппаратные ограничения

Оба сохранённых JSON сформированы реальным новым audit из **синтетических**
fixtures. Temporary Git source commit/tree не SHA проекта TV4-CNC; app336 bytes,
ELF23 bytes, Motion769 bytes и test SemVer не настоящие firmware binaries.
Unsigned CI-shaped record намеренно содержит fixture SemVer descriptor claim,
не обычный git-derived PROJECT_VER настоящего CI. Это не provenance/CI artifact
или попытка сделать текущую сборку publishable. Private keys только RAM signer
processes; images/public keys/signatures/temporary Git repo удалены cleanup.

В paired report подписи обоих компонентов проверены, но Motion build evidence
отсутствует. В обоих reports candidate factory-single-2m не совпадает с signed
tv4-16m-ab-v1: `layout_match=false`, blocker явный, status blocked. Image/ELF/
external pin совпадение не разрешает migration/OTA/install/publish. Source/tree/
config/partition/dependency/license hashes — unsigned claims. CI identity и
build provenance остаются false. Новый CLI даёт **exit3**, invalid inputs exit1
с пустым stdout; missing args exit2. Legacy review exit0 не install authority.

Owner F407 SWD/ST-Link V2, core Cortex-M4r0p1, raw ID0x101f6413, Flash1024KiB,
3,252V и backup1048576bytes/vector structure PASS сохранены без новых чтений.
Full marking/HSE/RAM/CCM/IRQ/Flash erase/protection/GPIO/CAN/backup SHA/restore
не подтверждены; Flash capacity не выбирает addresses или RAM/staging budget.
На ESP32 ordinary8f0d899, owner HMI/touch/Wi-Fi PASS. Hardware latency/FPS/
performance/resources/soak не измерялись этим этапом.

## Воспроизведение

С exact LVGL9.2.2 и Mbed TLS3.6.7 source/archive из versions.json:

```bash
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
ctest --test-dir build-boot-host -V

cmake -S firmware/stm32-bootloader -B build-boot-arm -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/firmware/stm32-motion/cmake/arm-none-eabi.cmake" \
  -DCMAKE_BUILD_TYPE=MinSizeRel -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE"
cmake --build build-boot-arm -j4
python3 tools/check_boot_admission_build.py build-boot-arm
```

Actual-source analyzer/scope/board/output commands — existing bootloader workflow;
synthetic probe — motion workflow. SDK matrix8 остаётся без новых SDK/dependency
versions. Local ESP builds не повторялись для этого host-only tooling этапа;
точные итоговые head CI результаты будут в PR Checks. Все SDK jobs теперь
сохраняют self-generated candidate integrity pin и проверяют его через CLI;
pin не независимое owner approval/authentication.

Текстовые логи очищены только от trailing whitespace/лишних финальных пустых
строк; все39/39 rows сохранены. Sanitizer options/suppressions не задавались.
Firmware C/UI/settings/PROJECT_VER/partitions/dependency pins прежние; нет
devices/SWD/UART/reset/Flash/backup rereads/network probes/CAN/motion/installer,
публикации tags/releases/secrets или самостоятельного merge. Откат — revert PR.
