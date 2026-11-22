# ESP32 OTA: фазные измерения RAM и стека

[Проверенная software сборка и cold-render evidence](evidence/build/esp32-startup-memory-order/README.md).

## Порядок старта после main973a9ba

Первый полный dashboard refresh и UI_READY теперь предшествуют запуску
существующих export/Wi-Fi services. OTA worker/deadline по-прежнему запускается
до board init; touch-release gate, минимальное время заставки и буферы сохранены.
Семь ID стабильны, хронология теперь `0,1,2,3,5,6,4`. Worker печатает slots по
ID, поэтому sample_ms важнее порядка строк: WIFI_STARTED может иметь более
позднее время, чем UI_READY. Старые отчёты fc90200 не переинтерпретируются.
При ошибке первого dashboard flush optional services не стартуют.

Host integration компилирует настоящий app_main с настоящим LVGL9.2.2 и
SDK/board doubles. Она проверяет полное покрытие кадра до services, отказы и
held-touch. Её builtin pool/64-bit ABI/fake clock/radio не измеряют ESP32 heap,
FPS или сетевую нагрузку. Изменение порядка требует нового отдельного стенда;
на плате остаётся fecc05d, ни ключ, ни resource floors не назначены.

## Завершённый аппаратный стенд fc90200

22ноября [новый стенд](evidence/hardware/2026-11-22-esp32-ota-memory/README.md) по отдельному разрешению выполнен:
fresh16MiB/two-NVMe, app-only trace,7startup/BOOT window/16samples,
возврат всего factory2MiB fecc05d/verify/4samples, owner HMI/touch/Wi-Fi/SETUP PASS.
APP_ENTRY history202308 → local BOOT minimum1887 к UI_READY → history112 после
stop; OTA-worker watermark1488из3072bytes. Гипотеза112 ещё до HMI не
подтвердилась этим диагностическим образом. Конкретный startup allocator и
configured8192/TLS/install budget остаются открытыми. На плате fecc05d,
сеанс608.700s закрыт. Дальнейший текст сохраняет контракт подготовки PR #62;
его «новых device операций нет» относится к программной интеграции.

P1 от main `6f95d27d90093d1dffcf43cf2bbc12acd1063fde`, после слитого PR #61.
На плате остаётся проверенный `fecc05d/rev12-ota`, factory + A/B. Этот этап
готовит следующий ELF/BIN программно; новых UART/reset/Flash/backup операций нет.
[План интеграции](plans/P1_ESP32_MEMORY_PHASES.md).

## Что означает прежний минимум 112 байт

В [первом стенде](evidence/hardware/2026-11-22-esp32-ota-migration/README.md)
`internal_free` составлял 22851–22907, largest block 7680, historical minimum112.
`heap_caps_get_minimum_free_size` суммирует per-heap minima, а не измеряет
одновременный остаток всех heaps. В установленном SDK6.1.1 перед `app_main`
`reclaim_startup_stack_memory_for_heap` включает startup heaps и выделяет резерв
DMA. Резерв выделяется блоками из существующих heaps и регистрируется как новые
heaps; после startup их historical minima инициализируются нулём. Эта комбинация
рассматривалась при программной подготовке как возможный источник малого
минимума до HMI. Выполненный fc90200 дал APP_ENTRY202308, local BOOT1887 и
history112 после stop: гипотеза малого112 уже на APP_ENTRY не подтвердилась.
Конкретный allocator требует локализации; возвращаемая per-heap история не
измеряет simultaneous free. Данные прежнего fecc05d до HMI не снимались.

