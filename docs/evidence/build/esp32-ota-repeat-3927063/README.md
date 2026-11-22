# P1 repeat package 3927063 — software evidence

22 ноября 2026. Чистый source `392706355193b7544132e2296f7ef8572e1f3c10`,
PR #70 уже слит. Устройства не открывались, firmware не записывалась.
[Процедура и exact hashes](../../../ESP32_OTA_BENCH_3927063.md).

- [Main CI](main-ci.json): пять завершённых workflows, 15/15 jobs success.
  ESP-IDF matrix включает 16 сборок, host и sanitizer — по159 tests.
- [Четыре новые сборки](builds.json): rev12-ota trace/worker8192, control/TLS,
  SDK6.1.1 local и SDK5.3.2 pinned Docker. Реальные BIN/ELF descriptor/SHA,
  Flash16MiB/profile/locks/config/partition/compiler frames/monitor-owner PASS.
  6.1.1 checkout `051e4c4eb20346e043cdd47bc388fde75ff8af63` (v6.1.1).
  5.3.2 image `espressif/idf@sha256:5b8f58d9d9caa45b689a31a0b6a0cc53b84594e0ba5fdd8d28eeef7d6123242a`.
- [Checks и private script hashes](checks.json): 39/39 procedure tests с RAM SDK/
  UART doubles; stale-control500/501/UINT32_MAX запрещает TLS,499 принимается.
  Strict analyzer byte-identical прежнему. Actual app_main clock/report/stale
  tests3/3 повторены; полной новой host matrix при docs-only изменении нет.

Source для четырёх firmware builds фиксирован до изменения документации.
Новый PR меняет только инструкции/план/evidence и обязательность документа в CI,
не исполняемую firmware. Compiler frames не full stack peak, host fixture не
RAM reserve или аппаратный результат. TLS/crypto/install/runtime budget ещё
не подтверждён; CHECK/INSTALL без owner settings остаются закрытыми.

Пакет одноразовый, отдельно согласуется. Предварительное разрешение подключить
плату не является разрешением записать exact image. Raw UART, dumps/NVS,
адреса/идентификаторы устройства, binaries и authorization не опубликованы.
