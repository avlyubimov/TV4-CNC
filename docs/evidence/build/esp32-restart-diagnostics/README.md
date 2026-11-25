# ESP32-S3: образ диагностики одного software restart

Код [PR #93](https://github.com/avlyubimov/TV4-CNC/pull/93) слит владельцем.
Source `f8199557def895f301cd7e0cbbdde32e54b87681`, merge main
`d541bafc9e8dba8d261d95b1d3f2debf45ced250`; деревья firmware/tools/tests совпали.
[Режим и границы](../../../ESP32_RESTART_BENCH.md).

Результат — настоящие ELF/BIN для двух SDK с выключенной по умолчанию
диагностикой, а не исправление причины RTC watchdog. Оригинальный отказ bench.5
сохраняется. Публикации бинарного релиза и новой криптографии нет.

## Проверенные сборки

Одна чистая сборка каждого SDK из архива точного source, без OTA-контекста.
SDK 6.1.1 — установленный на Ubuntu; SDK 5.3.2 — прежний закреплённый Docker.
[Дескрипторы, SHA-256, положения символов и compiler frames](builds.json).

| SDK | App, байт | SHA-256 |
|---|---:|---|
| 5.3.2 | 1556224 | `1fd316cfc9e82cf75b990315e171f6076854919260f59bbddf88b9348bbdc30f` |
| 6.1.1 | 1603152 | `b998d2f9e53e01f39697d4caad1b696575a79617e4f67c348b1042f74fa15ac2` |

Оба помещаются в временный factory-раздел 2 МиБ. Общий инспектор проверил
Flash16MiB, PSRAM/XiP, rollback/watchdog, разметку, TLS certificate dates,
unprovisioned/exclusive flags и отсутствие compile-time date/time.
Это по одной диагностической сборке на SDK; собственная повторная пара не
заявляется. CI отдельно сохраняет прежние пары обычного reproducible-профиля.

В фактических ELF проверены переход SDK→wrapper→исходный SDK restart,
IRAM двух финальных маркеров и DRAM их строк. Собственные compiler frames
пяти диагностических функций — по 32 байта на обоих SDK. Это не полный
runtime stack SDK/ROM/shutdown и не измерение производительности.

[Первоначальная инспекция a8503d1](first-inspection.json) отказала на SDK5:
разреженная nonallocating .xt.prop не описывала esp_restart, objdump показывал
его как данные. Итоговый инспектор анализирует временную копию с нулевой
длиной этой таблицы, требуя равенства entry point, всех PT_LOAD адресов,
размеров, flags и каждого загружаемого байта. Оригинальные ELF/BIN остаются
неизменными. Отсутствующий вызов или изменение loaded data по-прежнему дают FAIL.

## Программные проверки

[Source push/PR](source-ci.json): 34/34 PASS.
[Новый main](main-ci.json): 17/17 PASS. Самостоятельного merge не было.

313/313 host и 313/313 ASan/UBSan/LSan PASS. Добавлены 22 CTest suites:
actual worker/marker C с SDK doubles и build guards. Шесть методов Python
проверяют default-off/exclusive shell/config/C gates, запрет миграции,
отказ при cached-зависимостях/обходе wrapper и сохранность loaded ELF bytes.

Новый конечный аппаратный packet отдельно прошёл
[11 групп RAM-Flash/serial-проверок](procedure-tests.txt) и
[те же 11 при Python `-O`](procedure-tests-optimized.txt).
Включены отсутствие записи до двух копий, точные spans, partial/outside write,
одна full recovery без повтора, WDT/stale touch как FAIL, полное 90-секундное
окно без промежуточного host reset, deadlines и sealed admission до SDK import.
Преобразованные прежние приватные UART — только software fixtures.

## Аппаратный результат и ограничения

[Отдельный отчёт](../../hardware/2026-11-25-esp32-restart-f819955/README.md).
Его результат не выводится из компиляции, SDK doubles или наличия маркеров.
Ключи, полные Flash/NVS, SSID/IP/MAC, контексты и сырой UART не публикуются.
Physical FPS/latency, runtime SDK5.3.2 и production budget не приняты.