Официальные API: [6.1.1](https://docs.espressif.com/projects/esp-idf/en/v6.1.1/esp32s3/api-reference/system/mem_alloc.html),
[5.3.2](https://docs.espressif.com/projects/esp-idf/en/v5.3.2/esp32s3/api-reference/system/mem_alloc.html).
Для поведения важны также закреплённые исходники `components/heap/heap_caps.c`,
`heap_caps_init.c`, `components/freertos/app_startup.c` и PSRAM DMA pool.
В проверенном SDK6.1.1 local-minimum start выделяет bookkeeping с `assert`,
а повторный start не отклоняет действующее окно вопреки описанию в заголовке.
Поэтому сам API не является арбитром владельцев или гарантией отсутствия OOM.

## Обычная и диагностическая сборки

Обычный `rev12-ota` сохраняет global minima. Добавлены:

- UART-маркер `boot_self_test checks_pass=… sdk_finish=…` после возврата
  существующего SDK finish; предыдущие health/PSRAM/layout/rollback условия те же.
- `ota_stack_valid` и `ota_stack` в `tv4_diag`: watermark берёт только фактический
  OTA-worker. При неудаче создания задачи стек main больше не выдаётся за OTA.
- `heap_min_scope`: 0 — historical, 1 — local window, 2 — unknown/transition.
  Два коротких чтения revision окружают прежние heap queries, без добавочного
  обхода PSRAM heap в периодическом пути main. Изменение scope за время чтения
  помечается unknown. Это не атомарный снимок всех heaps.

Только явный **`TV4_OTA_MEMORY_DIAGNOSTICS=1`** включает SDK monitor и фазные
records. Новый profile не вводится; Kconfig default n, только при OTA_ENABLED.
Build tree имеет суффикс `-memory-diagnostics`; inspector требует отдельный
`--memory-diagnostics`. Flag1 на headless/ordinary/TLS profiles отвергается.
Production USB migration tool этот диагностический вариант не принимает.

```bash
TV4_IDF_PATH="$HOME/.espressif/v6.1.1/esp-idf" ./tools/build_idf.sh rev12-ota 6.1.1 local
TV4_OTA_MEMORY_DIAGNOSTICS=1 TV4_IDF_PATH="$HOME/.espressif/v6.1.1/esp-idf" ./tools/build_idf.sh rev12-ota 6.1.1 local
python3 tools/check_ota_memory_diagnostics.py build-idf-6.1.1-rev12-ota-memory-diagnostics rev12-ota 6.1.1 --memory-diagnostics
```

Это только build/inspection. Нового доверенного ключа/production SemVer/time/
resource context нет, SNTP/HTTPS не запускаются по факту включения диагностики.
Проверка/установка по-прежнему закрыты без настройки владельца. В Docker CI
каждый из двух существующих OTA jobs собирает оба варианта; остальные profiles
и оба SDK сохраняются. SDK/dependency/action pins не меняются.

## Границы и формат замеров

Main публикует семь одноразовых point samples в фиксированные слоты. Вызовы
делаются в startup-коде, без UART I/O/новых задач/аллокации и вне LVGL callbacks.
OTA-worker после self-test выводит до семи строк `tv4_ota_mem kind=0 id=…` и
`startup_mask=127 expected_mask=127 complete=1`. Недостающая маска означает
неполное покрытие. Повторный flush или post в закрытый ledger не добавляет строк.

| Startup id | Граница sample |
|---|---|
| 0 | APP_ENTRY, до OTA task/board init и нового SDK окна |
| 1 | RGB init вернулся |
| 2 | LVGL draw buffer создан |
| 3 | GT911/input готовы |
| 4 | Wi-Fi service start вернулся, не обещание DHCP |
| 5 | Dashboard/firmware metadata созданы |
| 6 | Splash закрыт, первый dashboard refresh завершён, до optional services |

SDK окно BOOT начинается до создания worker/board init и заканчивается после
health/256-byte PSRAM/layout checks. Startup minima после APP_ENTRY кумулятивны
от начала этого окна: отдельный точный пик каждой из семи фаз не измеряется.
Затем worker может измерять четыре конечных окна **только внутри уже разрешённой
оператором операции**, не инициируя её автоматически:

| Window id | Окно |
|---|---|
| 1 | BOOT |
| 2 | WORKSPACE acquire |
| 3 | Существующая SNTP sync |
| 4 | Существующая signed HTTPS/session/install, без per-chunk logging |
| 5 | Forget time/release workspace cleanup, если boot ещё не выбран |

На окно выводится четыре строки (kind1 before, kind2 start, kind3 end,
kind4 after SDK stop) и одна строка результатов start/stop. Вывод только worker,
после закрытия окна; нет I/O внутри heap/mux critical sections. Main↔worker
публикация slots защищена коротким mux, SDK heap calls выполняются вне него.
В одном boot — не более 13 trace-строк, в операции — не более 20. Private raw
UART сохраняется целиком локально; для публикации разрешены только разобранные
числовые поля, потому что штатные Wi-Fi логи могут содержать SSID/IP/MAC.

`sample_ms` и heap values относятся к моменту sample. `sample_stack` — watermark
задачи, сделавшей sample: main для startup/BOOT before-start, OTA-worker для
остальных границ. `worker_stack_at_emit` получен позже в worker при выводе;
его нельзя выдавать за стек main на старой startup-границе. Периодический
`ota_stack` обновляется worker после tracing/logging, затем в ожидании команды.
Все значения стека — ESP-IDF bytes; они включают стоимость instrumentation.

## Владение, отказы и пределы

Один serialized service владеет монитором; boot begin выполняет main до
публикации worker, все остальные lifecycle calls — worker. При xTaskCreate
failure main закрывает своё окно без логирования. Kconfig и source/ELF guards
исключают одновременный UTC/TLS bench. SDK не обнаруживает другого владельца
самостоятельно; нельзя добавлять параллельного вызывающего этот API.

Перед SDK start используются только диагностические bookkeeping guards:
internal free8192, internal largest4096, PSRAM largest4096. Это не production
floors, не reservation и не гарантия против конкурентной OOM. Они не меняют
никакого решения installer. Guard/start failure не вызывает SDK stop; неизвестный
stop помечает minima unknown и отключает дальнейшие monitor calls до reset.
Point samples остаются доступны, `complete=0` запрещает принимать окно за замер.
После успешной активации boot partition окно cleanup не начинается: новая
диагностическая аллокация/assert не добавляется в путь ожидаемого ручного reboot.

SDK monitor имеет небольшое дополнительное выделение, resets/restores global
minima; UART/heap traversals и stack scans имеют стоимость. Поэтому trace
default off, нет новых циклов/timers/per-chunk callbacks; нормальные DMA reserve
64 КиБ, TLS records16384/4096, framebuffer, PCLK/PSRAM clocks сохранены. Данные
диагностического варианта ещё требуют отдельного hardware overhead/load review.
Дополнительные432bytes state и SDK bookkeeping меняют геометрию heaps;
повторения ровно112bytes в новом image не обещается. APP_ENTRY и local window
сравнивают внутри одного запуска диагностической firmware.
Они не доказывают полный call-stack peak, CPU load, FPS, scanout, latency или soak.

## Программная и будущая аппаратная приёмка

Actual SDK adapter проверяется fake API: bounded slots/вывод, отсутствие heap/
logging под mux, start/guard/stop failure, смена scope во время sample, cleanup
без worker и disabled mode. Inspector проверяет реальные ELF/objects/.su:
state≤512bytes, own compiler frame≤192bytes, отсутствие monitor с flag0 и
единственного project owner с flag1. Эти ceilings не заменяют общий runtime
RAM/stack budget SDK/HMI/TLS. [Результаты сборок](evidence/build/esp32-ota-memory-phases/README.md).

Перед новым стендом: review exact app/hash/flag, отдельное разрешение владельца,
та же изолированная плата и проверенный snapshot/recovery. Действующая A/B карта
уже установлена: это не повод повторять четыре записи миграции или обычный
`rev12 flash`. Процедуру app-only диагностического запуска и возврата проверенного
fecc05d готовят отдельно; этот документ её не запускает. Установка подписанного
образа в OTA-slot/confirmation/rollback/power loss остаётся следующим испытанием
после owner key/context/resource решений. eFuse/STM32/CAN/motion не затрагиваются.
