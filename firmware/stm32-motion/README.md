# STM32 Motion — foundation без активации оборудования

Плата DevEBox STM32F4VX_M Ver. 2.0 получена. Владелец уточнил суффикс **VGT6**
(STM32F407VGT6). Теперь владелец подтвердил SWD/ST-Link V2, Cortex-M4r0p1,
raw device ID0x101f6413, Flash1024KiB,3,252V, backup1048576bytes/vector structure
PASS. Агент этого не перечитывал. Package marking/RAM/CCM/HSE/pinout/erase
geometry/physical GPIO/CAN/restore остаются unknown; [owner evidence](../../docs/evidence/hardware/stm32f407-owner-swd/README.md).

PR-02 добавляет переносимый C11 supervisor: BOOT/DISCONNECTED/IDLE/FAULT,
защёлки ошибок, самопроверку, heartbeat session, контроль периода цикла и
watchdog interface. Во всех состояниях выходы запрещены. IDLE означает только
готовность software supervisor, не разрешение движения или OTA. Нет ARM/START,
STEP, spindle enable, planner, target main, startup/linker или загрузчика.

Предыдущий абзац описывает PR-02. PR-02b добавляет [startup building blocks](../../docs/STM32_STARTUP.md):
portable memory init, no-I/O adapter (inhibit=false/watchdog unavailable),
Cortex-M4 reset object и sections contract без MEMORY/Flash defaults. Opt-in
synthetic link probe проверяется без исполнения ARM-кода. Нет полного F407
vector/memory/clock profile или deployable image; BOARD_IMAGE=ON остаётся fail.

```bash
cmake -S firmware/stm32-motion -B build-motion -G Ninja
cmake --build build-motion
ctest --test-dir build-motion --output-on-failure

# Только библиотека Cortex-M4F, GNU ARM GCC 14.2.1:
cmake -S firmware/stm32-motion -B build-motion-arm -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build build-motion-arm
python3 tools/check_motion_size.py build-motion-arm/libtv4_motion_core.a
```

Host test включён и в корневой CTest/ASan matrix. ARM CI использует официальный
14.2.Rel1 archive с SHA-256; точная версия GCC проверяется toolchain. Ни одна
команда не прошивает и не подключается к плате. BOARD_IMAGE=ON намеренно
отклоняется; libtv4_motion_core.a не является прошивкой и не публикуется как .bin.
CMSIS/HAL/LL пока не подключены: лицензии/версии будут закреплены при выборе
реального board adapter. Новых внешних source dependencies нет.

Сборка synthetic probe/negative link tests — команды в [STM32_STARTUP](../../docs/STM32_STARTUP.md).
Test-only ROM0x01000000/RAM0x30000000/stack2048/core vectors64 не являются
памятью/разметкой F407 и не выбирают bootloader/OTA offsets. Probe ELF не
исполняется, не конвертируется в .bin и не загружается в CI artifact.

Контракт, приёмка, результаты и ограничения: [STM32_FOUNDATION](../../docs/STM32_FOUNDATION.md).
Правила: [AGENTS.md](AGENTS.md). Статус: [HARDWARE_STATUS](../../docs/HARDWARE_STATUS.md).
Будущий независимый bootloader: [OTA_ARCHITECTURE](../../docs/OTA_ARCHITECTURE.md).
