# OTA RGB/progress fix — программная проверка

[План и аппаратные пределы](../../../plans/P1_OTA_DISPLAY_PROGRESS.md).
Этот отдельный runtime PR следует за отчётом PR #84; исходные опубликованные
bench.3 bytes не меняются. [Числовые результаты](results.json) относятся к
development builds до source commit, не являются одобренным release inventory.

- Полные274 host tests PASS; [журнал](host-tests.log).
- Полные274 ASan/UBSan/LSan PASS, leak detection включён;
  [журнал](sanitizer-tests.log). Пути checkout в журналах заменены на `<source>`.
- Actual LVGL9.2.2 в XRGB8888 и RGB565: diagnostic-only snapshots не создают
  invalidations, progress не трогает captions/button rectangles,5Hz coalescing,
  немедленные completion/error/restart, timer wrap и прежние confirmation gates.
- Четыре настоящих ELF/BIN ESP-IDF6.1.1/5.3.2: proposed baseline `.3` с memory
  trace и normal candidate `.4` без trace. Public-only owner-key inputs, без private
  key/публикации/плат. Config/image/memory checks PASS; worker12288/service432bytes
  прежние. Runtime heap/stack headroom нового IRAM режима ещё не квалифицирован.
- В обоих actual ELF `rgb_lcd_default_isr_handler`, bounce fill и EOF handler
  находятся в `.iram0.text`; соответствующие vendor GDMA IRAM flags включены.
  Always-restart-in-VSYNC выключен. PCLK/bounce/framebuffer/pins не менялись.

Positive compiled context — только конкретное предложение для review; новые
версии/точный release commit/tag/assets должны быть отдельно одобрены владельцем
до публикации и аппаратного применения этого пакета. Original context/ключи вне
Git/CI, протокол/криптография/HTTPS/floors/timeouts прежние.

Software tests не доказывают устранение физических прыжков или новый FPS/touch
latency. После source CI и approval нужен точный стенд signed INSTALL с визуальным
наблюдением, Flash/VALID readback и длительным boot. По указанию владельца успешный
новый проверенный релиз должен остаться на плате; обычного возврата fecc05d нет.
