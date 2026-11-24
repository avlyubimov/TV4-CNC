# P1: устойчивый RGB и прогресс записи OTA

## Наблюдение и границы

Владелец подтвердил download/write/install/reboot bench.3, но сообщил о прыжках
всего интерфейса при изменении счётчика КиБ. Приватное видео просмотрено локально.
Signed INSTALL798checks/error0 и короткий ota_0 boot описаны в отдельном
[PR #84](https://github.com/avlyubimov/TV4-CNC/pull/84); это не полная приёмка
Flash/VALID readback или физического качества отображения.

Actual bench.3 config: PSRAM XiP уже включён, LCD/GDMA ISR не IRAM-safe. В
[описании Espressif](https://docs.espressif.com/projects/esp-idf/en/v6.1.1/esp32s3/api-reference/peripherals/lcd/rgb_lcd.html#bounce-buffer-with-single-psram-frame-buffer)
разобраны зависимость bounce-buffer режима от доступности cache и конкуренция
за PSRAM. Pinned SDK Kconfig связывает IRAM-safe option с выполнением RGB/GDMA
interrupts при Flash/cache operations. Это обоснование изменения, не замер
единственной причины артефактов конкретной платы.

В LVGL9.2.2 `lv_label_set_text()` invalidates и reallocates даже тот же текст.
Прежний OTA presenter при изменении received/stack заново задавал status и все
captions, создавая ненужную нагрузку на PSRAM/отрисовку.

## Конкретное изменение

- Только explicit `sdkconfig.ota` включает `LCD_RGB_ISR_IRAM_SAFE`; SDK выбирает
  соответствующие GDMA/peripheral IRAM функции. Config verifier отвергает stale
  OTA build без этой защиты. Обычные профили не перенастраиваются.
- Presenter задаёт постоянный текст только при изменении, не пересоздаёт captions
  и не меняет button states при одном progress update. Nonvisual stack fields
  не вызывают redraw.
- Счётчик КиБ обновляется до5раз/с с показом последнего значения. Завершение,
  ошибка, смена состояния/identity/policy и отзыв confirmation не откладываются.
  Только представление имеет этот предел; network/writer/guard/команды прежние.
- Status имеет постоянную высоту24px; complete transfer показывает округлённые
  total/total КиБ. Timer wrap обработан unsigned elapsed.

PCLK16MHz, RGB pins/timings, bounce10lines/два внутренних bounce buffers,
единственный PSRAM framebuffer, PSRAM80MHz/XiP, LVGL allocator, worker12288,
TLS records, resource floors, timeout/clock/certificate/signature и двойное
подтверждение сохраняются. Always-restart-in-VSYNC не включается: pinned driver
уже восстанавливает обнаруженный bounce underflow, лишний постоянный restart
не заявляется исправлением производительности. Новых workers/transport/crypto нет.

## Программная приёмка

Actual LVGL XRGB8888/RGB565 tests считают invalidate-area events: diagnostic-only
snapshots не перерисовывают страницу, progress не затрагивает кнопки и геометрию,
199ms coalesces/200ms показывает последнее значение, completion/error/restart
немедленны; tick wrap проходит. Existing disabled/digest-change/cancel/double-tap
и layout regressions сохранены. Полные274 host и274 ASan/UBSan/LSan PASS.

Четыре actual ELF/BIN обоих pinned SDK проходят config/image/memory gates;
LCD ISR, bounce fill и DMA EOF callbacks находятся в `.iram0.text`. SDK5 использует
`GDMA_ISR_IRAM_SAFE`, SDK6 — `GDMA_ISR_HANDLER_IN_IRAM`; оба включены vendor Kconfig.
Это compile/link proof, не runtime OOM/stack/FPS/tearing acceptance.

## Следующий точный стенд

Нужны source CI PASS, independently verified signed candidate и exact owner
approval нового prerelease/context перед публикацией. Bench.3 assets/tag не менять;
новые `.3→.4` inputs — отдельная ограниченная bench proposal, не production policy.
Owner key остаётся локальным, на CI не передаётся.

Устройство разрешено владельцем заранее, повторного вопроса о доступе не нужно.
Каждый аппаратный пакет finite, с verified full snapshot/two-NVMe/recovery reserve,
без motion/CAN/eFuse/STM32. Процедура учитывает ранние UI операции и reboot,
проверяет точные bytes скачанного inactive slot/отдельно native VALID и длительный
boot. FPS/touch-to-display и visual artifact отмечаются как measured/unmeasured.
**При успешной проверке новый проверенный release остаётся на плате**, как
потребовал владелец; обычного возврата fecc05d больше нет. Native rollback и
условное восстановление после реального отказа сохраняются.

Если software build проходит, но визуальный дефект остаётся, не снижать PCLK,
floors или частоту UI без отдельного измеренного решения. Double framebuffer/
bounce-depth tuning требуют собственного RAM/CPU/visual comparison. P1 не считать
полностью закрытым до оставшихся failure/rollback/Wi-Fi/power-loss/production gates.
