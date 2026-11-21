# PR-07h — software release handoff evidence

21 ноября2026, Asia/Almaty. База main `c17e89c550026771be31fed14cc03fb1566734d0`,
PR #1–37 слиты, открытых PR перед работой нет; [main CI5/5](base-ci.json).
[Контракт](../../../STM32_RELEASE_HANDOFF.md), [план](../../../plans/PR07H_RELEASE_HANDOFF.md),
[source hashes](source-hashes.json), [версии/лицензии](versions.json).

| Проверка | Фактический результат | Evidence |
|---|---|---|
| Root host/LVGL | **34/34 PASS**,2.97s | [CTest](host-ctest.txt), [full log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **34/34 PASS**,6.78s; default env, без suppressions | [CTest](sanitizer-ctest.txt), [full log](sanitizer-tests.log) |
| Standalone bootloader/protocol | **11/11 PASS**,0.82s | [log](standalone-tests.log) |
| Новый release handoff | **8 groups PASS**,6 full chains со всеми четырьмя handshake losses/DATA/final ACK/report/ACK loss | [groups](case-groups.txt) |
| Legacy handoff/completion | Прежние19 groups PASS, включая54 transfer variants/6 negotiated chains/3 binding failures | case-groups.txt |
| Standalone release verifier/SemVer | Прежние8 groups/2745 comparisons PASS | host-tests.log |
| ARM4 libraries | **41690text/data8/bss27**, +1460text,64KiB ceiling/imports PASS | [budget](arm-budget.json), [objects](admission-size.txt) |
| Individual compiler frames | Max admission984/crypto376/handoff232/transport64 прежние; новый begin232/completion wrapper40 | [handoff frames](handoff-frames.txt) |
| Actual-source GCC analyzer | **7 units PASS**, `-fanalyzer -c`, warnings as errors | [analysis](static-analysis.txt) |
| Independent Mbed TLS3.6.7 source | **178 files PASS**, pinned archive/tree | [source](source-check.txt) |
| Synthetic ARM startup link | **1/1 PASS**, ELF только локальная fixture, без исполнения | [contract](motion-link-contract.txt), [inspection](motion-link-probe.json) |
| Deployable board/output gates | Expected rejection PASS, temporary marker removed | [board](board-gate.txt), [output](output-gate.txt) |
| Documentation | Local file links validated | [check](docs-check.txt) |

34 =33 слитых root suites +новый `boot_release_handoff`; standalone11 =10+1.
Host signer использует ephemeral P-256 private key только в памяти OpenSSL;
проверка в библиотеке идёт через независимую pinned Mbed TLS. Новых production
keys/defaults, crypto/workspace/allocator instances или dependency changes нет.

Новый typed adapter проверяет strict15-field release document внутри существующего
live immutable lease/owner/source/RX/deadline/model coordinator. Local mode не
выбирается по JSON. Legacy positive/clear операции отвергают release mode;
legacy poll/close/abort оставлены для guard/cleanup. Общие private helpers
используют прежние алгоритмы, без копии state machine или success callback.

В новых6 цепочках реальная подпись/SHA/полный image/vector model предшествуют
одному clean release и историческому outcome. Raw SHA документа связан с fresh
negotiation. Signed неверные selection/source/channel, rollback, signature/hash,
vectors и valid re-signed metadata с чужим negotiated SHA отвергаются. Изменения
owner/source/RX, clock/cancel, alias/BUSY/reentry, callback snapshot, acquire/
release failures, retries/quarantine/post-close faults не дают positive result.
Original input buffers/policy/key меняются внутри acquire callback после snapshot;
проверка использует исходную captured policy. Модельный provider обязан удерживать
immutable source; generation сам не гарантирует блокировку.

Host adapter528/full receipt360 bytes — новые caller-owned objects. Core2136/
legacy receipt136/completion568/workspace17664 прежние. Нет mutable globals в
handoff/transport или новых heap/OS/driver imports. Object totals/individual
frames — не полный call-stack/RAM/CCM/CPU/latency report и не board image.
Новые begin232/completion40/helper frames входят в будущий full-stack review.

## Воспроизведение

LVGL9.2.2 source commit7f07a129e8d77f4984fff8e623fd5be18ff42e74.
Mbed TLS3.6.7 archive SHA из third_party/mbedtls/origin.json проверяется и при
local cache override. Выполнять из repo root, задав пути к exact sources:

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

Analyzer/gates — `.github/workflows/bootloader.yml`, теперь7 actual source units;
new CTest автоматически выполняется в root/standalone. Прежние motion/protocol/
startup/invalid-layout gates и ESP SDK matrix8 сохранены. Exact CI results — в
Checks соответствующего SHA; local ESP builds на этом STM32-only этапе не
повторялись. Root/sanitizer/ARM PASS не является результатом испытаний устройств.

Sanitizers выполнялись вне ptrace, без options/suppressions; logs проверены на
ASan/LSan/UBSan diagnostics. Trailing whitespace text logs нормализован без
изменения test content; summary сохраняет все34/34 result rows. Exact local
Ubuntu GCC/ARM/Binutils provenance и отличия от pinned CI14.2.Rel1 — versions.json.
LVGL9.2.2/cJSON1.7.19/Mbed TLS3.6.7/ESP SDK locks и лицензии прежние.

Подтверждённые владельцем F407 SWD/ST-Link V2/core r0p1/rawID0x101f6413/
Flash1024KiB/3,252V/backup1048576bytes/vector structure PASS сохранены. Synthetic
RAM/image maps, keys/versions и leases — инженерные fixtures, не реальные F407
profile/installed state. HSE/full marking/RAM/CCM/IRQ/erase/protection/GPIO/CAN/
backup SHA/restore остаются unknown. На ESP32 ordinary8f0d899, owner HMI/touch/
Wi-Fi PASS. Нет новых device/SWD/UART/reset/Flash/backup reads, runtime/wire/
writer/commit/jump, production keys/releases/tags/installer или merge.
