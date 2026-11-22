# P1: разделить первый кадр HMI и запуск фоновых служб

База main973a9ba, PR #1–63 слиты. На плате fecc05d; прошлый стенд закрыт.
Владелец продолжил разработку. Docs-only ограничение отчётного PR #63 завершено.

Измеренный fc90200 BOOT minimum1887 не локализует отдельный allocator.
В исходниках Wi-Fi стартует до dashboard construction, export worker — до
ready splash/первого dashboard refresh. Эти allocations перекрываются с GUI.

1. Проверить actual app_main и настоящий LVGL9.2.2 на host с SDK/board doubles:
   cold render, allocation requests/pool watermark, first-frame completion и
   отказами board/buffer/touch/flush/export/network. Не выдавать host pool,
   размеры64-bit ABI или fake radio за actual ESP32 heap/performance.
2. Завершить первый dashboard frame до запуска существующих export/Wi-Fi
   services. Не вводить timer/delay/очередной framework, новых tasks или profiles.
   OTA worker/deadline остаётся до board init; touch-release gate сохраняется.
   Wi-Fi init/connect/password/scan logic, SDK/dependency pins, GPIO/DMA/TLS/
   framebuffer/криптография/Flash map и production floors не меняются.
3. Сохранить семь стабильных phase IDs; после изменения порядка WIFI_STARTED
   по времени идёт после UI_READY. Лог печатается по ID, не хронологии:
   документировать и проверять actual order без переименования старых данных.
4. Прогнать real startup integration, existing host/sanitizers/layout/boot/
   OTA/SDK checks, обе SDK×5 profiles+trace. Зафиксировать clean ELF/BIN hashes
   и software evidence. Сравнить настоящий cold UI frame и allocation trace
   до/после; performance/heap improvement на плате до замера не утверждать.
5. Отдельный PR без самостоятельного merge/release. Никаких UART/SWD/reset/
   Flash/STM32/CAN/motion/eFuse. Новая аппаратная проверка требует нового
   exact-image разрешения, действующая A/B-карта сохраняется.

Результат — настоящая прошиваемая HMI с проверенным порядком старта, затем
отдельный стенд измерений. Это не configured worker8192/TLS/install acceptance:
ключ/context пока отсутствуют, CHECK/INSTALL закрыты.
