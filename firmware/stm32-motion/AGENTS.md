# STM32F407 Motion — специализированные правила

Актуально после успешного базового стенда 22 ноября:
22 ноября 2026, база main `07fb406e37ebd1f1e019dbee0b54e8bb5571be1a`, PR #1–57 слиты.
После отдельного разрешения один стенд ee7d179 завершён за16,196s, exit0:
program/verify/exact identity, RUNNING, data/BSS/один CCM word/guards, progress,
watchdog reset и UsageFault capture/reset **PASS**. На F407 оставлен работающим
clean `ee7d179435f3f9e7598c7646b85ff64f4d5e9a32`, BIN2540bytes. Условный restore
не понадобился. Предыдущий c09 FAIL8 и проверенный полный snapshot restore — история.

Плата изолирована, только питание/SWD; option bytes/OTP/CAN/motion/ESP32 не менялись.
Этот разрешённый сеанс завершён. Полный RAM/CCM stress, stack high-water,
точные HSI/LSI/HSE частоты, physical GPIO/CAN/inhibit и power-loss остаются открытыми.
P0 выполнен на базовом стенде; следующий приоритет — P1 ESP32 OTA с existing
HTTPS/crypto и review16MiB factory+двух OTA slots/USB migration. Новых hardware
операций или будущих CAN loader addresses это не разрешает.

Наследовать корневой AGENTS.md и docs/HARDWARE_STATUS.md. Факты/пределы —
docs/evidence/hardware/2026-11-22-f407-ee7d179-bench/README.md.
Ниже сохранены контракты PR-02/P0, а не новая очередь software abstractions.

- Официальный контракт заявленного VGT6 проверен в docs/STM32_FIRST_IMAGE.md;
  фактическую маркировку/геометрию/защиту проверить отдельно перед hardware write.
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
  board image в историческом PR-02; текущий явный P0 bench описан ниже.
  Операции с устройством этим не разрешаются.

## STEP/DIR design — отдельное software задание владельца

- Читать docs/NEMA23_BENCH_PLAN.md. Первый closed-loop3,1Н·м заказан, не получен;
  точные motor/driver models, питание/ток, pins/ENABLE/STEP polarity неизвестны.
  Никаких физических выходов или испытаний этим заданием не разрешено.
- `stepdir/timer_plan` — arithmetic/register contract TIM1/TIM8 CH1 PWM2+OPM+RCR,
  finite1..256 pulses; explicit synthetic clock/timing в tests, без board defaults.
  OK не safety/admission/start permission. Только pure plan, без MMIO/HAL/IRQ/
  callbacks/heap или изменения hardware.c. CEN/CCER/MOE остаются disabled.
- ARM archive только opt-in `TV4_MOTION_STEPDIR_DESIGN=ON`; default ARM и P0
  image не содержат его. Комбинации real/synthetic image+design запрещены.
  Host model не доказывает физические pulses, DIR hold, stop или безопасность.
- Future adapter/осциллограф/driver/motor запуск требуют exact model/pinout/
  clock/independent inhibit review и отдельного аппаратного разрешения.
  Физический E-stop/STO/контактор независим от MCU/CAN/UI; noio по-прежнему false.

## PR-02b — историческая граница synthetic startup

- Читать docs/STM32_STARTUP.md/NEXT_STAGE.md. Generic Cortex-M4 reset/support
  и linker sections без MEMORY допустимы как software foundation; единственный
  link caller — явно synthetic fixture. Нет подтверждённого F407 full vector/
  memory/clock profile в PR-02b не было. Default/неизвестные profiles
  отклоняются; исключение — reviewed P0 bench ниже. Нет flash/ELF upload.
- Не переносить test ROM/RAM/stack/core-only vectors в реальную плату или OTA
  layout. Не резервировать bootloader/app/metadata sectors по1MiB или suffix.
- No-I/O adapter возвращает false inhibit и не предоставляет watchdog. Нельзя
  заменить это fake success/readiness или утверждать physical safe state.
- Startup не включает IRQ/FPU/HSE/PLL/VTOR/GPIO/CAN. HSI-only возможен позже
  после clock/reset/profile review; HSE unknown не оправдывает выдуманную частоту.

## P0 — один настоящий pinless образ

- Явно `TV4_MOTION_BOARD_IMAGE=ON` и единственный профиль
  `TV4_MOTION_BOARD_PROFILE=stm32f407vg-hsi-no-motion-v1`; ARM GCC14.2.1.
  Empty/unknown/host/mixed-synthetic configurations fail. Не ослаблять guards
  глобальной маской. `TV4_BOOT_BOARD_IMAGE` по-прежнему fail.
- Читать docs/STM32_FIRST_IMAGE.md и docs/STM32_STLINK_RUNBOOK.md. Официальные
  DS8626 Rev12/RM0090 Rev22/PM0214 Rev10 и ST vector reference v2.6.11 pinned.
  Real standalone memory/vector profile: Flash1MiB, SRAM112+16KiB, CCM64KiB
  data-only/noDMA,98vectors/512align/reserved IRQ79. Это спецификация выбранного
  устройства, не физический тест RAM/erase/clock. Полная надпись сверена владельцем; HSE неизвестен.
- HSI16MHz/dividers1, HSE/PLL off, integer-only; data/BSS/CCM отдельно; main4KiB
  и emergency1KiB stacks/guards. No GPIO/AF/STEP/DIR/ENABLE/spindle/TIM/DMA/CAN,
  Flash writer/bootloader/heap/semihosting. Не угадывать physical safe levels.
- Watchdog только из main при прогрессе/guards/RUNNING; ISR/fault/halt не feed.
  Debug HALT freeze IWDG явно документирован. До старта IWDG ошибка может halt; started фиксируется сразу после start key,
  до PR/RLR sync. После start ошибка не feed и ведёт к watchdog reset; никаких motion readiness/physical inhibit claims.
- Проверять реальные ELF/BIN/map/segments/vectors/stack/identity/SHA и compiler
  frames; negative link/corruption, actual-source MMIO+sanitizers, repeat build.
  CI публикует только text evidence. ELF/BIN создаются локально для review/
  отдельно разрешённого стенда, не как public release. Не коммитить бинарники.
- Sector0/16KiB bench budget не резервирует будущий CAN loader. P4 layout/write/
  recovery решение после physical CAN P3 и protection/preflight; existing verifier
  и протокол не переписывать. Успешный аппаратный сеанс P0 отдельно разрешён и завершён; новые
  device/SWD/reset/Flash/backup read/option-byte действия этим не разрешаются. PR review не заменяет owner exact-image permission.
