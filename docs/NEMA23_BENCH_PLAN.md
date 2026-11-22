# Первый NEMA23: план стенда и проект таймерного STEP/DIR

## Граница текущего результата

По сообщению владельца заказан один closed-loop двигатель NEMA23 с заявленными
3,1 Н·м; он ещё не получен. Модель двигателя и STEP/DIR-драйвера не известна.
Момент не измерен. Ни питание, ни ток, microstep, угол шага, pinout, STEP/ENA
полярность, encoder/alarm interface или рабочая частота здесь не назначаются.

Этот PR даёт software результат: чистый расчёт TIM1/TIM8 CH1 register plan,
host counter/comparator/repetition model и отдельно собираемый ARM archive.
**Аппаратного адаптера, physical outputs, работающего двигателя или firmware
для привода пока нет.** P1 ESP32 OTA остаётся основным приоритетом; протокол CAN,
startup/linker/HSI и существующий pinless P0 image не изменены.

На F407 остаётся ee7d179; его data/BSS/CCM-word/watchdog/fault проверка не
подтверждает GPIO/timer/pinout/clock jitter или аппаратный safe state.
[Аппаратный статус](HARDWARE_STATUS.md), [foundation](STM32_FOUNDATION.md),
[базовый образ](STM32_FIRST_IMAGE.md).

## Конкретный проект генератора

