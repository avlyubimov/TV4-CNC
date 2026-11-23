# Configured BASE c013ddf: ранние CHECK и закрытый возврат

Один изолированный стенд разрешён владельцем, календарное окно затем явно
продлено для проверки; общий лимит1800s и recovery reserve сохранены.
[Протокол](measurements.json):619,542s, свежие16MiB/device MD5/SHA/two-NVMe PASS,
одна app-only запись BASE/verify/MD5 вне factory, UART90, затем весь свежий
factory2MiB fecc05d/verify/UART30 PASS. Две writes/два captures, full restore0.
Bootloader/table/otadata/неактивные слоты сохранены; eFuse/STM32/CAN/motion нет.
Владелец подтвердил HMI, касания, saved Wi-Fi и OTA SETUP возвращённого fecc05d.

BASE SDK6.1.1 / rev12-ota /0.1.0-bench.1:
`9a11d06d5b434c82f11aa261c99c955efe1c9d38f91786e470cbd0942421928a`.
Public trust, worker8192, reviewed initial floors, memory diagnostics;
CA/hostname/date/signature/SHA/size/version gates сохранены.

Контрольный boot/self-test/PSRAM8MiB/XiP/RGB/GT911/Wi-Fi/touch_fresh PASS.
Во время90s контроля владелец дважды нажал CHECK до отдельной команды.
Получены два набора WORKSPACE2/SNTP3/SESSION4/CLEANUP5 и два stage9/error11
(SESSION_RESOURCE). Конкретный resource enum старый service не логировал;
остаток стека3024bytes ниже reviewed floor3072, поэтому дефицит стека измерен,
но другие одновременные причины resource failure этим не исключены.

Строгий контроль отвергнут по no_network_windows/no_early_check_request.
Запланированный отдельный420s CHECK не запускался; два ранних запроса нельзя
переименовать в успешный контроль или квалифицированный SESSION4. Установок,
выбора boot slot и power-loss тестов не было. Physical FPS/touch-to-display не
измерены. [Следующая интеграция](../../../plans/P1_CONFIGURED_WORKER_HEADROOM.md)
добавляет configured stack reserve и bounded resource diagnostics, сохраняя floors.
