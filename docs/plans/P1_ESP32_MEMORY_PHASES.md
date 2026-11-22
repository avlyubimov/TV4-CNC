# P1: измерения памяти действующей OTA-прошивки

База main6f95d27, PR #1–61 слиты. Установленный fecc05d/rev12-ota уже прошёл
USB migration/factory/XiP/HMI/saved Wi-Fi; разрешённый аппаратный сеанс закрыт.
Новая задача владельца «дальше» продолжает P1. Docs-only ограничение отчёта
PR #61 завершено; этот отдельный PR содержит следующую интеграцию firmware.

Результат: настоящий rev12-ota ELF/BIN с ограниченными фазными замерами и
явным boot self-test marker, готовый к отдельно разрешённому измерению.
Не добавлять новый профиль, криптографию, доверенные ключи или Flash writer.

1. Разобрать закреплённый SDK: DMA reserve перед app_main и сумма per-heap
   historical minima. Не объявлять причину hardware112 или OOM доказанной.
2. Только при явном TV4_OTA_MEMORY_DIAGNOSTICS=1 начать SDK local-minimum окно
   до создания OTA-worker/инициализации платы. Default off и отдельный build tree:
   pinned SDK start выделяет/asserts bookkeeping и не отклоняет nested start.
   Main публикует семь фиксированных startup snapshots без I/O/аллокации;
   существующий worker выводит их после self-test. Отдельные конечные окна
   workspace/SNTP/session/cleanup выполняются только при уже разрешённой операции.
3. Помечать scope minima и OTA-worker stack validity в serial diagnostics;
   не выдавать watermark caller main за worker. Нет logging/heap monitor
   start-stop из LVGL callbacks/flush/per-chunk пути. Очереди и вывод ограничены.
4. Монитор имеет одного владельца, profile/Kconfig/source guards исключают
   активный TLS bench: сам SDK не обеспечивает арбитраж владельцев. При start failure не останавливать чужое
   окно; при неизвестном результате stop пометить данные unknown, не подменять
   их floors. Диагностика не изменяет install/rollback/crypto authority.
5. Проверить фактический SDK adapter с fake API, отказами и лимитами; прогнать
   host/sanitizers, обе SDK×5 profiles, source/config/sizeof/frame проверки.
   Сохранить реальные результаты, SDK pins/DMA reserve/TLS records/framebuffer
   не менять. Подготовить отдельный PR, не сливать самостоятельно.

Приёмка software: фиксированные bounded UART records и ELF/BIN, все тесты и
матрица зелёные; ordinary profiles без OTA-монитора. Приёмка hardware остаётся
открытой: отдельное exact-image разрешение, сохранённая A/B карта/NVS и recovery,
замеры по фазам, owner HMI/touch/Wi-Fi и анализ стоимости instrumentation.
Никаких устройств/SWD/UART/reset/Flash, eFuse/option bytes, CAN/motion или release
в рамках программной подготовки. Production key/context/resource floors не назначать.
