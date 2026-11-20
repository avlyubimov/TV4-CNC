# PR-07a — software evidence

Записано 19 ноября 2026. База main `74a7af9fb9a84ffee5587f4956ccb485e725b848`,
PR #28 слит. [План](../../../plans/PR07A_UPDATE_TRANSPORT.md),
[контракт и ограничения](../../../STM32_UPDATE_TRANSPORT.md).
Хеши проверенного кода/тестов/CMake/workflow — [source-hashes.json](source-hashes.json),
точные локальные версии — [versions.json](versions.json).
Пути в трёх текстовых логах нормализованы к `<repo>`; числовые результаты сохранены.

## Выполнено на ПК

| Проверка | Результат | Evidence |
|---|---|---|
| Root host/LVGL XRGB8888/RGB565 | **22/22 PASS**, 1.58 s | [host-tests.log](host-tests.log) |
| ASan/UBSan/LeakSanitizer, без suppressions | **22/22 PASS**, 3.63 s | [sanitizer-tests.log](sanitizer-tests.log) |
| Transport unit | **17 groups PASS**, RX/TX по 384 bytes | [unit-cases.log](unit-cases.log) |
| Memory sender↔receiver | **2049 transfers PASS** | [loopback-cases.log](loopback-cases.log) |
| Loss injection | DATA 4318; ACK 4360, в т.ч. final 397 | тот же loopback log |
| Corrupt / reorder / pending duplicate | 3924 /4338 /4004 случаев | тот же loopback log |
| Reset/truncation/consumer | 10 volatile-reset cuts, truncated и partial RAM failure PASS | тот же loopback log |
| ARM GCC 14.2.1 compile-only | **1924 text, data 0, bss 0**; budget 8192 | [object-size.txt](object-size.txt) |
| Cortex-M4 / Thumb-2 / hard-float ABI | PASS; library compiled general-regs-only | [arm-attributes.txt](arm-attributes.txt) |
| Undefined imports | Только memcmp/memcpy/memmove/memset | [undefined-symbols.txt](undefined-symbols.txt) |
| Individual GCC frame | Максимум 64 bytes; static, budget 128 | [compiler-stack.txt](compiler-stack.txt) |
| GCC -fanalyzer / strict warnings | PASS | [static-checks.log](static-checks.log) |

CRC known vectors и collision проверяются unit suite. Изменённый image с
пересчитанной CRC успешно завершает **только транспорт**; независимый host
OpenSSL SHA oracle обнаруживает mismatch. Это намеренный отрицательный тест
границы доверия, а не embedded проверка подписи/SHA.

## Воспроизведение

Из корня checkout; `TV4_LVGL_SOURCE` указывает на точный LVGL 9.2.2 commit
`7f07a129e8d77f4984fff8e623fd5be18ff42e74`:

```bash
cmake -S . -B build-host -G Ninja -DLVGL_DIR="$TV4_LVGL_SOURCE"
cmake --build build-host -j 4
ctest --test-dir build-host --output-on-failure

cmake -S . -B build-sanitized -G Ninja -DLVGL_DIR="$TV4_LVGL_SOURCE" \
  '-DCMAKE_C_FLAGS=-fsanitize=address,undefined -fno-omit-frame-pointer' \
  '-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=address,undefined'
cmake --build build-sanitized -j 4
ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build-sanitized --output-on-failure

cmake -S protocol/can -B build-update-arm-checked -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/firmware/stm32-motion/cmake/arm-none-eabi.cmake" \
  -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-update-arm-checked
python3 tools/check_motion_size.py build-update-arm-checked/libtv4_update_transport.a
arm-none-eabi-nm --undefined-only build-update-arm-checked/libtv4_update_transport.a
arm-none-eabi-readelf -A build-update-arm-checked/libtv4_update_transport.a
cat build-update-arm-checked/CMakeFiles/tv4_update_transport.dir/update_session.c.su
gcc -std=c11 -Wall -Wextra -Werror -Wconversion -Wshadow -Wstrict-prototypes \
  -fanalyzer -fsyntax-only -Iprotocol/can protocol/can/update_session.c
```

Sanitizer run выполнен вне ptrace; leak detection включён. Root CTest реально
проверил прежние HMI/OTA/motion suites, новые 17 unit groups и 2049 loopback cases.
Прямой запуск обоих новых test executables использован для подробных case logs.
Ранняя standalone configure/build/CTest2/2 также прошла; актуальные результаты
с окончательными17 groups — из root build выше.

## CI и границы результата

Новые тесты входят в оба существующих host jobs. ARM workflow дополнен
отдельной library compile/static/import/frame проверкой; используется прежний
SHA-256-проверяемый официальный ARM GNU 14.2.Rel1 архив. Локальный Ubuntu GCC
тоже 14.2.1, но Binutils отличается от официального CI archive, что явно
записано в versions.json. Actions pins/contents:read сохранены.

В upload allowlist добавлены только update-size.txt, update-symbols.txt и
update-stack.txt. ELF/archive/.bin не публикуются. Прежние startup link tests,
BOARD_IMAGE gate и8 ESP SDK/profile jobs сохранены. ESP32 source/config/locks
не менялись; локальные SDK сборки заново не выполнялись. Текущий статус всех
workflow проверяется у PR, не выводится из этих локальных логов.

Новых зависимостей в embedded части нет. OpenSSL 3 — уже существующая host
dependency, здесь только test SHA oracle. В библиотеке нет crypto/Flash/RTOS/HAL,
runtime consumer или указаний драйверам. Индивидуальный frame не равен общему
стеку (включая callers/libc/IRQ); object text не является размером board image.
Нет измерений throughput/physical CAN/FPS/реального RAM/stack/Flash budget.

Устройства не открывались: ни SWD/UART/flash/read/reset, ни option bytes/eFuse,
ни network/device probes, ни drive outputs. Не читались raw backups/UID/пароли.
Reset cases — только переинициализация RAM objects с новой identity; это не
power-cut recovery. Образ STM32 не создавался, не устанавливался и не выпускался.
Independent signature/hash/context, quiescent proof, erase/recovery/wire policy
остаются обязательными следующими gates.
