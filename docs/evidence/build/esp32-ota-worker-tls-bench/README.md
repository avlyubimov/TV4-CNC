# Software evidence — UTC/HEAD в OTA-worker8192

Код `d27142ef0b6b39ac2d274a177aed13823416dc97`, tree
`518b0e20c30b2c03bc58ea0946824273f765e36d`, база main `53b0715` после PR #68.
Все 16 сборок выполнены из чистого source worktree 22 ноября 2026,
16:30:17–16:35:16 UTC. Использовались отдельные incremental build trees;
это clean-source проверка, не утверждение о fresh toolchain или битовой
воспроизводимости. Последующий commit содержит только документацию/evidence;
descriptor приведённых бинарников — `d27142e`, не SemVer релиза.

[Контракт режима](../../../ESP32_OTA_WORKER_TLS_BENCH.md),
[план](../../../plans/P1_OTA_WORKER_TLS_BENCH.md),
[полная матрица](matrix.json), [два новых образа](new-images.json).
BIN/ELF остаются локальными, в Git не опубликованы. Ни устройств, ни live
SNTP/HTTPS во время этих проверок не открывали; SDK I/O в host tests заменён
doubles. На ESP32 остаётся fecc05d, на F407 ee7d179. Hardware permission закрыто.

## Результаты

| Проверка | Результат |
|---|---|
| Полный host suite, реальный LVGL 9.2.2, startup/render | 156/156 PASS |
| Тот же suite, ASan+UBSan+LSan, detect_leaks=1 | 156/156 PASS |
| Actual native UTC/HEAD/monitor/service, GCC 15.2.0 analyzer | 7 ветвей PASS |
| Native SDK5/6 doubles, по 19 worker TLS отказных/успешных сценариев | 38/38 PASS, входят в 156 |
| Старый standalone backend, SDK5/6 success/open-fail | 4/4 PASS, входят в 156 |
| Actual service, control/stack/TLS, по 5 lifecycle сценариев | 15/15 PASS, входят в 156 |
| Build guards, 5 новых методов с subcases | PASS, входит в 156 |
| Реальные SDK builds/config/ELF/Flash16MiB/layout/dependency locks | 16/16 PASS |
| Новый backend imports | Только общий OTA monitor adapter; нет прямого SDK start/stop или task create/delete |
| UTF-8/локальные ссылки/3 workflow YAML/bash syntax/diff check | PASS |
| Новые compiler warnings/errors в 16 build logs | Не обнаружены |

[Host log](host-tests.log), [sanitizer log](sanitizer-tests.log),
[analyzer log](analyzer.log), [SDK footers](sdk-build-footers.log),
[software checks](software-checks.json), [scope scan](scope-checks.json).
CI выполняет те же ветви на push/PR; итог удалённых jobs виден в соответствующем
PR. Непрерывно пересобирать unchanged source после docs-only commit не требуется.

## Действительные образы

| SDK | Новый TLS-worker app | ELF SHA-256 |
|---|---:|---|
| 6.1.1 | 1 603 456 байт | `a9a22c6291fd950e5258e4c03098100197ef68ad2fec58c0adab08f9b56389ea` |
| 5.3.2 | 1 556 000 байт | `dcab04e510698cf7851f6b7eafa7ec3ca1d2eed86922206db2aafd389812b94d` |

App SHA-256 6.1.1:
`c78f9e8d641bb6439cc22093f604715f5b6d068a9a101ed6f95906cefebda04d`.
App SHA-256 5.3.2:
`08c1c5e30b8f24a626805f3f0edfcebfd26fd403534882a77217db59b6f6c1bf`.
Оба помещаются в factory2MiB действующей A/B-разметки; partition hash и image
descriptor/ELF SHA проверены отдельно в каждом build. Это не разрешение записи.

На каждом SDK: headless, rev12, memory-bench, standalone TLS-bench, normal OTA,
OTA trace3072, OTA stack8192 и новый OTA stack8192+TLS. [Сравнение](comparison.json)
подтверждает единственную config-разницу между двумя последними:
`TV4_OTA_WORKER_TLS_BENCH=false→true`. Значения DMA64KiB/TLS16384/4096,
PSRAM/XiP/RGB/pins/partition и dependency locks не менялись.

Прежние normal/control/stack image sizes совпадают с предыдущей software
матрицей `f655e56`; standalone TLS-bench вырос на 16 байт на обоих SDK после
совместного использования backend. Это размер Flash image, не измерение FPS,
latency, heap или stack. Производительность на устройстве здесь не измерялась.

Новый native object: entry compiler frame224 bytes, HTTP-open frame192,
snapshot80; adapter state432 bytes и максимальный own frame112 при сохранённых
ceiling512/192. На SDK5 headers-event48, на SDK6 32. Эти `.su` object frames
не полный runtime call stack и включают unlinked stubs. Requested worker8192
не подтверждает его runtime запас. Реальные TLS peaks/HWM пока не измерены.

## Закреплённые зависимости и ограничения

6.1.1: установленный SDK, commit
`051e4c4eb20346e043cdd47bc388fde75ff8af63`, describe v6.1.1,
tracked worktree clean; [запись](local-sdk.json). Toolchain esp-15.2.0_20251204.
5.3.2: immutable Docker image
`espressif/idf@sha256:5b8f58d9d9caa45b689a31a0b6a0cc53b84594e0ba5fdd8d28eeef7d6123242a`,
toolchain esp-13.2.0_20240530. CI6.1.1 сохраняет pinned image
`espressif/idf@sha256:2b4f966c37eaedc19542b0637a5f850b2eff9478d8e858192aa0073d92c2dc91`.
Локальный Docker build сохраняет nofile4096, cap-drop ALL/no-new-privileges
и отсутствие devices; CI container сохраняет явный nofile4096.
LVGL9.2.2/touch1.2.1/GT9111.1.3 и SDK-specific locks прежние; SHA locks в матрице.

Нет новых runtime budget floors, key/context, release/SemVer, installed image,
eFuse/option bytes, CAN/motion, USB migration или разрешений UART/SWD/reset.
Изолированный 2026 SNTP policy меняет UTC, source не authenticated. Будущий
exact-image стенд должен отдельно согласовать это и один verified HEAD, свежий
full backup/two-NVMe и возврат fecc05d. Crypto/workspace/installer/slot confirmation/
rollback/power-loss/cold boot/soak всё ещё требуют отдельной квалификации.
