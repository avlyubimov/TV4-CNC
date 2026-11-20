# PR-07d — проверка образа по явной модели памяти

20 ноября2026, main `4b7631e`, PR #1–31 слиты, пять main workflows PASS.
[План](plans/PR07D_IMAGE_PREFLIGHT.md), [API модели](../firmware/stm32-bootloader/include/image_profile.h),
[handoff API](../firmware/stm32-bootloader/include/update_handoff.h),
[evidence](evidence/build/stm32-image-preflight/README.md).

Проверка подписи/SHA не определяет допустимые SP/PC или таблицу IRQ. Новый
read-only preflight проверяет эти байты по **явно переданной локальной модели**.
Успех называется PROFILE_MODEL_MATCH: соответствие модели, не аппаратная
готовность, полноценная bootability или разрешение install/commit/jump/motion.
Real F407 profile отсутствует; BOARD_IMAGE=ON по-прежнему отвергается.

## Источник правил и инженерные ограничения

[Arm DUI0553A](https://documentation-service.arm.com/static/5f2ac4ab60a93e65927bbdbf),
разделы2.3.4/4.3.4, описывает stack/reset vectors, Thumb handlers и общее
выравнивание таблицы по размеру. [ST PM0214 Rev10](https://www.st.com/resource/en/programming_manual/pm0214-stm32-cortexm4-mcus-and-mpus-programming-manual-stmicroelectronics.pdf),
разделы2.3.4/4.4.4, уточняет STM32. Generic минимум32words отличается от
указанных ST128words. Поэтому alignment задаётся обязательным local input,
проверяется power-of-two/достаточность для таблицы; generic минимум не выбирает
реальное device alignment. IRQ count/reserved external slots тоже вводятся явно.
Эти reference facts не подтверждают маркировку/RAM/IRQ map конкретной платы.

Стек8-byte, ненулевые handlers для всех незарезервированных slots и запрет
перехода внутрь vector bytes — строгая политика этого программного checker.
Это не доказательство настройки STKALIGN, достаточности runtime stack или
того, что любой проверенный target содержит корректную последовательность команд.

## Локальный профиль

Нет default address, MCU map, slot/sector layout, CMSIS/HAL или trust key.
Профиль — trusted local input, **не данные из CAN/скачанного manifest**:

- ID1..31 символ `[A-Za-z0-9._-]`, load base/capacity, vector alignment,
  vector_count16..256 (core + до240 IRQ), ненулевой minimum_stack_bytes;
- до4 непересекающихся RAM spans, без overlap image capacity и overflow32bits;
  unused spans/IRQ mask bits должны быть нулевыми, неизвестные/нулевые inputs
  отвергаются. Minimum stack должен помещаться в одну RAM span;
- reserved external IRQ mask задаёт только локальную модель, не назначает
  реальные IRQ F407. Core reserved slots7–10,13 обязаны содержать ноль.

Vector table — little-endian words с image offset0. Raw length точно count×4,
signed image длиннее таблицы минимум на2bytes и помещается в capacity.
Initial SP выровнен8 и оставляет minimum stack внутри **одной** RAM span;
верхняя граница RAM разрешена как initial SP, переход через gap/соседние spans — нет.
Все non-reserved handler words (включая Reset) должны быть нечётными, а адрес
первого halfword — после таблицы и полностью внутри **фактических image bytes**,
не просто capacity. Duplicated default handler допустим. Числовые адреса нигде
не разыменовываются; проверка не пишет VTOR/MSP и не исполняет ARM.

Это модель диапазонов, не проверка атрибутов bus/MPU, исполнимости региона,
Flash erase/protection, linker sections/data/BSS/heap/FPU/reset policy или
валидности всех инструкций. Профиль должен пройти отдельный board/source review
до использования на MCU;1MiB capacity сам по себе его не заполняет.

## Связь с подписью и временем жизни

Pure inspect_vectors не аутентифицирует вход. Worker entry
`tv4_boot_handoff_preflight` разрешён только после VERIFIED существующего
[handoff](STM32_UPDATE_HANDOFF.md). Он копирует local profile до callbacks,
сверяет ID с **подписанным memory_profile**, читает vector prefix из того же
immutable lease блоками до256bytes. Полный image уже проверен P-256/SHA/length/EOF.
Неизменяемость lease гарантирует backend; generation сам по себе не блокировка.

Profile ID обозначает неизменный reviewed mapping; смена mapping/key/compatibility
требует нового owner policy generation/отмены сеанса. Нельзя переиспользовать
ID/generation для другой карты. Нет hardware-verified boolean или default
«подходит». Production profile/version/key provisioning ещё не выполнены.

Guards проверяют RX/boot/transfer/policy/source, clock/deadline/cancel до/после
reads и после wipe scratch. Текущий workspace переиспользуется и очищается;
нет нового буфера полного образа, heap или mutable globals. Отчёт публикуется
только после final guard и связывает SP/PC/count/profile с прежним receipt
image SHA/metadata SHA/session/source/policy. Это временный результат для текущего
owner turn: перед дальнейшим использованием снова проверять живой context,
после reset/storage change выполнять полную проверку заново. Скопированный
отчёт не отзывается физически и не является persistent install token.

Recognized profile/vector/read/context failure отзывает candidate, очищает report
и scratch и вызывает прежний bounded release/retry/quarantine. Не теряется token
при release failure. Invalid C arguments/aliases и BUSY не меняют live storage;
reentrant abort откладывает release до возврата callback. Обычные RX/local
deadlines не продлеваются, проверка cooperative, не аппаратный watchdog.
Diagnostic preflight_result используется только при preflight_attempted=true.

## Программная приёмка и следующие gates

Новая profile suite:6 групп,241 размеров таблицы, каждый slot максимальной
таблицы, SP/PC/length/overflow/overlap/gaps/reserved/alias cases. Handoff suite:
прежние54 signed transfers плюс15 signed vector inspections с fragmented reads,
snapshot mutation, корректная подпись при неверных vectors и context/cleanup/
reentry/quarantine. Root26/26, sanitizers26/26, standalone6/6; ARM/source/static/
object/frame/image/output checks — [evidence](evidence/build/stm32-image-preflight/README.md).
Все8 ESP SDK profiles сохранены, зависимости/настройки HMI прежние.

Owner SWD/ST-Link V2, Cortex-M4r0p1, ID0x101f6413, Flash1024KiB,3,252V,
backup1048576bytes/vector structure PASS остаются принятыми сообщениями владельца.
Package/RAM/CCM/HSE/full IRQ map/pinout/erase/protection/GPIO/CAN/restore не
подтверждены. Synthetic fixtures не создают F407 profile и не резервируют ни
одного Flash адреса. До writer нужны verified memory/erase/vector/recovery,
production release/context/keys и fresh physical quiescent/operator gate.
No-I/O/ACK/SIMULATION не подтверждают останов. Нет runtime/device/Flash/reset/
CAN/motion/release/merge действий. Откат — revert PR без миграции устройства.
