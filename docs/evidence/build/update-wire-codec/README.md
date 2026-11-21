# PR-07j — update byte codec evidence

21 ноября2026, Asia/Almaty. База main `deace1c1a2f10d5c55b8a56a351f4c7a47f7b762`,
PR #1–41 слиты, открытых PR перед работой нет; [main CI5/5](base-ci.json).
[Контракт](../../../STM32_UPDATE_WIRE.md), [план](../../../plans/PR07J_UPDATE_WIRE_CODEC.md),
[точные версии/лицензии](versions.json), [source SHA-256](source-hashes.json).

| Проверка | Фактический результат | Evidence |
|---|---|---|
| Root host/LVGL | **42/42 PASS**,4.25s | [summary](host-ctest.txt), [full log](host-tests.log) |
| ASan/UBSan/LeakSanitizer | **42/42 PASS**,8.90s, default env/no suppressions вне ptrace | [summary](sanitizer-ctest.txt), [full log](sanitizer-tests.log) |
| Standalone bootloader/protocol | **16/16 PASS**,0.96s | [log](standalone-tests.log) |
| Independent Python oracle | **12 golden messages PASS**,8 kinds/all5 outcome codes | [oracle](wire-reference.txt), [actual bytes](wire-vectors.txt) |
| Decoder/encoder faults | **256 DATA lengths,65536 outcome pairs,200000 mutations PASS** | host-tests.log/update_wire |
| Byte-path actual crypto integration | **8 signed RAM chains**,6 release15 +2 legacy11, прежние4 groups/negative gates | host-tests.log/boot_wire_handoff |
| Opt-in ARM codec archive | **1219text/data0/bss0,frame32** | [budget](wire-budget.json), [object](wire-size.txt), [frames](wire-frames.txt) |
| ARM4 previous libraries | **43362text/data8/bss27**, +18text; old limits прежние | [budget](arm-budget.json), [objects](admission-size.txt) |
| Individual previous frames | **984/376/328/64**, прежние | arm-budget.json |
| Wire guard negative checks | **6 methods PASS**, actual ARM archive inspections/temporary negative objects | [guard](wire-budget-negative.txt) |
| Actual-source GCC analyzer | **12 embedded +1 host PASS**, strict `-fanalyzer -c` | [analysis](static-analysis.txt) |
| Mbed TLS3.6.7 source | **178 files PASS** | [source](source-check.txt) |
| Synthetic ARM link | **1/1 PASS**, без исполнения | [contract](motion-link-contract.txt), [inspection](motion-link-probe.json) |
| Board/output/scope gates | **PASS**, marker удалён, no-I/O/board image запрещены | [boot](board-gate.txt), [motion](motion-board-gates.txt), [output](output-gate.txt), [scope](scope-gate.txt) |
| Documentation/source | Local Markdown links и source digests checked | [check](docs-check.txt) |

42 =39 base +`update_wire_reference`/`update_wire`/`boot_wire_handoff`.
Standalone16 =13 base +те же3 suites. Нормальные typed test modes сохранены;
только --wire-staging включает byte path вокруг existing receive/ACK APIs.
Production libraries/handoff не вызывают codec автоматически.

## Что проверено программно

Headers/version/kind/reserved, точная length, every truncation/capacity, extra
bytes, malformed body/zero identifiers/overflow/unknown enum pairs, unaligned
byte spans, untouched encode output on failure, decode output fully zero,
no C padding/unused payload tail. Input snapshots и canonical byte roundtrip
сохранены. Golden vectors созданы независимым Python struct encoder, не memcpy
того же C layout. Сохранённые wire-vectors — fake opaque IDs/digests/payload,
не CAN IDs, реальные boot epochs, private keys или executable images.

В200000 deterministic mutations113410 структурно valid/canonical и86590
rejected/zeroed. Изменённые opaque bytes могут успешно decode: codec не integrity/
authentication gate. Отдельный test повреждает DATA payload: decode OK, прежний
RX отклоняет CRC до progress. Consume/ACK, CRC collision/exact duplicates,
state/session/timeouts, independent signature/full SHA/model/live lease и cleanup
сохраняются на прежних слоях. Полностью matching forged peer reports всё ещё
не аутентифицированы и не становятся install/physical proof.

Actual signed RAM integration дала counts по kinds1..8:
39/22/22/17/1017/500/16/8. Потери всех4 negotiation сообщений, DATA/final ACK,
report/ACK, pending duplicates, stage lock/copy-before-ACK и post-wipe history
проверены вместе с прежними crypto/binding/cleanup/revocation/deadline negatives.
Новые процессы используют existing RAM-only test signer; private key files,
production keys/signing service/firmware artifacts не создавались.

## Подтверждённое и неподтверждённое аппаратное состояние

Owner SWD/ST-Link V2, Cortex-M4r0p1, raw ID0x101f6413, Flash1024KiB,3,252V и
backup1048576bytes/vector structure PASS сохранены. Новых чтений/измерений нет.
Full marking/HSE/RAM/CCM/IRQ/Flash erase/protection/GPIO/CAN/backup SHA/restore
не подтверждены. Synthetic startup/model addresses не actual board memory map.
Flash capacity не задаёт RAM/staging buffer или bootloader addresses.

ESP32 остаётся ordinary8f0d899 по последнему owner report, HMI/touch/Wi-Fi PASS.
UI/PROJECT_VER/SDK configs/partitions/security/pins/RGB/PSRAM/TLS/resources прежние.
Hardware FPS/latency/stack/CPU/throughput/soak не измерялись этим этапом.

Codec archive1219 bytes учитывается отдельно от4 прежних archives43362 bytes;
+18text — shared shape validator/NULL guard. Это compile-only object sums,
не final linked firmware и не physical Flash/RAM reservation. Code/table data
readonly, mutable globals отсутствуют. Individual compiler frame32 не full
stack. Explicit ceilings4096text/data0/bss0/frame128 имеют regression scope.
Negative guard6 проверяет real archive, missing/dynamic/negative frames,
excess frame, mutable globals, excess code/rodata и unexpected imports.

## Воспроизведение

Точные LVGL9.2.2/Mbed TLS3.6.7/ARM14.2.1 pins — versions.json и origin.json.
Из repo root с reviewed archive/source paths:

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

cmake -S protocol/can -B build-update-arm -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/firmware/stm32-motion/cmake/arm-none-eabi.cmake" \
  -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-update-arm -j4
python3 tools/check_update_wire_build.py build-update-arm
python3 protocol/can/tests/wire_budget_tests.py --build build-update-arm
```

Bootloader ARM/source/gates — existing bootloader workflow; wire checker добавлен
для его opt-in archive. Motion workflow выполняет4 actual-source protocol units
и6 negative wire-budget cases; text-only artifacts. SDK matrix8/pins прежние,
local ESP builds не повторялись на этом protocol-library этапе. Итоговый CI
конкретного head — Checks PR, не исторический main result выше.

Текстовые логи нормализованы только по trailing whitespace/финальным пустым
строкам;42/42 rows сохранены. Sanitizer options/suppressions не задавались.
Нет devices/SWD/UART/reset/Flash/backup reads/CAN/motion/installer или publication/
merge. Production framing/fragmentation/identity/metadata transport/authentication/
hardware/recovery/keys/context/physical/operator/resource gates открыты.
Откат — revert PR, без записи устройств или изменения разметки.
