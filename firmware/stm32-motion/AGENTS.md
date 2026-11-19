# STM32F407 Motion — специализированные правила

Наследовать корневой AGENTS.md и docs/HARDWARE_STATUS.md. Плата получена,
владелец сообщил суффикс VGT6 (STM32F407VGT6). Новым заданием подтверждены
owner SWD/ST-Link V2, Cortex-M4r0p1, raw ID0x101f6413, Flash1024KiB,3,252V,
backup1048576bytes/vector structure PASS. Package marking/RAM/CCM/HSE/секторы/
pinout/physical GPIO/CAN/restore не подтверждены. Новое чтение агентом запрещено
без отдельного разрешения. Есть foundation/probes, но не board firmware/loader.

- Сначала подтвердить MCU/геометрию/секторы и оформить board decision.
  Маркировка VGT6 сообщена владельцем; это не измерение памяти/HSE/pinout.
- PR-02: CMake + Ninja + точный ARM GCC, CMSIS/HAL/LL с версиями и лицензиями,
  host-тестируемая машина состояний/fault/watchdog interfaces, startup/linker
  только для подтверждённого target. Пока данных нет — host foundation и
  документированный блокер target build, без выдуманных адресов/периферии.
- Планировщик и state machine отделены от GPIO/таймеров и тестируются на ПК.
  Reset/timeout/fault держат outputs disabled; ACK не включает приводы.
  Начальный target не активирует силовые выходы. ENABLE/STEP/DIR полярности
  и безопасное состояние входов/подтяжек требуют аппаратного подтверждения.
- STEP/DIR — аппаратные timer/DMA/IRQ; не задержки RTOS/CAN callback.
  Энкодер, homing/limits, шпиндель/резьба — после отдельной верификации.
- CAN RX проверяет version/length/session/sequence/границы; принимает намерение,
  не исполняет моторную команду из ISR. Потеря связи → утверждённый safe state.
- Загрузчик независим от motion. Linker/vector/диапазоны SP/PC, секторы,
  резервная копия и механизм восстановления определяются до записи Flash.
- CAN update: подтверждённый quiescent state (оси/шпиндель/программа остановлены),
  исключительный update-сеанс, блоки/ACK/повторы, полный SHA-256 и подпись,
  аппаратная/protocol compatibility. Частичный образ никогда не запускается.
- Если A/B не помещается, проектировать staging + recovery; microSD ESP32
  не заменяет самостоятельное восстановление F407 после обрыва питания/связи.
- SWD/прошивка/option bytes/тест привода — только по отдельному разрешению.
  Не менять аппаратную безопасность ради bootloader/OTA. PR-02 не включает
  board image, физический backend или операции с устройством.

## PR-02b — граница startup

- Читать docs/STM32_STARTUP.md/NEXT_STAGE.md. Generic Cortex-M4 reset/support
  и linker sections без MEMORY допустимы как software foundation; единственный
  link caller — явно synthetic fixture. Нет подтверждённого F407 full vector/
  memory/clock profile, BOARD_IMAGE=ON всегда fail, нет .bin/flash/ELF upload.
- Не переносить test ROM/RAM/stack/core-only vectors в реальную плату или OTA
  layout. Не резервировать bootloader/app/metadata sectors по1MiB или suffix.
- No-I/O adapter возвращает false inhibit и не предоставляет watchdog. Нельзя
  заменить это fake success/readiness или утверждать physical safe state.
- Startup не включает IRQ/FPU/HSE/PLL/VTOR/GPIO/CAN. HSI-only возможен позже
  после clock/reset/profile review; HSE unknown не оправдывает выдуманную частоту.