Основание: официальный [RM0090 Rev22](https://www.st.com/resource/en/reference_manual/rm0090-stm32f407-advanced-armbased-32bit-mcus-stmicroelectronics.pdf),
§17.3.3,17.3.15,17.4.1,17.4.7,17.4.13; локальный PDF SHA-256 совпал с
[существующей pinned reference](../third_party/stm32f4-reference/origin.json).
TIM1/TIM8 имеют 16-битные PSC/ARR и 8-битный REP. В edge-aligned upcounting
update происходит после REP+1 периодов; OPM останавливает counter на update.
PWM2 даёт inactive OCREF до compare и после конечного overflow. UG перед
стартом загружает shadow registers и repetition counter. Это проектная схема,
а не проверенная на плате последовательность включения выходов.

[timer_plan.c](../firmware/stm32-motion/stepdir/timer_plan.c) принимает явный TIM
input clock, integer Hz, логическое направление и 1..256 pulses. Timings/
maximum rate берутся из будущей reviewed спецификации driver+interface;
`reviewed=false`, нулевые pulse widths, неизвестное направление и невозможный
период отклоняются. Caller flag не удостоверяет hardware approval.

Выбирается минимальный делитель PSC для representable period. Период округляется
вверх, actual frequency не превышает requested. Active/inactive и DIR setup
округляются вверх до counter ticks. Первый inactive interval покрывает DIR setup;
дополнительный post-stop DIR hold возвращается как обязательство будущего
adapter. Он здесь **не выполняется**. Точное отношение частоты возвращено
рациональным числом. При отказе старый plan очищается. Нет float/heap/ожидания
в target коде, поиска по частотам, RTOS/CAN callback или bit-banging.

Результат содержит PSC/ARR/CCR1/RCR, PWM2+preload и OPM/ARPE/URS, но **CEN=0,
CCER=0, MOE=false**. Это не полная BDTR/break/idle/polarity конфигурация; нулевой
CCER не определяет безопасное электрическое состояние невыбранного GPIO.
Не назначены TIM1 против TIM8, physical channel pin, clock tree, DIR/ENABLE
GPIO или уровни. Нет MMIO pointers, HAL, register writes, start/enable API,
IRQ/DMA owner, acceleration, multi-axis planner, automatic chaining/restart.
Длинные движения и смена направления потребуют отдельного measured adapter.

ARM `TV4_MOTION_STEPDIR_DESIGN=ON` создаёт только arithmetic archive. Default ARM
его не содержит; real P0 или synthetic image вместе с design запрещены CMake.
Host tests используют synthetic clocks/timings; они не предлагают настройки
покупаемого драйвера. Model с отдельным host-only CEN считает reference pulses;
ни один test не включает реальные pin gates.

```bash
cmake -S firmware/stm32-motion -B build-stepdir-host -G Ninja
cmake --build build-stepdir-host
ctest --test-dir build-stepdir-host --output-on-failure

cmake -S firmware/stm32-motion -B build-stepdir-arm -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake \
  -DCMAKE_BUILD_TYPE=MinSizeRel -DTV4_MOTION_STEPDIR_DESIGN=ON
cmake --build build-stepdir-arm --target tv4_stepdir_design
python3 tools/check_stepdir_design.py build-stepdir-arm
```

Это build/test команды без SWD/UART/flash. Архив не release или прошиваемый BIN.
[Software evidence](evidence/build/stm32-stepdir-design/README.md).

## Данные до аппаратного адаптера

| Нужное подтверждение | Что оно определяет |
|---|---|
| Полные motor/driver labels, exact manuals и revision | Допустимую пару, характеристики и ограничения |
| Driver supply/input specs, проводка/PE/защиты | Reviewed источник питания и входной interface; никаких guessed volts/amps |
| STEP high/low minimum, accepted edge и max frequency | Pulse timing и его проверку после интерфейсных преобразователей |
| DIR setup/hold, ENA truth table, startup/reset behavior | Полярности и безопасную последовательность; название ENA недостаточно |
| Connector pinout, optocoupler/common-anode/cathode/logic levels | Reviewed harness, изоляцию/level shifting; не прямое GPIO подключение |
| Encoder/ALM manuals и поведение при отказе | Диагностику; closed-loop внутри driver не доказывает feedback STM32 |
| Board schematic, свободные pins/AF и measured timer clock | Конкретный TIM/channel/pins без конфликтов SWD/CAN/кварца |
| Независимая E-stop/inhibit цепь и её квалификация | Снятие опасного момента независимо от software |

Требуются фотографии/маркировки после получения, документы и проверенное
аппаратное решение. До этого нет новых адресов Flash, bootloader layout,
motion-ready claims, параметров drive или команды включения выхода.

## Последовательность будущего стенда

Каждый аппаратный этап требует отдельного разрешения на конкретный образ,
состав подключения и действия; нынешнее software задание их не разрешает.

1. **Документы и защита.** Сверить модели/ревизии, механически закрепить мотор
   вне станка, исключить доступ к валу/муфте, проверить проводку и защиту с
   квалифицированным специалистом. Утвердить питание/ток/микрошаг из exact docs.
   Независимая NC E-stop цепь с подходящими проверенными контактором/STO должна
   снимать опасный момент без ESP32, STM32, CAN, driver firmware и UI STOP.
   ALM/ENA не объявляются safety-rated без данных производителя/проверки.
2. **Logic-only, motor disconnected.** После review pins/clock/adapter и exact
   firmware разрешения измерить анализатором STEP/DIR на согласованной точке
   интерфейса. Проверить 1,2,256 pulses, минимум/максимум approved frequencies,
   high/low, DIR setup/hold, quantization/jitter, начальный/последний импульс.
   Никаких автоматических цепочек пачек; считать edges независимо от MCU.
3. **Отказы logic-only.** Проверить explicit stop/timeout/fault, reset/watchdog,
   разрыв управления и отсутствие auto-resume; записать residual pulses и
   состояние выходов. Software stop не заменяет independent hardware chain.
   Допуски и реакция на step truncation утверждаются до energizing motor.
4. **Один мотор без нагрузки.** Отдельное разрешение, утверждённый driver setup,
   рабочая independent emergency chain и наблюдение. Только ограниченные
   конечные пачки, направления и low-speed tests из reviewed плана. Проверить
   реальное направление/величину перемещения, ALM, нагрев/шум и останов.
   Величины скорости/ускорения/тока не задавать до модели и предыдущих замеров.
5. **Отказы и восстановление.** В заранее утверждённом изолированном сценарии
   проверить аварийную цепь, alarm/encoder fault, reset, loss of controller
   и запрет автоматического перезапуска. Physical CAN loss проверять после P3
   с настоящими transceivers, уровнями, терминаторами и временем восстановления.
   Подключение оси станка/концевиков/homing/STEP planner — последующий P5.

Для каждого этапа сохранить source/ELF/BIN SHA, схему/модели/настройки, clock,
осциллограммы/counts/high-low/setup-hold/jitter, faults/stop latency/residual
steps и owner verdict. До измерений поля остаются «не проверено», а не PASS.
