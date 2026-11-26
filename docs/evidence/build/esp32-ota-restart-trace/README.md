# ESP32-S3: трассировка штатного перезапуска после OTA

Исходник `8b3db2f62584267e9e00f372d65b95c8029cdfe1`, база main
`ef3b47d242fab5d0271bac314998fafa421223c9`, [PR #95](https://github.com/avlyubimov/TV4-CNC/pull/95).
[План и критерии](../../../plans/P1_POST_INSTALL_RESTART_TRACE.md).

Результат — два настоящих ELF/BIN с выключенным по умолчанию
`TV4_OTA_RESTART_TRACE`. Используется прежний worker со стеком 12288 байт
и штатные действия оператора: CHECK → INSTALL → REBOOT. Обработчик shutdown
регистрируется до допуска обновлений. При отказе запись запрещена; после commit
нет новых регистраций, выделений памяти или проверок допуска. Новых задач,
криптографии, ключей, release/tag или изменений драйверов и watchdog нет.

## Проверенные сборки

По одной чистой сборке точного git archive на SDK с прежним утверждённым
публичным контекстом стенда. Версия принимающего образа `.4` нужна для policy
`.4→.5`; этот диагностический BIN отличается от опубликованного bench.4
и не подменяет его. [Дескрипторы, ELF, символы и память](builds.json).

| SDK | App, байт | SHA-256 |
|---|---:|---|
| 5.3.2 | 1556336 | `41912f5db6a45a7f8248c883e610d0bc84e8d8e4465fb22c0e0e69eaffbe394c` |
| 6.1.1 | 1603280 | `c5f689c6e259694c669f06e6372e39718fb86e9ee2b7d955d4e84d16be37de9c` |

SDK 6.1.1 установлен на Ubuntu: commit `051e4c4eb20346e043cdd47bc388fde75ff8af63`,
GCC 15.2.0 `esp-15.2.0_20251204`. SDK 5.3.2:
`espressif/idf@sha256:5b8f58d9d9caa45b689a31a0b6a0cc53b84594e0ba5fdd8d28eeef7d6123242a`,
GCC 13.2.0 `esp-13.2.0_20240530`. LVGL 9.2.2, touch 1.2.1, GT911 1.1.3
и отдельные lock-файлы сохранены.

Инспекторы обоих SDK подтвердили Flash 16 МиБ, PSRAM octal 80/XiP, rollback,
watchdog, RGB/GDMA IRAM guards, проверку дат TLS-сертификата, local trust/install
review, несовместимость диагностических режимов и отсутствие compile-time date.
DMA reserve 64 КиБ, TLS records 16384/4096, framebuffer и драйверы прежние.
Из обоих фактических ELF извлечены неизменные policy constants: internal 64/32 КиБ,
PSRAM 1 МиБ/128 КиБ, остаток стека 3072 байта; период 25 ноября 00:00 —
27 ноября 08:00 Asia/Almaty, anchor 600000/skew 5000 мс. Policy не продлена.

Проверены вызовы worker → boot/prepare/execute → SDK → wrapper → исходный noos.
Стартовый маркер использует штатный сериализованный SDK logger; финальные
маркеры и строки остаются в IRAM/DRAM с ROM-only выводом. Для SDK 5 применяется
прежняя analysis copy незагружаемой `.xt.prop`: entry point, PT_LOAD и каждый
загружаемый байт обязаны совпасть. Оригинальные ELF/BIN не изменяются.
Compiler frame стартового маркера — 48 байт, четырёх остальных — по 32 байта.
Ledger memory diagnostics занимает 432 из разрешённых 512 байт. Эти данные
не являются полным измерением runtime stack или производительности.

Повторная пара диагностических BIN не заявляется. CI сохраняет прежние пары
обычного reproducible-профиля и отдельно собирает trace на двух SDK с одноразовым
публичным software fixture версии `0.0.0-ci-trace`. Его приватная часть не
сохраняется. Fixture и BIN не поступают на плату или в release; owner context
не передаётся в CI.

## Программные проверки

[CI точного исходника](source-ci.json): 12 запусков workflow, 34/34 задачи PASS.
[Локальные результаты](checks.json): 334/334 host и 334/334 ASan/UBSan/LSan PASS,
leak detection включён. Статический анализ изменённого marker C прошёл.
Добавлен 21 сценарий actual-service C с SDK doubles: штатная установка,
ожидание оператора, отсутствие auto-reboot, отказ регистрации до записи,
stale health, ошибки транспорта/ресурсов, отмена и trust/version/partition/rollback.
11 методов Python проверяют compile/shell/ELF guards внутри одного CTest suite.

Конечный контроллер стенда прошёл [40 групп](procedure-tests.txt) и
[те же 40 при Python `-O`](procedure-tests-optimized.txt). Проверены две копии
до записи, точные spans, частичные и посторонние записи, одна recovery без
повтора, watchdog/touch/clock/cleanup, полные 90 секунд без host reset,
порядок маркеров после commit, отказ malformed/legacy boot marker, дедлайны
и проверка итоговых копий. RAM Flash и преобразованные UART — software fixtures.

[Пять metadata oracle cases](metadata-oracle.json) сопоставлены с реальным
официальным методом SDK 6.1.1 для seq 1…4 через FakeTarget, без SerialTarget.
Свежая native/Python проверка прежнего signed bench.5 приняла подпись,
совместимость и полный реальный image stream; tampered signature и wrong key
отклонены. Все 12 published assets сверены с утверждёнными SHA-256.

## Сохранённые отказы и аппаратный результат

[Первоначальные сборки](initial-builds.json) и [CI](initial-source-ci.json)
относятся к `2cae6c4`. Его [аппаратный контроль](../../hardware/2026-11-26-esp32-post-install-trace/attempt1.md)
остановлен до сети: startup ROM printf перемешался с параллельным SDK log.
Повреждённая строка не восстанавливается догадкой. Исправлены вывод маркера
и проверки; исходный отказ и единственная полная recovery сохранены.

При первоначальной подготовке `2cae6c4` dependency fetch отказал по DNS;
использован официальный MbedTLS 3.6.7 archive с исходной SHA-256 проверкой.
LSan отказал под ptrace; полный набор вне него прошёл с `detect_leaks=1`.
SDK 6 host wrapper сначала не получил IDF_PATH; тот же ELF затем проверен
с явным export. В первом synthetic UART fixture маркеры дописывались после
незавершённой строки; исправлен fixture, строгий collector не ослаблялся.
Первоначальные логи приватны. Это ограничения окружения, не подтверждённые
несовместимости API. Чистые сборки исправленного `8b3db2f` прошли полностью.

[Стенд SDK 6.1.1](../../hardware/2026-11-26-esp32-post-install-trace/README.md)
закрыт PASS: signed INSTALL, операторский restart/SW3/WDT0, native VALID,
реальные 16 МиБ и две финальные NVMe-копии. Оставлен published bench.5.
Поле runtime_qualified=false в ELF receipts обозначает границу software inspector;
аппаратный результат учитывается отдельно. Причина прежнего RTCWDT неизвестна.
Runtime SDK 5.3.2, production policy/budget, RF/AP/power loss и physical FPS/latency
открыты. Ключи/context, Flash/NVS, SSID/IP/MAC и сырой UART не публикуются.
