# PR-07i — portable RAM source evidence

21 ноября2026, Asia/Almaty. База main `2ea51b6d42cb1b48450274f65b67e421db41f9b6`,
PR #1–38 слиты, открытых PR перед работой нет. [Main CI5/5](base-ci.json);
при старте четыре workflow прошли, ESP matrix ещё завершалась, failures не было.
[Контракт](../../../STM32_STAGING_SOURCE.md), [план](../../../plans/PR07I_STAGING_SOURCE.md),
[source hashes](source-hashes.json), [точные версии/лицензии](versions.json).

| Проверка | Фактический результат | Evidence |
|---|---|---|
| Root host/LVGL | **36/36 PASS**,3.05s | [CTest](host-ctest.txt), [full log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **36/36 PASS**,6.93s; default env/no suppressions | [CTest](sanitizer-ctest.txt), [full log](sanitizer-tests.log) |
| Standalone bootloader/protocol | **13/13 PASS**,0.84s | [log](standalone-tests.log) |
| Новый source unit suite | **7 groups**,45 transfer/read variants/1536 cycles | [groups](case-groups.txt) |
| Новый actual backend integration | **4 groups**,8 signed chains (6 release15 +2 legacy11),5 crypto/model/binding failures,3 owner/cleanup cases и expired RX | case-groups.txt |
| Прежние legacy/release suites |19 handoff/completion,8 release handoff,8 standalone release/2745 SemVer comparisons PASS | case-groups.txt, host-tests.log |
| ARM4 libraries | **43344text/data8/bss27**, +1654text; прежний64KiB/import ceiling | [budget](arm-budget.json), [objects](admission-size.txt) |
| Individual frames | New consume**328**, handoff max +96, прежний512 ceiling; admission984/crypto376/transport64 прежние | [frames](handoff-frames.txt) |
| Actual-source GCC analyzer | **8 units PASS**, `-fanalyzer -c`, warnings as errors | [analysis](static-analysis.txt) |
| Independent Mbed TLS3.6.7 | **178 files PASS**, pinned archive/tree | [source](source-check.txt) |
| Synthetic ARM startup link | **1/1 PASS**, без исполнения и публикации ELF | [contract](motion-link-contract.txt), [inspection](motion-link-probe.json) |
| Board/output gates | Expected rejection PASS, временный marker удалён | [board](board-gate.txt), [output](output-gate.txt) |
| Documentation | Local file links validated | [check](docs-check.txt) |

36 =34 base suites +`boot_staging_source`/`boot_staging_handoff`; standalone13 =11+2.
Сохранены все старые тесты, crypto/workspace/dependency pins и DATA deadlines.

Backend копирует настоящий pending RX block в caller buffer до consumer ACK;
повтор пакета/final ACK не вызывает повторную запись. Проверены boundaries/
sentinels, aliases/overflow spans, order/descriptor/limits/clock/cursor/reset
faults, partial discard, lease exclusion/stale token/idempotent release,
monotonic generation/high-water/token counter и exhaustion без wrap. Init не
сбрасывает сконфигурированный объект. Raw C/DMA/thread writes остаются запрещённой
caller aliasing ошибкой; API не является memory protection.

В actual signed chains работают real source acquire/inspect/read/release вместо
mock storage. Потери handshake/DATA/final ACK/report/ACK не обходят independent
P-256/SHA/vector/close checks. Read callback пытается writes/discard/reinit/
new begin/acquire/wrong-token release: все отклоняются без изменения source.
После clean close действительные source bytes стираются; outcome остаётся
historical и доставляется однократно. Context fault отзывают candidate;
release wrapper failures сохраняют actual LEASED backend до retry/quarantine,
не разрешая force discard/clear. Истёкший completed RX не оживает от SEALED.

Host source state160 bytes плюс explicit whole-image caller buffer; core2136/
completion568/release adapter528/workspace17664 прежние. Новый consume frame328
объясняется bounded pending block copy; общий handoff frame потолок512 не менялся.
Это не полный call-stack/CPU/RAM/CCM/latency measurement. Discard O(written) —
worker-only. Нет новых heap/OS/driver imports или mutable globals в source/
handoff/transport; data8/bss27 принадлежат прежней crypto configuration.

## Воспроизведение

LVGL9.2.2 commit7f07a129e8d77f4984fff8e623fd5be18ff42e74. Mbed TLS3.6.7 official
archive SHA из third_party/mbedtls/origin.json проверяется при local override.
Команды из repo root с путями к exact sources:

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

Analyzer/gates — `.github/workflows/bootloader.yml`, теперь8 actual units.
Новые CTests включены в root/standalone автоматически. Прежние motion/protocol/
startup/invalid-layout gates и ESP SDK matrix8 сохранены. Local ESP builds не
повторялись на STM32-only этапе; exact PR CI results смотреть в Checks.

Sanitizers выполнены вне ptrace, без suppressions/options; полные логи проверены
на ASan/LSan/UBSan diagnostics. Trailing whitespace text logs нормализован без
изменения test content; сохранены все36/36 result rows. Local Ubuntu GCC/ARM/
Binutils и отличие от official pinned CI14.2.Rel1 явно записаны в versions.json.
Host signer использует ephemeral OpenSSL keys только в памяти; firmware verifier
независим, pinned Mbed TLS. Новых dependencies/production keys/versions нет.

Подтверждения владельца F407 SWD/ST-Link V2/core r0p1/rawID0x101f6413/
Flash1024KiB/3,252V/backup1048576bytes/vector structure PASS сохранены.
Engineering fixtures используют synthetic identities/maps/versions/buffer sizes.
Flash capacity не доказывает whole-image fit в RAM: production medium/budget,
RAM/CCM/IRQ/HSE/full marking/erase/protection/GPIO/CAN/backup SHA/restore unknown.
На ESP32 ordinary8f0d899, owner HMI/touch/Wi-Fi PASS. Нет новых devices/SWD/
UART/reset/Flash/backup reads, runtime/wire/Flash writer/commit/jump/ARM/START,
production release/keys/installer, tags или самостоятельного merge.
