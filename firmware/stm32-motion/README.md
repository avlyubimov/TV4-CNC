# STM32 Motion — foundation без активации оборудования

Плата DevEBox STM32F4VX_M Ver. 2.0 получена. Владелец уточнил суффикс **VGT6**
(STM32F407VGT6); частота кварца, schematic/pinout, фактическая память/сектора и
порядок восстановления именно этой платы ещё не проверены. SWD не выполнялся.

PR-02 добавляет переносимый C11 supervisor: BOOT/DISCONNECTED/IDLE/FAULT,
защёлки ошибок, самопроверку, heartbeat session, контроль периода цикла и
watchdog interface. Во всех состояниях выходы запрещены. IDLE означает только
готовность software supervisor, не разрешение движения или OTA. Нет ARM/START,
STEP, spindle enable, planner, target main, startup/linker или загрузчика.

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

Контракт, приёмка, результаты и ограничения: [STM32_FOUNDATION](../../docs/STM32_FOUNDATION.md).
Правила: [AGENTS.md](AGENTS.md). Статус: [HARDWARE_STATUS](../../docs/HARDWARE_STATUS.md).
Будущий независимый bootloader: [OTA_ARCHITECTURE](../../docs/OTA_ARCHITECTURE.md).
