# PR-05c — offline release cohort evidence

21 ноября2026, Asia/Almaty. База main `2c06cdc31167a04c293e7a3d06c9ea397809bd52`,
PR #1–39 слиты, открытых PR перед работой нет; [main CI5/5](base-ci.json).
[Контракт](../../../RELEASE_COHORT_REVIEW.md), [план](../../../plans/PR05C_RELEASE_COHORT.md),
[source hashes](source-hashes.json), [версии/лицензии](versions.json).

| Проверка | Фактический результат | Evidence |
|---|---|---|
| Root host/LVGL | **38/38 PASS**,3.32s | [CTest](host-ctest.txt), [full log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **38/38 PASS**,7.71s; default env/no suppressions | [CTest](sanitizer-ctest.txt), [full log](sanitizer-tests.log) |
| Standalone bootloader/protocol | **13/13 PASS**,0.87s | [log](standalone-tests.log) |
| Host Motion frontend | **3 methods PASS**, actual independent signature/full image/policy/cleanup и bounded regular-file faults | [new cases](case-groups.txt) |
| Cohort tool | **9 methods PASS**, разные role keys, paired/HMI-only, stable/prerelease/разные component versions, mismatches/peer/descriptor/FS/CLI/snapshots | case-groups.txt |
| Фактически сформированные отчёты | Metadata-only synthetic fixtures, все authority gates false | [paired](paired-fixture-review.json), [HMI-only](hmi-only-fixture-review.json) |
| ARM4 libraries | **43344text/data8/bss27**, delta0; прежние imports/ceilings | [budget](arm-budget.json), [objects](admission-size.txt) |
| Individual frames | **984/376/328/64** прежние | [frames](handoff-frames.txt) |
| Actual-source GCC analyzer | **8 embedded +1 host frontend PASS**, `-fanalyzer -c`, warnings as errors | [analysis](static-analysis.txt) |
| Pinned Mbed TLS3.6.7 | **178 files PASS**, archive/tree прежние | [source](source-check.txt) |
| Host/ARM scope | Host reviewer отсутствует в ARM targets | [scope gate](host-review-scope.txt) |
| Synthetic ARM startup | **1/1 PASS**, без исполнения/публикации ELF | [contract](motion-link-contract.txt), [inspection](motion-link-probe.json) |
| Board/output gates | Expected rejection PASS, temporary marker удалён | [board](board-gate.txt), [output](output-gate.txt) |
| Documentation | Local file links validated | [check](docs-check.txt) |

38 =36 base suites +`release_review_cli`/`release_review_cohort`. Standalone13
прежние; host frontend дополнительно собирается в host CMake, а его CLI/FIFO/
symlink/boundary/signature/policy tests включены в root suites. Все прежние
handoff/staging/SemVer/native HMI corpus/render/lifecycle tests сохранены.

Motion frontend использует тот же independent tv4_boot_admit_release/Mbed TLS,
не ESP32 verifier. HMI использует существующий offline OpenSSL checker плюс
полный ESP app inspector. Context/selectors/keys/driver path заданы локально,
не извлечены из downloaded JSON. В новых fixtures два независимых ephemeral
P-256 private keys остаются в RAM signer processes; в files только public DER/
signatures/synthetic images, temporary data удаляется. Новых private key files,
публичных firmware binaries или signer/service/ключей production нет.

Cohort сравнивает signed source/release/channel с отдельным expected selection,
HMI descriptor version/SDK с signed component и current/prospective HMI peer
range. Source signer claim не доказывает build origin. HMI protocol SemVer и
Motion numeric counter проверяются отдельно, без вывода production mapping.
HMI-only выдаёт один component; две существующие metadata schemas не объединены
в новый production manifest. Отчёт не подпись/installer input/live candidate.

Negative cases: signed mismatched source/release/channel, rollback/counter/
profile/protocol/extra fields; corrupted keys/signatures/images, wrong descriptor/
SDK; current peer принимает2.0.0, но prospective2.1.0 вне signed range отвергается;
unknown/zero/bool/extra/duplicate/partial context, oversized/symlink/FIFO inputs;
CLI failure без partial successful stdout. Snapshot test изменяет original
manifest/image и уже authenticated private manifest: fields/report остаются от
исходного единственного verified snapshot. Это не hostile filesystem guarantee.

Оба примерных JSON — **synthetic fixture reports**, не настоящий релиз.
Версия3.0.0 и source012345… — test labels, не текущая готовая прошивка. Их fingerprints различны и публичны; исходные
private keys/images не сохранены в evidence. Driver SHA относится к local
non-sanitized host binary и не attestation. Все publish/install/provenance/
actual context/layout/keys/protocol mapping/vector/live/physical/recovery/
resource/license/owner gates false. Исходный Motion fixture не bootable, HMI
fixture только структурно валидный app model, не hardware result.

## Воспроизведение

LVGL9.2.2 commit7f07a129e8d77f4984fff8e623fd5be18ff42e74; Mbed TLS3.6.7 archive/
source SHA из third_party/mbedtls/origin.json проверяются при local override.
Из repo root с путями к exact sources:

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
ctest --test-dir build-boot-host -V

cmake -S firmware/stm32-bootloader -B build-boot-arm -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/firmware/stm32-motion/cmake/arm-none-eabi.cmake" \
  -DCMAKE_BUILD_TYPE=MinSizeRel -DTV4_BOOT_MBEDTLS_ARCHIVE="$TV4_MBEDTLS_SOURCE_ARCHIVE"
cmake --build build-boot-arm -j4
python3 tools/check_boot_admission_build.py build-boot-arm
```

CI host включает новые2 suites и actual-source analysis host frontend;
bootloader workflow проверяет отсутствие host target в ARM. Прежние embedded8
analysis, motion/protocol/startup/invalid-layout/board/output gates и ESP SDK
matrix8 сохранены. Exact CI results — Checks соответствующего head. Новые local
ESP builds не выполнялись: firmware C/UI/pins/config/partitions/resources прежние.

Sanitizers выполнялись вне ptrace, без options/suppressions; логи проверены на
ASan/LSan/UBSan diagnostics. Trailing whitespace text logs нормализован, все38/38
result rows сохранены. Toolchain/host OpenSSL/Python versions и отличие local
Ubuntu ARM/Binutils от pinned official CI14.2.Rel1 явно записаны в versions.json.
Firmware dependency pins/licenses/arena/crypto owner/budgets не менялись.

Owner F407 SWD/ST-Link V2/core r0p1/rawID0x101f6413/Flash1024KiB/3,252V/
backup1048576bytes/vector structure PASS сохранены. Никаких новых hardware facts.
HSE/full marking/RAM/CCM/IRQ/erase/protection/GPIO/CAN/backup SHA/restore unknown.
Actual installed versions/keys/protocol/layout и publication gates не утверждены;
current PROJECT_VER git-derived/factory2MiB прежние. На ESP32 ordinary8f0d899,
owner HMI/touch/Wi-Fi PASS. Нет devices/SWD/UART/reset/Flash/backup reads,
network probes, runtime/Flash writer/COMMIT/jump/ARM/START, новых release/keys/
тегов/installer или самостоятельного merge.
