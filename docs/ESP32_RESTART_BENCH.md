# Диагностика одного software restart ESP32-S3

Результат этого изменения — отдельный прошиваемый HMI-образ для локализации
RTC watchdog после bench.5. Исправление причины пока не заявляется.
[Первоначальный отказ и восстановление](evidence/hardware/2026-11-25-esp32-bench5/README.md)
сохраняются. Рабочий релиз на плате — bench.4 OTA1 VALID.

## Границы образа

`TV4_OTA_RESTART_BENCH=1` принимается только вместе с
`TV4_OTA_MEMORY_DIAGNOSTICS=1` в `rev12-ota`, без локального OTA-контекста,
signed CHECK, worker8192 и TLS bench. По умолчанию выключен. Скрипт, CMake,
C и инспектор проверяют границы; migration/release authoring не принимают
такой образ как обычный OTA-кандидат.

Используется существующий OTA-worker со стеком 12288 байт, как в настроенном
OTA. Новых задач нет. LVGL/GT911/RGB/Wi-Fi, PSRAM XiP, DMA reserve, watchdog,
TLS и разметка сохраняются. Внешние CHECK/INSTALL/REBOOT закрыты. Нет SNTP,
HTTPS, Flash writer или постоянных маркеров в NVS/RTC. Приводы/CAN/eFuse/
option bytes и STM32 не затрагиваются.

После self-test только POWERON/EXT допускают одну попытку. Worker ждёт минимум
45 секунд, проверяет непрерывную свежую HMI-health/Wi-Fi не менее 2 секунд.
Общий предел ожидания — 60 секунд после self-test. Пороги перед сбросом:
internal free/largest ≥64/32 КиБ, PSRAM free/largest ≥1 МиБ/128 КиБ,
остаток стека ≥3072 байт. Это защитные ограничения изолированного стенда,
не production policy. Нехватка ресурсов, недоступный sampler, отсутствие сети,
устаревший health или ошибка shutdown callback дают отказ без перезапуска.

SW/WDT/panic/unknown после загрузки не запускают новую автоматическую попытку.
Таким образом, ожидаемый программный сброс и отказ RTC watchdog не образуют
автоматическую петлю. Внешние аппаратные сбросы — отдельные события;
конечный host-пакет ограничивает и их число.

## Что показывают маркеры

| Маркер | Место |
|---|---|
| `stage=boot reset=… core=… eligible=…` | Диагностическая ветка worker после boot self-test |
| `stage=ready` | Ресурсы проверены; числа относятся к этому worker |
| `stage=request core=…` | Непосредственно перед штатным `esp_restart()` |
| `stage=shutdown_hook core=…` | Зарегистрированный SDK shutdown callback |
| `stage=noos core=…` | Wrapper перед исходным `esp_restart_noos()` |

Используется [GNU ld --wrap](https://sourceware.org/binutils/docs/ld/Options.html).
В установленных SDK проверяется исходник `components/esp_system/esp_system.c`
и S3 `port/soc/esp32s3/system_internal.c`; исходники SDK не изменяются.
Wrapper и callback находятся в IRAM, строки в DRAM, вывод через ROM printf.
На SDK 5.3.2 обнаружена разреженная nonallocating .xt.prop: она не описывает
esp_restart, и objdump отображает его байты как данные. [Binutils использует
эту таблицу для выбора code/data](https://sourceware.org/pipermail/binutils-cvs/2018-June/048352.html).
Инспектор в этом точном случае создаёт временную копию для анализа, скрывая
только размер незагружаемой таблицы. Entry point, все PT_LOAD адреса/размеры/
flags и каждый загружаемый байт обязаны остаться равными. Исходные ELF/BIN
не записываются; при любом изменении приёмка прекращается. Вызовы wrapper и
оригинального SDK по-прежнему должны быть видны в декодированных инструкциях.
Это не нормализация артефактов для сравнения воспроизводимости.

После маркера вызывается прежний SDK reset; ни WDT, ни caches, ни периферия
в wrapper не перенастраиваются. Собственные compiler frames ограничены,
но это не доказательство полного стека SDK/shutdown или времени выполнения.

Проверка реального ELF подтверждает вызов wrapper из SDK, передачу в исходный
reset и отсутствие cached-зависимостей в двух маркерах. В обычных образах
инструментация должна отсутствовать. Наличие `noos` локализует последнюю
наблюдавшуюся границу; оно не различает внутренний сбой reset и ранний ROM.
Отсутствие строки также может означать потерю UART. Временная инструментация
может повлиять на тайминг; единичный PASS не отменяет прежний FAIL.

## Сборка без устройства

```sh
TV4_OTA_MEMORY_DIAGNOSTICS=1 TV4_OTA_RESTART_BENCH=1 \
TV4_REPRODUCIBLE_BUILD=1 ./tools/build_idf.sh rev12-ota 6.1.1 docker
```

Отдельный каталог: `build-idf-6.1.1-rev12-ota-reproducible-memory-diagnostics-restart-bench`.
Повторить для 5.3.2. В активированной среде соответствующего SDK:

```sh
python tools/check_restart_bench.py \
  build-idf-6.1.1-rev12-ota-reproducible-memory-diagnostics-restart-bench \
  rev12-ota 6.1.1 --enabled
```

CI собирает оба SDK и проверяет отсутствие маркеров во всех обычных профилях.
Host-тесты выполняют actual service/marker C с SDK doubles: штатный одноразовый
сброс, warm/WDT/panic/unknown, timeout/health/Wi-Fi, пять порогов памяти/стека,
недоступный sampler, ошибка callback и изменение health перед сбросом.

## Закрытый аппаратный сеанс и последующие проверки

[Точный source `f819955` и SDK 6.1.1](evidence/hardware/2026-11-25-esp32-restart-f819955/README.md) дали один PASS:
reset_reason=1→3, watchdog_boot=0, 36/36 guards на каждом запуске, 90 секунд после reset,
исходные factory/otadata и весь bench.4 проверенно возвращены. Это не
post-INSTALL тест; причина исходного bench.5 FAIL остаётся открытой.
Закрытый пакет не повторять. Следующие требования относятся к новому пакету.

Сначала exact source/CI/ELF/BIN/хеши и проверенный конечный пакет. Существующее
разрешение владельца на изолированные платы учитывается, закрытый пакет
bench.5 повторно не используется. Новый пакет обязан предусмотреть свежие
16 МиБ и две проверенные копии, ограниченные записи и runtime capture,
сохранность Flash вне разрешённых областей и резерв времени на восстановление.

Временный диагностический образ не является опубликованным релизом. По окончании
вернуть проверенный bench.4; прежние release assets/tag не менять.
Watchdog reason/counter и строгий критерий отказа сохраняются даже при рабочем
экране и SDK self-test. Единичный аппаратный PASS без INSTALL описан выше;
настроенный post-INSTALL restart и SDK5.3.2 runtime не квалифицированы.

