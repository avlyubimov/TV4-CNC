# PR-04l — software checks, 19 ноября 2026

База main82743d2de2f3756058c94ae216078f093f7093b5 (PR #23 слит, CI4workflows success).
Результаты рабочего дерева до commit ветки fix/tls-bench-headroom. Локальные
project_version82743d2-dirty (6.1.1), fallback1 (Docker5.3.2, worktree Git metadata
вне mount); точные source hashes ниже и matrix.json. Это не release/device images.
[Контракт, изменения и hardware gate](../../../TLS_HEADROOM.md).

## Результаты

- Host CTest17/17 PASS, ASan/UBSan/LeakSanitizer17/17 PASS с detect_leaks=1
  вне ptrace sandbox, без suppressions. host.log/sanitizers.log.
- Новый read-only online accessor: все6 состояний, stale synthetic IP при
  NET_OFF/SCANNING/CONNECTING/FAILED, полный16-network snapshot, fixture reset
  и отсутствие изменения snapshot. Host fixture не моделирует native mutex/radio.
- Сохранены HEAD time/cancel/deadline/status/open/header/cleanup/final-clock
  mock cases, UI/layout XRGB8888/RGB565 и Wi-Fi profiles/storage image tests.
  Реальных сетевых вызовов/сертификатов эти mocks не исполняют.
- Validator10tests PASS, прежние negative config/CA/date/allocator/record/reserve
  guards сохранены. -fanalyzer syntax-only portable probe/network/native host
  stub PASS; ESP worker body проверен SDK компиляцией, не host execution.
- Восемь build/validation PASS: idf-*.json/log (только финал build log, полные
  локально вне Git). Все ELF entry points/profile gates/date funcs/workspace
  inspected. Target workspace34000bytes, не runtime allocation test.
- В обоих TLS ELF retained wifi_service_online и worker; обычные profiles worker
  off. Все SDK configurations/locks/partition/ordinary overlays неизменны от main;
  effective ordinary rev12 sdkconfig6.1.1 сравнен с1bfc7e2, differences0.
- Оба SDK parser/layout checks PASS, активная factory2MiB не менялась.
  YAML/existing CI matrix symbol code/relative links/diff checks PASS.

| SDK | Profile | App bytes | TLS probe | Build/validation |
|---|---|---:|---|---|
|6.1.1|headless|1055488|off|PASS|
|6.1.1|rev12|1573568|off|PASS|
|6.1.1|rev12-memory-bench|1575216|off|PASS|
|6.1.1|rev12-tls-bench|1578432|on|PASS|
|5.3.2|headless|1069920|off|PASS|
|5.3.2|rev12|1529280|off|PASS|
|5.3.2|rev12-memory-bench|1531040|off|PASS|
|5.3.2|rev12-tls-bench|1534288|on|PASS|

## Native stack frame, не runtime stack maximum

| SDK | Before, bytes | After, bytes | Direct frame reduction | Allocated worker stack |
|---|---:|---:|---:|---:|
|6.1.1|1136|208|928|6144|
|5.3.2|1136|208|928|6144|

frames.json/frame-*.txt — entry instruction в реальном utc_tls_bench.c.obj.
Before source byte-for-byte соответствует1bfc7e2; after — это рабочее дерево.
Один и тот же официальный toolchain/SDK внутри каждой строки. Это собственный
кадр функции, не суммарная глубина TLS/SDK/logging/IRQ, не high-watermark и не
обещание runtime экономии ровно928bytes. Firmware stack allocation не менялась.
Дополнительные phase logs также входят в будущий runtime замер.

HTTP RX request4096→2048 экономит2048 запрошенных bytes receive buffer. Это
не measured internal peak improvement: fragmentation/fallback/parser realloc/
параллельные HMI/Wi-Fi/SDK могут изменить реальный minimum. HTTP RX не ограничивает
TLS record16384 или допустимый header value8192. В обоих SDK просмотрены
esp_http_client.c (RX malloc/read/parser callbacks) и http_utils.c (fragment append).
Live oversized/fragmented headers на новом image ещё не проверялись.

## Воспроизведение/версии

./tools/build_idf.sh PROFILE SDK local (6.1.1), docker (5.3.2), четыре existing
profiles с раздельными sdkconfig. Host CMake/Ninja -j4/LVGL_DIR=../lvgl, ctest.
Sanitized C flags -fsanitize=address,undefined -fno-omit-frame-pointer,
link -fsanitize=address,undefined, ASAN_OPTIONS=detect_leaks=1 вне ptrace.

SDK6.1.1 установленный exact v6.1.1, git051e4c4eb20346e043cdd47bc388fde75ff8af63.
5.3.2 official espressif/idf@sha256:5b8f58d9d9caa45b689a31a0b6a0cc53b84594e0ba5fdd8d28eeef7d6123242a,
git9d7f2d69f50d1288526d4f1027108e314e8c879f. CI6.1.1 image digest прежний.
LVGL9.2.2 commit7f07a129e8d77f4984fff8e623fd5be18ff42e74/touch1.2.1/GT9111.1.3 и
per-SDK locks прежние. Host compiler/CMake/Ninja/OpenSSL — versions.txt.
Изменённые runtime/test исходники — source-files.sha256, build JSON содержит
хеши бинарников; SDK mbedTLS allocator disassembly по-прежнему caps0x404.

## Границы

В этом PR не было device/serial/reset/Flash/SNTP/HEAD probes. На плате остаётся
**ordinary1bfc7e2**, owner-confirmed HMI/touch/Wi-Fi. Его исторические minima
3123/window,2408/history,stack912 не заменяются compile frame/RX request size.
Новые peak/stack/CPU/latency/fragmented-header/failure/soak результаты UNKNOWN.
До отдельно согласованного exact-image теста production OTA budget/worker/body/
installer не разрешены. Нет новых ключей/eFuse/security/layout/CAN/motion/release.
