# ESP32: OTA-worker8192 без ключа

## Выполненный аппаратный стенд

База main `dfa9f28`, PR #1–67 слиты. [Стенд3072/8192](evidence/hardware/2026-11-22-esp32-worker-stack/README.md)
завершён22ноября за928.998s по отдельному разрешению: fresh16MiB/device MD5/
SHA-256/two-NVMe, контроль3072 и три reset-запуска8192, три app/factory writes,
пять UART captures и возврат всего factory2MiB fecc05d/verify PASS.
Владелец подтвердил HMI/касания/Wi-Fi/OTA SETUP временного8192 и возвращённого
fecc05d. На ESP32 fecc05d/factory/tv4-16m-ab-v1; F407 ee7d179 не открывался.
Полный16MiB restore не понадобился; сеанс и разрешение закрыты.

Worker8192 watermark6608–6784bytes, BOOT local15767–15863; periodic internal
free14291–16863/largest7680 против control21795–21799/largest12288 после IP event.
Первый8192 включал UI-проверку: нагрузка различалась. Это unprovisioned reset
boot evidence, не configured TLS/install/production floor или cold-power/soak.
Ключ/context не заданы, CHECK/INSTALL закрыты. FPS/latency/slot confirm/rollback/
power-loss/full recovery не приняты. Новых hardware разрешений нет.

Software evidence ниже фиксирует подготовку до hardware; его static
`worker_stack_runtime_qualified=false` не заменяется общим production PASS.
Реальная scope-limited приёмка описана в отдельном hardware report.

## Выбор и границы

`TV4_OTA_WORKER_STACK_BENCH=1` принимается только для rev12-ota вместе с
`TV4_OTA_MEMORY_DIAGNOSTICS=1`, без `TV4_OTA_CONTEXT`. Kconfig default n,
build script/CMake/C/artifact guards отклоняют другие сочетания/provisioning.
Прямое включение через sdkconfig не обходит C compile guards. Обычный USB
migration tool этот диагностический образ не принимает.

Тот же xTaskCreate получает8192bytes, как у настроенного OTA, вместо3072 у
ненастроенного. Нет резервного malloc5120 или новой задачи. Normal/trace-control
без нового флага сохраняют3072; configured OTA без нового флага сохраняет8192.
Ключ/context не подставляются: после self-test остаётся SETUP, CHECK/INSTALL
закрыты. Это не configured OTA и не TLS/install нагрузка.

Меняется размер существующего worker и его стендовая boot-result строка.
Нет новых сетевых проб/SNTP/HTTPS/timers/Flash действий. Wi-Fi profile, rollback
deadline до board init, PSRAM/layout/self-test, monitor ownership и touch-release
gate прежние. DMA64KiB/TLS16384/4096/framebuffer/clocks/partition/pins сохранены.

## Сборка без устройств

```bash
# Контроль: прежний ненастроенный worker3072 с tracing.
TV4_IDF_PATH="$HOME/.espressif/v6.1.1/esp-idf" TV4_OTA_MEMORY_DIAGNOSTICS=1 \
  ./tools/build_idf.sh rev12-ota 6.1.1 local
# Стендовый worker8192, без provisioning.
TV4_IDF_PATH="$HOME/.espressif/v6.1.1/esp-idf" TV4_OTA_MEMORY_DIAGNOSTICS=1 \
  TV4_OTA_WORKER_STACK_BENCH=1 ./tools/build_idf.sh rev12-ota 6.1.1 local
# Закреплённый резервный SDK, без devices в контейнере.
TV4_OTA_MEMORY_DIAGNOSTICS=1 TV4_OTA_WORKER_STACK_BENCH=1 \
  ./tools/build_idf.sh rev12-ota 5.3.2 docker
```

Выход — `build-idf-SDK-rev12-ota-memory-diagnostics-worker-stack-bench`.
Normal/trace-control используют прежние отдельные build trees. Проверка:

```bash
python3 tools/check_ota_memory_diagnostics.py \
  build-idf-6.1.1-rev12-ota-memory-diagnostics-worker-stack-bench rev12-ota 6.1.1 \
  --memory-diagnostics --worker-stack-bench
```

`worker_stack_bytes_requested` — выбранный размер, не аппаратный замер.
В boot-result строке после закрытия BOOT window выводятся
`worker_stack_bytes=8192 stack_bench=1 provisioned=0`; control строка прежняя.
Семь phase IDs/time-order и bounded ledger не изменены. Нового владельца SDK
minimum monitor нет; watermark не равен размеру выделения или full stack bound.

## Проверки и следующий аппаратный шаг

Actual ESP branch service.c исполняется на host через SDK boundary для обоих
размеров: наблюдается реальный xTaskCreate argument, self-test, task/PSRAM/
health-timeout отказы, pending-verify reset и закрытые команды SETUP. SDK doubles
не выделяют target stack и не измеряют RTOS/ESP heap/PSRAM/timing.
Negative compilation проверяет OTA/trace/provisioning guards; artifact/script
tests — explicit selection, container flags и запрет migration.

CI сохраняет10 matrix jobs и trace control. Два OTA jobs дополнительно строят
worker-stack variant: всего14 SDK сборок. Evidence связывает source/image/ELF/
config/requested stack, а runtime qualification остаётся false.
[Результаты](evidence/build/esp32-ota-worker-stack-bench/README.md):14/14 clean
ELF/BIN, host108/108, ASan/UBSan/LSan108/108, actual ESP service analyzer0/1 PASS.
Docker script и Actions SDK containers явно задают `nofile=4096:4096` вместо
daemon default; это настройка среды компиляции, не изменение памяти firmware.
[Формат Docker ulimit](https://docs.docker.com/reference/cli/docker/container/run/#set-ulimits-in-container---ulimit).

Выполнены реальные аллокация/три reset boots/стеки/UI/Wi-Fi/SETUP и возврат
factory. Следующий exact-image стенд снова требует отдельного разрешения. Не вычитать5120
из старых minimum вместо замера и не открывать TLS/install без reviewed local
trust/settings. Этот PR не выполняет UART/SWD/reset/Flash/eFuse/CAN/motion.

[План](plans/P1_OTA_WORKER_STACK_BENCH.md), [следующий этап](NEXT_STAGE.md),
[предыдущий стенд](evidence/hardware/2026-11-22-esp32-startup-order/README.md).
