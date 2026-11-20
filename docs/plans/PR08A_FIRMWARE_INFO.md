# PR-08a — сведения о ПО и доступности обновлений

20 ноября2026. База main `b56986f`, PR #1–34 слиты; новых открытых PR нет.
Roadmap разрешает отдельный UI scaffold без вымышленных версий/установки.

1. Добавить «Параметры» с переходами Wi-Fi / «ПО и обновления»; сохранить
   shortcut Wi-Fi, существующие экраны, кириллицу и постоянный STOP/status bar.
2. Bounded read-only local build/SDK/profile snapshot: на ESP32 из app descriptor
   и compile configuration, в preview явно simulator fixture. Не подставлять
   тестовые SemVer/peer/protocol/key/layout значения как подтверждённые.
3. На новом экране показывать неизвестные STM32/CAN версии и отсутствие
   доступной проверки/установки. Нет вызовов HTTPS/verifier/session/OTA writes,
   download/install/cancel/progress или фиктивной истории.
4. Callbacks только запрашивают navigation; создание/refresh/delete в LVGL owner.
   Новые панели создавать по требованию, освобождать при выходе: не удерживать
   дополнительный набор widgets в heap обычного dashboard/Wi-Fi/TLS bench.
5. Host model boundary/unknown/profile/copy tests; real LVGL navigation/glyph/
   bounds/overlap/long-string/fault/keyboard-cleanup/lifecycle render checks на
   XRGB8888 и RGB565; sanitizers, SDK matrix8 и прежние STM32 gates.
6. Обновить docs/status/evidence, показать preview, создать PR без merge.

Критерии: текущие сведения имеют provenance, неизвестные остаются неизвестными;
Wi-Fi доступен прежним shortcut; элементы не перекрываются на800×480; новые
виджеты не остаются после выхода; нет нового сетевого/Flash/двигательного пути.
Точные зависимости и аппаратные настройки прежние. Не выполнять device/SWD/
serial/reset/flash/option bytes/network bench. F407 HSE/полная маркировка/RAM/
секторы/pinout/GPIO/CAN остаются неизвестными, адреса Flash не резервируются.
